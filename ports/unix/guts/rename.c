/* 
 * Copyright (c) 2008-2010, 2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * static int
 * wrap_rename(const char *oldpath, const char *newpath) {
 *	int rc = -1;
 */
 	pseudo_msg_t *msg;
 	PSEUDO_STATBUF oldbuf, newbuf;
	int oldrc, newrc;
	int save_errno;
	int old_db_entry = 0;
	int may_unlink_new = 0;
	int may_unlink_old = 0;

	pseudo_debug(PDBGF_OP, "rename: %s->%s\n",
		oldpath ? oldpath : "<nil>",
		newpath ? newpath : "<nil>");

	if (!oldpath || !newpath) {
		errno = EFAULT;
		return -1;
	}

	save_errno = errno;

	newrc = base_lstat(newpath, &newbuf);
	oldrc = base_lstat(oldpath, &oldbuf);

	/* nothing to do for a "rename" of a link to itself */
	if (newrc != -1 && oldrc != -1 &&
	    newbuf.st_dev == oldbuf.st_dev &&
	    newbuf.st_ino == oldbuf.st_ino) {
		pseudo_debug(PDBGF_OP, "rename: paths are the same\n");
		return real_rename(oldpath, newpath);
        }

	errno = save_errno;

	/* newpath must be removed. */
	/* as with unlink, we have to mark that the file may get deleted */
	msg = pseudo_client_op(OP_MAY_UNLINK, 0, -1, -1, newpath, newrc ? NULL : &newbuf);
	if (msg && msg->result == RESULT_SUCCEED)
		may_unlink_new = 1;
        /* oldpath is also likely to disappear. Something could call stat() after 
           real_rename so we need to mark as MAY_UNLINK too */
	msg = pseudo_client_op(OP_MAY_UNLINK, 0, -1, -1, oldpath, oldrc ? NULL : &oldbuf);
	if (msg && msg->result == RESULT_SUCCEED)
		may_unlink_old = 1;

	msg = pseudo_client_op(OP_STAT, 0, -1, -1, oldpath, oldrc ? NULL : &oldbuf);
	if (msg && msg->result == RESULT_SUCCEED)
		old_db_entry = 1;
	rc = real_rename(oldpath, newpath);
	save_errno = errno;

	if (rc == -1) {
		/* since we failed, that wasn't really unlinked -- put
		 * it back.
		 */
		if (may_unlink_new)
			pseudo_client_op(OP_CANCEL_UNLINK, 0, -1, -1, newpath, &newbuf);
		if (may_unlink_old)
			pseudo_client_op(OP_CANCEL_UNLINK, 0, -1, -1, oldpath, &oldbuf);
		/* and we're done. */
		errno = save_errno;
		return rc;
	}

	/* confirm that the file was removed */
	if (may_unlink_new)	
		pseudo_client_op(OP_DID_UNLINK, 0, -1, -1, newpath, &newbuf);
	/* OP_DID_UNLINK for oldpath is handled by the server */
	save_errno = errno;

	/* rename(3) is not mv(1).  rename(file, dir) fails; you must provide
	 * the corrected path yourself.  You can rename over a directory only
	 * if the source is a directory.  Symlinks are simply removed.
	 *
	 * If we got here, the real rename call succeeded.  That means newpath
	 * has been unlinked and oldpath has been linked to it.
	 *
	 * There are a ton of special cases to error check.  I don't check
	 * for any of them, because in every such case, the underlying rename
	 * failed, and there is nothing to do.
	 * The only tricky part is that, because we used to ignore symlinks,
	 * we may have to rename or remove directory trees even though in
	 * theory rename can never destroy a directory tree.
	 */
	if (!old_db_entry) {
		/* create an entry under the old name, which will then be
		 * renamed; this way, children would get renamed too, if there
		 * were any.
		 */
		if (newrc == 0) {
			if (newbuf.st_dev != oldbuf.st_dev) {
				oldbuf.st_dev = newbuf.st_dev;
				oldbuf.st_ino = newbuf.st_ino;
			}
		}
		if (pseudo_client_ignore_path(oldpath)) {
			/* oldpath is ignored (not in INCLUDE_PATHS), so
			 * OP_LINK on it would be silently dropped and the
			 * subsequent OP_RENAME would be a no-op. Instead,
			 * create the entry directly at newpath.
			 * OP_LINK does not override uid/gid with pseudo_fuid/
			 * pseudo_fgid (unlike OP_CREAT), so set them explicitly
			 * to avoid recording the host user's real ids.
			 */
			oldbuf.st_uid = pseudo_fuid;
			oldbuf.st_gid = pseudo_fgid;
			pseudo_debug(PDBGF_OP, "creating new '%s' [%llu] directly at newpath (oldpath ignored)\n",
				newpath, (unsigned long long) oldbuf.st_ino);
			pseudo_client_op(OP_LINK, 0, -1, -1, newpath, &oldbuf);
		} else {
			pseudo_debug(PDBGF_FILE, "creating new '%s' [%llu] to rename\n",
				oldpath, (unsigned long long) oldbuf.st_ino);
			pseudo_client_op(OP_LINK, 0, -1, -1, oldpath, &oldbuf);
			pseudo_client_op(OP_RENAME, 0, -1, -1, newpath, &oldbuf, oldpath);
		}
	} else {
		pseudo_client_op(OP_RENAME, 0, -1, -1, newpath, &oldbuf, oldpath);
	}

	errno = save_errno;
/*	return rc;
 * }
 */
