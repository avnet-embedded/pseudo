#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#
# See Yocto Project bugzilla 16028
#
# It was determined that a utility that use /dev/fd may fail when run under
# pseudo, while working properly outside.
#
# For example: echo "test" | cat /dev/fd/0

# Tests
result=$(echo "test" | cat 2>&1)
rc=$?
if [ $rc -ne 0 -o "$result" != "test" ]; then
    echo Failed simple echo pipe test - $rc:
    echo $result
    exit 1
fi

result=$(echo "test" | cat /dev/fd/0 2>&1)
rc=$?
if [ $rc -ne 0 -o "$result" != "test" ]; then
    echo Failed /dev/fd/0 echo pipe test - $rc:
    echo $result
    exit 1
fi
