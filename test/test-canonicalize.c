/*
 * Test canonicalize_file_name
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

static int test_basic(void) {
    char *result;
    char cwd[PATH_MAX];
    int fd;

    if (!getcwd(cwd, sizeof(cwd))) {
        perror("getcwd");
        return 1;
    }

    /* Create a test file and symlink */
    fd = open("test_canon_file", O_CREAT | O_WRONLY, 0644);
    close(fd);
    if (symlink("test_canon_file", "test_canon_link") != 0) {
        perror("symlink");
        return 1;
    }

    /* canonicalize_file_name on regular file */
    result = canonicalize_file_name("test_canon_file");
    check("canon file", result != NULL);
    if (result) {
        check("canon file starts with /", result[0] == '/');
        check("canon file contains name", strstr(result, "test_canon_file") != NULL);
        free(result);
    }

    /* canonicalize_file_name on symlink (should resolve) */
    result = canonicalize_file_name("test_canon_link");
    check("canon link", result != NULL);
    if (result) {
        check("canon link resolves", strstr(result, "test_canon_file") != NULL);
        /* Should NOT contain the link name */
        check("canon link not link", strstr(result, "test_canon_link") == NULL);
        free(result);
    }

    /* canonicalize_file_name on . */
    result = canonicalize_file_name(".");
    check("canon dot", result != NULL);
    if (result) {
        check("canon dot matches cwd", strcmp(result, cwd) == 0);
        free(result);
    }

    /* canonicalize_file_name on nonexistent should return NULL */
    result = canonicalize_file_name("test_canon_noexist");
    check("canon noexist NULL", result == NULL);

    unlink("test_canon_link");
    unlink("test_canon_file");

    return 0;
}

static int test_chroot(const char *chroot_dir) {
    char *result;
    char path[PATH_MAX];
    int fd;

    /* Create test file and symlink inside the chroot directory */
    snprintf(path, sizeof(path), "%s/cr_canon_file", chroot_dir);
    fd = open(path, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) { perror("open chroot file"); return 1; }
    close(fd);

    snprintf(path, sizeof(path), "%s/cr_canon_link", chroot_dir);
    if (symlink("cr_canon_file", path) != 0) {
        perror("symlink in chroot dir");
        return 1;
    }

    /* Also create an absolute-target symlink */
    snprintf(path, sizeof(path), "%s/cr_abs_link", chroot_dir);
    if (symlink("/cr_canon_file", path) != 0) {
        perror("abs symlink in chroot dir");
        return 1;
    }

    if (chroot(chroot_dir) != 0) {
        perror("chroot");
        return 1;
    }
    if (chdir("/") != 0) {
        perror("chdir /");
        return 1;
    }

    /* canonicalize on a file: result must be /cr_canon_file, not
     * /real/path/to/chroot/cr_canon_file */
    result = canonicalize_file_name("/cr_canon_file");
    check("chroot: canon abs file", result != NULL);
    if (result) {
        check("chroot: canon abs file path", strcmp(result, "/cr_canon_file") == 0);
        free(result);
    }

    /* canonicalize on a relative symlink */
    result = canonicalize_file_name("/cr_canon_link");
    check("chroot: canon rel symlink", result != NULL);
    if (result) {
        check("chroot: canon rel symlink resolves", strcmp(result, "/cr_canon_file") == 0);
        free(result);
    }

    /* canonicalize on an absolute-target symlink */
    result = canonicalize_file_name("/cr_abs_link");
    check("chroot: canon abs symlink", result != NULL);
    if (result) {
        check("chroot: canon abs symlink resolves", strcmp(result, "/cr_canon_file") == 0);
        free(result);
    }

    /* canonicalize on . should return / */
    result = canonicalize_file_name(".");
    check("chroot: canon dot", result != NULL);
    if (result) {
        check("chroot: canon dot is /", strcmp(result, "/") == 0);
        free(result);
    }

    /* path traversal: /../cr_canon_file must stay confined */
    result = canonicalize_file_name("/../cr_canon_file");
    check("chroot: canon traverse", result != NULL);
    if (result) {
        check("chroot: canon traverse resolves", strcmp(result, "/cr_canon_file") == 0);
        free(result);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    int rc;

    rc = test_basic();
    if (rc)
        return rc;

    if (argc > 1) {
        rc = test_chroot(argv[1]);
        if (rc)
            return rc;
    }

    return failures;
}
