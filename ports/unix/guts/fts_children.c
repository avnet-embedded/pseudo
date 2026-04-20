/*
 * Copyright (c) 2026 Yocto Project; see
 * guts/COPYRIGHT for information.
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * static FTSENT *
 * wrap_fts_children(FTS *ftsp, int options) {
 *	FTSENT * rc = NULL;
 */

	rc = real_fts_children(ftsp, options);

	/* glibc's fts_children calls fstatat internally using glibc-private
	 * symbols, which bypass pseudo's LD_PRELOAD wrappers. We need to
	 * re-stat each entry through pseudo so that pseudo-tracked
	 * ownership and permissions are visible to callers.
	 */
	FTSENT *p;
	for (p = rc; p != NULL; p = p->fts_link) {
		if (p->fts_statp && p->fts_path) {
			pseudo_msg_t *msg;
			int save_errno = errno;
			const char *fts_rpath;
			PSEUDO_STATBUF buf64;

			switch (p->fts_info) {
			case FTS_F:
			case FTS_D:
			case FTS_SL:
			case FTS_SLNONE:
			case FTS_DEFAULT:
				/* See fts_read.c: fts_path may already contain
				 * the chroot prefix from fts_open's path resolution.
				 * Use it directly in that case.
				 */
				if (p->fts_path[0] == '/' && pseudo_chroot_len &&
				    !memcmp(p->fts_path, pseudo_chroot, pseudo_chroot_len) &&
				    (p->fts_path[pseudo_chroot_len] == '/' ||
				     p->fts_path[pseudo_chroot_len] == '\0')) {
					fts_rpath = p->fts_path;
				} else {
					fts_rpath = PSEUDO_ROOT_PATH(AT_FDCWD, p->fts_path, AT_SYMLINK_NOFOLLOW);
				}
				if (fts_rpath) {
					pseudo_stat64_from32(&buf64, p->fts_statp);
					msg = pseudo_client_op(OP_STAT, 0, -1, -1, fts_rpath, &buf64);
					if (msg && msg->result == RESULT_SUCCEED) {
						pseudo_stat_msg(&buf64, msg);
						pseudo_stat32_from64(p->fts_statp, &buf64);
					}
				}
				break;
			default:
				break;
			}

			errno = save_errno;
		}
	}

/*	return rc;
 * }
 */
