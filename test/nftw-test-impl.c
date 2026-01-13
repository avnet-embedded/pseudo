#define _GNU_SOURCE
#include <stdio.h>
#include <ftw.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define TEST_CHDIR 1
#define TEST_NO_CHDIR 0

static int print_filename = 0;
static int print_ownership = 0;
static int print_cwd = 0;

static int skip_subtree = 0;
static int skip_subtree_counter = 0;

static int skip_siblings = 0;
char* basedir;


// When FTW_CHDIR is enabled, cwd changes each time, so realpath
// might not be able to resolve the path
//char* filepath = realpath(fpath, NULL);
char* get_absolute_path(const char* fpath) {
    char* filepath = realpath(fpath, NULL);

    if (filepath == NULL) {
        if (fpath[0] == '/') {
            filepath = malloc(strlen(fpath) + 1);
            strcpy(filepath, fpath);
        } else {
            filepath = malloc(strlen(fpath) + strlen(basedir));
            memset(filepath, 0, strlen(fpath) + strlen(basedir));
            strcat(filepath, basedir);
            // skip the initial "."
            strcat(filepath, fpath + 1);
        }
    }
    return filepath;
}

static int callback(const char* fpath, const struct NFTW_STAT_STRUCT *sb, int typeflag, struct FTW *ftwbuf){
    (void) ftwbuf;

    if (print_filename) {
        char* filepath = get_absolute_path(fpath);
        printf("%s\n", filepath);
        free(filepath);
    }


    if (print_ownership) {
        printf("%d - %d\n", sb->st_gid, sb->st_uid);
    }

    if (print_cwd) {
        char* cwd = getcwd(NULL, 0);
        printf("%s\n", cwd);
        free(cwd);
    }


    // if subtrees are skipped, don't skip immediately at the root,
    // otherwise it's not much of a test. Skip it after encountering 2
    // subfolders (which is an arbitrary numbers, without much science,
    // admittedly, but better than 0)
    if (skip_subtree && typeflag == FTW_D) {
        if (skip_subtree_counter >= 2)
            return FTW_SKIP_SUBTREE;

        skip_subtree_counter++;
    }

    if (skip_siblings && typeflag == FTW_F) {
        return FTW_SKIP_SIBLINGS;
    }

    return 0;
}

static int run_test(int flags) {
    return NFTW_NAME("./walking", callback, 10, flags);
}

static int test_skip_siblings_file_depth_walking(int change_dir){
    int flags = FTW_ACTIONRETVAL | FTW_DEPTH;

    // store base_dir, because the fpath returned by (n)ftw can be relative to this
    // folder - that way a full absolute path can be constructed and compared,
    // if needed.
    if (change_dir){
        basedir = getcwd(NULL, 0);
        flags |= FTW_CHDIR;
    }

    skip_siblings = 1;
    return run_test(flags);
}

/*
 * Every time a folder entry is sent to the callback, respond with FTW_SKIP_SUBTREE.
 * This should skip that particular folder completely, and continue processing
 * with its siblings (or parent, if there are no siblings).
 * Return value is expected to be 0, default walking order.
 */
static int test_skip_subtree_on_folder(){
    int flags = FTW_ACTIONRETVAL;
    skip_subtree = 1;
    return run_test(flags);
}

/*
 * Arguments:
 * argv[1]: always the test name
 * argv[2], argv[3]: in case the test name refers to a test without using
 *                   pseudo (no_pseudo), then they should be the gid and uid
 *                   of the current user. Otherwise these arguments are ignored.
 *
 * skip_subtree_pseudo/skip_subtree_no_pseudo: these tests are calling nftw()
 * with the FTW_ACTIONRETVAL flag, which reacts based on the return value from the
 * callback. These tests check the call's reaction to FTW_SKIP_SUBTREE call,
 * upon which nftw() should stop processing the current folder, and continue
 * with the next sibling of the folder.
 *
 * skip_siblings_pseudo/skip_siblings_no_pseudo: very similar to skip_subtree
 * tests, but it verified FTW_SKIP_SIBLINGS response, which should stop processing
 * the current folder, and continue in its parent.
 *
 * skip_siblings_chdir_pseudo/skip_siblings_chdir_no_pseudoL same as skip_siblings
 * tests, but also pass the FTW_CHDIR flag and verify that the working directory
 * is changed as expected between callback calls.
 */
int main(int argc, char* argv[])
{
    if (argc != 3) {
        printf("Usage: %s TESTNAME DETAILS\n", argv[0]);
        printf("TESTNAME is one of: skip_subtree, skip_siblings, skip_siblings_chdir\n");
        printf("DETAILS is one of: filename, ownership, cwd\n");
        return 1;
    }

    if (strcmp(argv[2], "filename") == 0) {
        print_filename = 1;
    } else if (strcmp(argv[2], "ownership") == 0) {
        print_ownership = 1;
    } else if (strcmp(argv[2], "cwd") == 0) {
        print_cwd = 1;
    } else {
        printf("Unknown second parameter\n");
        return 1;
    }


    if (strcmp(argv[1], "skip_subtree") == 0) {
        return test_skip_subtree_on_folder();
    } else if (strcmp(argv[1], "skip_siblings") == 0) {
        return test_skip_siblings_file_depth_walking(TEST_NO_CHDIR);
    } else if (strcmp(argv[1], "skip_siblings_chdir") == 0) {
        return test_skip_siblings_file_depth_walking(TEST_CHDIR);
    } else {
        printf("Unknown test name\n");
        return 1;
    }
}
