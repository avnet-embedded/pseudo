#!/bin/bash
#
# Test nftw call and its behavior modifying flags
#
# The ./test/test-(n)ftw binaries print out different details of the filentries
# they encounter during walking the test directory. These details are saved, both using
# glibc (pseudo disabled), and with using the pseudo shim. glibc output is used
# as the oracle, and it is expected that the two outputs are always the same - except
# for ownership details. When it comes to file ownership, pseudo shim is expected to
# return root owner, while glibc should return the current user.
#
# The verify_results() function compares the saved details, with- and without- pseudo:
# filename, ownership and current_working_directory. If they don't match, the test fails.
#
# Below there are a number of tests executed, separated visually, they save different details
# of the same testcases with and without pseudo, before calling the verification function.
#
# SPDX-License-Identifier: LGPL-2.1-only
#

trap "rm -rf ./walking ./with_pseudo* ./without_pseudo*" 0

ret=0

verify_results(){
  current_user_uid=`env -i id -u`
  current_user_gid=`env -i id -g`

  # the list of files should match with and without pseudo
  diff -Naur ./with_pseudo_filename ./without_pseudo_filename > /dev/null
  file_list_result=$?

  # in case there is a cwd file (recording the current working dir for each entry),
  # than they should match also
  cwd_list_result=0
  if [ -f ./with_pseudo_cwd ]; then
    diff -Naur ./with_pseudo_cwd ./without_pseudo_cwd > /dev/null
    cwd_list_result=$?
  fi

  # ownership file's content is "$GID - $UID", per line
  # when using pseudo, both GID and UID should be 0
  incorrect_pseudo_ownership_count=`grep -vc "0 - 0" with_pseudo_ownership || true`

  # when not using pseudo, the GID and UID should match the
  # current user's IDs
  incorrect_no_pseudo_ownership_count=`grep -vc "$current_user_gid - $current_user_uid" without_pseudo_ownership || true`

  if [ $file_list_result -ne 0 ]; then
    echo test-nftw: $1: Failed, invalid file list
    ret=1
  elif [ $incorrect_pseudo_ownership_count -ne 0 ]; then
    echo test-nftw: $1: Failed, incorrect pseudo ownership details
    ret=1
  elif [ $incorrect_no_pseudo_ownership_count -ne 0 ]; then
    echo test-nftw: $1: Failed, incorrect no pseudo ownership details
    ret=1
  elif [ $cwd_list_result -ne 0 ]; then
    echo test-nftw: $1: Failed, incorrect current working directory
    ret = 1
  else
    echo test-nftw: $1: Passed
  fi  
}



mkdir -p walking/a1/b1/c1
touch walking/a1/b1/c1/file
mkdir walking/a1/b2
mkdir walking/a1/b3
touch walking/a1/b1/c1/file2
touch walking/a1/b1/c1/file3
touch walking/a1/b2/file4
touch walking/a1/b2/file5




# Test for ftw(), walk tree without recursion
unset PSEUDO_DISABLED
./test/test-ftw no_recursion filename > with_pseudo_filename
./test/test-ftw no_recursion ownership > with_pseudo_ownership

export PSEUDO_DISABLED=1
./test/test-ftw no_recursion filename > without_pseudo_filename
./test/test-ftw no_recursion ownership > without_pseudo_ownership

verify_results "ftw no recursion"







# Test for ftw64(), walk tree without recursion
unset PSEUDO_DISABLED
./test/test-ftw64 no_recursion filename > with_pseudo_filename
./test/test-ftw64 no_recursion ownership > with_pseudo_ownership

export PSEUDO_DISABLED=1
./test/test-ftw64 no_recursion filename > without_pseudo_filename
./test/test-ftw64 no_recursion ownership > without_pseudo_ownership

verify_results "ftw64 no recursion"







# Test for ftw(), walk tree while calling ftw() again recursively, from
# within an ftw() callback itself
unset PSEUDO_DISABLED
./test/test-ftw recursion filename > with_pseudo_filename
./test/test-ftw recursion ownership > with_pseudo_ownership

export PSEUDO_DISABLED=1
./test/test-ftw recursion filename > without_pseudo_filename
./test/test-ftw recursion ownership > without_pseudo_ownership

verify_results "ftw recursion"






# Test for ftw64(), walk tree while calling ftw64() again recursively, from
# within an ftw64() callback itself
unset PSEUDO_DISABLED
./test/test-ftw64 recursion filename > with_pseudo_filename
./test/test-ftw64 recursion ownership > with_pseudo_ownership

export PSEUDO_DISABLED=1
./test/test-ftw64 recursion filename > without_pseudo_filename
./test/test-ftw64 recursion ownership > without_pseudo_ownership

verify_results "ftw64 recursion"





# Test for nftw(), walk tree, but return SKIP_SUBTREE response each
# time a directory entry is received by the callback.
unset PSEUDO_DISABLED
./test/test-nftw skip_subtree filename > with_pseudo_filename
./test/test-nftw skip_subtree ownership > with_pseudo_ownership

export PSEUDO_DISABLED=1
./test/test-nftw skip_subtree filename > without_pseudo_filename
./test/test-nftw skip_subtree ownership > without_pseudo_ownership

verify_results "nftw skip_subtree"





# Test for nftw64(), walk tree, but return SKIP_SUBTREE response each
# time a directory entry is received by the callback.
unset PSEUDO_DISABLED
./test/test-nftw64 skip_subtree filename > with_pseudo_filename
./test/test-nftw64 skip_subtree ownership > with_pseudo_ownership

export PSEUDO_DISABLED=1
./test/test-nftw64 skip_subtree filename > without_pseudo_filename
./test/test-nftw64 skip_subtree ownership > without_pseudo_ownership

verify_results "nftw64 skip_subtree"



# Test for nftw(), walk tree, but return SKIP_SIBLINGS response each
# time a file entry is received by the callback
unset PSEUDO_DISABLED
./test/test-nftw skip_siblings filename > with_pseudo_filename
./test/test-nftw skip_siblings ownership > with_pseudo_ownership

#export PSEUDO_DISABLED=1
./test/test-nftw skip_siblings filename > without_pseudo_filename
./test/test-nftw skip_siblings ownership > without_pseudo_ownership

verify_results "nftw skip_siblings"






# Test for nftw64(), walk tree, but return SKIP_SIBLINGS response each
# time a file entry is received by the callback
unset PSEUDO_DISABLED
./test/test-nftw64 skip_siblings filename > with_pseudo_filename
./test/test-nftw64 skip_siblings ownership > with_pseudo_ownership

export PSEUDO_DISABLED=1
./test/test-nftw64 skip_siblings filename > without_pseudo_filename
./test/test-nftw64 skip_siblings ownership > without_pseudo_ownership

verify_results "nftw64 skip_siblings"







# Test for nftw(), walk tree, but return SKIP_SIBLINGS response each
# time a file entry is received by the callback, and also set the
# FTW_CHDIR flag on the call to switch to the corresponding folder

unset PSEUDO_DISABLED
./test/test-nftw skip_siblings_chdir filename > with_pseudo_filename
./test/test-nftw skip_siblings_chdir ownership > with_pseudo_ownership
./test/test-nftw skip_siblings_chdir cwd > with_pseudo_cwd



export PSEUDO_DISABLED=1
./test/test-nftw skip_siblings_chdir filename > without_pseudo_filename
./test/test-nftw skip_siblings_chdir ownership > without_pseudo_ownership
./test/test-nftw skip_siblings_chdir cwd > without_pseudo_cwd

verify_results "nftw skip_siblings_chdir"



exit $ret
