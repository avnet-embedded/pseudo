/*
 * Copyright (c) 2026 Yocto Project; see
 * guts/COPYRIGHT for information.
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * static FTSENT *
 * wrap_fts_read(FTS *ftsp) {
 *	FTSENT * rc = NULL;
 */

	rc = real_fts_read(ftsp);

	/* glibc's fts_read calls fstatat internally using glibc-private
	 * symbols, which bypass pseudo's LD_PRELOAD wrappers. We need to
	 * re-stat the entry through pseudo so that pseudo-tracked
	 * ownership and permissions are visible to callers.
	 */
	if (rc && rc->fts_statp && rc->fts_accpath) {
		pseudo_msg_t *msg;
		int save_errno = errno;
		const char *fts_rpath;
		PSEUDO_STATBUF buf64;

		switch (rc->fts_info) {
		case FTS_F:
		case FTS_D:
		case FTS_DP:
		case FTS_SL:
		case FTS_SLNONE:
		case FTS_DEFAULT:
			/* fts_open passes real (chroot-resolved) paths to real_fts_open,
			 * so fts_path is already a real filesystem path. We must use
			 * it directly without PSEUDO_ROOT_PATH, which would
			 * incorrectly prepend the chroot prefix a second time.
			 *
			 * However, for the non-chroot case (or when fts was given
			 * relative paths that are still relative in fts_path),
			 * we still need PSEUDO_ROOT_PATH to make them absolute.
			 */
			if (rc->fts_path[0] == '/' && pseudo_chroot_len &&
			    !memcmp(rc->fts_path, pseudo_chroot, pseudo_chroot_len) &&
			    (rc->fts_path[pseudo_chroot_len] == '/' ||
			     rc->fts_path[pseudo_chroot_len] == '\0')) {
				/* Already a real path with chroot prefix */
				fts_rpath = rc->fts_path;
			} else {
				fts_rpath = PSEUDO_ROOT_PATH(AT_FDCWD, rc->fts_path, AT_SYMLINK_NOFOLLOW);
			}
			if (fts_rpath) {
				pseudo_stat64_from32(&buf64, rc->fts_statp);
				msg = pseudo_client_op(OP_STAT, 0, -1, -1, fts_rpath, &buf64);
				if (msg && msg->result == RESULT_SUCCEED) {
					pseudo_stat_msg(&buf64, msg);
					pseudo_stat32_from64(rc->fts_statp, &buf64);
				}
			}
			break;
		default:
			break;
		}

		errno = save_errno;
	}

/*	return rc;
 * }
 */
