#include "bash.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <talloc.h>

#include "json_allocator.h"
#include "panic.h"
#include "wrapper.h"

#include "vendor/yyjson/yyjson.h"

int32_t bash_execute(void *ctx, const char *command)
{
    // Execute command via popen
    FILE *pipe = popen_(command, "r");
    if (pipe == NULL) {
        // popen() failed - treat as exit code 127
        yyjson_alc fail_allocator = ik_make_talloc_allocator(ctx);
        yyjson_mut_doc *fail_doc = yyjson_mut_doc_new(&fail_allocator);
        if (fail_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *fail_obj = yyjson_mut_obj(fail_doc);
        if (fail_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *fail_output = yyjson_mut_str(fail_doc, "");
        if (fail_output == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *fail_exit = yyjson_mut_int(fail_doc, 127);
        if (fail_exit == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_obj_add_val(fail_doc, fail_obj, "output", fail_output);
        yyjson_mut_obj_add_val(fail_doc, fail_obj, "exit_code", fail_exit);
        yyjson_mut_doc_set_root(fail_doc, fail_obj);

        char *fail_json = yyjson_mut_write_opts(fail_doc, 0, &fail_allocator, NULL, NULL);
        if (fail_json == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        printf("%s\n", fail_json);
        return 0;
    }

    // Read output from pipe into buffer starting at 4KB
    size_t output_buffer_size = 4096;
    size_t output_total_read = 0;
    char *output = talloc_array(ctx, char, (unsigned int)output_buffer_size);
    if (output == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    size_t output_bytes_read;
    while ((output_bytes_read = fread(output + output_total_read, 1, output_buffer_size - output_total_read,
                                      pipe)) > 0) {
        output_total_read += output_bytes_read;

        // If buffer is full, grow it
        if (output_total_read >= output_buffer_size) {
            output_buffer_size *= 2;
            output = talloc_realloc(ctx, output, char, (unsigned int)output_buffer_size);
            if (output == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        }
    }

    // Null-terminate the output
    if (output_total_read < output_buffer_size) {  // LCOV_EXCL_BR_LINE
        output[output_total_read] = '\0';
    } else {  // LCOV_EXCL_START
        output = talloc_realloc(ctx, output, char, (unsigned int)(output_total_read + 1));
        if (output == NULL) PANIC("Out of memory");
        output[output_total_read] = '\0';
    }  // LCOV_EXCL_STOP

    // Get exit code from pclose
    int32_t status = pclose_(pipe);
    int32_t exit_code = WEXITSTATUS(status);

    // Strip single trailing newline from output (if present)
    if (output_total_read > 0 && output[output_total_read - 1] == '\n') {
        output[output_total_read - 1] = '\0';
    }

    // Build JSON result with proper escaping using yyjson
    yyjson_alc output_allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *output_doc = yyjson_mut_doc_new(&output_allocator);
    if (output_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *result_obj = yyjson_mut_obj(output_doc);
    if (result_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *output_val = yyjson_mut_str(output_doc, output);
    if (output_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *exit_code_val = yyjson_mut_int(output_doc, exit_code);
    if (exit_code_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_obj_add_val(output_doc, result_obj, "output", output_val);
    yyjson_mut_obj_add_val(output_doc, result_obj, "exit_code", exit_code_val);
    yyjson_mut_doc_set_root(output_doc, result_obj);

    // Write JSON to stdout
    char *json_str = yyjson_mut_write_opts(output_doc, 0, &output_allocator, NULL, NULL);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    printf("%s\n", json_str);

    return 0;
}
