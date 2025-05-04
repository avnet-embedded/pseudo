#define _GNU_SOURCE
#include <stdio.h>
#include <ftw.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

static int current_recursion_level = 0;
static int max_recursion = 0;

static int print_filename = 0;
static int print_ownership = 0;


static int callback(const char* fpath, const struct FTW_STAT_STRUCT *sb, int __attribute__ ((unused)) typeflag){
    if (current_recursion_level < max_recursion) {
        ++current_recursion_level;
        if (FTW_NAME("./walking/a1", callback, 10) != 0) {
            printf("Recursive call failed\n");
            exit(1);
        }
    }

    if (print_filename) {
        char *absolute_path = realpath(fpath, NULL);
        printf("%s\n", absolute_path);
        free(absolute_path);
    }

    if (print_ownership) {
        printf("%d - %d\n", sb->st_gid, sb->st_uid);
    }

    return 0;
}

static int run_test() {
    int ret;
    ret = FTW_NAME("./walking", callback, 10);
    return ret;
}

/*
 * Walk the given directory structure, and print the given details
 * (ownership or absolute path) of the entries encountered.
 */
static int test_walking(){
    return run_test();
}

/*
 * This test is very similar to test_walking(), but the callback at the
 * start also calls ftw(), "max_recursion" times.
 * It is trying to test pseudo's implementation of handling multiple
 * concurrent (n)ftw calls in the same thread.
 */
static int test_walking_recursion(){
    max_recursion = 3;
    return run_test();
}

/*
 * Arguments:
 * argv[1]: the test name
 * argv[2]: "filename" or "ownership" - determine which detail to print
 */
int main(int argc, char* argv[])
{
    if (argc != 3) {
        printf("Usage: %s TESTNAME DETAILS\n", argv[0]);
        printf("TESTNAME is one of: no_recursion, recursion\n");
        printf("DETAILS is one of: filename, ownership\n");
        return 1;
    }

    if (strcmp(argv[2], "filename") == 0) {
        print_filename = 1;
    } else if (strcmp(argv[2], "ownership") == 0) {
        print_ownership = 1;
    } else {
        printf("Unknown second parameter");
        return 1;
    }
    
    if (strcmp(argv[1], "no_recursion") == 0) {
        return test_walking();
    } else if (strcmp(argv[1], "recursion") == 0) {
        return test_walking_recursion();
    } else {
        printf("Unknown test name: %s\n", argv[1]);
        return 1;
    }
}
