#include <errno.h>
#include <inttypes.h>
#include <libgen.h>
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
        printf("  \"name\": \"file_edit\",\n");
        printf("  \"description\": \"Edit a file by replacing exact text matches. You must read the file before editing.\",\n");
        printf("  \"parameters\": {\n");
        printf("    \"type\": \"object\",\n");
        printf("    \"properties\": {\n");
        printf("      \"file_path\": {\n");
        printf("        \"type\": \"string\",\n");
        printf("        \"description\": \"Absolute or relative path to file\"\n");
        printf("      },\n");
        printf("      \"old_string\": {\n");
        printf("        \"type\": \"string\",\n");
        printf("        \"description\": \"Exact text to find and replace\"\n");
        printf("      },\n");
        printf("      \"new_string\": {\n");
        printf("        \"type\": \"string\",\n");
        printf("        \"description\": \"Text to replace old_string with\"\n");
        printf("      },\n");
        printf("      \"replace_all\": {\n");
        printf("        \"type\": \"boolean\",\n");
        printf("        \"description\": \"Replace all occurrences (default: false, fails if not unique)\"\n");
        printf("      }\n");
        printf("    },\n");
        printf("    \"required\": [\"file_path\", \"old_string\", \"new_string\"]\n");
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
        fprintf(stderr, "file_edit: empty input\n");
        talloc_free(ctx);
        return 1;
    }

    // Parse JSON input
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(input, total_read, 0, &allocator, NULL);
    if (doc == NULL) {
        fprintf(stderr, "file_edit: invalid JSON\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *file_path = yyjson_obj_get(root, "file_path");
    if (file_path == NULL || !yyjson_is_str(file_path)) {
        fprintf(stderr, "file_edit: missing or invalid file_path field\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *old_string_val = yyjson_obj_get(root, "old_string");
    if (old_string_val == NULL || !yyjson_is_str(old_string_val)) {
        fprintf(stderr, "file_edit: missing or invalid old_string field\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *new_string_val = yyjson_obj_get(root, "new_string");
    if (new_string_val == NULL || !yyjson_is_str(new_string_val)) {
        fprintf(stderr, "file_edit: missing or invalid new_string field\n");
        talloc_free(ctx);
        return 1;
    }

    const char *path = yyjson_get_str(file_path);
    const char *old_string = yyjson_get_str(old_string_val);
    const char *new_string = yyjson_get_str(new_string_val);
    size_t old_len = yyjson_get_len(old_string_val);
    size_t new_len = yyjson_get_len(new_string_val);

    // Get optional replace_all flag
    yyjson_val *replace_all_val = yyjson_obj_get(root, "replace_all");
    bool replace_all = false;
    if (replace_all_val != NULL && yyjson_is_bool(replace_all_val)) {
        replace_all = yyjson_get_bool(replace_all_val);
    }

    // Validate old_string is not empty
    if (old_len == 0) {
        output_error(ctx, "old_string cannot be empty", "INVALID_ARG");
        talloc_free(ctx);
        return 0;
    }

    // Validate old_string != new_string
    if (strcmp(old_string, new_string) == 0) {
        output_error(ctx, "old_string and new_string are identical", "INVALID_ARG");
        talloc_free(ctx);
        return 0;
    }

    // Read file contents
    FILE *fp = fopen(path, "r");
    if (fp == NULL) {
        if (errno == ENOENT) {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "File not found: %s", path);
            output_error(ctx, error_msg, "FILE_NOT_FOUND");
        } else if (errno == EACCES) {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Permission denied: %s", path);
            output_error(ctx, error_msg, "PERMISSION_DENIED");
        } else {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Cannot open file: %s", path);
            output_error(ctx, error_msg, "OPEN_FAILED");
        }
        talloc_free(ctx);
        return 0;
    }

    struct stat st;
    if (fstat(fileno(fp), &st) != 0) {
        fclose(fp);
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Cannot get file size: %s", path);
        output_error(ctx, error_msg, "SIZE_FAILED");
        talloc_free(ctx);
        return 0;
    }

    size_t file_size = (size_t)st.st_size;
    char *content = talloc_array(ctx, char, (unsigned int)(file_size + 1));
    if (content == NULL) {
        fclose(fp);
        talloc_free(ctx);
        return 1;
    }

    size_t read_bytes = fread(content, 1, file_size, fp);
    if (read_bytes != file_size) {
        fclose(fp);
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to read file: %s", path);
        output_error(ctx, error_msg, "READ_FAILED");
        talloc_free(ctx);
        return 0;
    }
    content[file_size] = '\0';
    fclose(fp);

    // Count occurrences of old_string
    int32_t count = 0;
    const char *pos = content;
    while ((pos = strstr(pos, old_string)) != NULL) {
        count++;
        pos += old_len;
    }

    // Validate match count
    if (!replace_all && count != 1) {
        if (count == 0) {
            output_error(ctx, "String not found in file", "NOT_FOUND");
        } else {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "String found %d times, use replace_all to replace all", count);
            output_error(ctx, error_msg, "NOT_UNIQUE");
        }
        talloc_free(ctx);
        return 0;
    }

    // Build new content with replacements
    size_t new_content_size = file_size + (size_t)(count) * (new_len - old_len);
    char *new_content = talloc_array(ctx, char, (unsigned int)(new_content_size + 1));
    if (new_content == NULL) {
        talloc_free(ctx);
        return 1;
    }

    const char *src = content;
    char *dst = new_content;
    int32_t replacements = 0;

    while (*src) {
        const char *match = strstr(src, old_string);
        if (match == NULL) {
            // Copy rest of string
            strcpy(dst, src);
            break;
        }

        // Copy content before match
        size_t prefix_len = (size_t)(match - src);
        memcpy(dst, src, prefix_len);
        dst += prefix_len;

        // Copy new_string
        memcpy(dst, new_string, new_len);
        dst += new_len;

        // Advance source past old_string
        src = match + old_len;
        replacements++;
    }

    // Write new content back to file
    fp = fopen(path, "w");
    if (fp == NULL) {
        if (errno == EACCES) {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Permission denied: %s", path);
            output_error(ctx, error_msg, "PERMISSION_DENIED");
        } else {
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "Cannot open file: %s", path);
            output_error(ctx, error_msg, "OPEN_FAILED");
        }
        talloc_free(ctx);
        return 0;
    }

    size_t written = fwrite(new_content, 1, strlen(new_content), fp);
    if (written != strlen(new_content)) {
        fclose(fp);
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to write file: %s", path);
        output_error(ctx, error_msg, "WRITE_FAILED");
        talloc_free(ctx);
        return 0;
    }

    fclose(fp);

    // Extract basename for success message
    char *path_copy = talloc_strdup(ctx, path);
    if (path_copy == NULL) {
        talloc_free(ctx);
        return 1;
    }
    char *filename = basename(path_copy);

    // Build success message
    char success_msg[512];
    snprintf(success_msg, sizeof(success_msg), "Replaced %d occurrence%s in %s",
             replacements, replacements == 1 ? "" : "s", filename);

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

    yyjson_mut_val *output_val = yyjson_mut_str(output_doc, success_msg);
    if (output_val == NULL) {
        talloc_free(ctx);
        return 1;
    }

    yyjson_mut_val *replacements_val = yyjson_mut_int(output_doc, replacements);
    if (replacements_val == NULL) {
        talloc_free(ctx);
        return 1;
    }

    yyjson_mut_obj_add_val(output_doc, result_obj, "output", output_val);
    yyjson_mut_obj_add_val(output_doc, result_obj, "replacements", replacements_val);
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
