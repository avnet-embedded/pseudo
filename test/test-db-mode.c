/*
 * Test that PSEUDO_DB_MODE correctly reconstructs permissions at file
 * creation time. PSEUDO_DB_MODE is used when a file is created (open
 * with O_CREAT, mkdir, etc.) to recover the intended mode from the
 * filesystem mode (which has been mangled by PSEUDO_FS_MODE).
 *
 * If PSEUDO_DB_MODE is broken, the mode stored in the database at
 * creation time will be wrong, which we detect by stat'ing immediately
 * after creation (before any chmod).
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

static int test_creat_mode(mode_t mode) {
    const char *path = "test-db-mode-tmp";
    struct stat st;

    /* Remove any prior file */
    unlink(path);

    /* Clear umask so it doesn't interfere */
    mode_t old_umask = umask(0);

    int fd = open(path, O_CREAT | O_WRONLY | O_EXCL, mode);
    if (fd < 0) {
        perror("open");
        umask(old_umask);
        return 1;
    }
    close(fd);

    if (stat(path, &st) != 0) {
        perror("stat");
        unlink(path);
        umask(old_umask);
        return 1;
    }

    mode_t got = st.st_mode & 07777;
    unlink(path);
    umask(old_umask);

    if (got != mode) {
        fprintf(stderr, "FAIL: open(O_CREAT, 0%04o) -> stat 0%04o\n", mode, got);
        return 1;
    }
    return 0;
}

static int test_mkdir_mode(mode_t mode) {
    const char *path = "test-db-mode-dir-tmp";
    struct stat st;

    rmdir(path);

    mode_t old_umask = umask(0);

    if (mkdir(path, mode) != 0) {
        perror("mkdir");
        umask(old_umask);
        return 1;
    }

    if (stat(path, &st) != 0) {
        perror("stat");
        rmdir(path);
        umask(old_umask);
        return 1;
    }

    mode_t got = st.st_mode & 07777;
    rmdir(path);
    umask(old_umask);

    if (got != mode) {
        fprintf(stderr, "FAIL: mkdir(0%04o) -> stat 0%04o\n", mode, got);
        return 1;
    }
    return 0;
}

int main(void) {
    int failures = 0;
    int total = 0;

    /* Test all octet values simultaneously: 00000, 01111, 02222, ..., 07777 */
    mode_t modes[] = {
        00000, 01111, 02222, 03333, 04444, 05555, 06666, 07777,
    };
    int num_modes = sizeof(modes) / sizeof(modes[0]);

    for (int i = 0; i < num_modes; i++) {
        total++;
        if (test_creat_mode(modes[i]) != 0)
            failures++;
    }

    for (int i = 0; i < num_modes; i++) {
        total++;
        if (test_mkdir_mode(modes[i]) != 0)
            failures++;
    }

    if (failures > 0) {
        fprintf(stderr, "%d/%d mode tests failed\n", failures, total);
        return 2;
    }
    return 0;
}
