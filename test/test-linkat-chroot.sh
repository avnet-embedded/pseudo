#!/bin/bash
#
# Test that link/linkat inside a chroot does not segfault
# when the path is shorter than the chroot path.
# SPDX-License-Identifier: LGPL-2.1-only
#

set -e

# Create a chroot directory with a deeply nested path
# so pseudo_chroot_len is longer than the filenames used inside
CHROOTDIR=$(pwd)/linkat_chroot_test/deep/nested/path/to/make/it/long
mkdir -p "$CHROOTDIR"
touch "$CHROOTDIR/a"
trap "rm -rf $(pwd)/linkat_chroot_test test-linkat-chroot" 0

gcc -o test-linkat-chroot test/test-linkat-chroot.c

./test-linkat-chroot "$CHROOTDIR"
