/*
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */
/* the unix port wants to know that real_stat() and
 * friends exist.  So they do. And because the Linux
 * port really uses stat64 for those...
 */
int
pseudo_stat(const char *path, struct stat *buf) {
	return real___xstat(_STAT_VER, path, buf);
}

int
pseudo_lstat(const char *path, struct stat *buf) {
	return real___lxstat(_STAT_VER, path, buf);
}

int
pseudo_fstat(int fd, struct stat *buf) {
	return real___fxstat(_STAT_VER, fd, buf);
}

int
pseudo_stat64(const char *path, struct stat64 *buf) {
	return real___xstat64(_STAT_VER, path, buf);
}

int
pseudo_lstat64(const char *path, struct stat64 *buf) {
	return real___lxstat64(_STAT_VER, path, buf);
}

int
pseudo_fstat64(int fd, struct stat64 *buf) {
	return real___fxstat64(_STAT_VER, fd, buf);
}

/* similar thing happens with mknod */
int
pseudo_mknod(const char *path, mode_t mode, dev_t dev) {
	return real___xmknod(_MKNOD_VER, path, mode, &dev);
}

int
pseudo_mknodat(int dirfd, const char *path, mode_t mode, dev_t dev) {
	return real___xmknodat(_MKNOD_VER, dirfd, path, mode, &dev);
}

int pseudo_capset(cap_user_header_t hdrp, const cap_user_data_t datap) {
	(void)hdrp;
	(void)datap;

	return 0;
}

long
syscall(long number, ...) {
	long rc = -1;
	va_list ap;

	if (!pseudo_check_wrappers() || !real_syscall) {
		/* rc was initialized to the "failure" value */
		pseudo_enosys("syscall");
		return rc;
	}

#ifdef SYS_renameat2
	/* concerns exist about trying to parse arguments because syscall(2)
	 * specifies strange ABI behaviors. If we can get better clarity on
	 * that, it could make sense to redirect to wrap_renameat2().
	 */
	if (number == SYS_renameat2) {
		errno = ENOSYS;
		return -1;
	}
#else
	(void) number;
#endif

#ifdef SYS_seccomp
	/* pseudo and seccomp are incompatible as pseudo uses different syscalls
	 * so pretend to enable seccomp but really do nothing */
	if (number == SYS_seccomp) {
		unsigned long cmd;
		va_start(ap, number);
		cmd = va_arg(ap, unsigned long);
		va_end(ap);
		if (cmd == SECCOMP_SET_MODE_FILTER) {
		    return 0;
		}
	}
#endif

#ifdef SYS_openat2
	/* concerns exist about trying to parse arguments because syscall(2)
	 * specifies strange ABI behaviors. If we can get better clarity on
	 * that, it could make sense to redirect to wrap_openat2().
	 * There is a CVE patch (CVE-2025-45582) to tar 1.34 in Centos Stream which
	 * uses syscall to access openat2() and breaks builds if we don't redirect.
	 */
	if (number == SYS_openat2) {
		errno = ENOSYS;
		return -1;
	}
#endif

	/* gcc magic to attempt to just pass these args to syscall. we have to
	 * guess about the number of args; the docs discuss calling conventions
	 * up to 7, so let's try that?
	 */
	void *res = __builtin_apply((void (*)(void)) real_syscall, __builtin_apply_args(), sizeof(long) * 7);
	__builtin_return(res);
}

/* unused.
 */
static long wrap_syscall(long nr, va_list ap) {
	(void) nr;
	(void) ap;
	return -1;
}

int
prctl(int option, ...) {
	int rc = -1;
	va_list ap;

	if (!pseudo_check_wrappers() || !real_prctl) {
		/* rc was initialized to the "failure" value */
		pseudo_enosys("prctl");
		return rc;
	}

#ifdef SECCOMP_SET_MODE_FILTER
	/* pseudo and seccomp are incompatible as pseudo uses different syscalls
	 * so pretend to enable seccomp but really do nothing */
	if (option == PR_SET_SECCOMP) {
		unsigned long cmd;
		va_start(ap, option);
		cmd = va_arg(ap, unsigned long);
		va_end(ap);
		if (cmd == SECCOMP_SET_MODE_FILTER) {
		    return 0;
		}
	}
#endif

	/* gcc magic to attempt to just pass these args to prctl. we have to
	 * guess about the number of args; the docs discuss calling conventions
	 * up to 5, so let's try that?
	 */
	void *res = __builtin_apply((void (*)(void)) real_prctl, __builtin_apply_args(), sizeof(long) * 5);
	__builtin_return(res);
}

/* unused.
 */
static int wrap_prctl(int option, va_list ap) {
	(void) option;
	(void) ap;
	return -1;
}

#undef NFTW_NAME
#undef NFTW_STAT_NAME
#undef NFTW_STAT_STRUCT
#undef NFTW_LSTAT_NAME
#define NFTW_NAME nftw64
#define NFTW_STAT_NAME stat64
#define NFTW_STAT_STRUCT stat64
#define NFTW_LSTAT_NAME lstat64
#include "../unix/guts/nftw_wrapper_base.c"
