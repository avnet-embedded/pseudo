/*
 * Test that passing NULL to a parameter marked as nonnull works correctly
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */
#define _GNU_SOURCE

#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

// Passing a null pointer is the test scenario
#pragma GCC diagnostic ignored "-Wnonnull"

int statx_test() {
    if (statx(0, NULL, 0, 0, NULL) != -1) {
        return 1;
    }
    if (errno != EFAULT) {
        return 2;
    }
    return 0;
}

/* We run this multiple times to ensure that pseudo does not deadlock */
int main(void) {
    int rc = 0;

    for (int i = 0 ; i < 5 ; i++) {
        rc = statx_test();
        if (rc != 0)
            return rc;
    }

    return 0;
}
