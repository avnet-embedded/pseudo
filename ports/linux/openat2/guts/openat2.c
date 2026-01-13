/*
 * Copyright (c) 2026 Mark Hatle <mark.hatle@amd.com>; see
 * guts/COPYRIGHT for information.
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * int openat2(int dirfd, const char *path, struct open_how *how, size_t size)
 *	int rc = -1;
 */

	(void) dirfd;
	(void) path;
	(void) how;
	(void) size;
	/* for now, let's try just failing out hard, and hope things retry with a
	 * different syscall.
	 */
	errno = ENOSYS;
	rc = -1;

/*	return rc;
 * }
 */
