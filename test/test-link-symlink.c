/*
 * Test link, linkat, symlink, symlinkat, readlink, readlinkat
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
#include <errno.h>

static int failures = 0;

static void check(const char *desc, int condition) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", desc);
        failures++;
    }
}

static int test_basic(void) {
    struct stat st, st2;
    char buf[PATH_MAX];
    ssize_t len;
    int fd, dirfd;

    /* Create a test file */
    fd = open("test_link_file", O_CREAT | O_WRONLY, 0644);
    check("write", write(fd, "hello", 5) == 5);
    close(fd);
    check("chown", chown("test_link_file", 100, 200) == 0);

    /* symlink */
    check("symlink", symlink("test_link_file", "test_link_sym") == 0);
    check("symlink lstat", lstat("test_link_sym", &st) == 0);
    check("symlink is link", S_ISLNK(st.st_mode));

    /* readlink */
    len = readlink("test_link_sym", buf, sizeof(buf));
    check("readlink len", len > 0);
    buf[len] = '\0';
    check("readlink value", strcmp(buf, "test_link_file") == 0);

    /* symlink target stat should show chown'd values */
    stat("test_link_sym", &st);
    check("symlink target uid", st.st_uid == 100);
    check("symlink target gid", st.st_gid == 200);

    /* link (hard link) */
    check("link", link("test_link_file", "test_link_hard") == 0);
    stat("test_link_hard", &st);
    stat("test_link_file", &st2);
    check("hardlink same inode", st.st_ino == st2.st_ino);
    check("hardlink uid", st.st_uid == 100);
    check("hardlink gid", st.st_gid == 200);

    /* linkat */
    dirfd = open(".", O_RDONLY | O_DIRECTORY);
    check("linkat", linkat(dirfd, "test_link_file", dirfd, "test_link_hard2", 0) == 0);
    stat("test_link_hard2", &st);
    check("linkat uid", st.st_uid == 100);

    /* symlinkat */
    check("symlinkat", symlinkat("test_link_file", dirfd, "test_link_sym2") == 0);
    check("symlinkat lstat", fstatat(dirfd, "test_link_sym2", &st, AT_SYMLINK_NOFOLLOW) == 0);
    check("symlinkat is link", S_ISLNK(st.st_mode));

    /* readlinkat */
    len = readlinkat(dirfd, "test_link_sym2", buf, sizeof(buf));
    check("readlinkat len", len > 0);
    buf[len] = '\0';
    check("readlinkat value", strcmp(buf, "test_link_file") == 0);

    close(dirfd);
    unlink("test_link_sym");
    unlink("test_link_sym2");
    unlink("test_link_hard");
    unlink("test_link_hard2");
    unlink("test_link_file");

    return 0;
}

static int test_chroot(const char *chroot_dir) {
    struct stat st;
    char buf[PATH_MAX];
    char path[PATH_MAX];
    ssize_t len;
    int fd, dirfd;

    /* Create a file inside the chroot directory */
    snprintf(path, sizeof(path), "%s/cr_link_file", chroot_dir);
    fd = open(path, O_CREAT | O_WRONLY, 0644);
    if (fd < 0) { perror("open chroot file"); return 1; }
    check("chroot: write", write(fd, "hello", 5) == 5);
    close(fd);
    check("chroot: chown", chown(path, 100, 200) == 0);

    if (chroot(chroot_dir) != 0) {
        perror("chroot");
        return 1;
    }
    if (chdir("/") != 0) {
        perror("chdir /");
        return 1;
    }

    /* symlink with absolute target inside chroot.
     * The fix in pseudo_util.c strips a doubled chroot prefix
     * from absolute symlink targets during path resolution.
     * Without the fix, stat through this symlink would fail or
     * resolve to the wrong path.
     */
    check("chroot: symlink abs", symlink("/cr_link_file", "/cr_sym_abs") == 0);
    check("chroot: stat through abs symlink", stat("/cr_sym_abs", &st) == 0);
    check("chroot: abs symlink uid", st.st_uid == 100);
    check("chroot: abs symlink gid", st.st_gid == 200);

    /* readlink should return the chroot-relative target */
    len = readlink("/cr_sym_abs", buf, sizeof(buf));
    check("chroot: readlink abs len", len > 0);
    if (len > 0) {
        buf[len] = '\0';
        check("chroot: readlink abs value", strcmp(buf, "/cr_link_file") == 0);
    }

    /* symlink with relative target inside chroot */
    check("chroot: symlink rel", symlink("cr_link_file", "/cr_sym_rel") == 0);
    check("chroot: stat through rel symlink", stat("/cr_sym_rel", &st) == 0);
    check("chroot: rel symlink uid", st.st_uid == 100);

    /* symlinkat with absolute target inside chroot */
    dirfd = open("/", O_RDONLY | O_DIRECTORY);
    check("chroot: open dirfd", dirfd >= 0);
    check("chroot: symlinkat abs", symlinkat("/cr_link_file", dirfd, "cr_symat_abs") == 0);
    check("chroot: stat symlinkat abs", fstatat(dirfd, "cr_symat_abs", &st, 0) == 0);
    check("chroot: symlinkat abs uid", st.st_uid == 100);

    /* readlinkat should return the chroot-relative target */
    len = readlinkat(dirfd, "cr_symat_abs", buf, sizeof(buf));
    check("chroot: readlinkat abs len", len > 0);
    if (len > 0) {
        buf[len] = '\0';
        check("chroot: readlinkat abs value", strcmp(buf, "/cr_link_file") == 0);
    }

    /* linkat inside chroot */
    check("chroot: linkat", linkat(dirfd, "cr_link_file", dirfd, "cr_hard", 0) == 0);
    check("chroot: stat linkat", fstatat(dirfd, "cr_hard", &st, 0) == 0);
    check("chroot: linkat uid", st.st_uid == 100);

    close(dirfd);
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
