#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#

printf "statx null path: "
tmplog="$(mktemp pseudo.log.XXXXXXXX)"
./test/test-statx >${tmplog} 2>&1
rc=$?

if grep -q "couldn't allocate absolute path for 'null'." ${tmplog} ; then
    echo "Unexpected message: \"couldn't allocate absolute path for 'null'.\" found."
    rc=1
else
    echo "passed"
fi

cat ${tmplog}

rm $tmplog

printf "uutils date: "
# Try to figure out if we're using rust corutils
if $(date --version 2>&1 | grep -q "uutils") ; then
    tmplog="$(mktemp pseudo.log.XXXXXXXX)"
    date >${tmplog} 2>&1
    rc=$?

    if grep -q "couldn't allocate absolute path for 'null'." ${tmplog} ; then
        echo "Unexpected message: \"couldn't allocate absolute path for 'null'.\" found."
        rc=1
    else
        echo passed
    fi

    cat ${tmplog}

    rm $tmplog
else
    echo skipped
fi

exit $rc
