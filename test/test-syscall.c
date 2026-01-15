/* This test case must match the implementation in:
 *   ports/linux/pseudo_wrappers.c
 */

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

int main() {
    long rc = 0;

#ifdef SYS_renameat2
    int retval;

    #ifndef RENAME_NOREPLACE
      # define RENAME_NOREPLACE (1 << 0)
    #endif

    /* The two paths don't exist, but we're just checking on error what the errno is. */
    retval = syscall(SYS_renameat2, AT_FDCWD, "empty_path_1", AT_FDCWD, "empty_path_2", RENAME_NOREPLACE);
    if (retval != -1) {
        errno = 0;
        printf("renameat2: fail: function implemented\n");
        rc++;
    } else {
        if (errno != ENOSYS) {
            printf("renameat2: fail: function implemented: %s\n", strerror(errno));
            rc++;
        }
        else
            printf("renameat2: pass\n");
    }

#endif

#ifdef SYS_seccomp
    /* We should verify this was intercepted, but at present we're not able to. */
    errno = 0;
    printf("seccomp: skip\n");
#endif

#ifdef SYS_openat2
    struct open_how
    {
    #  ifdef __UINT64_TYPE__
      __UINT64_TYPE__ flags, mode, resolve;
    #  else
      unsigned long long int flags, mode, resolve;
    #  endif
    };
    #  define RESOLVE_NO_XDEV	0x01
    #  define RESOLVE_NO_MAGICLINKS	0x02
    #  define RESOLVE_NO_SYMLINKS	0x04
    #  define RESOLVE_BENEATH	0x08
    #  define RESOLVE_IN_ROOT	0x10
    #  define RESOLVE_CACHED	0x20

    struct open_how how;
    memset(&how, 0, sizeof(how));
    how.flags = O_DIRECTORY;
    how.resolve = RESOLVE_IN_ROOT;

    int fd;
    fd = syscall(SYS_openat2, AT_FDCWD, ".", &how, sizeof(how));
    printf("diag: openat2: %d (%s)\n", fd, strerror(errno));
    if (fd == -1) {
        if (errno != ENOSYS) {
            printf("openat2: fail: function implemented: %s\n", strerror(errno));
            rc++;
        }
        else {
            printf("openat2: fail: %s", strerror(errno));
            rc++;
	}
    }
    else {
        printf("openat2: pass\n");
    }

    close(fd);
#endif

    return rc;
}
