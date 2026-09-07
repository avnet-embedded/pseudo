#!/bin/sh

./test/test-open_tree-syscall

PSEUDO_IGNORE_PATHS=/ ./test/test-open_tree-syscall