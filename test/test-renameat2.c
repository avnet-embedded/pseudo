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

#ifndef RENAME_NOREPLACE
#define RENAME_NOREPLACE (1 << 0)
#endif
#ifndef RENAME_EXCHANGE
#define RENAME_EXCHANGE (1 << 1)
#endif

static int do_renameat2(int olddirfd, const char *oldpath,
			int newdirfd, const char *newpath,
			unsigned int flags) {
#ifdef SYS_renameat2
	return syscall(SYS_renameat2, olddirfd, oldpath, newdirfd, newpath, flags);
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

static int file_exists(int dirfd, const char *name) {
	struct stat st;
	return fstatat(dirfd, name, &st, AT_SYMLINK_NOFOLLOW) == 0;
}

int main(void) {
	char template[] = "test-renameat2.XXXXXX";
	char *base;
	int dirfd = -1;
	int rc = 1;
	int ret;
	int renameat2_works = 1;

	base = mkdtemp(template);
	if (!base) {
		perror("mkdtemp");
		return 1;
	}

	if (chdir(base) == -1) {
		perror("chdir to base");
		rmdir(base);
		return 1;
	}

	dirfd = open(".", O_RDONLY | O_DIRECTORY);
	if (dirfd == -1) {
		perror("open base dir");
		goto out;
	}

	/* ---------------------------------------------------------- */
	/* Part 1: renameat (classic) semantics via dirfd             */
	/* ---------------------------------------------------------- */

	if (touch_file("fileA") == -1)
		goto out_in_base;

	/* Rename fileA -> fileB using renameat with dirfd */
	if (renameat(AT_FDCWD, "fileA", AT_FDCWD, "fileB") == -1) {
		perror("renameat fileA -> fileB");
		goto out_in_base;
	}
	if (file_exists(AT_FDCWD, "fileA")) {
		fprintf(stderr, "renameat: source still exists after rename\n");
		goto out_in_base;
	}
	if (!file_exists(AT_FDCWD, "fileB")) {
		fprintf(stderr, "renameat: destination missing after rename\n");
		goto out_in_base;
	}
	unlink("fileB");

	/* Same test but with actual dirfd instead of AT_FDCWD */
	if (touch_file("fileC") == -1)
		goto out_in_base;

	if (renameat(dirfd, "fileC", dirfd, "fileD") == -1) {
		perror("renameat dirfd fileC -> fileD");
		goto out_in_base;
	}
	if (file_exists(dirfd, "fileC")) {
		fprintf(stderr, "renameat dirfd: source still exists\n");
		goto out_in_base;
	}
	if (!file_exists(dirfd, "fileD")) {
		fprintf(stderr, "renameat dirfd: destination missing\n");
		goto out_in_base;
	}
	unlinkat(dirfd, "fileD", 0);

	/* ---------------------------------------------------------- */
	/* Part 2: renameat2 with flags=0 (should match renameat)     */
	/* ---------------------------------------------------------- */

	if (touch_file("fileE") == -1)
		goto out_in_base;

	ret = do_renameat2(AT_FDCWD, "fileE", AT_FDCWD, "fileF", 0);
	if (ret == -1) {
		if (errno == ENOSYS) {
			/* renameat2 not available (kernel too old or pseudo stub) */
			renameat2_works = 0;
			unlink("fileE");
		} else {
			perror("renameat2 flags=0 fileE -> fileF");
			goto out_in_base;
		}
	} else {
		if (file_exists(AT_FDCWD, "fileE")) {
			fprintf(stderr, "renameat2 flags=0: source still exists\n");
			goto out_in_base;
		}
		if (!file_exists(AT_FDCWD, "fileF")) {
			fprintf(stderr, "renameat2 flags=0: destination missing\n");
			goto out_in_base;
		}
		unlink("fileF");
	}

	if (!renameat2_works) {
		/* renameat2 returned ENOSYS; skip the remaining renameat2-specific tests */
		rc = 0;
		goto out;
	}

	/* ---------------------------------------------------------- */
	/* Part 3: RENAME_NOREPLACE — must fail when target exists    */
	/* ---------------------------------------------------------- */

	if (touch_file("src_noreplace") == -1)
		goto out_in_base;
	if (touch_file("dst_noreplace") == -1)
		goto out_in_base;

	ret = do_renameat2(AT_FDCWD, "src_noreplace",
			   AT_FDCWD, "dst_noreplace", RENAME_NOREPLACE);
	if (ret != -1) {
		fprintf(stderr, "RENAME_NOREPLACE: should have failed when target exists\n");
		goto out_in_base;
	}
	if (errno != EEXIST) {
		fprintf(stderr, "RENAME_NOREPLACE: expected EEXIST, got %s\n",
			strerror(errno));
		goto out_in_base;
	}
	/* Both files must still exist */
	if (!file_exists(AT_FDCWD, "src_noreplace") ||
	    !file_exists(AT_FDCWD, "dst_noreplace")) {
		fprintf(stderr, "RENAME_NOREPLACE: files disappeared\n");
		goto out_in_base;
	}

	/* Positive case: RENAME_NOREPLACE succeeds when target does not exist */
	unlink("dst_noreplace");
	ret = do_renameat2(AT_FDCWD, "src_noreplace",
			   AT_FDCWD, "dst_noreplace", RENAME_NOREPLACE);
	if (ret == -1) {
		perror("RENAME_NOREPLACE (no target)");
		goto out_in_base;
	}
	if (file_exists(AT_FDCWD, "src_noreplace")) {
		fprintf(stderr, "RENAME_NOREPLACE: source still exists\n");
		goto out_in_base;
	}
	if (!file_exists(AT_FDCWD, "dst_noreplace")) {
		fprintf(stderr, "RENAME_NOREPLACE: destination missing\n");
		goto out_in_base;
	}
	unlink("dst_noreplace");

	/* ---------------------------------------------------------- */
	/* Part 4: RENAME_EXCHANGE — atomically swap two paths        */
	/* ---------------------------------------------------------- */

	if (touch_file("swap_a") == -1)
		goto out_in_base;
	if (touch_file("swap_b") == -1)
		goto out_in_base;

	/* Write distinct content so we can verify the swap */
	{
		struct stat sa, sb;
		ino_t ino_a, ino_b;

		if (stat("swap_a", &sa) == -1 || stat("swap_b", &sb) == -1) {
			perror("stat before exchange");
			goto out_in_base;
		}
		ino_a = sa.st_ino;
		ino_b = sb.st_ino;

		ret = do_renameat2(AT_FDCWD, "swap_a",
				   AT_FDCWD, "swap_b", RENAME_EXCHANGE);
		if (ret == -1) {
			perror("RENAME_EXCHANGE");
			goto out_in_base;
		}

		if (stat("swap_a", &sa) == -1 || stat("swap_b", &sb) == -1) {
			perror("stat after exchange");
			goto out_in_base;
		}

		/* After exchange, inodes should be swapped */
		if (sa.st_ino != ino_b || sb.st_ino != ino_a) {
			fprintf(stderr, "RENAME_EXCHANGE: inodes not swapped "
				"(a: %lu->%lu, b: %lu->%lu)\n",
				(unsigned long) ino_a, (unsigned long) sa.st_ino,
				(unsigned long) ino_b, (unsigned long) sb.st_ino);
			goto out_in_base;
		}
	}
	unlink("swap_a");
	unlink("swap_b");

	rc = 0;
	goto out;

out_in_base:
	/* Best-effort cleanup of any leftover files inside the temp dir */
	unlink("fileA"); unlink("fileB"); unlink("fileC"); unlink("fileD");
	unlink("fileE"); unlink("fileF");
	unlink("src_noreplace"); unlink("dst_noreplace");
	unlink("swap_a"); unlink("swap_b");

out:
	if (dirfd != -1)
		close(dirfd);
	if (chdir("..") == 0)
		rmdir(base);

	return rc;
}
