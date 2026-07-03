#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#

# Return vals: 2 - invalid arg list
#              1 - chroot failed
#              0 - chroot succeeded

$(dirname "$0")/test-chroot `pwd`

if [ "$?" = "0" ]
then
    #echo "Passed."
    exit 0
fi
#echo "Failed"
exit 1
