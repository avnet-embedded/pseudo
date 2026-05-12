#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#
# Test that rename() from outside PSEUDO_INCLUDE_PATHS followed by
# hardlink properly tracks file ownership.

srcdir=$(mktemp -d "$(realpath "${PWD}")/ren_hl_src_XXXXXX")
destdir=$(mktemp -d "$(realpath "${PWD}")/ren_hl_dest_XXXXXX")
trap "rm -rf '$srcdir' '$destdir'" EXIT

export PSEUDO_INCLUDE_PATHS="$destdir"

./test/test-rename-hardlink "$srcdir" "$destdir"
