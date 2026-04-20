#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#
# Test canonicalize_file_name (basic + chroot)

CHROOT_DIR=$(mktemp -d "${PWD}/chroot_canon_XXXXXX")
trap "rm -rf '$CHROOT_DIR'" 0

rm -f test_canon_file test_canon_link
./test/test-canonicalize "$CHROOT_DIR"
