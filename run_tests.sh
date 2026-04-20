#!/bin/bash
#
# SPDX-License-Identifier: LGPL-2.1-only
#

opt_verbose=
test_args=()

usage()
{
    echo >&2 "usage:"
    echo >&2 "  run_tests [-v|--verbose] [test ...]"
    echo >&2 ""
    echo >&2 "If no tests are specified, all tests are run."
    echo >&2 "Tests can be specified as filenames (test-fstat.sh) or basenames (test-fstat)."
    exit 1
}

for arg
do
        case $arg in
        --)     shift; break ;;
        -v | --verbose)
                opt_verbose=-v
                ;;
        -h | --help)
                usage
                ;;
        -*)
                usage
                ;;
        *)
                test_args+=("$arg")
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

if [ ${#test_args[@]} -gt 0 ]; then
    test_files=()
    for t in "${test_args[@]}"; do
        # Strip directory prefix and ensure .sh suffix
        t="${t##*/}"
        t="${t%.sh}.sh"
        if [ -f "test/$t" ]; then
            test_files+=("test/$t")
        else
            echo >&2 "Warning: test/$t not found, skipping."
        fi
    done
else
    test_files=(test/test*.sh)
fi

for file in "${test_files[@]}"
do
    filename=${file#test/}
    let num_tests++
    mkdir -p var/pseudo
    ./bin/pseudo $file ${opt_verbose} >${tmplog} 2>&1
    rc=$?
    if [ "${opt_verbose}" = "-v" ]; then
        echo
        cat ${tmplog}
        if [ $rc -ne 0 -a $rc -ne 255 ]; then
            # Include the contents of the pseudo.log before we delete it
            cat var/pseudo/pseudo.log
        fi
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
