/*
 * Test that rename() from outside PSEUDO_INCLUDE_PATHS followed by
 * hardlink properly tracks file ownership.
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 */
#define _GNU_SOURCE

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

static int failures = 0;

static void check(const char *desc, int condition) {
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", desc);
		failures++;
	}
}

int main(int argc, char *argv[])
{
	struct stat st1, st2;
	char src_path[PATH_MAX];
	char dest_path[PATH_MAX];
	char link_path[PATH_MAX];
	int fd;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s <src_dir> <dest_dir>\n", argv[0]);
		return 1;
	}

	/* Create a file in src_dir (outside PSEUDO_INCLUDE_PATHS) */
	snprintf(src_path, sizeof(src_path), "%s/testfile.txt", argv[1]);
	fd = open(src_path, O_CREAT | O_WRONLY, 0644);
	if (fd < 0) {
		perror("create source file");
		return 1;
	}
	if (write(fd, "hello\n", 6) != 6) {
		perror("write");
		close(fd);
		return 1;
	}
	close(fd);

	/* rename() from untracked src_dir to tracked dest_dir */
	snprintf(dest_path, sizeof(dest_path), "%s/testfile.txt", argv[2]);
	if (rename(src_path, dest_path) != 0) {
		perror("rename");
		return 1;
	}

	/* Create a hardlink in the tracked directory */
	snprintf(link_path, sizeof(link_path), "%s/testfile2.txt", argv[2]);
	if (link(dest_path, link_path) != 0) {
		perror("link");
		return 1;
	}

	/* Stat both files and verify consistent uid 0 */
	if (stat(dest_path, &st1) != 0) {
		perror("stat dest");
		return 1;
	}
	if (stat(link_path, &st2) != 0) {
		perror("stat link");
		return 1;
	}

	check("same inode", st1.st_ino == st2.st_ino);
	check("UIDs match", st1.st_uid == st2.st_uid);
	check("dest uid is 0", st1.st_uid == 0);
	check("link uid is 0", st2.st_uid == 0);

	unlink(link_path);
	unlink(dest_path);

	return failures;
}
