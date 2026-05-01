// #ifndef UTILS_H
// #define UTILS_H

#include <ctype.h>
#include <stdbool.h>

bool is_valid_path(const char *s);
bool contains(const char *str, const char *word);
char *remove_comment_prefix(char *input);
bool is_empty_or_whitespace(const char *str);
bool is_comment_closer(const char *str);
char *xstrdup(const char *s);
char *join_lines(char **lines, int count);
bool is_empty_doc_comment(char *input);
char *expand_path(const char *input);
bool starts_with(const char *str, const char *word);
void rem_spec_chars(char *str);
char *get_first_line(char *str);
char *get_last_line(char *str);
int get_second_word_with_ret(char *str);
char *get_second_word(char *str);
char *get_func_name_from_sign(char *str);
char *expand_path_with_conc_home(const char *input);
char *remove_comment_prefixes_and_whitespace_at_beginning(char *input);
char *strip_path(char *file_name);
int run_cmd(char *output_buff, char *cmd);
char *get_executable_dir(void);
char *str_to_upper(const char *str);

// #endif
