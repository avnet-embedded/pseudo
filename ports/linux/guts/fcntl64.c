/*
 * Copyright (c) 2008-2010 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * static int
 * wrap_fcntl64(int fd, int cmd, ...struct flock *lock) {
 *	int rc = -1;
 */
#if !defined(F_GETPIPE_SZ)
#define F_GETPIPE_SZ (1032)
#endif

#if F_GETPIPE_SZ != 1032
#error System F_GETPIPE_SZ has unexpected value
#endif

#if !defined(F_SETPIPE_SZ)
#define F_SETPIPE_SZ (1031)
#endif

#if F_SETPIPE_SZ != 1031
#error System F_SETPIPE_SZ has unexpected value
#endif

 	long arg;
	int save_errno;

	/* we don't know whether we need lock or arg; grab both, which
	 * should be safe enough on Linuxy systems. */
	va_start(ap, cmd);
	arg = va_arg(ap, long);
	va_end(ap);

	switch (cmd) {
	case F_DUPFD:
#ifdef F_DUPFD_CLOEXEC
	case F_DUPFD_CLOEXEC:
#endif
		/* actually do something */
		rc = real_fcntl64(fd, cmd, arg);
		save_errno = errno;
		if (rc != -1) {
			pseudo_debug(PDBGF_OP, "fcntl64_dup: %d->%d\n", fd, rc);
			pseudo_client_op(OP_DUP, 0, fd, rc, 0, 0);
		}
		errno = save_errno;
		break;
	case F_SETPIPE_SZ:
		/* actually do something */
		rc = real_fcntl64(fd, cmd, arg);
		break;
	/* no argument: */
	case F_GETFD:
	case F_GETFL:
	case F_GETOWN:
	case F_GETSIG:
	case F_GETLEASE:
	case F_GETPIPE_SZ:
		rc = real_fcntl64(fd, cmd);
		break;
	/* long argument */
	case F_SETFD:
	case F_SETFL:
	case F_SETOWN:
	case F_SETSIG:
	case F_SETLEASE:
	case F_NOTIFY:
		rc = real_fcntl64(fd, cmd, arg);
		break;
	/* struct flock * argument */
	case F_GETLK:
	case F_SETLK:
	case F_SETLKW:
#ifdef F_OFD_GETLK
	case F_OFD_GETLK:
	case F_OFD_SETLK:
	case F_OFD_SETLKW:
#endif
		rc = real_fcntl64(fd, cmd, lock);
		break;
#if defined(F_GETLK64) && (F_GETLK64 != F_GETLK)
	/* the cast is safe, all struct pointers must smell the same */
	case F_GETLK64:
	case F_SETLK64:
	case F_SETLKW64:
		rc = real_fcntl64(fd, cmd, (struct flock64 *) lock);
		break;
#endif
	default:
		pseudo_error("unknown fcntl64 argument %d, assuming long argument.\n",
			cmd);
		rc = real_fcntl64(fd, cmd, arg);
		break;
	}
/*	return rc;
 * }
 */
