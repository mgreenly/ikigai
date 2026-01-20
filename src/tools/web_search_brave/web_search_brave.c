#include "web_search_brave.h"

#include "auth_error.h"
#include "domain_utils.h"
#include "json_allocator.h"
#include "panic.h"
#include "paths.h"
#include "wrapper_posix.h"
#include "wrapper_stdlib.h"
#include "wrapper_web.h"

#include <curl/curl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "vendor/yyjson/yyjson.h"

struct response_buffer {
    void *ctx;
    char *data;
    size_t size;
};

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct response_buffer *buf = (struct response_buffer *)userp;

    char *new_data = talloc_realloc(buf->ctx, buf->data, char, (unsigned int)(buf->size + realsize + 1));
    if (new_data == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    buf->data = new_data;
    memcpy(&(buf->data[buf->size]), contents, realsize);
    buf->size += realsize;
    buf->data[buf->size] = '\0';

    return realsize;
}

int32_t web_search_brave_execute(void *ctx, const web_search_brave_params_t *params)
{
    char *api_key = NULL;
    const char *env_key = getenv_("BRAVE_API_KEY");
    if (env_key != NULL && env_key[0] != '\0') {
        api_key = talloc_strdup(ctx, env_key);
        if (api_key == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    } else {
        ik_paths_t *paths = NULL;
        res_t paths_res = ik_paths_init(ctx, &paths);
        if (is_ok(&paths_res)) {
            const char *config_dir = ik_paths_get_config_dir(paths);
            char *cred_path = talloc_asprintf(ctx, "%s/credentials.json", config_dir);
            if (cred_path == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

            FILE *f = fopen_(cred_path, "r");
            if (f != NULL) {
                fseek_(f, 0, SEEK_END);
                int64_t fsize = ftell_(f);
                fseek_(f, 0, SEEK_SET);

                char *content = talloc_array(ctx, char, (unsigned int)(fsize + 1));
                if (content == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

                size_t cred_bytes_read = fread_(content, 1, (size_t)fsize, f);
                fclose_(f);
                content[cred_bytes_read] = '\0';

                yyjson_alc cred_allocator = ik_make_talloc_allocator(ctx);
                yyjson_doc *cred_doc = yyjson_read_opts(content, cred_bytes_read, 0, &cred_allocator, NULL);
                if (cred_doc != NULL) {
                    yyjson_val *cred_root = yyjson_doc_get_root(cred_doc);  // LCOV_EXCL_BR_LINE
                    yyjson_val *web_search = yyjson_obj_get(cred_root, "web_search");
                    if (web_search != NULL) {
                        yyjson_val *brave = yyjson_obj_get(web_search, "brave");
                        if (brave != NULL) {
                            yyjson_val *api_key_val = yyjson_obj_get(brave, "api_key");
                            if (api_key_val != NULL && yyjson_is_str(api_key_val)) {
                                api_key = talloc_strdup(ctx, yyjson_get_str(api_key_val));
                                if (api_key == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                            }
                        }
                    }
                }
            }
        }
    }

    if (api_key == NULL) {
        write_auth_error_json();
        return 0;
    }

    CURL *curl = curl_easy_init_();
    if (curl == NULL) {
        printf(
            "{\"success\": false, \"error\": \"Failed to initialize HTTP client\", \"error_code\": \"NETWORK_ERROR\"}\n");
        return 0;
    }

    char *url = talloc_asprintf(ctx,
                                "https://api.search.brave.com/res/v1/web/search?q=%s&count=%" PRId32 "&offset=%" PRId32,
                                params->query,
                                params->count,
                                params->offset);
    if (url == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char *escaped_query = curl_easy_escape(curl, params->query, 0);
    talloc_free(url);
    url = talloc_asprintf(ctx,
                          "https://api.search.brave.com/res/v1/web/search?q=%s&count=%" PRId32 "&offset=%" PRId32,
                          escaped_query,
                          params->count,
                          params->offset);
    curl_free(escaped_query);

    if (url == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    curl_easy_setopt_(curl, CURLOPT_URL, url);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    char *auth_header = talloc_asprintf(ctx, "X-Subscription-Token: %s", api_key);
    if (auth_header == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    headers = curl_slist_append(headers, auth_header);
    curl_easy_setopt_(curl, CURLOPT_HTTPHEADER, headers);

    struct response_buffer response = {.ctx = ctx, .data = NULL, .size = 0};
    curl_easy_setopt_(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt_(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform_(curl);

    int64_t http_code = 0;
    curl_easy_getinfo_(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup_(curl);

    if (res != CURLE_OK) {
        printf("{\"success\": false, \"error\": \"Network request failed\", \"error_code\": \"NETWORK_ERROR\"}\n");
        return 0;
    }

    if (http_code == 401 || http_code == 403) {
        printf(
            "{\"success\": false, \"error\": \"API key is invalid or unauthorized\", \"error_code\": \"AUTH_INVALID\"}\n");
        return 0;
    }

    if (http_code == 429) {
        printf(
            "{\"success\": false, \"error\": \"Rate limit exceeded. You've used your free search quota (2,000/month).\", \"error_code\": \"RATE_LIMIT\"}\n");
        return 0;
    }

    if (http_code != 200) {
        printf("{\"success\": false, \"error\": \"API returned error\", \"error_code\": \"API_ERROR\"}\n");
        return 0;
    }

    if (response.data == NULL) {
        printf("{\"success\": false, \"error\": \"Empty response from API\", \"error_code\": \"API_ERROR\"}\n");
        return 0;
    }

    yyjson_alc response_allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *response_doc = yyjson_read_opts(response.data, response.size, 0, &response_allocator, NULL);
    if (response_doc == NULL) {
        printf("{\"success\": false, \"error\": \"Invalid JSON response from API\", \"error_code\": \"API_ERROR\"}\n");
        return 0;
    }

    yyjson_val *response_root = yyjson_doc_get_root(response_doc);  // LCOV_EXCL_BR_LINE
    yyjson_val *web = yyjson_obj_get(response_root, "web");
    if (web == NULL) {
        printf(
            "{\"success\": false, \"error\": \"Missing web results in API response\", \"error_code\": \"API_ERROR\"}\n");
        return 0;
    }

    yyjson_val *results = yyjson_obj_get(web, "results");
    if (results == NULL || !yyjson_is_arr(results)) {
        printf("{\"success\": true, \"results\": [], \"count\": 0}\n");
        return 0;
    }

    yyjson_alc output_allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *output_doc = yyjson_mut_doc_new(&output_allocator);
    if (output_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *output_root = yyjson_mut_obj(output_doc);
    if (output_root == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *success_val = yyjson_mut_bool(output_doc, true);
    if (success_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_obj_add_val(output_doc, output_root, "success", success_val);

    yyjson_mut_val *results_array = yyjson_mut_arr(output_doc);
    if (results_array == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    size_t idx, max;
    yyjson_val *item;
    yyjson_arr_foreach(results, idx, max, item) {  // LCOV_EXCL_BR_LINE
        yyjson_val *url_val = yyjson_obj_get(item, "url");  // LCOV_EXCL_BR_LINE
        yyjson_val *title_val = yyjson_obj_get(item, "title");  // LCOV_EXCL_BR_LINE
        yyjson_val *description_val = yyjson_obj_get(item, "description");

        if (url_val == NULL || !yyjson_is_str(url_val)) {
            continue;
        }

        const char *url_str = yyjson_get_str(url_val);

        if (params->allowed_domains != NULL && yyjson_is_arr(params->allowed_domains)) {  // LCOV_EXCL_BR_LINE
            int32_t matches = 0;
            size_t aidx, amax;
            yyjson_val *domain;
            yyjson_arr_foreach(params->allowed_domains, aidx, amax, domain) {  // LCOV_EXCL_BR_LINE
                if (yyjson_is_str(domain)) {  // LCOV_EXCL_BR_LINE
                    const char *domain_str = yyjson_get_str(domain);
                    if (url_matches_domain(url_str, domain_str)) {
                        matches = 1;
                        break;
                    }
                }
            }
            if (!matches) {
                continue;
            }
        }

        if (params->blocked_domains != NULL && yyjson_is_arr(params->blocked_domains)) {  // LCOV_EXCL_BR_LINE
            int32_t blocked = 0;
            size_t bidx, bmax;
            yyjson_val *domain;
            yyjson_arr_foreach(params->blocked_domains, bidx, bmax, domain) {  // LCOV_EXCL_BR_LINE
                if (yyjson_is_str(domain)) {  // LCOV_EXCL_BR_LINE
                    const char *domain_str = yyjson_get_str(domain);
                    if (url_matches_domain(url_str, domain_str)) {
                        blocked = 1;
                        break;
                    }
                }
            }
            if (blocked) {
                continue;
            }
        }

        yyjson_mut_val *result_obj = yyjson_mut_obj(output_doc);
        if (result_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        if (title_val != NULL && yyjson_is_str(title_val)) {
            yyjson_mut_val *title_mut = yyjson_mut_str(output_doc, yyjson_get_str(title_val));
            if (title_mut == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            yyjson_mut_obj_add_val(output_doc, result_obj, "title", title_mut);
        }

        yyjson_mut_val *url_mut = yyjson_mut_str(output_doc, url_str);
        if (url_mut == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        yyjson_mut_obj_add_val(output_doc, result_obj, "url", url_mut);

        if (description_val != NULL && yyjson_is_str(description_val)) {
            yyjson_mut_val *snippet_mut = yyjson_mut_str(output_doc, yyjson_get_str(description_val));
            if (snippet_mut == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            yyjson_mut_obj_add_val(output_doc, result_obj, "snippet", snippet_mut);
        }

        yyjson_mut_arr_append(results_array, result_obj);
    }

    yyjson_mut_obj_add_val(output_doc, output_root, "results", results_array);

    size_t result_count = yyjson_mut_arr_size(results_array);
    yyjson_mut_val *count_mut = yyjson_mut_int(output_doc, (int64_t)result_count);
    if (count_mut == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    yyjson_mut_obj_add_val(output_doc, output_root, "count", count_mut);

    yyjson_mut_doc_set_root(output_doc, output_root);

    char *json_str = yyjson_mut_write_opts(output_doc, 0, &output_allocator, NULL, NULL);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    printf("%s\n", json_str);

    return 0;
}
