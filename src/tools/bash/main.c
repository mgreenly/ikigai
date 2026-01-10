#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <talloc.h>

#include "json_allocator.h"

#include "vendor/yyjson/yyjson.h"

int32_t main(int32_t argc, char **argv)
{
    void *ctx = talloc_new(NULL);

    if (argc == 2 && strcmp(argv[1], "--schema") == 0) {
        printf("{\n");
        printf("  \"name\": \"bash_tool\",\n");
        printf("  \"description\": \"Execute a shell command and return output\",\n");
        printf("  \"parameters\": {\n");
        printf("    \"type\": \"object\",\n");
        printf("    \"properties\": {\n");
        printf("      \"command\": {\n");
        printf("        \"type\": \"string\",\n");
        printf("        \"description\": \"Shell command to execute\"\n");
        printf("      }\n");
        printf("    },\n");
        printf("    \"required\": [\"command\"]\n");
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

        // If buffer is full, grow it
        if (total_read >= buffer_size) {
            buffer_size *= 2;
            input = talloc_realloc(ctx, input, char, (unsigned int)buffer_size);
            if (input == NULL) {
                talloc_free(ctx);
                return 1;
            }
        }
    }

    // Null-terminate the input
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

    // Check for empty input
    if (total_read == 0) {
        fprintf(stderr, "bash_tool: empty input\n");
        talloc_free(ctx);
        return 1;
    }

    // Parse JSON input
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(input, total_read, 0, &allocator, NULL);
    if (doc == NULL) {
        fprintf(stderr, "bash_tool: invalid JSON\n");
        talloc_free(ctx);
        return 1;
    }

    // Check for required "command" field
    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *command = yyjson_obj_get(root, "command");
    if (command == NULL) {
        fprintf(stderr, "bash_tool: missing command field\n");
        talloc_free(ctx);
        return 1;
    }

    // Check that "command" is a string
    if (!yyjson_is_str(command)) {
        fprintf(stderr, "bash_tool: command must be a string\n");
        talloc_free(ctx);
        return 1;
    }

    // Execute command via popen
    const char *cmd_str = yyjson_get_str(command);
    FILE *pipe = popen(cmd_str, "r");
    if (pipe == NULL) {
        // popen() failed - treat as exit code 127
        // Build JSON result with proper escaping using yyjson
        yyjson_alc fail_allocator = ik_make_talloc_allocator(ctx);
        yyjson_mut_doc *fail_doc = yyjson_mut_doc_new(&fail_allocator);
        if (fail_doc == NULL) {
            talloc_free(ctx);
            return 1;
        }

        yyjson_mut_val *fail_obj = yyjson_mut_obj(fail_doc);
        if (fail_obj == NULL) {
            talloc_free(ctx);
            return 1;
        }

        yyjson_mut_val *fail_output = yyjson_mut_str(fail_doc, "");
        if (fail_output == NULL) {
            talloc_free(ctx);
            return 1;
        }

        yyjson_mut_val *fail_exit = yyjson_mut_int(fail_doc, 127);
        if (fail_exit == NULL) {
            talloc_free(ctx);
            return 1;
        }

        yyjson_mut_obj_add_val(fail_doc, fail_obj, "output", fail_output);
        yyjson_mut_obj_add_val(fail_doc, fail_obj, "exit_code", fail_exit);
        yyjson_mut_doc_set_root(fail_doc, fail_obj);

        char *fail_json = yyjson_mut_write(fail_doc, 0, NULL);
        if (fail_json == NULL) {
            talloc_free(ctx);
            return 1;
        }

        printf("%s\n", fail_json);
        free(fail_json);
        talloc_free(ctx);
        return 0;
    }

    // Read output from pipe into buffer starting at 4KB
    size_t output_buffer_size = 4096;
    size_t output_total_read = 0;
    char *output = talloc_array(ctx, char, (unsigned int)output_buffer_size);
    if (output == NULL) {
        pclose(pipe);
        talloc_free(ctx);
        return 1;
    }

    size_t output_bytes_read;
    while ((output_bytes_read = fread(output + output_total_read, 1, output_buffer_size - output_total_read, pipe)) > 0) {
        output_total_read += output_bytes_read;

        // If buffer is full, grow it
        if (output_total_read >= output_buffer_size) {
            output_buffer_size *= 2;
            output = talloc_realloc(ctx, output, char, (unsigned int)output_buffer_size);
            if (output == NULL) {
                pclose(pipe);
                talloc_free(ctx);
                return 1;
            }
        }
    }

    // Null-terminate the output
    if (output_total_read < output_buffer_size) {
        output[output_total_read] = '\0';
    } else {
        output = talloc_realloc(ctx, output, char, (unsigned int)(output_total_read + 1));
        if (output == NULL) {
            pclose(pipe);
            talloc_free(ctx);
            return 1;
        }
        output[output_total_read] = '\0';
    }

    // Get exit code from pclose
    int32_t status = pclose(pipe);
    int32_t exit_code = WEXITSTATUS(status);

    // Strip single trailing newline from output (if present)
    if (output_total_read > 0 && output[output_total_read - 1] == '\n') {
        output[output_total_read - 1] = '\0';
    }

    // Build JSON result with proper escaping using yyjson
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

    yyjson_mut_val *exit_code_val = yyjson_mut_int(output_doc, exit_code);
    if (exit_code_val == NULL) {
        talloc_free(ctx);
        return 1;
    }

    yyjson_mut_obj_add_val(output_doc, result_obj, "output", output_val);
    yyjson_mut_obj_add_val(output_doc, result_obj, "exit_code", exit_code_val);
    yyjson_mut_doc_set_root(output_doc, result_obj);

    // Write JSON to stdout
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
