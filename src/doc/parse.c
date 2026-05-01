#include "../main.h"
#include "../utils.h"
#include <ctype.h>
#include <dirent.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/stat.h>

// prefixes
#define FILE_DOC_PF "//!"
#define CODE_DOC_PF "///"

#define BLACKLIST                                                              \
{".git", "node_modules", ".md",    "target", "python3", "venv",              \
    "nob",  ".cargo",       ".cache", ".html",  ".toml"}

#define LINE_BUF_SIZE 512
#define CONTENT_BUF_SIZE 8024
#define DOCITEM_LIST_INIT_SIZE 8

typedef enum {
    STRUCT,
    TYPEDEF_STRUCT,
    ENUM,
    TYPEDEF_ENUM,
    MACRO_DEFINE,
    FUNCTION,
    UNKNOWN,
} CodeType;

static const char *codetype_strings[] = {
    [STRUCT] = "struct",      [TYPEDEF_STRUCT] = "typedef struct",
    [ENUM] = "enum",          [TYPEDEF_ENUM] = "typedef enum",
    [MACRO_DEFINE] = "macro", [FUNCTION] = "function",
    [UNKNOWN] = "unknown",
};

const char *codetype_to_str(CodeType ct) {
    if (ct >= 0 && ct < UNKNOWN) {
        return codetype_strings[ct];
    }
    return NULL;
}
typedef struct {
    CodeType ct;
    char *name;
} CodeParserOutput;

typedef enum {
    TAG_PAR, // parameter(s)
    TAG_RET, // return value(s)
    TAG_IMP, // important thing(s) to know
    TAG_UND, // undefined
} Tag;

static const char *tag_strings[] = {
    [TAG_PAR] = "@param",
    [TAG_RET] = "@return",
    [TAG_IMP] = "@important",
    [TAG_UND] = NULL,
};

Tag find_tag_in_str(const char *s) {
    for (int i = 0; i < TAG_UND; i++) {
        if (contains(s, tag_strings[i])) {
            return (Tag)i;
        }
    }
    return TAG_UND;
}

const char *tag_to_str(Tag tag) {
    if (tag >= 0 && tag < TAG_UND) {
        return tag_strings[tag];
    }
    return NULL;
}

char *get_content_after_tag(char *input) {
    if (!input) {
        ERROR("No content was found after a tag declaration");
    }

    Tag tag_pattern = find_tag_in_str(input);
    const char *tag_str = tag_to_str(tag_pattern);

    if (!tag_str) {
        ERROR("No content was found after a tag declaration");
    }

    char *tag_pos = strstr(input, tag_str);
    if (!tag_pos) {
        ERROR("No content was found after a tag declaration");
    }

    char *content_start = tag_pos + strlen(tag_str);

    // skip spaces after the tag
    while (*content_start == ' ' || *content_start == '\t') {
        content_start++;
    }

    size_t content_len = strlen(content_start);
    char *output = malloc(content_len + 1);
    if (!output) {
        ERROR("No content was found after a tag declaration");
    }

    memcpy(output, content_start, content_len + 1);
    return output;
}

typedef struct {
    Tag tag; // @return, @param, @important
    char *content;
} DocItemTag;

typedef enum {
    UNDEF = 0,
    _FILE = 1,
    CODE = 2,
} DocItemType;

typedef struct {
    DocItemType type;
    int *id;         // id for searching on static site later on
    int *line_nr;    // nr at start of doc comment
    char *file_path; // important: absolute path
    char *header;
    char *descr;
    CodeType code_type;
    char *code_name;
    char *code;       // the actual code the comment is referring to
    DocItemTag *tags; // @return, @param, @important
    size_t tag_count;
} DocItem;

void DocItem_init(DocItem *item) {
    memset(item, 0, sizeof(*item));
    item->type = UNDEF;
}
// void DocItem_init(DocItem *item) {
//     item->type = UNDEF;
//     item->id = NULL;
//     item->line_nr = NULL;
//     item->file_path = NULL;
//     item->code = NULL;
//     item->tags = NULL;
//     item->tag_count = 0;
// }

void FileReadIterator_init_codelines_buff(char **dlb) {
    memset(dlb, 0, LINE_BUF_SIZE * sizeof(dlb[0]));
}

typedef struct {
    FILE *file;
    char line[LINE_BUF_SIZE];
    int line_nr;

    // flags to signal that collection is in progress
    bool file_doc_item_in_prog;
    bool code_doc_item_in_prog;
    bool code_collect_in_prog; // new code field content being filled

    char *code_lines_buff[LINE_BUF_SIZE];
    int code_lines_buff_counter;

    char *desc_lines_buff[LINE_BUF_SIZE];
    int desc_lines_buff_counter;

    bool has_pushed_line;
    char pushed_line[LINE_BUF_SIZE];

    DocItem di;

} FileReadIterator;

bool FileReadIterator_init(FileReadIterator *it, const char *path) {
    it->file = fopen(path, "r");
    if (!it->file) {
        ERROR("Unable to open file: %s", path);
        return false;
    }

    FileReadIterator_init_codelines_buff(it->code_lines_buff);
    it->code_lines_buff_counter = 0;
    FileReadIterator_init_codelines_buff(it->desc_lines_buff);
    it->desc_lines_buff_counter = 0;

    it->line_nr = 0;
    it->file_doc_item_in_prog = false;
    it->code_doc_item_in_prog = false;
    it->code_collect_in_prog = false;

    it->has_pushed_line = false;
    it->pushed_line[0] = '\0';

    DocItem_init(&it->di);

    return true;
}

typedef struct {
    int id_count;
    size_t size;
    size_t capacity;
    DocItem *items;
} DocItemList;

void DocItem_print(const DocItem *di) {
    if (!di)
        return;

    printf("---- DOCITEM ----\n");

    if (di->file_path)
        printf("FILE: %s\n", di->file_path);
    if (di->id)
        printf("ID: %d\n", *di->id);
    if (di->type == _FILE) {
        printf("TYPE: file header\n");
    } else {
        printf("TYPE: code doc\n");
    }
    if (di->line_nr)
        printf("LINE: %d\n", *di->line_nr);
    if (di->header)
        printf("HEAD: %s\n", di->header);
    if (di->descr)
        printf("DESC: %s\n", di->descr);
    // if (di->code_type)
    //     printf("CODE_NAME:\n%s\n", di->code);
    if (di->code_name)
        printf("CODE_NAME:\n%s\n", di->code_name);
    if (di->code)
        printf("CODE:\n%s\n", di->code);
    printf("tags (%zu):\n", di->tag_count);

    for (size_t i = 0; i < di->tag_count; i++) {
        DocItemTag *tag = &di->tags[i];
        printf("  TAG %zu:\n", i);

        // assuming Tag is printable as int or enum
        printf("    TAG: %s\n", tag_to_str(tag->tag));

        if (tag->content)
            printf("    CONTENT: %s\n", tag->content);
    }

    printf("-----------------\n");
}

void DocItemList_print(const DocItemList *list) {
    if (!list)
        return;

    printf("==== DocItemList (size=%zu) ====\n", list->size);

    for (size_t i = 0; i < list->size; i++) {
        printf("Index %zu:\n", i);
        DocItem_print(&list->items[i]);
    }

    printf("================================\n");
}

void code_parser(CodeParserOutput *lo, char *code) {
    lo->ct = UNKNOWN;
    lo->name = NULL;
    if (code[0] == '\n') {
        memmove(code, code + 1, strlen(code));
    }

    char *first_line = get_first_line(code);
    char *last_line = get_last_line(code);

    if (contains(first_line, "#define")) {
        char *name = get_second_word(first_line);
        if (name != NULL) {
            char *paren = strchr(name, '(');
            if (paren != NULL) {
                *paren = '\0';
            }
            lo->name = strdup(name);
            lo->ct = MACRO_DEFINE;
        }
        return;
    }

    if (contains(first_line, "typedef")) {
        if (contains(first_line, "struct")) {
            if (last_line) {
                rem_spec_chars(last_line);
                lo->name = strdup(last_line);
            }
            lo->ct = TYPEDEF_STRUCT;
            return;
        }
        if (contains(first_line, "enum")) {
            char *close_brace = strrchr(first_line, '}');

            if (close_brace) {
                close_brace++;

                while (*close_brace == ' ' || *close_brace == '\t') {
                    close_brace++;
                }

                rem_spec_chars(close_brace);

                if (!is_empty_or_whitespace(close_brace)) {
                    lo->name = strdup(close_brace);
                }
            } else if (last_line) {
                rem_spec_chars(last_line);
                lo->name = strdup(last_line);
            }

            lo->ct = TYPEDEF_ENUM;
            return;
        }
    }

    if (contains(first_line, "enum")) {
        char *name = get_second_word(first_line);
        if (name != NULL) {
            rem_spec_chars(name);

            if (!is_empty_or_whitespace(name)) {
                lo->name = strdup(name);
            }
        }
        if ((lo->name == NULL || lo->name[0] == '\0') && last_line) {
            rem_spec_chars(last_line);

            if (!is_empty_or_whitespace(last_line)) {
                lo->name = strdup(last_line);
            }
        }
        lo->ct = ENUM;
        return;
    }

    if (contains(first_line, "struct")) {
        char *name = get_second_word(first_line);
        if (name != NULL) {
            rem_spec_chars(name);

            if (!is_empty_or_whitespace(name)) {
                lo->name = strdup(name);
            }
        }
        if ((lo->name == NULL || lo->name[0] == '\0') && last_line) {
            rem_spec_chars(last_line);

            if (!is_empty_or_whitespace(last_line)) {
                lo->name = strdup(last_line);
            }
        }
        lo->ct = STRUCT;
        return;
    }

    // functions
    if (contains(first_line, "(") && contains(first_line, ")")) {
        lo->name = get_func_name_from_sign(first_line);
        lo->ct = FUNCTION;
        return;
    }

    // fallback
    lo->name = strdup("unknown");
    lo->ct = UNKNOWN;
}
void DocItemList_insert_DocItem(DocItemList *dil, DocItem item) {
    // LOG("NEW DOC ITEM WAS ADDED\n");
    if (dil->size == dil->capacity) {
        size_t new_capacity = dil->capacity << 1;
        DocItem *new_items = realloc(dil->items, new_capacity * sizeof(DocItem));
        if (!new_items) {
            ERROR("Out of memory\n");
        }
        dil->items = new_items;
        dil->capacity = new_capacity;
    }
    dil->items[dil->size++] = item;
}

void DocItem_push_new_tag(DocItem *di, DocItemTag *dit) {
    DocItemTag *tmp = realloc(di->tags, (di->tag_count + 1) * sizeof(DocItemTag));
    if (!tmp) {
        ERROR("Allocation failure");
        return;
    }

    di->tags = tmp;
    di->tags[di->tag_count] = *dit;
    di->tag_count++;
}

static void append_str(char **out, size_t *len, size_t *cap, const char *s) {
    size_t n = strlen(s);
    bool has_newline = (n > 0 && s[n - 1] == '\n');

    size_t needed =
        n + (has_newline ? 0 : 1) + 1; // +1 for '\n' if needed, +1 for '\0'

    if (*len + needed > *cap) {
        size_t new_cap = (*cap == 0) ? 1024 : *cap * 2;
        while (*len + needed > new_cap)
            new_cap *= 2;

        char *tmp = realloc(*out, new_cap);
        if (!tmp)
            ERROR("Out of memory while collecting code");

        *out = tmp;
        *cap = new_cap;
    }

    memcpy(*out + *len, s, n);
    *len += n;

    if (!has_newline) {
        (*out)[(*len)++] = '\n';
    }

    (*out)[*len] = '\0';
}

static int brace_delta(const char *s) {
    int delta = 0;

    for (; *s; s++) {
        if (*s == '{')
            delta++;
        else if (*s == '}')
            delta--;
    }

    return delta;
}

static bool line_ends_code(const char *line, int brace_depth, bool saw_brace) {
    const char *p = line;

    while (*p == ' ' || *p == '\t') {
        p++;
    }

    // one-line macro:
    // #define MIN_VALUE(a, b) ...
    if (starts_with(p, "#define")) {
        return true;
    }

    if (saw_brace) {
        return brace_depth <= 0;
    }

    // one-line declaration:
    // void config_init(ConfigStore *store);
    return strchr(line, ';') != NULL;
}

static char *collect_code_after_doc(FileReadIterator *it,
        const char *first_line) {
    char *code = NULL;
    size_t len = 0;
    size_t cap = 0;

    int depth = 0;
    bool saw_brace = false;
    bool collected_any = false;

    const char *line = first_line;
    while (line) {
        if (collected_any &&
                (starts_with(line, CODE_DOC_PF) || starts_with(line, FILE_DOC_PF))) {
            return code;
        }

        if (!collected_any && is_empty_or_whitespace(line)) {
            return NULL;
        }

        if (!collected_any &&
                (starts_with(line, CODE_DOC_PF) || starts_with(line, FILE_DOC_PF))) {
            return NULL;
        }

        append_str(&code, &len, &cap, line);
        collected_any = true;

        int d = brace_delta(line);
        if (d != 0 || strchr(line, '{')) {
            saw_brace = true;
        }

        depth += d;

        if (line_ends_code(line, depth, saw_brace)) {
            return code;
        }

        if (!fgets(it->line, sizeof(it->line), it->file)) {
            return code;
        }

        it->line_nr++;
        line = it->line;
    }

    return code;
}
bool FileReadIterator_next(FileReadIterator *it) {

    if (!fgets(it->line, sizeof(it->line), it->file)) {
        return false;
    }

    switch (find_tag_in_str(it->line)) {
        case TAG_IMP:
            DocItem_push_new_tag(
                    &it->di, &(DocItemTag){.tag = TAG_IMP,
                    .content = get_content_after_tag(it->line)});
            // LOG("pushed new tag @important");
            break;
        case TAG_PAR:
            if (it->code_doc_item_in_prog) {
                DocItem_push_new_tag(
                        &it->di, &(DocItemTag){.tag = TAG_PAR,
                        .content = get_content_after_tag(it->line)});
                // LOG("pushed new tag @param");
            }
            break;
        case TAG_RET:
            if (it->code_doc_item_in_prog) {
                DocItem_push_new_tag(
                        &it->di, &(DocItemTag){.tag = TAG_RET,
                        .content = get_content_after_tag(it->line)});
                // LOG("pushed new tag @return");
            }
            break;
        case TAG_UND: {
                      }
    }

    it->line_nr++;
    return true;
}

void file_read(const char *path, DocItemList *dil) {
    FileReadIterator it;
    if (FileReadIterator_init(&it, path)) {
        while (FileReadIterator_next(&it)) {
            if (starts_with(it.line, FILE_DOC_PF)) {
                if (is_empty_doc_comment(it.line)) {

                    // check if header has been collected
                    // LOG("%s - empty file doc prefix: %s", path, it.line);

                } else {
                    if (!it.file_doc_item_in_prog) {
                        // add id
                        it.di.id = malloc(sizeof(int));
                        memcpy(it.di.id, &dil->id_count, sizeof(int));
                        dil->id_count++;

                        // add absolute path
                        it.di.file_path = xstrdup(expand_path(path));

                        // add line_nr
                        it.di.line_nr = malloc(sizeof(int));
                        memcpy(it.di.line_nr, &it.line_nr, sizeof(int));

                        // add docitem type
                        it.di.type = _FILE;

                        // add docitem header
                        it.di.header = xstrdup(remove_comment_prefix(it.line));

                        it.file_doc_item_in_prog = true;

                        // LOG("%s: file doc prefix: %s", it.di.file_path, it.line);
                    } else {
                        // collect description lines
                        if (!contains(it.line, tag_to_str(TAG_IMP))) {
                            char *l;
                            l = (char *)malloc(sizeof(char) * LINE_BUF_SIZE);
                            strcpy(l, remove_comment_prefix(it.line));
                            it.desc_lines_buff[it.desc_lines_buff_counter] = l;
                            it.desc_lines_buff_counter++;
                        }
                    }
                }
            }
            // check if an item is currenly being collected. if so, check if
            // suff. fields have been collected and push, else throw error
            if (it.code_doc_item_in_prog && !starts_with(it.line, CODE_DOC_PF)) {
                if (it.di.type != UNDEF && it.di.header && it.di.header[0] != '\0') {
                    char *desc_content =
                        join_lines(it.desc_lines_buff, it.desc_lines_buff_counter);

                    // it.di.descr = xstrdup(desc_content);
                    it.di.descr = xstrdup(desc_content ? desc_content : "");
                    it.di.code = collect_code_after_doc(&it, it.line);

                    if (!it.di.code) {
                        ERROR("No code found after doc comment in %s at line %d", path,
                                it.di.line_nr ? *it.di.line_nr : -1);

                        DocItem_init(&it.di);
                        it.code_doc_item_in_prog = false;
                        it.code_collect_in_prog = false;

                        FileReadIterator_init_codelines_buff(it.code_lines_buff);
                        FileReadIterator_init_codelines_buff(it.desc_lines_buff);
                        it.code_lines_buff_counter = 0;
                        it.desc_lines_buff_counter = 0;

                        continue;
                    }

                    CodeParserOutput cpo = {0};

                    char *code_copy = xstrdup(it.di.code);
                    if (!code_copy) {
                        ERROR("Failed to copy code");
                        continue;
                    }

                    code_parser(&cpo, code_copy);
                    free(code_copy);

                    it.di.code_type = cpo.ct;
                    it.di.code_name = xstrdup(cpo.name ? cpo.name : "unknown");
                    free(cpo.name);

                    DocItemList_insert_DocItem(dil, it.di);

                    DocItem_init(&it.di);
                    it.code_doc_item_in_prog = false;
                    it.code_collect_in_prog = false;

                    FileReadIterator_init_codelines_buff(it.code_lines_buff);
                    FileReadIterator_init_codelines_buff(it.desc_lines_buff);
                    it.code_lines_buff_counter = 0;
                    it.desc_lines_buff_counter = 0;
                } else {
                    ERROR("tried to push an un-initialized CODE DocItem");
                }

                continue;
            }

            if (starts_with(it.line, CODE_DOC_PF) && !is_empty_doc_comment(it.line)) {
                // if the doc comment is not empty and no code doc item in
                // progress yet, assume that current line is the header
                // LOG("new code doc item found");
                if (!it.code_doc_item_in_prog) {
                    // add id
                    it.di.id = malloc(sizeof(int));
                    memcpy(it.di.id, &dil->id_count, sizeof(int));
                    dil->id_count++;

                    // add absolute path
                    it.di.file_path = xstrdup(expand_path(path));

                    // add line_nr
                    it.di.line_nr = malloc(sizeof(int));
                    memcpy(it.di.line_nr, &it.line_nr, sizeof(int));

                    // add docitem type
                    it.di.type = CODE;

                    // add docitem header
                    it.di.header = xstrdup(remove_comment_prefix(it.line));

                    it.code_doc_item_in_prog = true;

                    // LOG("%s: file doc prefix: %s", it.di.file_path, it.line);
                } else {
                    // collect description lines
                    if (!contains(it.line, tag_to_str(TAG_IMP)) &&
                            !contains(it.line, tag_to_str(TAG_PAR)) &&
                            !contains(it.line, tag_to_str(TAG_RET))) {
                        char *l;
                        l = (char *)malloc(sizeof(char) * LINE_BUF_SIZE);
                        strcpy(l, remove_comment_prefix(it.line));
                        it.desc_lines_buff[it.desc_lines_buff_counter] = l;
                        it.desc_lines_buff_counter++;
                    }
                }
            }

            if (is_empty_or_whitespace(it.line)) {
                if (it.file_doc_item_in_prog) {
                    if (it.di.type != UNDEF && it.di.header && it.di.header[0] != '\0') {
                        char *desc_content =
                            join_lines(it.desc_lines_buff, it.desc_lines_buff_counter);

                        // it.di.descr = malloc(sizeof(char) * strlen(desc_content));
                        // it.di.descr = xstrdup(desc_content);
                        it.di.descr = xstrdup(desc_content ? desc_content : "");

                        DocItemList_insert_DocItem(dil, it.di);
                        DocItem_init(&it.di);
                        it.file_doc_item_in_prog = false;
                        FileReadIterator_init_codelines_buff(it.code_lines_buff);
                        FileReadIterator_init_codelines_buff(it.desc_lines_buff);
                        it.code_lines_buff_counter = 0;
                        it.desc_lines_buff_counter = 0;
                        // LOG("new _FILE DocItem initialized");
                    } else {
                        ERROR("tried to push an un-initialized _FILE DocItem");
                    }
                    continue;
                }
                // LOG("%s: empty line", path);
            }
        }
    }
}

void DocItemList_init(DocItemList **arr_ptr) {
    DocItemList *container = malloc(sizeof(DocItemList));
    if (!container) {
        ERROR("Memory allocation failed\n");
    }

    container->id_count = 0;
    container->size = 0;
    container->capacity = DOCITEM_LIST_INIT_SIZE;
    container->items = malloc(DOCITEM_LIST_INIT_SIZE * sizeof(DocItem));
    if (!container->items) {
        free(container);
        ERROR("Memory allocation failed\n");
    }

    *arr_ptr = container;
}

typedef enum {
    IS_FILE,
    IS_DIR,
    IS_ELSE,
} PathType;

typedef struct {
    PathType type;
    char *dest;
} Path;

PathType check_path_type(const char *path) {
    struct stat path_stat;

    if (lstat(path, &path_stat) != 0) {
        ERROR("Path does not exist: %s", path);
    }

    if (S_ISLNK(path_stat.st_mode)) {
        return IS_ELSE;
    }

    if (S_ISREG(path_stat.st_mode)) {
        return IS_FILE;
    } else if (S_ISDIR(path_stat.st_mode)) {
        return IS_DIR;
    } else {
        return IS_ELSE;
    }
}

void walk_dir(DocItemList *dil, char *target_path) {
    struct dirent *file_info;
    DIR *dir = opendir(target_path);
    if (dir == NULL) {
        return;
    }

    char *file_paths[30000];

    while ((file_info = readdir(dir)) != 0) {
        if (strcmp(file_info->d_name, ".") == 0 ||
                strcmp(file_info->d_name, "..") == 0) {
            continue;
        }
        // ignore files/dirs with blacklisted terms
        char *blkl[] = BLACKLIST;
        bool skip = false;
        for (size_t i = 0; i < sizeof(blkl) / sizeof(blkl[0]); i++) {
            if (contains(file_info->d_name, blkl[i])) {
                skip = true;
                break;
            }
        }
        if (skip) {
            continue;
        }

        char file_path[1000];
        snprintf(file_path, sizeof(file_path), "%s/%s", target_path,
                file_info->d_name);
        switch (check_path_type(file_path)) {
            case IS_FILE:
                // LOG("READ FILE: %s\n", file_path);
                file_read(file_path, dil);
                break;
            case IS_DIR:
                // LOG("CD DIR: %s\n", file_path);
                walk_dir(dil, file_path);
                break;
            case IS_ELSE:
            default:

                break;
        }
    }
    closedir(dir);
}

void DocItemList_free(DocItemList *dil) {
    if (!dil) {
        return;
    }

    for (size_t i = 0; i < dil->size; i++) {
        free(dil->items[i].file_path);
        free(dil->items[i].code);
        free(dil->items[i].line_nr);
        free(dil->items[i].id);

        for (size_t j = 0; j < dil->items[i].tag_count; j++) {
            free(dil->items[i].tags[j].content);
        }
        free(dil->items[i].tags);
    }

    free(dil->items);
    free(dil);
}

// entrypoint to recursively read and parse files in target_path
void parse(char *target_path, DocItemList *dil) {
    PathType target_path_type = check_path_type(target_path);

    switch (target_path_type) {
        case IS_DIR:
            walk_dir(dil, target_path);
            break;

        case IS_FILE:
            file_read(target_path, dil);
            break;

        case IS_ELSE:
        default:
            // DocItemList_free(dil);
            ERROR("Unexpected malfunction");
            break;
    }
}
