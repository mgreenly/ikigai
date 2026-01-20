#include "credentials.h"

#include "../../json_allocator.h"
#include "../../panic.h"
#include "../../paths.h"
#include "../../wrapper.h"

#include "../../vendor/yyjson/yyjson.h"

#include <inttypes.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

// LCOV_EXCL_START
static int32_t load_credential_from_file(void *ctx, const char *file_path, const char *key_path, char **out_value)
{
    FILE *fp = fopen_(file_path, "r");
    if (fp == NULL) {
        return -1;
    }

    size_t buffer_size = 4096;
    size_t total_read = 0;
    char *content = talloc_array(ctx, char, (unsigned int)buffer_size);
    if (content == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    size_t bytes_read;
    while ((bytes_read = fread(content + total_read, 1, buffer_size - total_read, fp)) > 0) {
        total_read += bytes_read;

        if (total_read >= buffer_size) {
            buffer_size *= 2;
            content = talloc_realloc(ctx, content, char, (unsigned int)buffer_size);
            if (content == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        }
    }

    fclose_(fp);

    content[total_read] = '\0';

    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(content, total_read, 0, &allocator, NULL);
    if (doc == NULL) {
        return -1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *web_search = yyjson_obj_get(root, "web_search");
    if (web_search == NULL) {
        return -1;
    }

    yyjson_val *google = yyjson_obj_get(web_search, "google");
    if (google == NULL) {
        return -1;
    }

    yyjson_val *key = yyjson_obj_get(google, key_path);
    if (key == NULL || !yyjson_is_str(key)) {
        return -1;
    }

    const char *value = yyjson_get_str(key);
    *out_value = talloc_strdup(ctx, value);

    return 0;
}
// LCOV_EXCL_STOP

int32_t load_credentials(void *ctx, char **out_api_key, char **out_engine_id)
{
    const char *api_key_env = getenv_("GOOGLE_SEARCH_API_KEY");
    if (api_key_env != NULL) {
        *out_api_key = talloc_strdup(ctx, api_key_env);
    } else {  // LCOV_EXCL_START
        ik_paths_t *paths = NULL;
        res_t paths_res = ik_paths_init(ctx, &paths);
        if (is_ok(&paths_res)) {
            const char *config_dir = ik_paths_get_config_dir(paths);
            char *config_path = talloc_asprintf(ctx, "%s/credentials.json", config_dir);
            if (load_credential_from_file(ctx, config_path, "api_key", out_api_key) != 0) {
                *out_api_key = NULL;
            }
        } else {
            *out_api_key = NULL;
        }
    }  // LCOV_EXCL_STOP

    const char *engine_id_env = getenv_("GOOGLE_SEARCH_ENGINE_ID");
    if (engine_id_env != NULL) {
        *out_engine_id = talloc_strdup(ctx, engine_id_env);
    } else {  // LCOV_EXCL_START
        ik_paths_t *paths = NULL;
        res_t paths_res = ik_paths_init(ctx, &paths);
        if (is_ok(&paths_res)) {
            const char *config_dir = ik_paths_get_config_dir(paths);
            char *config_path = talloc_asprintf(ctx, "%s/credentials.json", config_dir);
            if (load_credential_from_file(ctx, config_path, "engine_id", out_engine_id) != 0) {
                *out_engine_id = NULL;
            }
        } else {
            *out_engine_id = NULL;
        }
    }  // LCOV_EXCL_STOP

    if (*out_api_key == NULL || *out_engine_id == NULL) {
        return -1;
    }

    if (strlen(*out_api_key) == 0 || strlen(*out_engine_id) == 0) {
        return -1;
    }

    return 0;
}
