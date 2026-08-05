/*
 * Copyright (c) 2021 Richard Purdie
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * int close_range(unsigned int lowfd, unsigned int maxfd, int flags)
 *      int rc = -1;
 */
	pseudo_msg_t *msg;
	int maxintfd;

	/* The kernel rejects both of these outright and closes nothing when
	 * it does, so validate before touching anything.
	 */
	if (flags & ~(CLOSE_RANGE_UNSHARE | CLOSE_RANGE_CLOEXEC)) {
		errno = EINVAL;
		return -1;
	}
	if (lowfd > maxfd) {
		errno = EINVAL;
		return -1;
	}

	/* CLOSE_RANGE_UNSHARE has to take effect before anything is closed:
	 * while the descriptor table is still shared, closing a descriptor
	 * would close it for everyone sharing the table, not just for us.
	 */
	if (flags & CLOSE_RANGE_UNSHARE) {
		if (unshare(CLONE_FILES) == -1)
			return -1;
		flags &= ~CLOSE_RANGE_UNSHARE;
	}

	/* CLOSE_RANGE_CLOEXEC closes nothing, it only marks descriptors, and
	 * pseudo's own are close-on-exec already (pseudo_fd() sets that on
	 * every one of them), so there is nothing here to protect.
	 */
	if (flags & CLOSE_RANGE_CLOEXEC)
		return real_close_range(lowfd, maxfd, flags);

	/* Descriptors are ints, so a range starting above INT_MAX cannot hold
	 * any of pseudo's own and there is nothing to step around. Worth its
	 * own case because pseudo_client_op() takes the low end as an int.
	 */
	if (lowfd > INT_MAX)
		return real_close_range(lowfd, maxfd, flags);
	if (maxfd > INT_MAX)
		maxintfd = INT_MAX;
	else
		maxintfd = (int) maxfd;

	/* The op closefrom() also goes through: it closes the descriptors
	 * pseudo's own are mixed in with by hand, stepping around the ones
	 * pseudo needs to keep, and hands back the first fd the kernel can
	 * safely be turned loose on.
	 */
	msg = pseudo_client_op(OP_CLOSE_RANGE, 0, lowfd, -1, 0, 0, maxintfd);
	if (maxfd >= (unsigned int) msg->fd)
		rc = real_close_range(msg->fd, maxfd, flags);
	else
		rc = 0;

/*      return rc;
 * }
 */
