#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#

tmplog="$(mktemp pseudo.log.XXXXXXXX)"
./test/test-statx >${tmplog} 2>&1
rc=$?

if grep -q "couldn't allocate absolute path for 'null'." ${tmplog} ; then
    echo "Unexpected message: \"couldn't allocate absolute path for 'null'.\" found."
    rc=1
fi

cat ${tmplog}

rm $tmplog
exit $rc
