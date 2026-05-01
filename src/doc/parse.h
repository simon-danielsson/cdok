#pragma once
#ifndef PARSE_H
#define PARSE_H
#include <ctype.h>
#include <stddef.h>

typedef enum {
  STRUCT,
  TYPEDEF_STRUCT,
  ENUM,
  TYPEDEF_ENUM,
  MACRO_DEFINE,
  FUNCTION,
  UNKNOWN,
} CodeType;

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

typedef struct {
  int *id_count;
  size_t size;
  size_t capacity;
  DocItem *items;
} DocItemList;

void parse(char *target_path, DocItemList *dil);

void DocItemList_free(DocItemList *dil);
void DocItemList_init(DocItemList **dil);
void DocItemList_print(DocItemList *dil);

const char *tag_to_str(Tag tag);
const char *codetype_to_str(CodeType ct);

#endif
