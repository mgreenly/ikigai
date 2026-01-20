#include "credentials.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "json_allocator.h"
#include "vendor/yyjson/yyjson.h"

int32_t load_api_key(void *ctx, char **out_key)
{
    const char *env_key = getenv("BRAVE_API_KEY");
    if (env_key != NULL && env_key[0] != '\0') {
        *out_key = talloc_strdup(ctx, env_key);
        return 0;
    }

    const char *home = getenv("HOME");
    if (home == NULL) {
        return -1;
    }

    char *cred_path = talloc_asprintf(ctx, "%s/.config/ikigai/credentials.json", home);
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

void write_auth_error_json(void)
{
    printf("{\n");
    printf("  \"success\": false,\n");
    printf(
        "  \"error\": \"Web search requires API key configuration.\\n\\nBrave Search offers 2,000 free searches/month.\\nGet your key: https://brave.com/search/api/\\nAdd to: ~/.config/ikigai/credentials.json as 'web_search.brave.api_key'\",\n");
    printf("  \"error_code\": \"AUTH_MISSING\",\n");
    printf("  \"_event\": {\n");
    printf("    \"kind\": \"config_required\",\n");
    printf(
        "    \"content\": \"⚠ Configuration Required\\n\\nWeb search needs an API key. Brave Search offers 2,000 free searches/month.\\n\\nGet your key: https://brave.com/search/api/\\nAdd to: ~/.config/ikigai/credentials.json\\n\\nExample:\\n{\\n  \\\"web_search\\\": {\\n    \\\"brave\\\": {\\n      \\\"api_key\\\": \\\"your-api-key-here\\\"\\n    }\\n  }\\n}\",\n");
    printf("    \"data\": {\n");
    printf("      \"tool\": \"web_search_brave\",\n");
    printf("      \"credential\": \"api_key\",\n");
    printf("      \"signup_url\": \"https://brave.com/search/api/\"\n");
    printf("    }\n");
    printf("  }\n");
    printf("}\n");
}
