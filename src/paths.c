#include "paths.h"
#include "panic.h"
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>


#include "poison.h"
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

    // Check if all required environment variables are set and non-empty
    if (bin_dir == NULL || bin_dir[0] == '\0' ||
        config_dir == NULL || config_dir[0] == '\0' ||
        data_dir == NULL || data_dir[0] == '\0' ||
        libexec_dir == NULL || libexec_dir[0] == '\0' ||
        cache_dir == NULL || cache_dir[0] == '\0' ||
        state_dir == NULL || state_dir[0] == '\0') {
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

// Helper: Check if character is alphanumeric or underscore
static inline bool is_alnum_or_underscore(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
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
    const char *data_dir = ik_paths_get_data_dir(paths);
    const char *uri_prefix = "ik://";
    const char *system_suffix = "system";
    const size_t uri_prefix_len = 5;  // strlen("ik://")
    const size_t system_suffix_len = 6;  // strlen("system")
    const size_t state_dir_len = strlen(state_dir);
    const size_t data_dir_len = strlen(data_dir);

    // Count occurrences to estimate output size
    size_t generic_count = 0;
    size_t system_count = 0;
    const char *pos = input;
    while ((pos = strstr(pos, uri_prefix)) != NULL) {
        // Check that it's not a false positive (e.g., "myik://")
        bool is_false_positive = (pos != input && is_alnum_or_underscore(pos[-1]));
        if (is_false_positive) {
            pos += uri_prefix_len;
            continue;
        }

        const char *after_prefix = pos + uri_prefix_len;
        bool is_system_ns = (strncmp(after_prefix, system_suffix, system_suffix_len) == 0);
        if (is_system_ns) {
            char following = after_prefix[system_suffix_len];
            if (following == '\0' || following == '/') {
                system_count++;
                pos += uri_prefix_len + system_suffix_len;
                continue;
            }
        }

        generic_count++;
        pos += uri_prefix_len;
    }

    // If no replacements needed, return copy
    if (generic_count == 0 && system_count == 0) {
        *out = talloc_strdup(ctx, input);
        if (*out == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return OK(NULL);
    }

    // Allocate output buffer
    size_t output_size = strlen(input) +
                        (state_dir_len - uri_prefix_len + 1) * generic_count +
                        (data_dir_len + system_suffix_len + 1 - uri_prefix_len - system_suffix_len + 1) * system_count + 1;
    char *result = talloc_zero_array(ctx, char, (unsigned int)output_size);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Build output with replacements
    char *dest = result;
    const char *src = input;
    while (*src != '\0') {
        const char *next = strstr(src, uri_prefix);
        if (next == NULL) {
            size_t remainder_len = strlen(src);
            memcpy(dest, src, remainder_len);
            dest += remainder_len;
            break;
        }

        bool is_false_positive = (next != input && is_alnum_or_underscore(next[-1]));
        if (is_false_positive) {
            size_t copy_len = (size_t)(next - src) + uri_prefix_len;
            memcpy(dest, src, copy_len);
            dest += copy_len;
            src = next + uri_prefix_len;
            continue;
        }

        if (next > src) {
            size_t copy_len = (size_t)(next - src);
            memcpy(dest, src, copy_len);
            dest += copy_len;
        }

        const char *after_prefix = next + uri_prefix_len;
        bool is_system_ns = (strncmp(after_prefix, system_suffix, system_suffix_len) == 0);
        bool system_boundary = false;
        if (is_system_ns) {
            char following = after_prefix[system_suffix_len];
            system_boundary = (following == '\0' || following == '/');
        }

        if (system_boundary) {
            memcpy(dest, data_dir, data_dir_len);
            dest += data_dir_len;
            *dest++ = '/';
            memcpy(dest, system_suffix, system_suffix_len);
            dest += system_suffix_len;
            src = after_prefix + system_suffix_len;
            continue;
        }

        memcpy(dest, state_dir, state_dir_len);
        dest += state_dir_len;
        bool need_slash = (*after_prefix != '\0' && *after_prefix != '/' &&
                          state_dir[state_dir_len - 1] != '/');
        if (need_slash) {
            *dest++ = '/';
        }
        src = after_prefix;
    }

    // Add null terminator
    *dest = '\0';

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
    const char *data_dir = ik_paths_get_data_dir(paths);
    const char *uri_prefix = "ik://";
    const char *system_suffix = "system";
    const size_t uri_prefix_len = 5;  // strlen("ik://")
    const size_t system_suffix_len = 6;  // strlen("system")
    size_t state_dir_len = strlen(state_dir);
    size_t data_dir_len = strlen(data_dir);

    // Build system path for matching
    char *system_path = talloc_asprintf(ctx, "%s/system", data_dir);
    if (system_path == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    size_t system_path_len = data_dir_len + 1 + system_suffix_len;

    // Count occurrences
    size_t generic_count = 0;
    size_t system_count = 0;
    const char *pos = input;
    while (true) {
        const char *system_match = strstr(pos, system_path);
        const char *state_match = strstr(pos, state_dir);

        if (system_match == NULL && state_match == NULL) {
            break;
        }

        // Check which match comes first
        if (system_match != NULL && (state_match == NULL || system_match < state_match)) {
            system_count++;
            pos = system_match + system_path_len;
        } else {
            generic_count++;
            pos = state_match + state_dir_len;
        }
    }

    talloc_free(system_path);

    // If no replacements needed, return copy
    if (generic_count == 0 && system_count == 0) {
        *out = talloc_strdup(ctx, input);
        if (*out == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return OK(NULL);
    }

    // Allocate output buffer
    size_t output_size = strlen(input) +
                        (uri_prefix_len - state_dir_len) * generic_count +
                        (uri_prefix_len + system_suffix_len - (data_dir_len + 1 + system_suffix_len)) * system_count +
                        generic_count + system_count + 1;
    char *result = talloc_zero_array(ctx, char, (unsigned int)output_size);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Rebuild system_path for replacement loop
    system_path = talloc_asprintf(ctx, "%s/system", data_dir);
    if (system_path == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    // Build output with replacements
    char *dest = result;
    const char *src = input;
    while (*src != '\0') {
        const char *system_match = strstr(src, system_path);
        const char *state_match = strstr(src, state_dir);

        if (system_match == NULL && state_match == NULL) {
            size_t remainder_len = strlen(src);
            memcpy(dest, src, remainder_len);
            dest += remainder_len;
            break;
        }

        bool use_system = (system_match != NULL &&
                          (state_match == NULL || system_match < state_match));

        const char *match = use_system ? system_match : state_match;
        size_t match_len = use_system ? system_path_len : state_dir_len;

        if (match > src) {
            size_t copy_len = (size_t)(match - src);
            memcpy(dest, src, copy_len);
            dest += copy_len;
        }

        memcpy(dest, uri_prefix, uri_prefix_len);
        dest += uri_prefix_len;

        if (use_system) {
            memcpy(dest, system_suffix, system_suffix_len);
            dest += system_suffix_len;
        }

        src = match + match_len;

        if (*src == '/') {
            if (use_system) {
                *dest++ = '/';
            }
            src++;
        }
    }

    *dest = '\0';
    talloc_free(system_path);

    *out = result;
    return OK(NULL);
}
