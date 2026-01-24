#include "test_utils_helper.h"
#include "../src/agent.h"
#include "../src/db/migration.h"
#include "../src/panic.h"
#include "../src/vendor/yyjson/yyjson.h"
#include <check.h>
#include <libpq-fe.h>
#include <talloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// ========== Allocator Wrapper Overrides ==========
// Strong symbols that override the weak symbols in src/wrapper.c

// Thread-local mocking controls - tests can set these to inject failures
// Using __thread to ensure each test file running in parallel has its own state
__thread int ik_test_talloc_realloc_fail_on_call = -1;  // -1 = don't fail, >= 0 = fail on this call
__thread int ik_test_talloc_realloc_call_count = 0;

void *talloc_zero_(TALLOC_CTX *ctx, size_t size)
{
    return talloc_zero_size(ctx, size);
}

char *talloc_strdup_(TALLOC_CTX *ctx, const char *str)
{
    return talloc_strdup(ctx, str);
}

void *talloc_array_(TALLOC_CTX *ctx, size_t el_size, size_t count)
{
    return talloc_zero_size(ctx, el_size * count);
}

void *talloc_realloc_(TALLOC_CTX *ctx, void *ptr, size_t size)
{
    int current_call = ik_test_talloc_realloc_call_count++;
    if (ik_test_talloc_realloc_fail_on_call >= 0 && current_call == ik_test_talloc_realloc_fail_on_call) {
        return NULL;  // Simulate OOM
    }
    return talloc_realloc_size(ctx, ptr, size);
}

char *talloc_asprintf_(TALLOC_CTX *ctx, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    char *result = talloc_vasprintf(ctx, fmt, ap);
    va_end(ap);
    return result;
}

// ========== Test Config Helper ==========

ik_config_t *ik_test_create_config(TALLOC_CTX *ctx)
{
    ik_config_t *cfg = talloc_zero(ctx, ik_config_t);
    if (cfg == NULL) return NULL;

    // Set minimal required fields for testing
    cfg->openai_model = talloc_strdup(cfg, "gpt-4-turbo");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 4096;
    cfg->openai_system_message = NULL;
    cfg->listen_address = talloc_strdup(cfg, "127.0.0.1");
    cfg->listen_port = 8080;
    cfg->history_size = 10000;  // Default history size

    return cfg;
}

// ========== File I/O Helpers ==========

char *load_file_to_string(TALLOC_CTX *ctx, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }

    // Get file size
    struct stat st;
    if (fstat(fileno(f), &st) != 0) {
        fclose(f);
        return NULL;
    }

    size_t size = (size_t)st.st_size;

    // Allocate buffer (talloc panics on OOM)
    char *buffer = talloc_zero_size(ctx, size + 1);

    // Read file
    size_t bytes_read = fread(buffer, 1, size, f);
    fclose(f);

    if (bytes_read != size) {
        talloc_free(buffer);
        return NULL;
    }

    buffer[size] = '\0';
    return buffer;
}

// ========== Database Test Utilities ==========

const char *ik_test_db_name(TALLOC_CTX *ctx, const char *file_path)
{
    if (file_path == NULL) {
        return NULL;
    }

    // Find the last '/' to get basename
    const char *basename = strrchr(file_path, '/');
    if (basename != NULL) {
        basename++;  // Skip the '/'
    } else {
        basename = file_path;
    }

    // Find the last '.' to remove extension
    const char *dot = strrchr(basename, '.');
    size_t name_len;
    if (dot != NULL && dot > basename) {
        name_len = (size_t)(dot - basename);
    } else {
        name_len = strlen(basename);
    }

    // Build result: "ikigai_test_" + basename (without extension)
    if (ctx != NULL) {
        return talloc_asprintf(ctx, "ikigai_test_%.*s", (int)name_len, basename);
    } else {
        // Use thread-local buffer for NULL ctx (for suite-level setup before talloc)
        // Using __thread to ensure each test file running in parallel has its own buffer
        static __thread char static_buf[256];
        snprintf(static_buf, sizeof(static_buf), "ikigai_test_%.*s", (int)name_len, basename);
        return static_buf;
    }
}

void ik_test_set_log_dir(const char *file_path)
{
    if (file_path == NULL) {
        return;
    }

    // Find basename (same logic as ik_test_db_name)
    const char *basename = strrchr(file_path, '/');
    if (basename != NULL) {
        basename++;
    } else {
        basename = file_path;
    }

    // Remove extension
    const char *dot = strrchr(basename, '.');
    size_t name_len;
    if (dot != NULL && dot > basename) {
        name_len = (size_t)(dot - basename);
    } else {
        name_len = strlen(basename);
    }

    // Build path: /tmp/ikigai_logs_{basename}
    char log_dir[256];
    snprintf(log_dir, sizeof(log_dir), "/tmp/ikigai_logs_%.*s", (int)name_len, basename);

    // Set environment variable
    setenv("IKIGAI_LOG_DIR", log_dir, 1);
}

// ========== Terminal Utilities ==========

char *ik_test_sanitize_ansi(TALLOC_CTX *ctx, const char *input, size_t len)
{
    if (ctx == NULL || input == NULL) {
        return NULL;
    }

    // Count ESC characters to calculate output size
    // Each ESC (1 byte) becomes "<ESC>" (5 bytes), so we add 4 bytes per ESC
    size_t esc_count = 0;
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '\x1b') {
            esc_count++;
        }
    }

    // Allocate output buffer: original length + 4 bytes per ESC + null terminator
    size_t out_size = len + (esc_count * 4) + 1;
    char *output = talloc_size(ctx, out_size);
    if (output == NULL) {
        return NULL;
    }

    // Copy input, replacing ESC with <ESC>
    size_t out_pos = 0;
    for (size_t i = 0; i < len; i++) {
        if (input[i] == '\x1b') {
            output[out_pos++] = '<';
            output[out_pos++] = 'E';
            output[out_pos++] = 'S';
            output[out_pos++] = 'C';
            output[out_pos++] = '>';
        } else {
            output[out_pos++] = input[i];
        }
    }
    output[out_pos] = '\0';

    return output;
}

void ik_test_reset_terminal(void)
{
    // Reset sequence:
    // - \x1b[?25h  Show cursor (may be hidden)
    // - \x1b[0m    Reset text attributes (future-proof)
    //
    // Do NOT exit alternate screen - tests don't enter it.
    // Write to stdout which is where test output goes.
    const char reset_seq[] = "\x1b[?25h\x1b[0m";
    (void)write(STDOUT_FILENO, reset_seq, sizeof(reset_seq) - 1);
}

// ========== Agent Test Utilities ==========

res_t ik_test_create_agent(TALLOC_CTX *ctx, ik_agent_ctx_t **out)
{
    if (ctx == NULL || out == NULL) {
        TALLOC_CTX *tmp_ctx = talloc_new(NULL);
        if (tmp_ctx == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return ERR(tmp_ctx, INVALID_ARG, "NULL argument to ik_test_create_agent");
    }

    // Create minimal shared context
    ik_shared_ctx_t *shared = talloc_zero(ctx, ik_shared_ctx_t);
    if (shared == NULL) {
        return ERR(ctx, OUT_OF_MEMORY, "Failed to allocate shared context");
    }

    // Create agent (ik_agent_create will initialize display state)
    return ik_agent_create(ctx, shared, NULL, out);
}

// ========== Tool JSON Test Helpers ==========

/**
 * Parse JSON tool response and verify success=true
 */
yyjson_val *ik_test_tool_parse_success(const char *json, yyjson_doc **out_doc)
{
    ck_assert_msg(json != NULL, "json parameter cannot be NULL");
    ck_assert_msg(out_doc != NULL, "out_doc parameter cannot be NULL");

    // Parse JSON
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_msg(doc != NULL, "Failed to parse JSON: %s", json);

    // Get root object
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_msg(yyjson_is_obj(root), "JSON root is not an object");

    // Get success field
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_msg(success != NULL, "Missing 'success' field in JSON response");
    ck_assert_msg(yyjson_get_bool(success) == true,
                  "Expected success=true but got success=false");

    // Get data field
    yyjson_val *data = yyjson_obj_get(root, "data");
    ck_assert_msg(data != NULL, "Missing 'data' field in success response");
    ck_assert_msg(yyjson_is_obj(data), "'data' field is not an object");

    *out_doc = doc;
    return data;
}

/**
 * Parse JSON tool response and verify success=false
 */
const char *ik_test_tool_parse_error(const char *json, yyjson_doc **out_doc)
{
    ck_assert_msg(json != NULL, "json parameter cannot be NULL");
    ck_assert_msg(out_doc != NULL, "out_doc parameter cannot be NULL");

    // Parse JSON
    yyjson_doc *doc = yyjson_read(json, strlen(json), 0);
    ck_assert_msg(doc != NULL, "Failed to parse JSON: %s", json);

    // Get root object
    yyjson_val *root = yyjson_doc_get_root(doc);
    ck_assert_msg(yyjson_is_obj(root), "JSON root is not an object");

    // Get success field
    yyjson_val *success = yyjson_obj_get(root, "success");
    ck_assert_msg(success != NULL, "Missing 'success' field in JSON response");
    ck_assert_msg(yyjson_get_bool(success) == false,
                  "Expected success=false but got success=true");

    // Get error field
    yyjson_val *error = yyjson_obj_get(root, "error");
    ck_assert_msg(error != NULL, "Missing 'error' field in error response");
    ck_assert_msg(yyjson_is_str(error), "'error' field is not a string");

    *out_doc = doc;
    return yyjson_get_str(error);
}

/**
 * Extract output field from tool data object
 */
const char *ik_test_tool_get_output(yyjson_val *data)
{
    ck_assert_msg(data != NULL, "data parameter cannot be NULL");

    // Get output field
    yyjson_val *output = yyjson_obj_get(data, "output");
    ck_assert_msg(output != NULL, "Missing 'output' field in data object");
    ck_assert_msg(yyjson_is_str(output), "'output' field is not a string");

    return yyjson_get_str(output);
}

/**
 * Extract exit_code field from tool data object
 */
int64_t ik_test_tool_get_exit_code(yyjson_val *data)
{
    ck_assert_msg(data != NULL, "data parameter cannot be NULL");

    // Get exit_code field
    yyjson_val *exit_code = yyjson_obj_get(data, "exit_code");
    ck_assert_msg(exit_code != NULL, "Missing 'exit_code' field in data object");
    ck_assert_msg(yyjson_is_int(exit_code), "'exit_code' field is not an integer");

    return yyjson_get_int(exit_code);
}

// ========== Paths Test Helpers ==========

// Thread-local buffer for test path prefix
static __thread char test_path_prefix[256] = {0};

const char *test_paths_setup_env(void)
{
    // Generate PID-based unique prefix
    snprintf(test_path_prefix, sizeof(test_path_prefix),
             "/tmp/ikigai_test_%d", getpid());

    // Create base directory
    mkdir(test_path_prefix, 0755);

    // Create and set each subdirectory
    char buf[512];

    // bin directory
    snprintf(buf, sizeof(buf), "%s/bin", test_path_prefix);
    mkdir(buf, 0755);
    setenv("IKIGAI_BIN_DIR", buf, 1);

    // config directory
    snprintf(buf, sizeof(buf), "%s/config", test_path_prefix);
    mkdir(buf, 0755);
    setenv("IKIGAI_CONFIG_DIR", buf, 1);

    // share directory (data)
    snprintf(buf, sizeof(buf), "%s/share", test_path_prefix);
    mkdir(buf, 0755);
    setenv("IKIGAI_DATA_DIR", buf, 1);

    // Create migrations subdirectory under share
    char migrations_dir[768];
    snprintf(migrations_dir, sizeof(migrations_dir), "%s/migrations", buf);
    mkdir(migrations_dir, 0755);

    // libexec directory
    snprintf(buf, sizeof(buf), "%s/libexec", test_path_prefix);
    mkdir(buf, 0755);
    setenv("IKIGAI_LIBEXEC_DIR", buf, 1);

    // cache directory
    snprintf(buf, sizeof(buf), "%s/cache", test_path_prefix);
    mkdir(buf, 0755);
    setenv("IKIGAI_CACHE_DIR", buf, 1);

    // state directory
    snprintf(buf, sizeof(buf), "%s/state", test_path_prefix);
    mkdir(buf, 0755);
    setenv("IKIGAI_STATE_DIR", buf, 1);

    return test_path_prefix;
}

void test_paths_cleanup_env(void)
{
    // Unset environment variables
    unsetenv("IKIGAI_BIN_DIR");
    unsetenv("IKIGAI_CONFIG_DIR");
    unsetenv("IKIGAI_DATA_DIR");
    unsetenv("IKIGAI_LIBEXEC_DIR");
    unsetenv("IKIGAI_CACHE_DIR");
    unsetenv("IKIGAI_STATE_DIR");

    // Remove test directory tree if it exists
    if (test_path_prefix[0] != '\0') {
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "rm -rf '%s' 2>/dev/null || true",
                 test_path_prefix);
        system(cmd);
        test_path_prefix[0] = '\0';
    }
}
