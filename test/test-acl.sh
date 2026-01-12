#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#

# Return vals: 2 - Unable to run ACL commands, assertion failure
#              1 - Invalid return value
#              0 - Pass
#              255 - acl programs unavailable, skip

# NOTE: these test exclusively test setfacl -m

set -u

rc=0
which getfacl 2>/dev/null
if [ $? -ne 0 ]; then
    echo "getfacl command missing"
    rc=255
fi
which setfacl 2>/dev/null
if [ $? -ne 0 ]; then
    echo "setfacl command missing"
    rc=255
fi
if [ $rc -ne 0 ]; then
    exit $rc
fi

check_owner () {
    local file="$1"
    local expected="$2"
    local msg="$3"
    local actual=$(stat -c "%U" "$file")
    if [ "$actual" != "$expected" ]
    then
        echo "$msg" "Fail, '$file' unexpected owner '$actual'"
        exit 2
    fi
}

check_group () {
    local file="$1"
    local expected="$2"
    local msg="$3"
    local actual=$(stat -c "%G" "$file")
    if [ "$actual" != "$expected" ]
    then
        echo "$msg" "Fail, '$file' unexpected group '$actual'"
        exit 2
    fi
}

check_acl_contains () {
    local file="$1"
    local acl="$2"
    local msg="$3"
    IFS=',' read -ra acls <<< "$acl"
    for pattern in "${acls[@]}"; do
        result=$(getfacl -c "$file" | grep -o "^$pattern")
        if [ "$result" != "$pattern" ]
        then
            echo "$msg" "Fail, did not find desired acl '$pattern' in '$file'"
            exit 2
        fi
    done
}

check_acl_minimal () {
    local file="$1"
    local msg="${2:-''}"
    local acls
    acls=$(getfacl -c "${file}" | grep -v "::")
    if [ -n "$acls" ]
    then
        echo "$msg" "Fail, '$file' unexpected getfacl result '$acls'"
        exit 1
    fi
}

test_modify_once () {
    local file="$1"
    local acl="$2"
    local msg="${3:-''}"
    # ensure that file is pristine
    check_acl_minimal "$file" "$msg precondition:"
    check_owner "$file" root "$msg precondition:"
    check_group "$file" root "$msg precondition:"
    if ! setfacl -m "$acl" "$file"
    then
        echo "$msg"  "Fail, unable to call setfacl"
        exit 2
    fi
    check_acl_contains "$file" "$acl" "$msg: acl not set:"
    check_owner "$file" root "$msg owner corrupted:"
    check_group "$file" root "$msg group corrupted:"
}


trap "rm -rf testdir" EXIT
mkdir testdir || exit 1


# user
touch testdir/f1 || exit 1
mkdir testdir/d1 || exit 1
# regular file
test_modify_once testdir/f1 "user:root:r" "$LINENO:"
# directory
test_modify_once testdir/d1 "user:root:r" "$LINENO:"
rm -rf testdir/f1 testdir/d1

#group
rm -rf testdir/f1 testdir/d1
touch testdir/f1 || exit 1
mkdir testdir/d1 || exit 1
# regular file
test_modify_once testdir/f1 "group:root:r" "$LINENO:"
# directory
test_modify_once testdir/d1 "group:root:r" "$LINENO:"
rm -rf testdir/f1 testdir/d1

# multiple users
touch testdir/f1 || exit 1
mkdir testdir/d1 || exit 1
# regular file
test_modify_once testdir/f1 "user:root:r,group:root:r,user:bin:rw" "$LINENO:"
# directory
test_modify_once testdir/d1 "user:root:r,group:root:r,user:bin:rw" "$LINENO:"
rm -rf testdir/f1 testdir/d1


# setfacl default acls
mkdir testdir/d1 || exit 1
test_modify_once testdir/d1 "default:user:root:r,user:root:r" "$LINENO:"
rm -rf testdir/d1


# multiple calls to setfacl -m on same file
touch testdir/f1 || exit 1
mkdir testdir/d1 || exit 1
check_owner testdir/f1 root "$LINENO: precondition:"
check_group testdir/f1 root "$LINENO: precondition:"
check_acl_minimal testdir/f1 "$LINENO: precondition:"

acl1="user:root:r"
acl2="user:bin:rw"

if ! setfacl -m "$acl1" testdir/f1 # first setfacl
then
    echo "$LINENO:"  "Fail, unable to call setfacl"
    exit 2
fi
check_acl_contains testdir/f1 "$acl1" "$LINENO: acl1 not set:"
check_owner testdir/f1 root "$LINENO: owner corrupted:"
check_group testdir/f1 root "$LINENO: group corrupted:"

if ! setfacl -m "$acl2" testdir/f1 # second setfacl
then
    echo "$LINENO:"  "Fail, unable to call setfacl"
    exit 2
fi

check_acl_contains testdir/f1 "$acl1" "$LINENO: acl1 not set:"
check_acl_contains testdir/f1 "$acl2" "$LINENO: acl2 not set:"
check_owner testdir/f1 root "$LINENO: owner corrupted:"
check_group testdir/f1 root "$LINENO: group corrupted:"
rm -rf testdir/f1 testdir/d1

# setfacl recursive
test_modify_recursive () {
    local root_dir="$1"
    local acl="$2"
    local msg="${3:-''}"

    find "$root_dir" | while read -r file; do
        check_owner "$file" root "$msg precondition:"
        check_group "$file" root "$msg precondition:"
        check_acl_minimal "$file" "$msg precondition:"
    done
    if ! setfacl -R -m "$acl" "$root_dir"
    then
        echo "$msg"  "Fail, unable to call setfacl"
        exit 2
    fi
    find "$root_dir" | while read -r file; do
        check_owner "$file" root "$msg owner corrupted:"
        check_group "$file" root "$msg group corrupted:"
        check_acl_contains "$file" "$acl" "$msg acl not set:"
    done
}

mkdir -p testdir/d1/d2 || exit 1
touch testdir/d1/d2/f1 || exit 1
test_modify_recursive testdir/d1 "user:root:r,group:root:r,user:bin:rw" "$LINENO:"
rm -rf testdir/d1

mkdir -p testdir/d1/d2 || exit 1
mkdir -p testdir/d1/d3 || exit 1
test_modify_recursive testdir/d1 "default:user:root:rwx,user:root:r,group:root:r,user:bin:rw" "$LINENO:"
rm -rf testdir/d1

#echo "Passed."
exit 0
