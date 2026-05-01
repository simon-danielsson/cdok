#include <stdio.h>
#define NOB_IMPLEMENTATION
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#include "nob.h"
#include <stdlib.h>
#include <string.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

char *get_executable_dir(void) {
    char path[PATH_MAX];
    size_t len = 0;

#if defined(__linux__)
    ssize_t r = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (r == -1)
        return NULL;
    path[r] = '\0';
    len = (size_t)r;

#elif defined(__APPLE__)
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0)
        return NULL;

    // mac can return a relative path
    char resolved[PATH_MAX];
    if (realpath(path, resolved) == NULL)
        return NULL;

    strncpy(path, resolved, sizeof(path));
    path[sizeof(path) - 1] = '\0';
    len = strlen(path);
#endif

    // strip filename to get directory
    for (size_t i = len; i > 0; i--) {
        if (path[i] == '/') {
            path[i] = '\0';
            break;
        }
    }

    return strdup(path); // caller must free
}

#define BINARY_NAME "main"
#define RELEASE_FOLDER "./build/release/"
#define DEBUG_FOLDER "./build/debug/"
#define TEST_FOLDER "./build/tests/"
#define TEST_SRC_FOLDER "./tests"
#define SRC_FOLDER "./src"

typedef struct {
    char **items;
    size_t count;
    size_t capacity;
} Path_List;

static bool has_c_extension(const char *path) {
    size_t len = strlen(path);
    return len >= 2 && strcmp(path + len - 2, ".c") == 0;
}

static void path_list_append(Path_List *arr, const char *str) {
    if (arr->count >= arr->capacity) {
        size_t new_cap = arr->capacity == 0 ? 8 : arr->capacity * 2;
        char **new_items = realloc(arr->items, new_cap * sizeof(char *));
        NOB_ASSERT(new_items != NULL);
        arr->items = new_items;
        arr->capacity = new_cap;
    }
    arr->items[arr->count++] = strdup(str);
    NOB_ASSERT(arr->items[arr->count - 1] != NULL);
}

static bool collect_files(Nob_Walk_Entry entry) {
    Path_List *arr = (Path_List *)entry.data;
    if (entry.type == FILE_REGULAR && has_c_extension(entry.path)) {
        path_list_append(arr, entry.path);
    }
    return true;
}

int main(int argc, char **argv) {
    NOB_GO_REBUILD_URSELF(argc, argv);

    Nob_Cmd cmd = {0};

    nob_cc(&cmd);
    nob_cc_flags(&cmd);

    // arg 1: source dir
    // everything after: flags

    // append flags
    const char *std = argv[argc - 1];
    nob_cmd_append(&cmd, std);

    // build binary into in nob folder
    char target_bin[PATH_MAX];
    char *nob_dir = get_executable_dir();
    if (!nob_dir) {
        fprintf(stderr, "failed to get executable dir\n");
        exit(1);
    }
    snprintf(target_bin, sizeof target_bin, "%s/main", nob_dir);
    nob_cmd_append(&cmd, "-o", target_bin);

    Path_List files = {0};

    nob_walk_dir(argv[1], collect_files, &files);

    for (size_t i = 0; i < files.count; i++) {
        nob_cmd_append(&cmd, files.items[i]);
    }

    if (!nob_cmd_run(&cmd))
        return 1;

    for (size_t i = 0; i < files.count; i++) {
        free(files.items[i]);
    }
    free(nob_dir);
    free(files.items);

    return 0;
}
