#! /bin/sh

# Run with and without ignored paths, matching test-openat coverage.

chroot ./test ./test-renameat2
rc=$?
if [ "$rc" -ne 0 ]; then
	exit "$rc"
fi

PSEUDO_IGNORE_PATHS=/ chroot ./test ./test-renameat2
