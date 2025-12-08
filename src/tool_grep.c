#include "tool.h"

#include "panic.h"

#include <assert.h>
#include <dirent.h>
#include <glob.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>

// Build an error response JSON
static char *build_grep_error(void *parent, const char *error_msg)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_bool(doc, root, "success", false);
    yyjson_mut_obj_add_str(doc, root, "error", error_msg);

    char *json = yyjson_mut_write_opts(doc, 0, NULL, NULL, NULL);
    if (json == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    char *result = talloc_strdup(parent, json);
    free(json);
    yyjson_mut_doc_free(doc);

    if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return result;
}

// Search a single file for pattern matches
static void search_file(void *parent, const char *filename, regex_t *regex,
                        char **output_buffer, size_t *match_count)
{
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        return; // Skip files we can't open
    }

    // Search line by line
    char *line = NULL;
    size_t line_cap = 0;
    ssize_t line_len;
    size_t line_num = 0;

    while ((line_len = getline(&line, &line_cap, f)) != -1) {
        line_num++;

        // Remove trailing newline for cleaner output
        if (line_len > 0 && line[line_len - 1] == '\n') { // LCOV_EXCL_BR_LINE
            line[line_len - 1] = '\0';
        }

        // Test if line matches pattern
        if (regexec(regex, line, 0, NULL, 0) == 0) {
            // Match found - format as "filename:line_number: line_content"
            char *match_line;
            if (*match_count > 0) {
                match_line = talloc_asprintf(parent, "\n%s:%zu: %s",
                                             filename, line_num, line);
            } else {
                match_line = talloc_asprintf(parent, "%s:%zu: %s",
                                             filename, line_num, line);
            }

            if (match_line == NULL) { // LCOV_EXCL_BR_LINE
                free(line); // LCOV_EXCL_LINE
                fclose(f); // LCOV_EXCL_LINE
                PANIC("Out of memory"); // LCOV_EXCL_LINE
            }

            // Append to output buffer
            char *new_buffer = talloc_asprintf(parent, "%s%s", *output_buffer, match_line);
            if (new_buffer == NULL) { // LCOV_EXCL_BR_LINE
                free(line); // LCOV_EXCL_LINE
                fclose(f); // LCOV_EXCL_LINE
                PANIC("Out of memory"); // LCOV_EXCL_LINE
            }

            *output_buffer = new_buffer;
            (*match_count)++;
        }
    }

    free(line);
    fclose(f);
}

// Build success response JSON
static char *build_grep_success(void *parent, const char *output, size_t count)
{
    yyjson_mut_doc *doc = yyjson_mut_doc_new(NULL);
    if (doc == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    yyjson_mut_val *root = yyjson_mut_obj(doc);
    if (root == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }
    yyjson_mut_doc_set_root(doc, root);

    yyjson_mut_obj_add_bool(doc, root, "success", true);

    yyjson_mut_val *data = yyjson_mut_obj(doc);
    if (data == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    yyjson_mut_obj_add_str(doc, data, "output", output);
    yyjson_mut_obj_add_uint(doc, data, "count", count);
    yyjson_mut_obj_add_val(doc, root, "data", data);

    char *json = yyjson_mut_write_opts(doc, 0, NULL, NULL, NULL);
    if (json == NULL) { // LCOV_EXCL_BR_LINE
        yyjson_mut_doc_free(doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    char *result = talloc_strdup(parent, json);
    free(json);
    yyjson_mut_doc_free(doc);

    if (result == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    return result;
}

res_t ik_tool_exec_grep(void *parent, const char *pattern, const char *glob_filter, const char *path)
{
    assert(pattern != NULL); // LCOV_EXCL_BR_LINE

    // Compile regular expression
    regex_t regex;
    int regex_ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (regex_ret != 0) {
        char error_msg[256];
        regerror(regex_ret, &regex, error_msg, sizeof(error_msg));
        char *err_str = talloc_asprintf(parent, "Invalid pattern: %s", error_msg);
        if (err_str == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        char *result = build_grep_error(parent, err_str);
        return OK(result);
    }

    // Build glob pattern to find files
    const char *search_path = (path != NULL && path[0] != '\0') ? path : ".";
    char *full_glob_pattern;

    if (glob_filter != NULL && glob_filter[0] != '\0') {
        full_glob_pattern = talloc_asprintf(parent, "%s/%s", search_path, glob_filter);
    } else {
        full_glob_pattern = talloc_asprintf(parent, "%s/*", search_path);
    }
    if (full_glob_pattern == NULL) { // LCOV_EXCL_BR_LINE
        regfree(&regex); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    // Execute glob to find matching files
    glob_t glob_result;
    int glob_ret = glob(full_glob_pattern, 0, NULL, &glob_result);

    // Build result buffer for matches
    char *output_buffer = talloc_strdup(parent, "");
    if (output_buffer == NULL) { // LCOV_EXCL_BR_LINE
        regfree(&regex); // LCOV_EXCL_LINE
        if (glob_ret == 0) globfree(&glob_result); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    }

    size_t match_count = 0;

    // Search in each file (if glob succeeded)
    if (glob_ret == 0 && glob_result.gl_pathc > 0) { // LCOV_EXCL_BR_LINE
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            const char *filename = glob_result.gl_pathv[i];

            // Skip non-regular files (directories, symlinks, etc.)
            struct stat file_stat;
            if (stat(filename, &file_stat) != 0 || !S_ISREG(file_stat.st_mode)) { // LCOV_EXCL_BR_LINE
                continue;
            }

            search_file(parent, filename, &regex, &output_buffer, &match_count);
        }

        globfree(&glob_result);
    }

    regfree(&regex);

    // Build and return success response
    char *result = build_grep_success(parent, output_buffer, match_count);
    return OK(result);
}
