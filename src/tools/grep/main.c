#include <glob.h>
#include <inttypes.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <talloc.h>

#include "json_allocator.h"

#include "vendor/yyjson/yyjson.h"

static void output_error(void *ctx, const char *error, const char *error_code)
{
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    if (doc == NULL) {
        exit(1);
    }

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (obj == NULL) {
        exit(1);
    }

    yyjson_mut_val *error_val = yyjson_mut_str(doc, error);
    if (error_val == NULL) {
        exit(1);
    }

    yyjson_mut_val *error_code_val = yyjson_mut_str(doc, error_code);
    if (error_code_val == NULL) {
        exit(1);
    }

    yyjson_mut_obj_add_val(doc, obj, "error", error_val);
    yyjson_mut_obj_add_val(doc, obj, "error_code", error_code_val);
    yyjson_mut_doc_set_root(doc, obj);

    char *json_str = yyjson_mut_write(doc, 0, NULL);
    if (json_str == NULL) {
        exit(1);
    }

    printf("%s\n", json_str);
    free(json_str);
}

int32_t main(int32_t argc, char **argv)
{
    void *ctx = talloc_new(NULL);

    if (argc == 2 && strcmp(argv[1], "--schema") == 0) {
        printf("{\n");
        printf("  \"name\": \"grep\",\n");
        printf("  \"description\": \"Search for pattern in files using regular expressions\",\n");
        printf("  \"parameters\": {\n");
        printf("    \"type\": \"object\",\n");
        printf("    \"properties\": {\n");
        printf("      \"pattern\": {\n");
        printf("        \"type\": \"string\",\n");
        printf("        \"description\": \"Regular expression pattern (POSIX extended)\"\n");
        printf("      },\n");
        printf("      \"glob\": {\n");
        printf("        \"type\": \"string\",\n");
        printf("        \"description\": \"Glob pattern to filter files (e.g., '*.c')\"\n");
        printf("      },\n");
        printf("      \"path\": {\n");
        printf("        \"type\": \"string\",\n");
        printf("        \"description\": \"Directory to search in (default: current directory)\"\n");
        printf("      }\n");
        printf("    },\n");
        printf("    \"required\": [\"pattern\"]\n");
        printf("  }\n");
        printf("}\n");
        talloc_free(ctx);
        return 0;
    }

    // Read all of stdin into buffer
    size_t buffer_size = 4096;
    size_t total_read = 0;
    char *input = talloc_array(ctx, char, (unsigned int)buffer_size);
    if (input == NULL) {
        talloc_free(ctx);
        return 1;
    }

    size_t bytes_read;
    while ((bytes_read = fread(input + total_read, 1, buffer_size - total_read, stdin)) > 0) {
        total_read += bytes_read;

        if (total_read >= buffer_size) {
            buffer_size *= 2;
            input = talloc_realloc(ctx, input, char, (unsigned int)buffer_size);
            if (input == NULL) {
                talloc_free(ctx);
                return 1;
            }
        }
    }

    if (total_read < buffer_size) {
        input[total_read] = '\0';
    } else {
        input = talloc_realloc(ctx, input, char, (unsigned int)(total_read + 1));
        if (input == NULL) {
            talloc_free(ctx);
            return 1;
        }
        input[total_read] = '\0';
    }

    if (total_read == 0) {
        fprintf(stderr, "grep: empty input\n");
        talloc_free(ctx);
        return 1;
    }

    // Parse JSON input
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(input, total_read, 0, &allocator, NULL);
    if (doc == NULL) {
        fprintf(stderr, "grep: invalid JSON\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *pattern_val = yyjson_obj_get(root, "pattern");
    if (pattern_val == NULL || !yyjson_is_str(pattern_val)) {
        fprintf(stderr, "grep: missing or invalid pattern field\n");
        talloc_free(ctx);
        return 1;
    }

    const char *pattern = yyjson_get_str(pattern_val);

    // Get optional glob filter and path
    yyjson_val *glob_val = yyjson_obj_get(root, "glob");
    const char *glob_pattern = NULL;
    if (glob_val != NULL && yyjson_is_str(glob_val)) {
        glob_pattern = yyjson_get_str(glob_val);
    }

    yyjson_val *path_val = yyjson_obj_get(root, "path");
    const char *path = ".";
    if (path_val != NULL && yyjson_is_str(path_val)) {
        path = yyjson_get_str(path_val);
    }

    // Compile regex pattern
    regex_t regex;
    int32_t regex_ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (regex_ret != 0) {
        char error_buf[256];
        regerror(regex_ret, &regex, error_buf, sizeof(error_buf));
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Invalid pattern: %s", error_buf);
        output_error(ctx, error_msg, "INVALID_PATTERN");
        talloc_free(ctx);
        return 0;
    }

    // Build glob pattern to find files
    char *file_glob = NULL;
    if (glob_pattern != NULL && glob_pattern[0] != '\0') {
        size_t path_len = strlen(path);
        size_t glob_len = strlen(glob_pattern);
        file_glob = talloc_array(ctx, char, (unsigned int)(path_len + glob_len + 2));
        if (file_glob == NULL) {
            regfree(&regex);
            talloc_free(ctx);
            return 1;
        }
        snprintf(file_glob, path_len + glob_len + 2, "%s/%s", path, glob_pattern);
    } else {
        size_t path_len = strlen(path);
        file_glob = talloc_array(ctx, char, (unsigned int)(path_len + 3));
        if (file_glob == NULL) {
            regfree(&regex);
            talloc_free(ctx);
            return 1;
        }
        snprintf(file_glob, path_len + 3, "%s/*", path);
    }

    // Execute glob to find files
    glob_t glob_result;
    int32_t glob_ret = glob(file_glob, 0, NULL, &glob_result);

    // Silently ignore glob errors - just return empty result
    if (glob_ret != 0 && glob_ret != GLOB_NOMATCH) {
        globfree(&glob_result);
        regfree(&regex);

        // Return empty result
        yyjson_alc output_allocator = ik_make_talloc_allocator(ctx);
        yyjson_mut_doc *output_doc = yyjson_mut_doc_new(&output_allocator);
        if (output_doc == NULL) {
            talloc_free(ctx);
            return 1;
        }

        yyjson_mut_val *result_obj = yyjson_mut_obj(output_doc);
        if (result_obj == NULL) {
            talloc_free(ctx);
            return 1;
        }

        yyjson_mut_val *output_val = yyjson_mut_str(output_doc, "");
        if (output_val == NULL) {
            talloc_free(ctx);
            return 1;
        }

        yyjson_mut_val *count_val = yyjson_mut_int(output_doc, 0);
        if (count_val == NULL) {
            talloc_free(ctx);
            return 1;
        }

        yyjson_mut_obj_add_val(output_doc, result_obj, "output", output_val);
        yyjson_mut_obj_add_val(output_doc, result_obj, "count", count_val);
        yyjson_mut_doc_set_root(output_doc, result_obj);

        char *json_str = yyjson_mut_write(output_doc, 0, NULL);
        if (json_str == NULL) {
            talloc_free(ctx);
            return 1;
        }

        printf("%s\n", json_str);
        free(json_str);
        talloc_free(ctx);
        return 0;
    }

    // Build output buffer for matches
    size_t output_buffer_size = 4096;
    char *output = talloc_array(ctx, char, (unsigned int)output_buffer_size);
    if (output == NULL) {
        globfree(&glob_result);
        regfree(&regex);
        talloc_free(ctx);
        return 1;
    }
    output[0] = '\0';
    size_t output_size = 0;
    int32_t match_count = 0;

    // Search each file
    if (glob_ret == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            const char *filename = glob_result.gl_pathv[i];

            // Skip non-regular files
            struct stat st;
            if (stat(filename, &st) != 0 || !S_ISREG(st.st_mode)) {
                continue;
            }

            // Open file
            FILE *fp = fopen(filename, "r");
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
                        if (output == NULL) {
                            free(line);
                            fclose(fp);
                            globfree(&glob_result);
                            regfree(&regex);
                            talloc_free(ctx);
                            return 1;
                        }
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
            fclose(fp);
        }
    }

    globfree(&glob_result);
    regfree(&regex);

    // Build JSON response
    yyjson_alc output_allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *output_doc = yyjson_mut_doc_new(&output_allocator);
    if (output_doc == NULL) {
        talloc_free(ctx);
        return 1;
    }

    yyjson_mut_val *result_obj = yyjson_mut_obj(output_doc);
    if (result_obj == NULL) {
        talloc_free(ctx);
        return 1;
    }

    yyjson_mut_val *output_val = yyjson_mut_str(output_doc, output);
    if (output_val == NULL) {
        talloc_free(ctx);
        return 1;
    }

    yyjson_mut_val *count_val = yyjson_mut_int(output_doc, match_count);
    if (count_val == NULL) {
        talloc_free(ctx);
        return 1;
    }

    yyjson_mut_obj_add_val(output_doc, result_obj, "output", output_val);
    yyjson_mut_obj_add_val(output_doc, result_obj, "count", count_val);
    yyjson_mut_doc_set_root(output_doc, result_obj);

    char *json_str = yyjson_mut_write(output_doc, 0, NULL);
    if (json_str == NULL) {
        talloc_free(ctx);
        return 1;
    }

    printf("%s\n", json_str);
    free(json_str);

    talloc_free(ctx);
    return 0;
}
