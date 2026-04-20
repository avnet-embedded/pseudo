#! /bin/sh

# rc 77 indicates an ENOSYS, so return back we're skipped
# This usually indicates that glibc does not have the openat2 function
run_test() {
	$@
	rc=$?
	if [ "$rc" -eq 77 ]; then
		exit 255
	fi
	return "$rc"
}

# Run with and without ignored paths, matching test-openat coverage.
run_test chroot ./test ./test-openat2-func || exit $?
run_test env PSEUDO_IGNORE_PATHS=/ chroot ./test ./test-openat2-func || exit $?

exit 0
