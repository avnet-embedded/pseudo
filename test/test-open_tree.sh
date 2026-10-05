#!/bin/sh

./test/test-open_tree
rc1=$?

PSEUDO_IGNORE_PATHS=/ ./test/test-open_tree
rc2=$?

exit $((rc1 | rc2))
