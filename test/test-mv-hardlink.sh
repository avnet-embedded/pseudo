#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#
# Test that rename (mv) from outside PSEUDO_INCLUDE_PATHS followed by
# hardlink properly tracks file ownership.
#
# Reproduces: https://lists.openembedded.org/g/openembedded-core/message/236712
#
# In Yocto, files are often moved from ${B} (build dir, outside
# PSEUDO_INCLUDE_PATHS) to ${D} (image dir, tracked by pseudo) and
# then hardlinked. If pseudo doesn't track the rename, the hardlink
# gets recorded with the real UID, causing inconsistent ownership
# and pseudo abort on subsequent stat.

# Create two directories:
#   srcdir  - simulates ${B}, outside PSEUDO_INCLUDE_PATHS
#   destdir - simulates ${D}, inside PSEUDO_INCLUDE_PATHS
# Use realpath to resolve symlinks, since pseudo canonicalizes paths
# internally and the PSEUDO_INCLUDE_PATHS prefix must match.
srcdir=$(mktemp -d "$(realpath "${PWD}")/mv_hl_src_XXXXXX")
destdir=$(mktemp -d "$(realpath "${PWD}")/mv_hl_dest_XXXXXX")
trap "rm -rf '$srcdir' '$destdir'" EXIT

# Restrict pseudo tracking to only destdir
export PSEUDO_INCLUDE_PATHS="$destdir"

echo hello > ${srcdir}/hello.txt

mv ${srcdir}/hello.txt ${destdir}/hello.txt
ln ${destdir}/hello.txt ${destdir}/hello2.txt

# Both files should report uid 0 under pseudo
dest_uid=$(\ls -n1 ${destdir}/hello.txt | awk '{ print $3 }')
link_uid=$(\ls -n1 ${destdir}/hello2.txt | awk '{ print $3 }')

if [ "$dest_uid" != "0" ]; then
    echo "FAIL: dest uid is $dest_uid, expected 0"
    exit 1
fi

if [ "$link_uid" != "0" ]; then
    echo "FAIL: link uid is $link_uid, expected 0"
    exit 1
fi

if [ "$dest_uid" != "$link_uid" ]; then
    echo "FAIL: UIDs don't match (dest=$dest_uid, link=$link_uid)"
    exit 1
fi

exit 0
