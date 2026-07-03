#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#
# Test for the bash/pseudo environment conflict:
#   https://bugzilla.yoctoproject.org/show_bug.cgi?id=16078
#
# The compiled helper binary (test-bash-exec-env.c) exports its own
# getenv/setenv/unsetenv (simulating bash) and calls maybe_make_export_env()
# before each execve(), exactly reproducing the opkg-build crash pattern.
#
# Detection mechanism:
#   The binary strips all PSEUDO_* vars (except PSEUDO_PREFIX and
#   PSEUDO_LOCALSTATEDIR) from its internal table and environ before
#   starting.  This forces pseudo_setupenv() — called by pseudo's fork
#   wrapper in each child — to ADD those vars back.
#
#   The check is deterministic: each child inspects the real environ after
#   pseudo's fork wrapper has run.
#
#   Unfixed pseudo: SETENV() -> dlsym(RTLD_NEXT) -> glibc setenv writes the
#     stripped PSEUDO_* vars straight back into the real environ array.  The
#     child sees them reappear and reports the bug (exit 42); the test fails.
#
#   Fixed pseudo: SETENV() -> our binary's setenv() -> internal_table only;
#     the real environ is left untouched, no PSEUDO_* vars reappear, the child
#     execs cleanly and the test passes.
#
#   MALLOC_CHECK_=3 is kept as a secondary safety net so any residual heap
#   corruption from the unfixed code also aborts the run.

MALLOC_CHECK_=3 $(dirname "$0")/test-bash-exec-env || { echo "FAILED: test-bash-exec-env returned $?" ; exit 1; }

exit 0
