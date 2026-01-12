#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#

opt_verbose=

usage()
{
    echo >&2 "usage:"
    echo >&2 "  run_tests [-v|--verbose]"
    exit 1
}

for arg
do
        case $arg in
        --)     shift; break ;;
        -v | --verbose)
                opt_verbose=-v
                ;;
        *)
                usage
                ;;
        esac
done

#The tests will be run on the build dir, not the installed versions
#This requires to following be set properly.
export PSEUDO_PREFIX=${PWD}

num_tests=0
num_passed_tests=0
num_skipped_tests=0
num_failed_tests=0

tmplog="$(mktemp pseudo.log.XXXXXXXX)"

for file in test/test*.sh
do
    filename=${file#test/}
    let num_tests++
    mkdir -p var/pseudo
    ./bin/pseudo $file ${opt_verbose} >${tmplog} 2>&1
    rc=$?
    if [ "${opt_verbose}" = "-v" ]; then
        echo
        cat ${tmplog}
    fi
    if [ "$rc" -eq "0" ]; then
        let num_passed_tests++
        echo "${filename%.sh}: Passed."
    elif [ "$rc" -eq "255" ]; then
        let num_skipped_tests++
        echo "${filename%.sh}: Skipped."
    else
        let num_failed_tests++
        echo "${filename/%.sh}: Failed."
    fi
    rm -rf var/pseudo/*
done
echo "${num_failed_tests}/${num_tests} test(s) failed."
echo "${num_skipped_tests}/${num_tests} test(s) skipped."
echo "${num_passed_tests}/${num_tests} test(s) passed."

rm ${tmplog}
