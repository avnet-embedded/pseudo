/*
 * Test fts_open and related functions
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fts.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

int main(void) {
    FTS *ftsp;
    FTSENT *p;
    int fd;
    int file_count = 0;
    int dir_count = 0;

    /* Create test directory tree */
    mkdir("test_fts_dir", 0755);
    mkdir("test_fts_dir/sub1", 0755);
    mkdir("test_fts_dir/sub2", 0755);
    fd = open("test_fts_dir/file1", O_CREAT | O_WRONLY, 0644);
    close(fd);
    fd = open("test_fts_dir/sub1/file2", O_CREAT | O_WRONLY, 0644);
    close(fd);

    /* Set ownership so we can check pseudo tracks it through fts */
    check("chown file1", chown("test_fts_dir/file1", 100, 200) == 0);
    check("chown file2", chown("test_fts_dir/sub1/file2", 300, 400) == 0);

    /* fts_open */
    {
        char *paths[] = { "test_fts_dir", NULL };
        ftsp = fts_open(paths, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
        check("fts_open", ftsp != NULL);
    }

    /* Walk the tree */
    while ((p = fts_read(ftsp)) != NULL) {
        switch (p->fts_info) {
        case FTS_F:
            file_count++;
            if (strcmp(p->fts_name, "file1") == 0) {
                check("fts file1 uid", p->fts_statp->st_uid == 100);
                check("fts file1 gid", p->fts_statp->st_gid == 200);
            }
            if (strcmp(p->fts_name, "file2") == 0) {
                check("fts file2 uid", p->fts_statp->st_uid == 300);
                check("fts file2 gid", p->fts_statp->st_gid == 400);
            }
            break;
        case FTS_D:
            dir_count++;
            break;
        default:
            break;
        }
    }

    check("fts found 2 files", file_count == 2);
    check("fts found dirs", dir_count >= 3); /* test_fts_dir, sub1, sub2 */

    fts_close(ftsp);

    /* Cleanup */
    unlink("test_fts_dir/sub1/file2");
    unlink("test_fts_dir/file1");
    rmdir("test_fts_dir/sub1");
    rmdir("test_fts_dir/sub2");
    rmdir("test_fts_dir");

    return failures;
}
