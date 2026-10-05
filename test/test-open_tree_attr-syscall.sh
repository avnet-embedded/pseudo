#!/bin/sh

./test/test-open_tree_attr-syscall
rc1=$?

PSEUDO_IGNORE_PATHS=/ ./test/test-open_tree_attr-syscall
rc2=$?

exit $((rc1 | rc2))
