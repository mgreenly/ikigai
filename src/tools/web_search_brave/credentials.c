#include "credentials.h"

#include "error.h"
#include "json_allocator.h"
#include "paths.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "vendor/yyjson/yyjson.h"

int32_t load_api_key(void *ctx, char **out_key)
{
    const char *env_key = getenv("BRAVE_API_KEY");
    if (env_key != NULL && env_key[0] != '\0') {
        *out_key = talloc_strdup(ctx, env_key);
        return 0;
    }

    ik_paths_t *paths = NULL;
    res_t paths_res = ik_paths_init(ctx, &paths);
    if (is_err(&paths_res)) {
        return -1;
    }

    const char *config_dir = ik_paths_get_config_dir(paths);
    char *cred_path = talloc_asprintf(ctx, "%s/credentials.json", config_dir);
    if (cred_path == NULL) {
        return -1;
    }

    FILE *f = fopen(cred_path, "r");
    if (f == NULL) {
        return -1;
    }

    fseek(f, 0, SEEK_END);
    int64_t fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *content = talloc_array(ctx, char, (unsigned int)(fsize + 1));
    if (content == NULL) {
        fclose(f);
        return -1;
    }

    size_t bytes_read = fread(content, 1, (size_t)fsize, f);
    fclose(f);
    content[bytes_read] = '\0';

    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(content, bytes_read, 0, &allocator, NULL);
    if (doc == NULL) {
        return -1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *web_search = yyjson_obj_get(root, "web_search");
    if (web_search == NULL) {
        return -1;
    }

    yyjson_val *brave = yyjson_obj_get(web_search, "brave");
    if (brave == NULL) {
        return -1;
    }

    yyjson_val *api_key = yyjson_obj_get(brave, "api_key");
    if (api_key == NULL || !yyjson_is_str(api_key)) {
        return -1;
    }

    *out_key = talloc_strdup(ctx, yyjson_get_str(api_key));
    return 0;
}
