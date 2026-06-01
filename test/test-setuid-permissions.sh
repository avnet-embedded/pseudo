#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#
set -e

# Verify that setuid/setgid bits tracked by pseudo do not
# bleed into the real filesystem.
#
# Return vals:
#
#                 2 - Setuid/setgid bits found on real file
#                 1 - Unexpected command error
#                 0 - Pass

mode() {
	stat -c "%a" "$1"
}

trap "rm -f testfile" EXIT

test_mode() {
	local octal_mode="$1"
	local expected_pseudo="$2"
	local expected_real="$3"

	chmod $octal_mode testfile

	# Under pseudo, verify mode is as requested
	local pseudo_mode=$(mode testfile)
	if [ "$pseudo_mode" != "$expected_pseudo" ]; then
		echo "FAIL: pseudo mode $pseudo_mode != expected $expected_pseudo (chmod $octal_mode)"
		exit 1
	fi

	# Check without pseudo - real file must NOT have setuid/setgid
	local real_mode=$(PSEUDO_DISABLED=1 stat -c "%a" testfile)
	if [ "$real_mode" != "$expected_real" ]; then
		echo "FAIL: real mode $real_mode != expected $expected_real (chmod $octal_mode)"
		exit 2
	fi
}

touch testfile

# Test setuid only (4755)
test_mode 4755 4755 755

# Test setgid only (2755)
test_mode 2755 2755 755

# Test setuid + setgid (6755)
test_mode 6755 6755 755

# Test setuid + setgid + other base perms (6644)
test_mode 6644 6644 644

exit 0
