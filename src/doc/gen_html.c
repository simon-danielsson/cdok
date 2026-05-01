#include "gen_html.h"
#include "../main.h"
#include "../utils.h"
#include "file_content.h"
#include <stdio.h>
#include <string.h>

char *append_to_path(char *path, char *to_append) {
    size_t ext = strlen(".html");
    size_t output_size = strlen(path) + strlen(to_append) + 1 + 2 + ext;
    char *output = (char *)malloc(sizeof(char) * output_size);

    strcpy(output, path);
    strcat(output, "/");
    strcat(output, to_append);
    strcat(output, ".html");

    return output;
}

typedef struct {
    char *file_path;     // canonical group path, always valid
    DocItem file_header; // optional
    DocItem *di;         // CODE items
    size_t di_count;
} HtmlIdxGrp;

static int find_group_by_path(HtmlIdxGrp *groups, size_t group_count,
        const char *file_path) {
    for (size_t i = 0; i < group_count; i++) {
        if (groups[i].file_path && file_path &&
                strcmp(groups[i].file_path, file_path) == 0) {
            return (int)i;
        }
    }
    return -1;
}

void HtmlIdxGrp_free(HtmlIdxGrp *groups, size_t group_count) {
    if (!groups)
        return;

    for (size_t i = 0; i < group_count; i++) {
        free(groups[i].file_path);
        free(groups[i].di);
    }

    free(groups);
}

HtmlIdxGrp *sort_docitemlist_to_html_idx_groups(DocItemList *dil,
        size_t *out_group_count) {

    if (!dil || !out_group_count) {
        return NULL;
    }

    *out_group_count = 0;

    HtmlIdxGrp *groups = NULL;
    size_t group_count = 0;
    size_t group_capacity = 0;

    for (size_t i = 0; i < dil->size; i++) {
        DocItem *item = &dil->items[i];

        if (item->type != _FILE && item->type != CODE) {
            continue;
        }
        if (!item->file_path) {
            continue;
        }
        int group_idx = find_group_by_path(groups, group_count, item->file_path);

        if (group_idx < 0) {
            if (group_count == group_capacity) {
                size_t new_capacity = group_capacity == 0 ? 8 : group_capacity * 2;

                HtmlIdxGrp *new_groups =
                    realloc(groups, new_capacity * sizeof(*groups));

                if (!new_groups) {
                    for (size_t j = 0; j < group_count; j++) {
                        free(groups[j].di);
                    }
                    free(groups);
                    return NULL;
                }

                groups = new_groups;

                for (size_t j = group_capacity; j < new_capacity; j++) {
                    memset(&groups[j], 0, sizeof(groups[j]));
                    groups[j].file_header.type = UNDEF;
                }

                group_capacity = new_capacity;
            }
            group_idx = (int)group_count;

            groups[group_idx].file_path = xstrdup(item->file_path);
            if (!groups[group_idx].file_path) {
                for (size_t j = 0; j < group_count; j++) {
                    free(groups[j].file_path);
                    free(groups[j].di);
                }
                free(groups);
                return NULL;
            }

            groups[group_idx].file_header.type = UNDEF;
            group_count++;
        }
        HtmlIdxGrp *grp = &groups[group_idx];

        if (item->type == _FILE) {
            grp->file_header = *item; // shallow copy
        } else if (item->type == CODE) {
            DocItem *new_di =
                realloc(grp->di, (grp->di_count + 1) * sizeof(*grp->di));
            if (!new_di) {
                for (size_t j = 0; j < group_count; j++) {
                    free(groups[j].di);
                }
                free(groups);
                return NULL;
            }

            grp->di = new_di;
            grp->di[grp->di_count++] = *item; // shallow copy
        }
    }

    if (group_count == 0) {
        free(groups);
        return NULL;
    }

    *out_group_count = group_count;
    return groups;
}

char *concat_tags_to_str(DocItemTag *dit, size_t tag_amt) {
    size_t len = 1; // for '\0'
    for (size_t i = 0; i < tag_amt; i++) {
        len += strlen(tag_to_str(dit[i].tag)) + 5 + strlen(dit[i].content);
    }
    char *output = malloc(len);
    if (!output)
        return NULL;

    output[0] = '\0';
    for (size_t i = 0; i < tag_amt; i++) {
        strcat(output, tag_to_str(dit[i].tag));
        strcat(output, " ");
        strcat(output, dit[i].content);
        strcat(output, "<br>");
    }
    return output;
}

void gen_index_html(DocItemList *dil, char *output_path) {

    size_t group_count = 0;
    HtmlIdxGrp *groups = sort_docitemlist_to_html_idx_groups(dil, &group_count);
    if (!groups) {
        ERROR("no documented code could be found or collected");
    }

    char *file_path = append_to_path(output_path, "index");
    FILE *fp = fopen(file_path, "w");

    // header content
    fprintf(fp, "%s\n", header_content);
    fprintf(fp, "%s\n", "<body>");
    fprintf(fp, "%s\n", main_header_content);
    fprintf(fp, "%s\n", "<div class=\"header-right\">");
    fprintf(fp, "%s\n", "<p>");
    fprintf(fp, "%s\n", "<a");
    fprintf(fp, "href=\"%s\">return to index</a>\n", "index.html");
    fprintf(fp, "%s\n", "</p>");
    fprintf(fp, "<p>%s</p>\n", expand_path_with_conc_home(output_path));
    fprintf(fp, "</div>\n</header>\n<hr>\n");

    // table body start
    fprintf(fp, "%s\n", table_start);

    for (size_t i = 0; i < group_count; i++) {
        // <tbody>
        // <tr class="file-section">
        //     <td>data</td> // id
        //     <td>data</td> // path
        //     <td>data</td> // name
        //     <td>data</td> // type
        //     <td>data</td> // header text
        //     <td>data</td> // tags
        // </tr>

        // printf("group: %s\n", groups[i].file_path);
        int file_id = groups[i].file_header.id ? *groups[i].file_header.id : -1;

        fprintf(fp,
                "<tr class=\"file-section\" onclick=\"window.location='%d.html'\" "
                "style=\"cursor:pointer;\">\n",
                file_id);
        // fprintf(fp,
        //         "<tr class=\"file-section\" onclick=\"window.location='%d.html'\"
        //         " "style=\"cursor:pointer;\">\n", *groups[i].file_header.id);

        // file-section: id
        if (groups[i].file_header.id != NULL) {
            fprintf(fp, "<td>%d</td>\n", *groups[i].file_header.id);
        } else {
            fprintf(fp, "<td>NULL</td>\n");
        }

        // file-section: path
        if (groups[i].file_header.file_path != NULL) {
            fprintf(fp, "<td>%s</td>\n", groups[i].file_header.file_path);
        } else {
            fprintf(fp, "<td>%s</td>\n", groups[i].file_path);
        }

        // file-section: name
        if (groups[i].file_header.file_path != NULL) {
            fprintf(fp, "<td>%s</td>\n",
                    expand_path_with_conc_home(groups[i].file_header.file_path));
        } else {
            fprintf(fp, "<td>%s</td>\n",
                    expand_path_with_conc_home(groups[i].file_path));
        }

        // file-section: type
        fprintf(fp, "<td>file</td>\n");

        // file-section: header text
        if (groups[i].file_header.header != NULL) {
            fprintf(fp, "<td>%s</td>\n", groups[i].file_header.header);
        } else {
            fprintf(fp, "<td>NULL</td>\n");
        }
        // file-section: tags
        if (groups[i].file_header.tags != NULL) {
            fprintf(fp, "<td>%s</td>\n",
                    concat_tags_to_str(groups[i].file_header.tags,
                        groups[i].file_header.tag_count));
        } else {
            fprintf(fp, "<td>No tags</td>\n");
        }
        fprintf(fp, "</tr>\n");

        // <tr
        //     onclick="window.location='/Users/simondanielsson/dev/html/cdok_page_sketch/item.html'"
        //     style="cursor:pointer;">
        //     <td>data</td> // id
        //     <td>data</td> // path
        //     <td>data</td> // name
        //     <td>data</td> // type
        //     <td>data</td> // header text
        //     <td>data</td> // tags
        // </tr>
        for (size_t j = 0; j < groups[i].di_count; j++) {
            char item_file_name[100];
            snprintf(item_file_name, sizeof(item_file_name), "%d",
                    *groups[i].di[j].id);
            // char *file_path = append_to_path(output_path, item_file_name);

            fprintf(fp,
                    "<tr onclick=\"window.location='%s.html'\" "
                    "style=\"cursor:pointer;\">\n",
                    item_file_name);

            // file id
            if (groups[i].di[j].id != NULL) {
                fprintf(fp, "<td>%d</td>\n", *groups[i].di[j].id);
            } else {
                fprintf(fp, "<td>NULL</td>\n");
            }

            // file path
            if (groups[i].di[j].file_path != NULL) {
                fprintf(fp, "<td>%s</td>\n", groups[i].di[j].file_path);
            } else {
                fprintf(fp, "<td>NULL</td>\n");
            }

            // file name
            if (groups[i].di[j].code_name != NULL) {
                fprintf(fp, "<td>%s</td>\n", groups[i].di[j].code_name);
            } else {
                fprintf(fp, "<td>NULL</td>\n");
            }
            // file code type
            fprintf(fp, "<td>%s</td>\n", codetype_to_str(groups[i].di[j].code_type));

            // file header text
            fprintf(fp, "<td>%s</td>\n", groups[i].di[j].header);

            // file-section: tags
            if (groups[i].di[j].tags != NULL) {
                char *tags =
                    concat_tags_to_str(groups[i].di[j].tags, groups[i].di[j].tag_count);
                fprintf(fp, "<td>%s</td>\n", tags ? tags : "");
                free(tags);
            } else {
                fprintf(fp, "<td>No tags</td>\n");
            }
            // printf("file: %s\n", groups[i].di[j].file_path);
            fprintf(fp, "</tr>\n");
        }
    }

    fprintf(fp, "%s\n", "</tbody>");

    // table body end
    fprintf(fp, "%s\n", "</table>");

    // footer
    fprintf(fp, "%s\n", footer_content);
    fprintf(fp, "%s\n", "</body></html>");
    fclose(fp);

    // free
    HtmlIdxGrp_free(groups, group_count);
}

void gen_item_html(DocItemList *dil, char *output_path) {

    if (!dil)
        return;
    // printf("NUM OF ITEMS IN: %d\n", (int)dil->size);

    // int counter = 0;
    for (size_t i = 0; i < dil->size; i++) {

        char item_file_name[100];
        snprintf(item_file_name, sizeof(item_file_name), "%d", *dil->items[i].id);

        char *file_path = append_to_path(output_path, item_file_name);
        FILE *fp = fopen(file_path, "w");

        // header content
        fprintf(fp, "%s\n", header_content);
        fprintf(fp, "%s\n", "<body>");
        fprintf(fp, "%s\n", main_header_content);
        fprintf(fp, "%s\n", "<div class=\"header-right\">");
        fprintf(fp, "%s\n", "<p>");
        fprintf(fp, "%s\n", "<a");
        fprintf(fp, "href=\"%s\">return to index</a>\n", "index.html");
        fprintf(fp, "%s\n", "</p>");
        fprintf(fp, "<p>%s</p>\n", expand_path_with_conc_home(output_path));
        fprintf(fp, "</div>\n</header>\n<hr>\n");

        // body start
        fprintf(fp, "<p class=\"item-tags\">%s</p>\n",
                expand_path_with_conc_home(dil->items[i].file_path));

        fprintf(fp, "%s\n", "<div class=\"item-nametype\">");
        fprintf(fp, "<h2 class=\"item-type\">%s</h2>\n",
                codetype_to_str(dil->items[i].code_type));
        fprintf(fp, "<h2 class=\"item-name\">%s</h2>\n", dil->items[i].code_name);
        fprintf(fp, "%s\n", "</div>");

        fprintf(fp, "<p class=\"item-tags\">%s</p>\n",
                concat_tags_to_str(dil->items[i].tags, dil->items[i].tag_count));

        if (!is_empty_or_whitespace(dil->items[i].descr)) {

            fprintf(fp, "<hr>\n");
            fprintf(fp, "<h3 class=\"item-desc\">description</h3>\n");
            fprintf(fp, "<p>%s</p>\n", dil->items[i].descr);
        }

        fprintf(fp, "%s\n", "<div class=\"item-code\">");
        fprintf(fp, "<pre><code class=\"language-c\">%s</code></pre>\n",
                dil->items[i].code);
        fprintf(fp, "%s\n", "</div>");

        // footer
        fprintf(fp, "%s\n", footer_content);
        fprintf(fp, "%s\n", "</body></html>");

        // close file
        fclose(fp);
        // counter++;
    }
    // printf("NUM OF ITEMS OUT: %d\n", counter);
}

void gen_file_item_html(DocItemList *dil, char *output_path) {
    if (!dil)
        return;

    size_t group_count = 0;
    HtmlIdxGrp *groups = sort_docitemlist_to_html_idx_groups(dil, &group_count);
    if (!groups) {
        ERROR("no documented code could be found or collected");
    }
    for (size_t i = 0; i < group_count; i++) {
        if (groups[i].file_header.type == UNDEF || !groups[i].file_header.id) {
            continue;
        }

        char item_file_name[100];
        snprintf(item_file_name, sizeof(item_file_name), "%d",
                *groups[i].file_header.id);

        char *file_path = append_to_path(output_path, item_file_name);
        FILE *fp = fopen(file_path, "w");

        // header content
        fprintf(fp, "%s\n", header_content);
        fprintf(fp, "%s\n", main_header_content);
        fprintf(fp, "%s\n", "<div class=\"header-right\">");
        fprintf(fp, "%s\n", "<p>");
        fprintf(fp, "%s\n", "<a");
        fprintf(fp, "href=\"%s\">return to index</a>\n", "index.html");
        fprintf(fp, "%s\n", "</p>");
        fprintf(fp, "<p>%s</p>\n", expand_path_with_conc_home(output_path));
        fprintf(fp, "</div>\n</header>\n<hr>\n");

        // body start
        fprintf(fp, "%s\n", "<body>");
        fprintf(fp, "<p class=\"item-tags\">%s</p>\n",
                expand_path_with_conc_home(groups[i].file_header.file_path));
        fprintf(fp, "%s\n", "<div class=\"item-nametype\">");
        fprintf(fp, "<h2 class=\"item-type\">file</h2>\n");
        fprintf(fp, "<h2 class=\"item-name\">%s</h2>\n",
                strip_path(groups[i].file_header.file_path));
        fprintf(fp, "%s\n", "</div>");
        fprintf(fp, "<h3 class=\"item-type\">%s</h3>\n",
                groups[i].file_header.header);

        fprintf(fp, "<p class=\"item-tags\">%s</p>\n",
                concat_tags_to_str(groups[i].file_header.tags,
                    groups[i].file_header.tag_count));

        if (!is_empty_or_whitespace(groups[i].file_header.descr)) {
            fprintf(fp, "<h3 class=\"item-desc\">description</h3>\n");
            fprintf(fp, "<p>%s</p>\n", groups[i].file_header.descr);
        }

        // TODO: the code part of a file page should just be the entire file

        FILE *codepr = fopen(groups[i].file_path, "rb");
        if (codepr) {
            // get file size
            fseek(codepr, 0, SEEK_END);
            long size = ftell(codepr);
            rewind(codepr);

            // allocate buffer
            char *buffer = malloc(size + 1);
            if (buffer) {
                // read file
                fread(buffer, 1, size, codepr);
                buffer[size] = '\0';

                fclose(codepr);

                fprintf(fp, "%s\n", "<div class=\"item-code\">");
                fprintf(fp, "<pre><code class=\"language-c\">%s</code></pre>\n",
                        buffer);
                fprintf(fp, "%s\n", "</div>");
                free(buffer);
            } else {
                free(buffer);
                break;
            }
        }

        fprintf(fp, "%s\n", footer_content);
        fprintf(fp, "%s\n", "</body></html>");

        // close file
        fclose(fp);
    }

    // free
    HtmlIdxGrp_free(groups, group_count);
}

void gen_html(char *output_path, DocItemList *dil) {
    if (is_valid_path(output_path)) {
        gen_index_html(dil, output_path);
        gen_item_html(dil, output_path);
        gen_file_item_html(dil, output_path);
    } else {
        ERROR("Path to gen folder is invalid");
    }
}
