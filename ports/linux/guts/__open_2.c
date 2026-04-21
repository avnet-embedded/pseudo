/*
 * Copyright (c) 2026 Richard Purdie
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * static int
 * wrap___open_2(const char *path, int flags) {
 *	int rc = -1;
 */

	rc = wrap_open(path, flags, 0);

/*	return rc;
 * }
 */
