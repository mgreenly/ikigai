#include "grep.h"

#include "json_allocator.h"
#include "panic.h"

#include <inttypes.h>
#include <regex.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "vendor/yyjson/yyjson.h"

/* LCOV_EXCL_START */
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
    if (input == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    size_t bytes_read;
    while ((bytes_read = fread(input + total_read, 1, buffer_size - total_read, stdin)) > 0) {
        total_read += bytes_read;

        if (total_read >= buffer_size) {
            buffer_size *= 2;
            input = talloc_realloc(ctx, input, char, (unsigned int)buffer_size);
            if (input == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        }
    }

    if (total_read < buffer_size) {
        input[total_read] = '\0';
    } else {
        input = talloc_realloc(ctx, input, char, (unsigned int)(total_read + 1));
        if (input == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
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
    const char *path = NULL;
    if (path_val != NULL && yyjson_is_str(path_val)) {
        path = yyjson_get_str(path_val);
    }

    // Validate pattern
    regex_t regex;
    int32_t regex_ret = regcomp(&regex, pattern, REG_EXTENDED);
    if (regex_ret != 0) {
        char error_buf[256];
        regerror(regex_ret, &regex, error_buf, sizeof(error_buf));

        yyjson_alc error_allocator = ik_make_talloc_allocator(ctx);
        yyjson_mut_doc *error_doc = yyjson_mut_doc_new(&error_allocator);
        if (error_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *error_obj = yyjson_mut_obj(error_doc);
        if (error_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Invalid pattern: %s", error_buf);
        yyjson_mut_val *error_val = yyjson_mut_str(error_doc, error_msg);
        if (error_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *error_code_val = yyjson_mut_str(error_doc, "INVALID_PATTERN");
        if (error_code_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_obj_add_val(error_doc, error_obj, "error", error_val);
        yyjson_mut_obj_add_val(error_doc, error_obj, "error_code", error_code_val);
        yyjson_mut_doc_set_root(error_doc, error_obj);

        char *json_str = yyjson_mut_write_opts(error_doc, 0, &error_allocator, NULL, NULL);
        if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        printf("%s\n", json_str);
        talloc_free(ctx);
        return 0;
    }
    regfree(&regex);

    // Call grep_search
    grep_params_t params = {
        .pattern = pattern,
        .glob = glob_pattern,
        .path = path
    };
    grep_result_t result;
    grep_search(ctx, &params, &result);

    // Build JSON response
    yyjson_alc output_allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *output_doc = yyjson_mut_doc_new(&output_allocator);
    if (output_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *result_obj = yyjson_mut_obj(output_doc);
    if (result_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *output_val = yyjson_mut_str(output_doc, result.output);
    if (output_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *count_val = yyjson_mut_int(output_doc, result.count);
    if (count_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_obj_add_val(output_doc, result_obj, "output", output_val);
    yyjson_mut_obj_add_val(output_doc, result_obj, "count", count_val);
    yyjson_mut_doc_set_root(output_doc, result_obj);

    char *json_str = yyjson_mut_write_opts(output_doc, 0, &output_allocator, NULL, NULL);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    printf("%s\n", json_str);

    talloc_free(ctx);
    return 0;
}

/* LCOV_EXCL_STOP */
