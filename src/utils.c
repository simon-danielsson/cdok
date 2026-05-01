//! common utilities for cenv

#include "main.h"
#include <ctype.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

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

char *remove_comment_prefixes_and_whitespace_at_beginning(char *input) {
    if (input == NULL) {
        return NULL;
    }

    char *p = input;

    while (*p && isspace((unsigned char)*p)) {
        p++;
    }

    if (strncmp(p, "//", 2) == 0) {
        p += 2;
    } else if (strncmp(p, "/**", 3) == 0) {
        p += 3;
    } else if (strncmp(p, "/*", 2) == 0) {
        p += 2;
    } else if (*p == '*') {
        p += 1;
    }

    while (*p && isspace((unsigned char)*p)) {
        p++;
    }

    memmove(input, p, strlen(p) + 1);
    return input;
}

char *expand_path(const char *input) {
    char absolute[PATH_MAX];

    if (realpath(input, absolute) == NULL) {
        ERROR("Could not expand relative path");
    }

    char *output = strdup(absolute);

    if (!output) {
        ERROR("Memory allocation failed");
        return NULL;
    }

    return output;
}

char *expand_path_with_conc_home(const char *input) {
    char absolute[PATH_MAX];

    if (realpath(input, absolute) == NULL) {
        ERROR("Could not expand relative path");
        return NULL;
    }

    // Get home directory
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        if (pw) {
            home = pw->pw_dir;
        } else {
            ERROR("Could not determine home directory");
            return strdup(absolute);
        }
    }

    size_t home_len = strlen(home);

    if (strncmp(absolute, home, home_len) == 0 &&
            (absolute[home_len] == '/' || absolute[home_len] == '\0')) {

        size_t new_len = strlen(absolute) - home_len + 1; // +1 for '~'
        char *output = malloc(new_len + 1);

        if (!output) {
            ERROR("Memory allocation failed");
            return NULL;
        }

        output[0] = '~';
        strcpy(output + 1, absolute + home_len);

        return output;
    }

    char *output = strdup(absolute);
    if (!output) {
        ERROR("Memory allocation failed");
        return NULL;
    }

    return output;
}

char *remove_comment_prefix(const char *input) {
    if (!input)
        return NULL;
    while (isspace((unsigned char)*input)) {
        input++;
    }

    if (strncmp(input, "///", 3) == 0 || strncmp(input, "//!", 3) == 0) {
        input += 3;

        if (*input == ' ')
            input++;
    }

    while (isspace((unsigned char)*input)) {
        input++;
    }
    char *output = strdup(input);
    if (!output) {
        ERROR("Memory allocation failed");
        return NULL;
    }

    return output;
}

bool is_empty_or_whitespace(const char *str) {
    if (str == NULL)
        return true;
    while (*str) {
        if (!isspace((unsigned char)*str)) {
            return false;
        }
        str++;
    }
    return true;
}

void rem_spec_chars(char *str) {
    char *dst = str;

    while (*str) {
        if (isalnum((unsigned char)*str) || *str == '_') {
            *dst++ = *str;
        }
        str++;
    }

    *dst = '\0';
}

char *get_first_line(char *str) {
    if (!str || *str == '\0')
        return NULL;

    char *start = str;

    // skip leading whitespace/newlines
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }

    if (*start == '\0')
        return NULL;

    char *end = start;

    // move until newline or end of string
    while (*end && *end != '\n') {
        end++;
    }

    return start;
}

char *get_last_line(char *str) {
    if (!str || *str == '\0')
        return NULL;
    char *end = str + strlen(str);
    // skip trailing whitespace/newlines
    while (end > str && isspace((unsigned char)*(end - 1))) {
        end--;
    }
    if (end == str)
        return NULL;
    char *start = end;
    while (start > str && *(start - 1) != '\n') {
        start--;
    }
    return start;
}

bool is_empty_doc_comment(char *input) {
    char *stripped = remove_comment_prefix(input);
    if (is_empty_or_whitespace(stripped)) {
        return true;
    }
    return false;
}

bool is_valid_path(const char *s) {
    struct stat path_stat;

    if (stat(s, &path_stat) != 0) {
        // path does not exist or is not accessible
        return false;
    }

    // path is:
    if (S_ISREG(path_stat.st_mode)) {
        // regular file
        return true;
    } else if (S_ISDIR(path_stat.st_mode)) {
        // directory
        return true;
    } else {
        // something else (symlink, socket etc.)
        return false;
    }
    return true;
}

bool contains(const char *str, const char *word) {
    return strstr(str, word) != NULL;
}

bool starts_with(const char *str, const char *word) {
    size_t len_str = strlen(str);
    size_t len_word = strlen(word);

    if (len_word > len_str)
        return false;
    return strncmp(str, word, len_word) == 0;
}

// returns true if input str is end of C block comment
bool is_comment_closer(const char *str) {
    if (!str)
        return false;

    while (isspace((unsigned char)*str)) {
        str++;
    }

    return strcmp(str, "*/") == 0 || strcmp(str, "*/\n") == 0;
}

// string duplication helper
char *xstrdup(const char *s) {
    if (!s)
        return NULL;
    char *out = malloc(strlen(s) + 1);
    if (!out) {
        ERROR("Memory allocation failed");
        return NULL;
    }
    strcpy(out, s);
    return out;
}

// helper to join collected lines
char *join_lines(char **lines, int count) {
    size_t total = 1; // for '\0'

    for (int i = 0; i < count; i++) {
        if (lines[i]) {
            total += strlen(lines[i]);
        }
    }

    char *out = malloc(total);
    if (!out) {
        ERROR("Memory allocation failed");
        return NULL;
    }

    out[0] = '\0';

    for (int i = 0; i < count; i++) {
        if (lines[i]) {
            strcat(out, lines[i]);
            free(lines[i]);
            lines[i] = NULL;
        }
    }

    return out;
}

// returns int (0|1) as success/failure
int get_second_word_with_ret(char *str) {
    char *token = strtok(str, " "); // first word
    token = strtok(NULL, " ");      // second word

    if (token != NULL) {
        str = xstrdup(token);
        return 0;
    } else {
        return 1;
    }
}

char *get_second_word(char *str) {
    char *token = strtok(str, " \t"); // first word
    token = strtok(NULL, " \t");      // second word

    return token; // pointer inside str
}

char *get_func_name_from_sign(char *str) {
    char *get_second_word(char *str);
    char *token = strtok(str, " \t");
    if (!contains(token, "(")) {
        while (true) {
            token = strtok(NULL, " \t");
            if (contains(token, "(")) {
                break;
            }
        }
    }

    // delete everything after the "(" to only get the name
    char *p;
    p = token;
    int i = 0;
    char name[64] = {0};
    while (*p != '\0') {
        if (p[i] == '(') {
            break;
        }
        name[i] = p[i];
        i++;
    }

    // delete "*" from beginning if it is there
    rem_spec_chars(name);

    return strdup(name); // pointer inside str
}

// takes in a path and leaves only the last filename/dir name
char *strip_path(char *file_name) {
    char output[200] = {0};
    int counter = strlen(file_name);
    while (true) {
        if (file_name[counter] != '/') {
            strcpy(output, &file_name[counter]);
            counter--;
        } else {
            break;
        }
    }
    return xstrdup(output);
}

/// buffer size for char input into run_cmd() function
#define CMD_BUFF_SIZE 512

/// runs shell terminal commands
/// @param buffer that owns the resulting string output
/// @param shell command
/// @return exit-code
int run_cmd(char *output_buff, char *cmd) {
    FILE *fp = popen(cmd, "r");
    if (fp == NULL) {
        ERROR("Couldn't run shell cmd: %s", cmd);
    }
    char temp[CMD_BUFF_SIZE];
    temp[0] = '\0';
    output_buff[0] = '\0';
    while (fgets(temp, sizeof(temp), fp) != NULL) {
        strncat(output_buff, temp, CMD_BUFF_SIZE);
    }
    pclose(fp);
    return 0;
}

/// converts string to uppercase
char *str_to_upper(const char *str) {
    int i = 0;
    char temp[124];
    temp[0] = '\0';
    while (str[i] != '\0') {
        temp[i] = toupper(str[i]); // Convert character to uppercase
        i++;
    }
    return xstrdup(temp);
}
