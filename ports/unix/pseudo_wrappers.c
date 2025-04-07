/*
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */
FILE *
popen(const char *command, const char *mode) {
	sigset_t saved;
	
	FILE *rc = NULL;

	if (!pseudo_check_wrappers() || !real_popen) {
		/* rc was initialized to the "failure" value */
		pseudo_enosys("popen");
		return rc;
	}

	pseudo_debug(PDBGF_WRAPPER, "called: popen\n");
	pseudo_sigblock(&saved);
	if (pseudo_getlock()) {
		errno = EBUSY;
		sigprocmask(SIG_SETMASK, &saved, NULL);
		return NULL;
	}

	int save_errno;
	/* exec*() use this to restore the sig mask */
	pseudo_saved_sigmask = saved;
	rc = wrap_popen(command, mode);
	
	save_errno = errno;
	pseudo_droplock();
	sigprocmask(SIG_SETMASK, &saved, NULL);
#if 0
/* This can cause hangs on some recentish systems which use locale
 * stuff for strerror...
 */
	pseudo_debug(PDBGF_WRAPPER, "completed: popen (maybe: %s)\n", strerror(save_errno));
#endif
	pseudo_debug(PDBGF_WRAPPER, "completed: popen (errno: %d)\n", save_errno);
	errno = save_errno;
	return rc;
}

static FILE *
wrap_popen(const char *command, const char *mode) {
	FILE *rc = NULL;
	
	

#include "guts/popen.c"

	return rc;
}


#undef NFTW_NAME
#undef NFTW_STAT_NAME
#undef NFTW_STAT_STRUCT
#undef NFTW_LSTAT_NAME
#define NFTW_NAME nftw
#define NFTW_STAT_NAME stat
#define NFTW_STAT_STRUCT stat
#define NFTW_LSTAT_NAME lstat
#include "guts/nftw_wrapper_base.c"
