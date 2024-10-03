/*
 * Copyright (c) 2024 Linux Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * static int
 * wrap_posix_spawn(pid_t *pid, const char *path, const posix_spawn_file_actions_t *file_actions, const posix_spawnattr_t *attrp, char *const *argv, char *const *envp) {
 *	int rc = -1;
 */
	char * const *new_environ;
	/* note:  we don't canonicalize this, because we are intentionally
	 * NOT redirecting execs into the chroot environment.  If you try
	 * to execute /bin/sh, you get the actual /bin/sh, not
	 * <CHROOT>/bin/sh.  This allows use of basic utilities.  This
	 * design will likely be revisited.
	 */
        if (antimagic == 0) {
		const char *path_guess = pseudo_exec_path(path, 0);
                pseudo_client_op(OP_EXEC, PSA_EXEC, -1, -1, path_guess, 0);
        }

	new_environ = pseudo_setupenvp(envp);
	if (pseudo_has_unload(new_environ))
		new_environ = pseudo_dropenvp(new_environ);

	/* if exec() fails, we may end up taking signals unexpectedly...
	 * not much we can do about that.
	 */
	sigprocmask(SIG_SETMASK, &pseudo_saved_sigmask, NULL);
	rc = real_posix_spawn(pid, path, file_actions, attrp, argv, new_environ);

/*	return rc;
 * }
 */
