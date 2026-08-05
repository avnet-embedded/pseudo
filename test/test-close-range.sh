#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#

# close_range() needs a 5.9 kernel. Ask with pseudo out of the way, so that a
# wrapper which has gone back to returning ENOSYS cannot pass itself off as an
# old kernel and quietly skip the test it is supposed to fail.
PSEUDO_DISABLED=1 ./test/test-close-range --probe
case $? in
0)  ;;
77) exit 255 ;;
*)  exit 1 ;;
esac

# CLOSE_RANGE_CLOEXEC needs 5.11. On a 5.9 or 5.10 kernel the syscall probe
# above succeeds but the flag does not exist yet, so probe for it the same
# way and leave only the CLOSE_RANGE_CLOEXEC part of the test out.
PSEUDO_DISABLED=1 ./test/test-close-range --probe-cloexec
case $? in
0)  ./test/test-close-range ;;
77) ./test/test-close-range --no-cloexec ;;
*)  exit 1 ;;
esac
