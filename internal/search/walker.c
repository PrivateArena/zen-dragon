#define _GNU_SOURCE
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <string.h>
#include <limits.h>
#include "walker.h"

struct linux_dirent {
    long           d_ino;
    off_t          d_off;
    unsigned short d_reclen;
    char           d_name[];
};

#define BUF_SIZE (1024 * 1024 * 5)

extern void goEntryCallback(const char *path, void *user_data);

static void list_dir(const char *path, int recursive, int full_path,
                     void *user_data, const char *base_path) {
    int fd, nread;
    char *buf = malloc(BUF_SIZE);
    if (!buf) return;

    struct linux_dirent *d;
    int bpos;
    char d_type;
    char entry_buf[PATH_MAX];

    fd = open(path, O_RDONLY | O_DIRECTORY);
    if (fd == -1) {
        free(buf);
        return;
    }

    for (;;) {
        nread = syscall(SYS_getdents, fd, buf, BUF_SIZE);
        if (nread == -1 || nread == 0) break;

        for (bpos = 0; bpos < nread;) {
            d = (struct linux_dirent *)(buf + bpos);
            d_type = *(buf + bpos + d->d_reclen - 1);

            int is_dot = (strcmp(d->d_name, ".") == 0 || strcmp(d->d_name, "..") == 0);

            if (d->d_ino != 0 && d_type == DT_REG && !is_dot) {
                if (full_path) {
                    snprintf(entry_buf, PATH_MAX, "%s/%s", path, d->d_name);
                    goEntryCallback(entry_buf, user_data);
                } else if (strcmp(path, base_path) == 0 || strcmp(base_path, ".") == 0) {
                    goEntryCallback(d->d_name, user_data);
                } else {
                    const char *rel = path + strlen(base_path);
                    if (*rel == '/') rel++;
                    snprintf(entry_buf, PATH_MAX, "%s/%s", rel, d->d_name);
                    goEntryCallback(entry_buf, user_data);
                }
            }

            if (recursive && d_type == DT_DIR && !is_dot) {
                char sub_path[PATH_MAX];
                snprintf(sub_path, PATH_MAX, "%s/%s", path, d->d_name);
                list_dir(sub_path, recursive, full_path, user_data, base_path);
            }

            bpos += d->d_reclen;
        }
    }

    close(fd);
    free(buf);
}

void walk_directory_cgo(const char *root, int recursive, int full_path,
                         void *user_data) {
    char abs_path[PATH_MAX];
    if (full_path) {
        if (realpath(root, abs_path) == NULL)
            strncpy(abs_path, root, PATH_MAX);
    } else {
        strncpy(abs_path, root, PATH_MAX);
    }
    list_dir(abs_path, recursive, full_path, user_data, abs_path);
}
