#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#
$(dirname "$0")/test-execl | grep -q "C=C"

if [ "$?" = "0" ]
then
    #echo "Passed."
    exit 0
fi
#echo "Failed"
exit 1
