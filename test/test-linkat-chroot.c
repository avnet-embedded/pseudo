/*
 * Test that link/linkat inside a chroot does not segfault
 * when the path is shorter than the chroot path.
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * The bug was that linkat's chroot path stripping used strncmp()
 * without negation, causing an out-of-bounds read at
 * oldpath[pseudo_chroot_len] when the path was shorter than the
 * chroot path. To reliably trigger this, we place the path string
 * at the end of a mapped page with the next page unmapped, so any
 * out-of-bounds access will SIGSEGV.
 */
#define _GNU_SOURCE

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

/* Place str at the end of a page with the next page unmapped.
 * Any read past the string will hit unmapped memory and SIGSEGV. */
static char *guarded_string(const char *str) {
    long pagesize = sysconf(_SC_PAGESIZE);
    size_t len = strlen(str) + 1;
    char *pages = mmap(NULL, pagesize * 2, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pages == MAP_FAILED) {
        perror("mmap");
        return NULL;
    }
    if (munmap(pages + pagesize, pagesize) == -1) {
        perror("munmap");
        return NULL;
    }
    char *dest = pages + pagesize - len;
    memcpy(dest, str, len);
    return dest;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <chrootdir>\n", argv[0]);
        return 2;
    }
    if (chroot(argv[1]) == -1) {
        perror("chroot");
        return 1;
    }
    if (chdir("/") == -1) {
        perror("chdir");
        return 1;
    }

    char *oldpath = guarded_string("/a");
    char *newpath = guarded_string("/b");
    if (!oldpath || !newpath)
        return 1;

    /* link() calls linkat() internally. The short paths are placed
     * at page boundaries so that the buggy out-of-bounds read at
     * oldpath[pseudo_chroot_len] will SIGSEGV instead of silently
     * reading adjacent memory. */
    if (link(oldpath, newpath) == -1) {
        perror("link");
        return 1;
    }
    return 0;
}
