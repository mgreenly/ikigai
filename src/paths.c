#include "paths.h"
#include "debug_log.h"
#include "panic.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

// Private struct definition (NOT in header)
struct ik_paths_t {
    char *bin_dir;
    char *config_dir;
    char *data_dir;
    char *libexec_dir;
    char *cache_dir;
    char *state_dir;
    char *tools_user_dir;
    char *tools_project_dir;
};

res_t ik_paths_expand_tilde(TALLOC_CTX *ctx, const char *path, char **out)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(out != NULL);   // LCOV_EXCL_BR_LINE

    if (path == NULL) {
        return ERR(ctx, INVALID_ARG, "path is NULL");
    }

    // Check if path starts with ~/ or is exactly ~
    if (path[0] != '~' || (path[1] != '\0' && path[1] != '/')) {
        // No tilde at start, return copy unchanged
        *out = talloc_strdup(ctx, path);
        if (*out == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return OK(NULL);
    }

    // Get HOME
    const char *home = getenv("HOME");
    if (home == NULL) {
        return ERR(ctx, IO, "HOME environment variable not set");
    }

    // Build expanded path
    if (path[1] == '\0') {
        // Just ~
        *out = talloc_strdup(ctx, home);
    } else {
        // ~/something
        *out = talloc_asprintf(ctx, "%s%s", home, path + 1);
    }

    if (*out == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    return OK(NULL);
}

res_t ik_paths_init(TALLOC_CTX *ctx, ik_paths_t **out)
{
    assert(ctx != NULL);   // LCOV_EXCL_BR_LINE
    assert(out != NULL);   // LCOV_EXCL_BR_LINE

    // Read environment variables
    const char *bin_dir = getenv("IKIGAI_BIN_DIR");
    const char *config_dir = getenv("IKIGAI_CONFIG_DIR");
    const char *data_dir = getenv("IKIGAI_DATA_DIR");
    const char *libexec_dir = getenv("IKIGAI_LIBEXEC_DIR");
    const char *cache_dir = getenv("IKIGAI_CACHE_DIR");
    const char *state_dir = getenv("IKIGAI_STATE_DIR");

    DEBUG_LOG("ik_paths_init: IKIGAI_BIN_DIR=%s", bin_dir ? bin_dir : "NULL");
    DEBUG_LOG("ik_paths_init: IKIGAI_CONFIG_DIR=%s", config_dir ? config_dir : "NULL");
    DEBUG_LOG("ik_paths_init: IKIGAI_DATA_DIR=%s", data_dir ? data_dir : "NULL");
    DEBUG_LOG("ik_paths_init: IKIGAI_LIBEXEC_DIR=%s", libexec_dir ? libexec_dir : "NULL");
    DEBUG_LOG("ik_paths_init: IKIGAI_CACHE_DIR=%s", cache_dir ? cache_dir : "NULL");
    DEBUG_LOG("ik_paths_init: IKIGAI_STATE_DIR=%s", state_dir ? state_dir : "NULL");

    // Check if all required environment variables are set and non-empty
    if (bin_dir == NULL || bin_dir[0] == '\0' ||
        config_dir == NULL || config_dir[0] == '\0' ||
        data_dir == NULL || data_dir[0] == '\0' ||
        libexec_dir == NULL || libexec_dir[0] == '\0' ||
        cache_dir == NULL || cache_dir[0] == '\0' ||
        state_dir == NULL || state_dir[0] == '\0') {
        DEBUG_LOG("ik_paths_init: ERROR - Missing required environment variable");
        return ERR(ctx, INVALID_ARG, "Missing required environment variable IKIGAI_*_DIR");
    }

    // Allocate paths structure
    ik_paths_t *paths = talloc_zero(ctx, ik_paths_t);
    if (paths == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Copy paths as children of paths instance
    paths->bin_dir = talloc_strdup(paths, bin_dir);
    if (paths->bin_dir == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    paths->config_dir = talloc_strdup(paths, config_dir);
    if (paths->config_dir == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    paths->data_dir = talloc_strdup(paths, data_dir);
    if (paths->data_dir == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    paths->libexec_dir = talloc_strdup(paths, libexec_dir);
    if (paths->libexec_dir == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    paths->cache_dir = talloc_strdup(paths, cache_dir);
    if (paths->cache_dir == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    paths->state_dir = talloc_strdup(paths, state_dir);
    if (paths->state_dir == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Expand tilde for user tools directory
    char *expanded_user_dir = NULL;
    res_t expand_result = ik_paths_expand_tilde(paths, "~/.ikigai/tools/", &expanded_user_dir);
    if (is_err(&expand_result)) {
        return expand_result;
    }
    paths->tools_user_dir = expanded_user_dir;

    // Project tools directory is always relative
    paths->tools_project_dir = talloc_strdup(paths, ".ikigai/tools/");
    if (paths->tools_project_dir == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    *out = paths;
    return OK(NULL);
}

const char *ik_paths_get_bin_dir(ik_paths_t *paths)
{
    assert(paths != NULL);  // LCOV_EXCL_BR_LINE
    return paths->bin_dir;
}

const char *ik_paths_get_config_dir(ik_paths_t *paths)
{
    assert(paths != NULL);  // LCOV_EXCL_BR_LINE
    return paths->config_dir;
}

const char *ik_paths_get_data_dir(ik_paths_t *paths)
{
    assert(paths != NULL);  // LCOV_EXCL_BR_LINE
    return paths->data_dir;
}

const char *ik_paths_get_libexec_dir(ik_paths_t *paths)
{
    assert(paths != NULL);  // LCOV_EXCL_BR_LINE
    return paths->libexec_dir;
}

const char *ik_paths_get_cache_dir(ik_paths_t *paths)
{
    assert(paths != NULL);  // LCOV_EXCL_BR_LINE
    return paths->cache_dir;
}

const char *ik_paths_get_state_dir(ik_paths_t *paths)
{
    assert(paths != NULL);  // LCOV_EXCL_BR_LINE
    return paths->state_dir;
}

const char *ik_paths_get_tools_system_dir(ik_paths_t *paths)
{
    assert(paths != NULL);  // LCOV_EXCL_BR_LINE
    return paths->libexec_dir;
}

const char *ik_paths_get_tools_user_dir(ik_paths_t *paths)
{
    assert(paths != NULL);  // LCOV_EXCL_BR_LINE
    return paths->tools_user_dir;
}

const char *ik_paths_get_tools_project_dir(ik_paths_t *paths)
{
    assert(paths != NULL);  // LCOV_EXCL_BR_LINE
    return paths->tools_project_dir;
}

res_t ik_paths_translate_ik_uri_to_path(TALLOC_CTX *ctx, ik_paths_t *paths,
                                         const char *input, char **out)
{
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
    assert(out != NULL);    // LCOV_EXCL_BR_LINE

    if (paths == NULL) {
        return ERR(ctx, INVALID_ARG, "paths is NULL");
    }
    if (input == NULL) {
        return ERR(ctx, INVALID_ARG, "input is NULL");
    }

    const char *state_dir = ik_paths_get_state_dir(paths);
    const char *uri_prefix = "ik://";
    const size_t uri_prefix_len = 5;  // strlen("ik://")
    const size_t state_dir_len = strlen(state_dir);

    // Count occurrences to estimate output size
    size_t count = 0;
    const char *pos = input;
    while ((pos = strstr(pos, uri_prefix)) != NULL) {
        // Check that it's not a false positive (e.g., "myik://")
        if (pos == input || !((pos[-1] >= 'a' && pos[-1] <= 'z') ||
                              (pos[-1] >= 'A' && pos[-1] <= 'Z') ||
                              (pos[-1] >= '0' && pos[-1] <= '9') ||
                              pos[-1] == '_')) {
            count++;
        }
        pos += uri_prefix_len;
    }

    // If no replacements needed, return copy
    if (count == 0) {
        *out = talloc_strdup(ctx, input);
        if (*out == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return OK(NULL);
    }

    // Allocate output buffer (input + (state_dir_len - uri_prefix_len) * count)
    size_t output_size = strlen(input) + (state_dir_len - uri_prefix_len) * count + 1;
    char *result = talloc_array(ctx, char, (unsigned int)output_size);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Build output with replacements
    char *dest = result;
    const char *src = input;
    while (*src != '\0') {
        const char *next = strstr(src, uri_prefix);
        if (next == NULL) {
            // Copy remainder
            strcpy(dest, src);
            break;
        }

        // Check for false positive
        if (next != input && ((next[-1] >= 'a' && next[-1] <= 'z') ||
                              (next[-1] >= 'A' && next[-1] <= 'Z') ||
                              (next[-1] >= '0' && next[-1] <= '9') ||
                              next[-1] == '_')) {
            // False positive - copy including "ik://"
            size_t copy_len = (size_t)(next - src) + uri_prefix_len;
            memcpy(dest, src, copy_len);
            dest += copy_len;
            src = next + uri_prefix_len;
            continue;
        }

        // Copy text before ik://
        if (next > src) {
            size_t copy_len = (size_t)(next - src);
            memcpy(dest, src, copy_len);
            dest += copy_len;
        }

        // Replace ik:// with state_dir
        memcpy(dest, state_dir, state_dir_len);
        dest += state_dir_len;

        // Add trailing slash if not present after ik://
        const char *after_uri = next + uri_prefix_len;
        if (*after_uri != '\0' && *after_uri != '/' && state_dir[state_dir_len - 1] != '/') {
            *dest++ = '/';
        }

        src = after_uri;
    }

    *out = result;
    return OK(NULL);
}

res_t ik_paths_translate_path_to_ik_uri(TALLOC_CTX *ctx, ik_paths_t *paths,
                                         const char *input, char **out)
{
    assert(ctx != NULL);    // LCOV_EXCL_BR_LINE
    assert(out != NULL);    // LCOV_EXCL_BR_LINE

    if (paths == NULL) {
        return ERR(ctx, INVALID_ARG, "paths is NULL");
    }
    if (input == NULL) {
        return ERR(ctx, INVALID_ARG, "input is NULL");
    }

    const char *state_dir = ik_paths_get_state_dir(paths);
    const char *uri_prefix = "ik://";
    const size_t uri_prefix_len = 5;  // strlen("ik://")
    size_t state_dir_len = strlen(state_dir);

    // Count occurrences
    size_t count = 0;
    const char *pos = input;
    while ((pos = strstr(pos, state_dir)) != NULL) {
        count++;
        pos += state_dir_len;
    }

    // If no replacements needed, return copy
    if (count == 0) {
        *out = talloc_strdup(ctx, input);
        if (*out == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return OK(NULL);
    }

    // Allocate output buffer
    size_t output_size = strlen(input) + (uri_prefix_len - state_dir_len) * count + count + 1;
    char *result = talloc_array(ctx, char, (unsigned int)output_size);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Build output with replacements
    char *dest = result;
    const char *src = input;
    while (*src != '\0') {
        const char *next = strstr(src, state_dir);
        if (next == NULL) {
            // Copy remainder
            strcpy(dest, src);
            break;
        }

        // Copy text before state_dir
        if (next > src) {
            size_t copy_len = (size_t)(next - src);
            memcpy(dest, src, copy_len);
            dest += copy_len;
        }

        // Replace state_dir with ik://
        memcpy(dest, uri_prefix, uri_prefix_len);
        dest += uri_prefix_len;

        src = next + state_dir_len;

        // Skip leading slash after state_dir if present
        // (state_dir without trailing slash leaves /path, we want path after ik://)
        if (*src == '/') {
            src++;
        }
    }

    *out = result;
    return OK(NULL);
}
