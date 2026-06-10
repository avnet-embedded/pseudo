/*
 * Copyright (c) 2026 Yocto Project
 * guts/COPYRIGHT for information.
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * int open_tree(int dirfd, const char *path, unsigned int flags)
 *	int rc = -1;
 */

	struct stat64 buf;
	int save_errno;
	char *pseudo_path;

	if (flags & OPEN_TREE_CLONE) {
		pseudo_error("OPEN_TREE_CLONE is not supported\n");
	}

	if (path && path[0] == '/') {
		/* If the path is absolute, we should account for a potential chroot */
		path = pseudo_path;
	}

	/*
	* Older libc versions do not provide open_tree(), so call the syscall
	* directly when the libc wrapper is not available.
	*/
	if (real_open_tree) {
		pseudo_debug(PDBGF_SYSCALL, "open_tree, calling open_tree.\n");
		rc = real_open_tree(dirfd, path, flags);
	} else {
		pseudo_debug(PDBGF_SYSCALL, "open_tree, calling syscall.\n");
		rc = real_syscall(SYS_open_tree, dirfd, path, flags);
	}

	if (rc != -1) {
		save_errno = errno;
		int stat_rc;

		pseudo_path = pseudo_root_path(__func__, __LINE__, dirfd, path, 0);

		stat_rc = real___fxstatat64(_STAT_VER, dirfd, pseudo_path, &buf, (flags & AT_SYMLINK_NOFOLLOW) ? AT_SYMLINK_NOFOLLOW : 0);

		pseudo_debug(PDBGF_FILE, "open_tree(path %s), flags %o, stat rc %d, stat mode %o\n",
			pseudo_path, flags, stat_rc, buf.st_mode);

		if (stat_rc != -1) {
			pseudo_client_op(OP_OPEN, 0, rc, dirfd, pseudo_path, &buf);
		} else {
			pseudo_debug(PDBGF_FILE, "open_tree(fd %d, path %d/%s, flags %d) succeeded, but stat failed (%s).\n",
				rc, dirfd, pseudo_path, flags, strerror(errno));
			pseudo_client_op(OP_OPEN, 0, rc, dirfd, pseudo_path, 0);
		}
		errno = save_errno;
	}

/*	return rc;
 * }
 */
