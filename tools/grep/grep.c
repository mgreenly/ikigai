#include "grep.h"

#include "shared/panic.h"
#include "shared/wrapper_posix.h"

#include <glob.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>

int32_t grep_search(void *ctx, const grep_params_t *params, grep_result_t *out)
{
    if (params == NULL || params->pattern == NULL || out == NULL) {
        return -1;
    }

    const char *path = (params->path != NULL && params->path[0] != '\0') ? params->path : ".";

    // Compile regex pattern
    regex_t regex;
    int32_t regex_ret = regcomp(&regex, params->pattern, REG_EXTENDED);
    if (regex_ret != 0) {
        return -1;
    }

    // Build glob pattern to find files
    char *file_glob = NULL;
    if (params->glob != NULL && params->glob[0] != '\0') {
        size_t path_len = strlen(path);
        size_t glob_len = strlen(params->glob);
        file_glob = talloc_array(ctx, char, (unsigned int)(path_len + glob_len + 2));
        if (file_glob == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        snprintf(file_glob, path_len + glob_len + 2, "%s/%s", path, params->glob);
    } else {
        size_t path_len = strlen(path);
        file_glob = talloc_array(ctx, char, (unsigned int)(path_len + 3));
        if (file_glob == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        snprintf(file_glob, path_len + 3, "%s/*", path);
    }

    // Execute glob to find files
    glob_t glob_result;
    int32_t glob_ret = glob_(file_glob, 0, NULL, &glob_result);

    // Silently ignore glob errors - just return empty result
    if (glob_ret != 0 && glob_ret != GLOB_NOMATCH) {
        regfree(&regex);
        out->output = talloc_strdup(ctx, "");
        if (out->output == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        out->count = 0;
        return 0;
    }

    // Build output buffer for matches
    size_t output_buffer_size = 4096;
    char *output = talloc_array(ctx, char, (unsigned int)output_buffer_size);
    if (output == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    output[0] = '\0';
    size_t output_size = 0;
    int32_t match_count = 0;

    // Search each file
    if (glob_ret == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            const char *filename = glob_result.gl_pathv[i];

            // Skip non-regular files
            struct stat st;
            if (posix_stat_(filename, &st) != 0 || !S_ISREG(st.st_mode)) {
                continue;
            }

            // Open file
            FILE *fp = fopen_(filename, "r");
            if (fp == NULL) {
                continue; // Silently skip files that can't be opened
            }

            // Search line by line
            char *line = NULL;
            size_t line_size = 0;
            int32_t line_number = 0;

            while (getline(&line, &line_size, fp) != -1) {
                line_number++;

                // Strip trailing newline for cleaner output
                size_t line_len = strlen(line);
                if (line_len > 0 && line[line_len - 1] == '\n') {
                    line[line_len - 1] = '\0';
                    line_len--;
                }

                // Test regex match
                if (regexec(&regex, line, 0, NULL, 0) == 0) {
                    // Build match line: filename:line_number: line_content
                    char match_line[4096];
                    int32_t match_line_len = snprintf(match_line, sizeof(match_line),
                                                      "%s:%d: %s", filename, line_number, line);

                    // Ensure buffer has space
                    while (output_size + (size_t)match_line_len + 2 > output_buffer_size) {
                        output_buffer_size *= 2;
                        output = talloc_realloc(ctx, output, char, (unsigned int)output_buffer_size);
                        if (output == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                    }

                    // Append match (with newline separator if not first match)
                    if (match_count > 0) {
                        output[output_size++] = '\n';
                    }
                    memcpy(output + output_size, match_line, (size_t)match_line_len);
                    output_size += (size_t)match_line_len;
                    output[output_size] = '\0';
                    match_count++;
                }
            }

            free(line);
            fclose_(fp);
        }
    }

    globfree_(&glob_result);
    regfree(&regex);

    out->output = output;
    out->count = match_count;
    return 0;
}
