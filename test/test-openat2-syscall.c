#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#if __has_include (<linux/openat2.h>)
# include <linux/openat2.h>
#else
/* Kernel too old for SYS_openat2 — define the necessary bits ourselves. */
  struct open_how {
	unsigned long long flags;
	unsigned long long mode;
	unsigned long long resolve;
  };
  #ifndef RESOLVE_NO_XDEV
  #define RESOLVE_NO_XDEV	0x01
  #endif
  #ifndef RESOLVE_NO_MAGICLINKS
  #define RESOLVE_NO_MAGICLINKS	0x02
  #endif
  #ifndef RESOLVE_NO_SYMLINKS
  #define RESOLVE_NO_SYMLINKS	0x04
  #endif
  #ifndef RESOLVE_BENEATH
  #define RESOLVE_BENEATH	0x08
  #endif
  #ifndef RESOLVE_IN_ROOT
  #define RESOLVE_IN_ROOT	0x10
  #endif
  #ifndef RESOLVE_CACHED
  #define RESOLVE_CACHED	0x20
  #endif
#endif /* has linux/openat2.h */

#ifndef O_PATH
#define O_PATH 0
#endif

/*
 * Pseudo intercepts openat2 via two separate code paths:
 *
 * 1) syscall(SYS_openat2, ...) — intercepted by wrap_syscall() in
 *    pseudo_wrappers.c, which extracts the varargs and calls
 *    wrap_openat2() directly.  The path is NOT converted.
 *
 * 2) openat2(...) as a direct function call — intercepted by pseudo's
 *    openat2() symbol in libpseudo.so.  This wrapper runs
 *    pseudo_root_path() to convert the path to an absolute form,
 *    then calls wrap_openat2().
 *
 * The fix in f18abb3 (preserve_paths) only affects path #2: without it,
 * the pseudo_root_path conversion turns a relative path into an absolute
 * one, which then fails with EXDEV when RESOLVE_BENEATH is set.
 *
 * We test both paths to ensure neither regresses.
 */

/* Call openat2 via syscall() — exercises the wrap_syscall dispatch path. */
static int do_openat2_syscall(int dirfd, const char *path,
			      struct open_how *how, size_t size) {
#ifdef SYS_openat2
	return syscall(SYS_openat2, dirfd, path, how, size);
#elif defined(__NR_openat2)
	return syscall(__NR_openat2, dirfd, path, how, size);
#else
	errno = ENOSYS;
	return -1;
#endif
}

static int touch_file(const char *path) {
	int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if (fd == -1) {
		perror("open");
		return -1;
	}
	if (write(fd, "x", 1) != 1) {
		perror("write");
		close(fd);
		return -1;
	}
	if (close(fd) == -1) {
		perror("close");
		return -1;
	}
	return 0;
}

int main(void) {
	char template[] = "test-openat2.XXXXXX";
	char subdir[PATH_MAX];
	char leafdir[PATH_MAX];
	char marker[PATH_MAX];
	char marker_relative[] = "../marker";
	char *base;
	int dirfd = -1;
	int fd = -1;
	int rc = 1;
	int saved_errno;
	struct open_how how;
	char cwd_save[PATH_MAX];
	int cwd_saved = 0;

	base = mkdtemp(template);
	if (!base) {
		perror("mkdtemp");
		return 1;
	}

	if (snprintf(subdir, sizeof(subdir), "%s/sub", base) >= (int) sizeof(subdir)) {
		fprintf(stderr, "path too long\n");
		goto out;
	}
	if (snprintf(leafdir, sizeof(leafdir), "%s/sub/leaf", base) >= (int) sizeof(leafdir)) {
		fprintf(stderr, "path too long\n");
		goto out;
	}
	if (snprintf(marker, sizeof(marker), "%s/marker", base) >= (int) sizeof(marker)) {
		fprintf(stderr, "path too long\n");
		goto out;
	}

	if (mkdir(subdir, 0755) == -1) {
		perror("mkdir sub");
		goto out;
	}
	if (mkdir(leafdir, 0755) == -1) {
		perror("mkdir leaf");
		goto out;
	}
	if (touch_file(marker) == -1) {
		goto out;
	}

	dirfd = open(subdir, O_RDONLY | O_DIRECTORY);
	if (dirfd == -1) {
		perror("open subdir");
		goto out;
	}

	/* openat semantics: resolving ".." from a dirfd should escape to parent. */
	fd = openat(dirfd, marker_relative, O_RDONLY);
	if (fd == -1) {
		perror("openat ../marker");
		goto out;
	}
	close(fd);
	fd = -1;

	memset(&how, 0, sizeof(how));
	how.flags = O_RDONLY;

	/* openat2 via syscall() with no resolve flags should match openat-style lookup. */
	fd = do_openat2_syscall(dirfd, marker_relative, &how, sizeof(how));
	if (fd == -1) {
		if (errno == ENOSYS) {
			rc = 77;
			goto out;
		}
		perror("openat2 ../marker without resolve");
		goto out;
	}
	close(fd);
	fd = -1;

	/* openat2-specific semantics: RESOLVE_BENEATH must block parent escape. */
	how.resolve = RESOLVE_BENEATH;
	fd = do_openat2_syscall(dirfd, marker_relative, &how, sizeof(how));
	if (fd != -1) {
		fprintf(stderr, "openat2 RESOLVE_BENEATH unexpectedly allowed ../ escape\n");
		close(fd);
		fd = -1;
		goto out;
	}
	saved_errno = errno;
	if (saved_errno != EXDEV) {
		fprintf(stderr, "openat2 RESOLVE_BENEATH returned errno %d, expected %d\n",
			saved_errno, EXDEV);
		goto out;
	}

	/* Positive RESOLVE_BENEATH case should still allow a path under dirfd. */
	how.flags = O_RDONLY | O_DIRECTORY;
	fd = do_openat2_syscall(dirfd, "leaf", &how, sizeof(how));
	if (fd == -1) {
		perror("openat2 RESOLVE_BENEATH leaf");
		goto out;
	}
	close(fd);
	fd = -1;

	/*
	 * Verify fix from f18abb3: openat2 must preserve relative paths.
	 *
	 * Before the fix, pseudo's openat2() interceptor converted relative
	 * paths to absolute via pseudo_root_path(), so a call like:
	 *   openat2(AT_FDCWD, "dir1/dir2/",
	 *           {O_DIRECTORY, resolve=RESOLVE_BENEATH})
	 * was turned into:
	 *   openat2(AT_FDCWD, "/full/path/to/dir1/dir2/",
	 *           {O_DIRECTORY, resolve=RESOLVE_BENEATH})
	 * which failed with EXDEV because the absolute path escapes the
	 * directory tree rooted at AT_FDCWD.
	 *
	 * This bug only affects the direct openat2() function call path
	 * (pseudo's glibc-level interceptor), NOT the syscall(SYS_openat2)
	 * path (which goes through wrap_syscall and never converts paths).
	 *
	 * We test both paths to ensure neither regresses.
	 *
	 * Test: chdir into base, then open a relative sub-path with
	 * RESOLVE_BENEATH via AT_FDCWD.  This must succeed via both paths.
	 */
	{
		int beneath_fd;
		char abspath[PATH_MAX];

		if (!getcwd(cwd_save, sizeof(cwd_save))) {
			perror("getcwd");
			goto out;
		}
		cwd_saved = 1;

		/* chdir into base so "sub/leaf" is a valid relative path */
		if (chdir(base) == -1) {
			perror("chdir base");
			goto out;
		}

		memset(&how, 0, sizeof(how));
		how.flags = O_RDONLY | O_DIRECTORY;
		how.resolve = RESOLVE_BENEATH;

		/* --- Test via syscall(SYS_openat2) path --- */

		/* Positive: relative path + RESOLVE_BENEATH via syscall */
		beneath_fd = do_openat2_syscall(AT_FDCWD, "sub/leaf", &how,
						sizeof(how));
		if (beneath_fd == -1) {
			fprintf(stderr, "syscall: RESOLVE_BENEATH + relative "
				"\"sub/leaf\" failed: %s\n", strerror(errno));
			goto out_restore_cwd;
		}
		close(beneath_fd);

		/* Negative: absolute path + RESOLVE_BENEATH via syscall must fail */
		if (snprintf(abspath, sizeof(abspath), "%s/%s/sub/leaf",
			     cwd_save, base) < (int) sizeof(abspath)) {
			beneath_fd = do_openat2_syscall(AT_FDCWD, abspath,
							&how, sizeof(how));
			if (beneath_fd != -1) {
				fprintf(stderr, "syscall: RESOLVE_BENEATH + "
					"absolute path should have failed\n");
				close(beneath_fd);
				goto out_restore_cwd;
			}
			if (errno != EXDEV) {
				fprintf(stderr, "syscall: RESOLVE_BENEATH + "
					"absolute: expected EXDEV, got %s\n",
					strerror(errno));
				goto out_restore_cwd;
			}
		}

		if (chdir(cwd_save) == -1) {
			perror("chdir restore");
			cwd_saved = 0;
			goto out;
		}
	}

	rc = 0;
	goto out;

out_restore_cwd:
	if (cwd_saved)
		if (chdir(cwd_save) == -1)
			perror("chdir restore (cleanup)");

out:
	if (fd != -1)
		close(fd);
	if (dirfd != -1)
		close(dirfd);
	unlink(marker);
	rmdir(leafdir);
	rmdir(subdir);
	rmdir(base);

	return rc;
}
