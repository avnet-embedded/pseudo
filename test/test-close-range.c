/*
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * close_range() was stubbed out to return ENOSYS unconditionally, on the
 * assumption that callers all cope with that. Current systemd does not: it
 * dropped its /proc fallback once its kernel baseline reached 5.10, so every
 * fork+exec under pseudo failed. Check the wrapper works, that it agrees
 * with the kernel about bad arguments, that CLOSE_RANGE_UNSHARE keeps a
 * process sharing the descriptor table unaffected, and that pseudo still
 * functions after a caller closes every descriptor from 3 up.
 */
#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* <linux/close_range.h> is missing on hosts with pre-5.9 kernel headers */
#ifndef CLOSE_RANGE_UNSHARE
#define CLOSE_RANGE_UNSHARE (1U << 1)
#endif
#ifndef CLOSE_RANGE_CLOEXEC
#define CLOSE_RANGE_CLOEXEC (1U << 2)
#endif

/* matches test-openat2-syscall: the shell wrapper turns 77 into "skipped" */
#define SKIP 77

#define TESTFILE "test-close-range-file"

static int open_null(void) {
	int fd = open("/dev/null", O_RDONLY);
	if (fd < 0)
		perror("open /dev/null");
	return fd;
}

static int is_closed(int fd) {
	return fcntl(fd, F_GETFD) == -1 && errno == EBADF;
}

/* Is close_range() there at all? The wrapper runs this with PSEUDO_DISABLED
 * set, so the answer is the kernel's and a stubbed wrapper cannot pass
 * itself off as an old kernel.
 */
static int probe(void) {
	int fd = open_null();

	if (fd < 0)
		return 1;
	if (close_range(fd, fd, 0) == -1) {
		int err = errno;
		close(fd);
		return (err == ENOSYS) ? SKIP : 1;
	}
	return 0;
}

/* CLOSE_RANGE_CLOEXEC is newer than the syscall: 5.11 rather than 5.9. Probe
 * for it separately, the same way, so that on a 5.9 or 5.10 kernel only the
 * CLOSE_RANGE_CLOEXEC part of the test is left out, not the whole thing.
 */
static int probe_cloexec(void) {
	int fd = open_null();
	int rc;

	if (fd < 0)
		return 1;
	rc = close_range(fd, fd, CLOSE_RANGE_CLOEXEC);
	if (rc == -1) {
		int err = errno;
		close(fd);
		return (err == EINVAL || err == ENOSYS) ? SKIP : 1;
	}
	close(fd);
	return 0;
}

/* for the CLONE_FILES children: close one descriptor, with or without
 * CLOSE_RANGE_UNSHARE, and report whether it is gone for the child itself.
 */
struct child_close {
	int fd;
	int flags;
};

static char child_stack[65536];

static int child_close_one(void *arg) {
	struct child_close *cc = arg;

	if (close_range(cc->fd, cc->fd, cc->flags) == -1)
		return 2;
	return is_closed(cc->fd) ? 0 : 3;
}

int main(int argc, char **argv) {
	struct child_close cc;
	struct stat st;
	int a, b, c;
	int status;
	int test_cloexec = 1;
	pid_t pid;

	if (argc > 1 && !strcmp(argv[1], "--probe"))
		return probe();
	if (argc > 1 && !strcmp(argv[1], "--probe-cloexec"))
		return probe_cloexec();
	if (argc > 1 && !strcmp(argv[1], "--no-cloexec"))
		test_cloexec = 0;

	/* the reported bug: this used to fail with ENOSYS every time */
	a = open_null();
	if (a < 0)
		return 1;
	if (close_range(a, a, 0) == -1) {
		fprintf(stderr, "close_range(%d, %d, 0): %s\n", a, a, strerror(errno));
		return 1;
	}
	if (!is_closed(a)) {
		fprintf(stderr, "close_range() left fd %d open\n", a);
		return 1;
	}

	/* a bounded range closes all of itself */
	a = open_null();
	b = open_null();
	c = open_null();
	if (a < 0 || b < 0 || c < 0)
		return 1;
	if (close_range(a, c, 0) == -1) {
		perror("close_range(a, c, 0)");
		return 1;
	}
	if (!is_closed(a) || !is_closed(b) || !is_closed(c)) {
		fprintf(stderr, "close_range(%d, %d, 0) left descriptors open\n", a, c);
		return 1;
	}

	/* CLOSE_RANGE_CLOEXEC marks descriptors, it does not close them. The
	 * flag needs a 5.11 kernel; the shell wrapper passes --no-cloexec
	 * when the probe says this kernel has the syscall but not the flag.
	 */
	if (test_cloexec) {
		a = open_null();
		if (a < 0)
			return 1;
		if (close_range(a, a, CLOSE_RANGE_CLOEXEC) == -1) {
			perror("close_range(a, a, CLOSE_RANGE_CLOEXEC)");
			return 1;
		}
		if (is_closed(a)) {
			fprintf(stderr, "CLOSE_RANGE_CLOEXEC closed fd %d\n", a);
			return 1;
		}
		if (!(fcntl(a, F_GETFD) & FD_CLOEXEC)) {
			fprintf(stderr, "CLOSE_RANGE_CLOEXEC did not set FD_CLOEXEC on fd %d\n", a);
			return 1;
		}
		close(a);
	}

	/* bad arguments are refused, and a refused call closes nothing */
	a = open_null();
	if (a < 0)
		return 1;
	if (close_range(a, a, 0x80) != -1 || errno != EINVAL) {
		fprintf(stderr, "close_range() with an unknown flag should fail EINVAL\n");
		return 1;
	}
	if (is_closed(a)) {
		fprintf(stderr, "a rejected close_range() closed fd %d anyway\n", a);
		return 1;
	}
	if (close_range(a + 1, a, 0) != -1 || errno != EINVAL) {
		fprintf(stderr, "close_range() with lowfd > maxfd should fail EINVAL\n");
		return 1;
	}
	close(a);

	/* a range entirely above INT_MAX holds no descriptors at all, but it
	 * still has to be accepted rather than mangled on the way through
	 */
	if (close_range((unsigned int) INT_MAX + 1, ~0U, 0) == -1) {
		fprintf(stderr, "close_range(INT_MAX+1, ~0U, 0): %s\n", strerror(errno));
		return 1;
	}

	/* CLOSE_RANGE_UNSHARE unshares the descriptor table before closing:
	 * when a child cloned with CLONE_FILES closes our shared descriptor
	 * with the flag set, we must still hold that descriptor afterwards.
	 * This is what the wrapper's unshare-first order preserves; closing
	 * by hand before unsharing would go through the still-shared table.
	 */
	a = open_null();
	if (a < 0)
		return 1;
	cc.fd = a;
	cc.flags = CLOSE_RANGE_UNSHARE;
	pid = clone(child_close_one, child_stack + sizeof(child_stack),
		    CLONE_FILES | SIGCHLD, &cc);
	if (pid == -1) {
		perror("clone(CLONE_FILES)");
		return 1;
	}
	if (waitpid(pid, &status, 0) == -1) {
		perror("waitpid");
		return 1;
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		fprintf(stderr, "CLOSE_RANGE_UNSHARE failed in the child (status %d)\n",
			status);
		return 1;
	}
	if (is_closed(a)) {
		fprintf(stderr, "child's CLOSE_RANGE_UNSHARE closed the parent's fd %d\n", a);
		return 1;
	}

	/* the control for the test above: the same close without the flag
	 * happens in the shared table, so this time fd a must be gone for
	 * us as well. A clone that quietly stopped sharing the table would
	 * pass the test above no matter what the wrapper did; it fails here
	 * instead.
	 */
	cc.flags = 0;
	pid = clone(child_close_one, child_stack + sizeof(child_stack),
		    CLONE_FILES | SIGCHLD, &cc);
	if (pid == -1) {
		perror("clone(CLONE_FILES)");
		return 1;
	}
	if (waitpid(pid, &status, 0) == -1) {
		perror("waitpid");
		return 1;
	}
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
		fprintf(stderr, "close_range() failed in the CLONE_FILES child (status %d)\n",
			status);
		return 1;
	}
	if (!is_closed(a)) {
		fprintf(stderr, "close in a CLONE_FILES child did not reach the parent's fd %d\n", a);
		return 1;
	}

	/* And the point of the care in the wrapper: pseudo keeps descriptors
	 * of its own in this range and has to come out of it still working.
	 */
	a = open(TESTFILE, O_CREAT | O_WRONLY, 0644);
	if (a < 0) {
		perror("open " TESTFILE);
		return 1;
	}
	close(a);
	if (chown(TESTFILE, 0, 0) == -1) {
		perror("chown " TESTFILE);
		return 1;
	}
	if (close_range(3, ~0U, 0) == -1) {
		fprintf(stderr, "close_range(3, ~0U, 0): %s\n", strerror(errno));
		return 1;
	}
	if (stat(TESTFILE, &st) == -1) {
		perror("stat after close_range");
		return 1;
	}
	if (st.st_uid != 0 || st.st_gid != 0) {
		fprintf(stderr, "pseudo lost track after close_range: uid %d, gid %d\n",
			(int) st.st_uid, (int) st.st_gid);
		return 1;
	}
	unlink(TESTFILE);

	return 0;
}
