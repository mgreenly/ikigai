#include <curl/curl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "json_allocator.h"
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
    if (new_data == NULL) {
        return 0;
    }

    buf->data = new_data;
    memcpy(&(buf->data[buf->size]), contents, realsize);
    buf->size += realsize;
    buf->data[buf->size] = '\0';

    return realsize;
}

static int32_t load_api_key(void *ctx, char **out_key)
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

static void write_auth_error_json(void)
{
    printf("{\n"
           "  \"success\": false,\n"
           "  \"error\": \"Web search requires API key configuration.\\n\\nBrave Search offers 2,000 free searches/month.\\nGet your key: https://brave.com/search/api/\\nAdd to: ~/.config/ikigai/credentials.json as 'web_search.brave.api_key'\",\n"
           "  \"error_code\": \"AUTH_MISSING\",\n"
           "  \"_event\": {\n"
           "    \"kind\": \"config_required\",\n"
           "    \"content\": \"⚠ Configuration Required\\n\\nWeb search needs an API key. Brave Search offers 2,000 free searches/month.\\n\\nGet your key: https://brave.com/search/api/\\nAdd to: ~/.config/ikigai/credentials.json\\n\\nExample:\\n{\\n  \\\"web_search\\\": {\\n    \\\"brave\\\": {\\n      \\\"api_key\\\": \\\"your-api-key-here\\\"\\n    }\\n  }\\n}\",\n"
           "    \"data\": {\n"
           "      \"tool\": \"web_search_brave\",\n"
           "      \"credential\": \"api_key\",\n"
           "      \"signup_url\": \"https://brave.com/search/api/\"\n"
           "    }\n"
           "  }\n"
           "}\n");
}

static int32_t domain_matches(const char *url, const char *domain)
{
    if (url == NULL || domain == NULL) {
        return 0;
    }

    const char *start = strstr(url, "://");
    if (start == NULL) {
        start = url;
    } else {
        start += 3;
    }

    const char *end = strchr(start, '/');
    size_t host_len;
    if (end == NULL) {
        host_len = strlen(start);
    } else {
        host_len = (size_t)(end - start);
    }

    size_t domain_len = strlen(domain);

    if (host_len == domain_len) {
        return strncasecmp(start, domain, host_len) == 0;
    }

    if (host_len > domain_len) {
        const char *suffix = start + (host_len - domain_len);
        if (suffix > start && suffix[-1] == '.') {
            return strncasecmp(suffix, domain, domain_len) == 0;
        }
    }

    return 0;
}

int32_t main(int32_t argc, char **argv)
{
    void *ctx = talloc_new(NULL);

    if (argc == 2 && strcmp(argv[1], "--schema") == 0) {
        printf("{\n"
               "  \"name\": \"web_search_brave\",\n"
               "  \"description\": \"Search the web using Brave Search API and use the results to inform responses. Provides up-to-date information for current events and recent data. Returns search result information formatted as search result blocks, including links as markdown hyperlinks.\",\n"
               "  \"parameters\": {\n"
               "    \"type\": \"object\",\n"
               "    \"properties\": {\n"
               "      \"query\": {\n"
               "        \"type\": \"string\",\n"
               "        \"description\": \"The search query to use\",\n"
               "        \"minLength\": 2\n"
               "      },\n"
               "      \"count\": {\n"
               "        \"type\": \"integer\",\n"
               "        \"description\": \"Number of results to return (1-20)\",\n"
               "        \"minimum\": 1,\n"
               "        \"maximum\": 20,\n"
               "        \"default\": 10\n"
               "      },\n"
               "      \"offset\": {\n"
               "        \"type\": \"integer\",\n"
               "        \"description\": \"Result offset for pagination\",\n"
               "        \"minimum\": 0,\n"
               "        \"default\": 0\n"
               "      },\n"
               "      \"allowed\": {\n"
               "        \"type\": \"array\",\n"
               "        \"items\": {\n"
               "          \"type\": \"string\"\n"
               "        },\n"
               "        \"description\": \"Only include search results from these domains\"\n"
               "      },\n"
               "      \"blocked\": {\n"
               "        \"type\": \"array\",\n"
               "        \"items\": {\n"
               "          \"type\": \"string\"\n"
               "        },\n"
               "        \"description\": \"Never include search results from these domains\"\n"
               "      }\n"
               "    },\n"
               "    \"required\": [\"query\"]\n"
               "  }\n"
               "}\n");
        talloc_free(ctx);
        return 0;
    }

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
        fprintf(stderr, "empty input\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(input, total_read, 0, &allocator, NULL);
    if (doc == NULL) {
        fprintf(stderr, "invalid JSON\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *query = yyjson_obj_get(root, "query");
    if (query == NULL || !yyjson_is_str(query)) {
        fprintf(stderr, "missing or invalid query\n");
        talloc_free(ctx);
        return 1;
    }

    const char *query_str = yyjson_get_str(query);
    int32_t count = 10;
    int32_t offset = 0;

    yyjson_val *cv = yyjson_obj_get(root, "count");
    if (cv != NULL && yyjson_is_int(cv)) {
        count = (int32_t)yyjson_get_int(cv);
    }

    yyjson_val *ov = yyjson_obj_get(root, "offset");
    if (ov != NULL && yyjson_is_int(ov)) {
        offset = (int32_t)yyjson_get_int(ov);
    }

    yyjson_val *allowed = yyjson_obj_get(root, "allowed");
    yyjson_val *blocked = yyjson_obj_get(root, "blocked");

    char *api_key = NULL;
    if (load_api_key(ctx, &api_key) != 0) {
        write_auth_error_json();
        talloc_free(ctx);
        return 0;
    }

    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        printf("{\"success\": false, \"error\": \"HTTP init failed\", \"error_code\": \"NETWORK_ERROR\"}\n");
        talloc_free(ctx);
        return 0;
    }

    char *escaped_query = curl_easy_escape(curl, query_str, 0);
    char *url = talloc_asprintf(ctx, "https://api.search.brave.com/res/v1/web/search?q=%s&count=%" PRId32 "&offset=%" PRId32, escaped_query, count, offset);
    curl_free(escaped_query);
    if (url == NULL) {
        curl_easy_cleanup(curl);
        talloc_free(ctx);
        return 1;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url);

    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: application/json");
    char *auth_header = talloc_asprintf(ctx, "X-Subscription-Token: %s", api_key);
    if (auth_header == NULL) {
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        talloc_free(ctx);
        return 1;
    }
    headers = curl_slist_append(headers, auth_header);
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    struct response_buffer response = {.ctx = ctx, .data = NULL, .size = 0};
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);

    int64_t http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        printf("{\"success\": false, \"error\": \"Network error\", \"error_code\": \"NETWORK_ERROR\"}\n");
        talloc_free(ctx);
        return 0;
    }

    if (http_code == 401 || http_code == 403) {
        printf("{\"success\": false, \"error\": \"Invalid API key\", \"error_code\": \"AUTH_INVALID\"}\n");
        talloc_free(ctx);
        return 0;
    }

    if (http_code == 429) {
        printf("{\"success\": false, \"error\": \"Rate limit exceeded (2,000/month quota used)\", \"error_code\": \"RATE_LIMIT\"}\n");
        talloc_free(ctx);
        return 0;
    }

    if (http_code != 200) {
        printf("{\"success\": false, \"error\": \"API returned error\", \"error_code\": \"API_ERROR\"}\n");
        talloc_free(ctx);
        return 0;
    }

    if (response.data == NULL) {
        printf("{\"success\": false, \"error\": \"Empty API response\", \"error_code\": \"API_ERROR\"}\n");
        talloc_free(ctx);
        return 0;
    }

    yyjson_alc response_allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *response_doc = yyjson_read_opts(response.data, response.size, 0, &response_allocator, NULL);
    if (response_doc == NULL) {
        printf("{\"success\": false, \"error\": \"Invalid API JSON\", \"error_code\": \"API_ERROR\"}\n");
        talloc_free(ctx);
        return 0;
    }

    yyjson_val *response_root = yyjson_doc_get_root(response_doc);
    yyjson_val *web = yyjson_obj_get(response_root, "web");
    if (web == NULL) {
        printf("{\"success\": false, \"error\": \"Missing web results\", \"error_code\": \"API_ERROR\"}\n");
        talloc_free(ctx);
        return 0;
    }

    yyjson_val *results = yyjson_obj_get(web, "results");
    if (results == NULL || !yyjson_is_arr(results)) {
        printf("{\"success\": true, \"results\": [], \"count\": 0}\n");
        talloc_free(ctx);
        return 0;
    }

    yyjson_alc output_allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *output_doc = yyjson_mut_doc_new(&output_allocator);
    if (output_doc == NULL) {
        talloc_free(ctx);
        return 1;
    }

    yyjson_mut_val *output_root = yyjson_mut_obj(output_doc);
    if (output_root == NULL) {
        talloc_free(ctx);
        return 1;
    }

    yyjson_mut_val *success_val = yyjson_mut_bool(output_doc, true);
    if (success_val == NULL) {
        talloc_free(ctx);
        return 1;
    }
    yyjson_mut_obj_add_val(output_doc, output_root, "success", success_val);

    yyjson_mut_val *results_array = yyjson_mut_arr(output_doc);
    if (results_array == NULL) {
        talloc_free(ctx);
        return 1;
    }

    size_t idx, max;
    yyjson_val *item;
    yyjson_arr_foreach(results, idx, max, item) {
        yyjson_val *url_val = yyjson_obj_get(item, "url");
        yyjson_val *title_val = yyjson_obj_get(item, "title");
        yyjson_val *description_val = yyjson_obj_get(item, "description");

        if (url_val == NULL || !yyjson_is_str(url_val)) {
            continue;
        }

        const char *url_str = yyjson_get_str(url_val);

        if (allowed != NULL && yyjson_is_arr(allowed)) {
            int32_t matches = 0;
            size_t aidx, amax;
            yyjson_val *domain;
            yyjson_arr_foreach(allowed, aidx, amax, domain) {
                if (yyjson_is_str(domain)) {
                    if (domain_matches(url_str, yyjson_get_str(domain))) {
                        matches = 1;
                        break;
                    }
                }
            }
            if (!matches) {
                continue;
            }
        }

        if (blocked != NULL && yyjson_is_arr(blocked)) {
            int32_t blk = 0;
            size_t bidx, bmax;
            yyjson_val *domain;
            yyjson_arr_foreach(blocked, bidx, bmax, domain) {
                if (yyjson_is_str(domain)) {
                    if (domain_matches(url_str, yyjson_get_str(domain))) {
                        blk = 1;
                        break;
                    }
                }
            }
            if (blk) {
                continue;
            }
        }

        yyjson_mut_val *result_obj = yyjson_mut_obj(output_doc);
        if (result_obj == NULL) {
            talloc_free(ctx);
            return 1;
        }

        if (title_val != NULL && yyjson_is_str(title_val)) {
            yyjson_mut_val *title_mut = yyjson_mut_str(output_doc, yyjson_get_str(title_val));
            if (title_mut == NULL) {
                talloc_free(ctx);
                return 1;
            }
            yyjson_mut_obj_add_val(output_doc, result_obj, "title", title_mut);
        }

        yyjson_mut_val *url_mut = yyjson_mut_str(output_doc, url_str);
        if (url_mut == NULL) {
            talloc_free(ctx);
            return 1;
        }
        yyjson_mut_obj_add_val(output_doc, result_obj, "url", url_mut);

        if (description_val != NULL && yyjson_is_str(description_val)) {
            yyjson_mut_val *snippet_mut = yyjson_mut_str(output_doc, yyjson_get_str(description_val));
            if (snippet_mut == NULL) {
                talloc_free(ctx);
                return 1;
            }
            yyjson_mut_obj_add_val(output_doc, result_obj, "snippet", snippet_mut);
        }

        yyjson_mut_arr_append(results_array, result_obj);
    }

    yyjson_mut_obj_add_val(output_doc, output_root, "results", results_array);

    size_t result_count = yyjson_mut_arr_size(results_array);
    yyjson_mut_val *count_mut = yyjson_mut_int(output_doc, (int64_t)result_count);
    if (count_mut == NULL) {
        talloc_free(ctx);
        return 1;
    }
    yyjson_mut_obj_add_val(output_doc, output_root, "count", count_mut);

    yyjson_mut_doc_set_root(output_doc, output_root);

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
