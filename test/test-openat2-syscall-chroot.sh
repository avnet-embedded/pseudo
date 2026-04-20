#! /bin/sh

# rc 77 indicates an ENOSYS, so return back we're skipped
# This shouldn't happen, but the user could disable openat2 syscall emulation
run_test() {
	$@
	rc=$?
	if [ "$rc" -eq 77 ]; then
		exit 255
	fi
	return "$rc"
}

# Run with and without ignored paths, matching test-openat coverage.
run_test chroot ./test ./test-openat2-syscall || exit $?
run_test env PSEUDO_IGNORE_PATHS=/ chroot ./test ./test-openat2-syscall || exit $?

exit 0
