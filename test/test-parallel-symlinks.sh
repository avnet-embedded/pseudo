#! /bin/bash

# * Create a temporary directory
# * Create some symlinks in the directory (in parallel)

tmpdir="$(mktemp -d -p "$PWD")"
trap "rm -rf '$tmpdir'" EXIT TERM INT

mkdir -p $tmpdir
touch $tmpdir/file

function symlink_test()
{
    for run in $(seq 10); do
        ln -sf file $tmpdir/link
        rc=$?
        if [ $rc -ne 0 ]; then
            echo "FAILED: ln -sf file $tmpdir/link - $rc" > $tmpdir/failed
            exit 1
        fi
        if [ -f $tmpdir/file -a -e $tmpdir/link ]; then
            :
        else
            echo "FAILED: missing files" > $tmpdir/failed
            exit 1
        fi
    done
}

for count in $(seq 4); do
    symlink_test $count &
done

wait

if [ -e $tmpdir/failed ]; then
    cat $tmpdir/failed
    exit 1
fi
