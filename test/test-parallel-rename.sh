#! /bin/bash

# * Create a temporary directory
# * Create some symlinks in the directory (in parallel)

tmpdir="$(mktemp -d -p "$PWD")"
trap "rm -rf '$tmpdir'" EXIT TERM INT

mkdir -p $tmpdir
touch $tmpdir/file

function mv_test()
{
    for run in $(seq 10); do
        file="$tmpdir/my_file.$1.$run"
        touch $file
        rc=$?
        if [ $rc -ne 0 ]; then
            echo "FAILED: touch $file - $rc" >> $tmpdir/failed
            exit 1
        fi
        if [ ! -e "$file" ]; then
            echo "FAILED: tmp file does not exist" >> $tmpdir/failed
            exit 1
        fi
        mv $file $tmpdir/final_name
        rc=$?
        if [ $rc -ne 0 ]; then
            echo "FAILED: mv $file $tmpdir/final_name - $rc" >> $tmpdir/failed
            exit 1
        fi
    done
}

for count in $(seq 4); do
    mv_test $count &
done

wait

if [ -e $tmpdir/failed ]; then
    cat $tmpdir/failed
    exit 1
fi
