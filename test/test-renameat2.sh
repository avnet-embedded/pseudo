#! /bin/sh

# Run with and without ignored paths, matching test-openat coverage.

./test/test-renameat2
rc=$?
if [ "$rc" -ne 0 ]; then
	exit "$rc"
fi

PSEUDO_IGNORE_PATHS=/ ./test/test-renameat2
