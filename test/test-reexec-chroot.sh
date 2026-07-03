#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#

# Test if we re-invoke pseudo that chroot still works

# Return vals: 2 - invalid arg list
#              1 - chroot failed
#              0 - chroot succeeded

# The following should just run chroot_test since pseudo is already loaded
./bin/pseudo $(dirname "$0")/test-reexec-chroot `pwd`

if [ "$?" = "0" ]
then
    #echo "Passed."
    exit 0
fi
#echo "Failed"
exit 1
