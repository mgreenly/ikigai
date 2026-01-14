#include <glob.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
        printf("  \"name\": \"glob\",\n");
        printf("  \"description\": \"Find files matching a glob pattern\",\n");
        printf("  \"parameters\": {\n");
        printf("    \"type\": \"object\",\n");
        printf("    \"properties\": {\n");
        printf("      \"pattern\": {\n");
        printf("        \"type\": \"string\",\n");
        printf("        \"description\": \"Glob pattern (e.g., '*.txt', 'src/**/*.c')\"\n");
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
        fprintf(stderr, "glob: empty input\n");
        talloc_free(ctx);
        return 1;
    }

    // Parse JSON input
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(input, total_read, 0, &allocator, NULL);
    if (doc == NULL) {
        fprintf(stderr, "glob: invalid JSON\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *pattern_val = yyjson_obj_get(root, "pattern");
    if (pattern_val == NULL || !yyjson_is_str(pattern_val)) {
        fprintf(stderr, "glob: missing or invalid pattern field\n");
        talloc_free(ctx);
        return 1;
    }

    const char *pattern = yyjson_get_str(pattern_val);

    // Get optional path
    yyjson_val *path_val = yyjson_obj_get(root, "path");
    const char *path = NULL;
    if (path_val != NULL && yyjson_is_str(path_val)) {
        path = yyjson_get_str(path_val);
    }

    // Build full pattern: path/pattern or just pattern
    char *full_pattern = NULL;
    if (path != NULL && path[0] != '\0') {
        size_t path_len = strlen(path);
        size_t pattern_len = strlen(pattern);
        full_pattern = talloc_array(ctx, char, (unsigned int)(path_len + pattern_len + 2));
        if (full_pattern == NULL) {
            talloc_free(ctx);
            return 1;
        }
        snprintf(full_pattern, path_len + pattern_len + 2, "%s/%s", path, pattern);
    } else {
        full_pattern = talloc_strdup(ctx, pattern);
        if (full_pattern == NULL) {
            talloc_free(ctx);
            return 1;
        }
    }

    // Execute glob
    glob_t glob_result;
    int32_t glob_ret = glob(full_pattern, 0, NULL, &glob_result);

    if (glob_ret == GLOB_NOSPACE) {
        globfree(&glob_result);
        output_error(ctx, "Out of memory during glob", "OUT_OF_MEMORY");
        talloc_free(ctx);
        return 0;
    } else if (glob_ret == GLOB_ABORTED) {
        globfree(&glob_result);
        output_error(ctx, "Read error during glob", "READ_ERROR");
        talloc_free(ctx);
        return 0;
    } else if (glob_ret != 0 && glob_ret != GLOB_NOMATCH) {
        globfree(&glob_result);
        output_error(ctx, "Invalid glob pattern", "INVALID_PATTERN");
        talloc_free(ctx);
        return 0;
    }

    // Build output string (one path per line, no trailing newline after last)
    size_t output_size = 0;
    if (glob_ret == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            output_size += strlen(glob_result.gl_pathv[i]);
            if (i < glob_result.gl_pathc - 1) {
                output_size += 1; // newline
            }
        }
    }

    char *output = talloc_array(ctx, char, (unsigned int)(output_size + 1));
    if (output == NULL) {
        globfree(&glob_result);
        talloc_free(ctx);
        return 1;
    }

    char *pos = output;
    if (glob_ret == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            size_t len = strlen(glob_result.gl_pathv[i]);
            memcpy(pos, glob_result.gl_pathv[i], len);
            pos += len;
            if (i < glob_result.gl_pathc - 1) {
                *pos++ = '\n';
            }
        }
    }
    *pos = '\0';

    int32_t count = (glob_ret == 0) ? (int32_t)glob_result.gl_pathc : 0;
    globfree(&glob_result);

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

    yyjson_mut_val *count_val = yyjson_mut_int(output_doc, count);
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
