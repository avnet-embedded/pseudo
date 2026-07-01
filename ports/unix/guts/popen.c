/*
 * Copyright (c) 2012 Wind River Systems; see
 * guts/COPYRIGHT for information.
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * FILE *popen(const char *command, const char *mode)
 *	FILE *rc = NULL;
 */
	char **new_environ, **orig_environ;

	/* on at least some systems, popen() calls fork and exec
	 * in ways that avoid our usual enforcement of the environment.
	 */
	/* Due to bash intercepting setenv/getenv/unsetenv and changing environ
	   internally itself at will, we create our own environ copy at process
	   creation based on it to ensure it is correct */
	orig_environ = environ;
	new_environ = pseudo_setupenvp(environ);
	if (pseudo_has_unload(new_environ))
		new_environ = pseudo_dropenvp(new_environ);
        environ = new_environ;

	rc = real_popen(command, mode);

	environ = orig_environ;
	free(new_environ);

/*	return rc;
 * }
 */
