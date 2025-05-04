#include <stdio.h>
#include "pseudo.h"
#include "pseudo_ipc.h"
#include "pseudo_client.h"

/*
 * glibc 2.38 would include  __isoc23_strtol and similar symbols if _GNU_SOURCE
 * is used. We have to set that for other definitions so delcare this wrapper
 * in a context where we don't have that set.
 *
 * We need to wrap strtol, strtoll, atoi, fscanf and sscanf.
 */

void readenv_uids(void)
{
	char *env;
	env = pseudo_get_value("PSEUDO_UIDS");
	if (env)
		sscanf(env, "%d,%d,%d,%d",
			&pseudo_ruid, &pseudo_euid,
			&pseudo_suid, &pseudo_fuid);
	free(env);
}

void readenv_gids(void)
{
	char *env;
	env = pseudo_get_value("PSEUDO_GIDS");
	if (env)
		sscanf(env, "%d,%d,%d,%d",
			&pseudo_rgid, &pseudo_egid,
			&pseudo_sgid, &pseudo_fuid);
	free(env);
}

int read_pidfile(FILE *fp, int *server_pid)
{
	return fscanf(fp, "%d", server_pid);
}
