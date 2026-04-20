#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#
# Test link, linkat, symlink, symlinkat, readlink, readlinkat (basic + chroot)

CHROOT_DIR=$(mktemp -d "${PWD}/chroot_link_XXXXXX")
trap "rm -rf '$CHROOT_DIR'" 0

rm -f test_link_file test_link_sym test_link_sym2 test_link_hard test_link_hard2
./test/test-link-symlink "$CHROOT_DIR"
