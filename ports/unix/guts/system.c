/*
 * Copyright (c) 2011, 2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * int system(const char *command)
 *	int rc = -1;
 */
	char **new_environ, **orig_environ;

	if (!command)
		return 1;

	/* Due to bash intercepting setenv/getenv/unsetenv and changing environ
	   internally itself at will, we create our own environ copy at process
	   creation based on it to ensure it is correct */
	orig_environ = environ;
	new_environ = pseudo_setupenvp(environ);
	if (pseudo_has_unload(new_environ))
		new_environ = pseudo_dropenvp(new_environ);
	environ = new_environ;

	rc = real_system(command);

	environ = orig_environ;
	pseudo_free_envp(new_environ);

/*	return rc;
 * }
 */
