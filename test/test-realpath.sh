#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#
# See Yocto Project bugzilla 16028
#
# Gauthier HADERER reported differences in realpath behavior
#

go_exit() {
  if [ -n "${link}" ]; then
    rm ${link}
  fi

  if [ -n "${file}" ]; then
    rm ${file}
  fi

  exit $rc
}

mypath=$(dirname $0)

# Verify that a missing file fails
${mypath}/test-realpath doesnt-exist
rc=$?

if [ $rc -eq 0 ]; then
   echo "Non-zero return code expected!"
   rc=1
   go_exit
fi

# Verify that a regular file passes
file=$(mktemp test-realpath.XXXXXX)

filepath=$(${mypath}/test-realpath ${file})
rc=$?

if [ $rc -ne 0 ]; then
   echo "Zero return code expected!  Unable to find ${file}."
   go_exit
fi

link=$(mktemp test-realpath.XXXXXX)
rm ${link}
ln -s ${file} ${link}
linkpath=`${mypath}/test-realpath ${link}`
rc=$?
rm ${link}

rm ${file}

if [ $rc -ne 0 ]; then
    echo "Zero return code expected!  Unable to find ${link}."
    go_exit
fi

if [ "${linkpath}" != "${filepath}" ]; then
    echo "Link didn't return to expected target!"
    rc=1
    go_exit
fi
