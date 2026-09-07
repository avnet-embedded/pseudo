#!/bin/sh

./test/test-open_tree_attr-syscall

PSEUDO_IGNORE_PATHS=/ ./test/test-open_tree_attr-syscall
