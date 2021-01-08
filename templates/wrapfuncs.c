@name pseudo_wrapfuncs.c
@header
/* wrapper functions. generated automatically. */

/* IF YOU ARE SEEING COMPILER ERRORS IN THIS FILE:
 * If you are seeing a whole lot of errors, make sure you aren't actually
 * trying to compile pseudo_wrapfuncs.c directly.  This file is #included
 * from pseudo_wrappers.c, which has a lot of needed include files and
 * static declarations.
 */

/* This file is generated and should not be modified.  See the makewrappers
 * script if you want to modify this. */
@body

static ${type} (*real_${name})(${decl_args}) = ${real_init};

${maybe_skip}

${type}
${name}(${decl_args}) {
	sigset_t saved;
	${variadic_decl}
	${rc_decl}
	PROFILE_START;

${maybe_async_skip}

	pseudo_debug(PDBGF_WRAPPER, "entry point $name wrapper=%p real=%p\n", &wrap_$name, real_$name);
	if (&wrap_$name == real_$name) {
		pseudo_debug(PDBGF_WRAPPER, "wrap_$name is the same as real_$name\n");
		abort();
	}

	if (!pseudo_check_wrappers() || !real_$name) {
		/* rc was initialized to the "failure" value */
		pseudo_enosys("${name}");
		PROFILE_DONE;
		${rc_return}
	}

	${variadic_start}

	if (pseudo_disabled) {
		pseudo_debug(PDBGF_SYSCALL, "pseudo disabled, ${name} calling real syscall %p.\n", *real_$name);
		${rc_assign} (*real_${name})(${call_args});
		${variadic_end}
		PROFILE_DONE;
		${rc_return}
	}

	pseudo_sigblock(&saved);
	pseudo_debug(PDBGF_WRAPPER | PDBGF_VERBOSE, "${name} - signals blocked, obtaining lock\n");
	if (pseudo_getlock()) {
		errno = EBUSY;
		sigprocmask(SIG_SETMASK, &saved, NULL);
		pseudo_debug(PDBGF_WRAPPER, "${name} failed to get lock, giving EBUSY.\n");
		PROFILE_DONE;
		${def_return}
	}

	int save_errno;
	if (antimagic > 0) {
		/* call the real syscall */
		pseudo_debug(PDBGF_SYSCALL, "${name} calling real syscall %p.\n", *real_$name);
		${rc_assign} (*real_${name})(${call_args});
	} else {
		${fix_paths}
		if (${ignore_paths}) {
			/* call the real syscall */
			pseudo_debug(PDBGF_SYSCALL, "${name} ignored path, calling real syscall %p.\n", *real_$name);
			${rc_assign} (*real_${name})(${call_args});
		} else {
			/* exec*() use this to restore the sig mask */
			pseudo_saved_sigmask = saved;
			${rc_assign} wrap_$name(${call_args});
		}
	}
	${variadic_end}
	save_errno = errno;
	pseudo_droplock();
	sigprocmask(SIG_SETMASK, &saved, NULL);
	pseudo_debug(PDBGF_WRAPPER | PDBGF_VERBOSE, "${name} - yielded lock, restored signals\n");
#if 0
/* This can cause hangs on some recentish systems which use locale
 * stuff for strerror...
 */
	pseudo_debug(PDBGF_WRAPPER, "wrapper completed: ${name} returns ${rc_format} (errno: %s)\n", ${rc_value}, strerror(save_errno));
#endif
	pseudo_debug(PDBGF_WRAPPER, "wrapper completed: ${name} returns ${rc_format} (errno: %d)\n", ${rc_value}, save_errno);
	errno = save_errno;
	PROFILE_DONE;
	${rc_return}
}

static ${type}
wrap_${name}(${wrap_args}) {
	$rc_decl
	${maybe_variadic_decl}
	${maybe_variadic_start}

#include "ports/${port}/guts/${name}.c"

	${rc_return}
}

${end_maybe_skip}
