/* Code contributed by Gauthier HADERER via lists.yoctoproject.org */

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    char *rpath = realpath(argv[1], NULL);
    if (!rpath) {
        perror("realpath");
        return 1;
    }
    printf("%s\n", rpath);
    free(rpath);
    return 0;
}
