#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#
# Test that PSEUDO_DB_MODE correctly reconstructs permission bits
# at file/directory creation time.

trap "rm -rf test-db-mode-tmp test-db-mode-dir-tmp" EXIT

./test/test-db-mode
