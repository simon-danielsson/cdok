//! main entry point

#include "main.h"
#include "doc/gen_html.h"
#include "doc/parse.h"
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#define BUF 512

void cmd_doc(char *src_dir, char *dest_dir, bool doc_open) {
    // initialize struct where doc tags will be accumulated
    DocItemList *dil;
    DocItemList_init(&dil);

    // be sure gen folder exists
    char mkdir_cmd[BUF];
    snprintf(mkdir_cmd, sizeof(mkdir_cmd), "mkdir -p %s", src_dir);
    system(mkdir_cmd);

    // parse and generate
    parse(src_dir, dil);
    gen_html(dest_dir, dil);
    // free
    DocItemList_free(dil);

    if (doc_open) {
        char cmd[BUF * 4];
        cmd[0] = '\0';
#ifdef __APPLE__
        strncat(cmd, "open ", BUF);

#elif defined(__linux__)
        strncat(cmd, "xdg-open ", size);

#else
        ERROR("Unknown or non-unix platform detected\n");
#endif
        char path[BUF * 2];
        snprintf(path, sizeof(path), "%s/index.html", dest_dir);
        strncat(cmd, path, BUF);
        system(cmd);
    }
}

int main(int argc, char *argv[]) {
    char *src_dir = NULL;
    char *dest_dir = NULL;
    int open = 0;

    int opt;

    while ((opt = getopt(argc, argv, "s:d:o")) != -1) {
        switch (opt) {
            case 's':
                src_dir = optarg;
                break;
            case 'd':
                dest_dir = optarg;
                break;
            case 'o':
                open = 1;
                break;
            default:
                fprintf(stderr, "%s -s <src_dir> -d <dest_dir> [-o: open in browser]\n",
                        argv[0]);
                return 1;
        }
    }

    cmd_doc(src_dir, dest_dir, open);

    return 0;
}
