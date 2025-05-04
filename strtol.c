#include <stdio.h>
#include <stdlib.h>
#include "pseudo.h"

/*
 * glibc 2.38 would include  __isoc23_strtol and similar symbols if _GNU_SOURCE
 * is used. We have to set that for other definitions so delcare this wrapper
 * in a context where we don't have that set.
 *
 * We need to wrap strtol, strtoll, atoi, fscanf and sscanf.
 */

int pseudo_atoi_wrapper(const char *nptr)
{
	return atoi(nptr);
}

long pseudo_strtol_wrapper(const char *nptr, char **endptr, int base)
{
	return strtol(nptr, endptr, base);
}

long long pseudo_strtoll_wrapper(const char *nptr, char **endptr, int base)
{
	return strtoll(nptr, endptr, base);
}
