#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#

which tclsh >/dev/null 2>&1
if [ $? -ne 0 ]; then
	exit 255
fi

# Check that tclsh doesn't hang.  Note that the timeout is not needed to
# reproduce the hang in tclsh, it's only there to ensure that this test script
# doesn't hang in case of a failing test.
timeout 2s bash -c "echo 'open {|true} r+' | tclsh"
