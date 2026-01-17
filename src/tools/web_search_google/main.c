#include <curl/curl.h>
#include <inttypes.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <unistd.h>

#include "json_allocator.h"

#include "vendor/yyjson/yyjson.h"

struct response_buffer {
    void *ctx;
    char *data;
    size_t size;
};

struct api_call {
    CURL *handle;
    struct response_buffer response;
    char *domain;
    int64_t num_for_domain;
    bool success;
    char *url;
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

static void output_error_with_event(void *ctx, const char *error, const char *error_code)
{
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    if (doc == NULL) {
        exit(1);
    }

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (obj == NULL) {
        exit(1);
    }

    yyjson_mut_val *success_val = yyjson_mut_bool(doc, false);
    if (success_val == NULL) {
        exit(1);
    }

    yyjson_mut_val *error_val = yyjson_mut_str(doc, error);
    if (error_val == NULL) {
        exit(1);
    }

    yyjson_mut_val *error_code_val = yyjson_mut_str(doc, error_code);
    if (error_code_val == NULL) {
        exit(1);
    }

    yyjson_mut_obj_add_val(doc, obj, "success", success_val);
    yyjson_mut_obj_add_val(doc, obj, "error", error_val);
    yyjson_mut_obj_add_val(doc, obj, "error_code", error_code_val);

    if (strcmp(error_code, "AUTH_MISSING") == 0) {
        const char *event_content = "⚠ Configuration Required\n\nWeb search needs an API key and Search Engine ID. Google Custom Search offers 100 free searches/day.\n\nGet API key: https://developers.google.com/custom-search/v1/overview\nGet Search Engine ID: https://programmablesearchengine.google.com/controlpanel/create\nAdd to: ~/.config/ikigai/credentials.json\n\nExample:\n{\n  \"web_search\": {\n    \"google\": {\n      \"api_key\": \"your-api-key-here\",\n      \"engine_id\": \"your-search-engine-id\"\n    }\n  }\n}";

        yyjson_mut_val *event_obj = yyjson_mut_obj(doc);
        if (event_obj == NULL) {
            exit(1);
        }

        yyjson_mut_val *kind_val = yyjson_mut_str(doc, "config_required");
        if (kind_val == NULL) {
            exit(1);
        }

        yyjson_mut_val *content_val = yyjson_mut_str(doc, event_content);
        if (content_val == NULL) {
            exit(1);
        }

        yyjson_mut_val *data_obj = yyjson_mut_obj(doc);
        if (data_obj == NULL) {
            exit(1);
        }

        yyjson_mut_val *tool_val = yyjson_mut_str(doc, "web_search_google");
        if (tool_val == NULL) {
            exit(1);
        }

        yyjson_mut_val *creds_arr = yyjson_mut_arr(doc);
        if (creds_arr == NULL) {
            exit(1);
        }

        yyjson_mut_val *cred1 = yyjson_mut_str(doc, "api_key");
        if (cred1 == NULL) {
            exit(1);
        }

        yyjson_mut_val *cred2 = yyjson_mut_str(doc, "engine_id");
        if (cred2 == NULL) {
            exit(1);
        }

        yyjson_mut_arr_append(creds_arr, cred1);
        yyjson_mut_arr_append(creds_arr, cred2);

        yyjson_mut_obj_add_val(doc, data_obj, "tool", tool_val);
        yyjson_mut_obj_add_val(doc, data_obj, "credentials", creds_arr);

        yyjson_mut_obj_add_val(doc, event_obj, "kind", kind_val);
        yyjson_mut_obj_add_val(doc, event_obj, "content", content_val);
        yyjson_mut_obj_add_val(doc, event_obj, "data", data_obj);

        yyjson_mut_obj_add_val(doc, obj, "_event", event_obj);
    }

    yyjson_mut_doc_set_root(doc, obj);

    char *json_str = yyjson_mut_write(doc, 0, NULL);
    if (json_str == NULL) {
        exit(1);
    }

    printf("%s\n", json_str);
    free(json_str);
}

static void output_error(void *ctx, const char *error, const char *error_code)
{
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    if (doc == NULL) {
        exit(1);
    }

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (obj == NULL) {
        exit(1);
    }

    yyjson_mut_val *success_val = yyjson_mut_bool(doc, false);
    if (success_val == NULL) {
        exit(1);
    }

    yyjson_mut_val *error_val = yyjson_mut_str(doc, error);
    if (error_val == NULL) {
        exit(1);
    }

    yyjson_mut_val *error_code_val = yyjson_mut_str(doc, error_code);
    if (error_code_val == NULL) {
        exit(1);
    }

    yyjson_mut_obj_add_val(doc, obj, "success", success_val);
    yyjson_mut_obj_add_val(doc, obj, "error", error_val);
    yyjson_mut_obj_add_val(doc, obj, "error_code", error_code_val);

    yyjson_mut_doc_set_root(doc, obj);

    char *json_str = yyjson_mut_write(doc, 0, NULL);
    if (json_str == NULL) {
        exit(1);
    }

    printf("%s\n", json_str);
    free(json_str);
}

static int32_t load_credential_from_file(void *ctx, const char *file_path, const char *key_path, char **out_value)
{
    FILE *fp = fopen(file_path, "r");
    if (fp == NULL) {
        return -1;
    }

    size_t buffer_size = 4096;
    size_t total_read = 0;
    char *content = talloc_array(ctx, char, (unsigned int)buffer_size);
    if (content == NULL) {
        fclose(fp);
        return -1;
    }

    size_t bytes_read;
    while ((bytes_read = fread(content + total_read, 1, buffer_size - total_read, fp)) > 0) {
        total_read += bytes_read;

        if (total_read >= buffer_size) {
            buffer_size *= 2;
            content = talloc_realloc(ctx, content, char, (unsigned int)buffer_size);
            if (content == NULL) {
                fclose(fp);
                return -1;
            }
        }
    }

    fclose(fp);

    if (total_read < buffer_size) {
        content[total_read] = '\0';
    } else {
        content = talloc_realloc(ctx, content, char, (unsigned int)(total_read + 1));
        if (content == NULL) {
            return -1;
        }
        content[total_read] = '\0';
    }

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

static int32_t load_credentials(void *ctx, char **out_api_key, char **out_engine_id)
{
    const char *api_key_env = getenv("GOOGLE_SEARCH_API_KEY");
    if (api_key_env != NULL) {
        *out_api_key = talloc_strdup(ctx, api_key_env);
    } else {
        const char *home = getenv("HOME");
        if (home == NULL) {
            struct passwd *pw = getpwuid(getuid());
            if (pw != NULL) {
                home = pw->pw_dir;
            }
        }

        if (home != NULL) {
            char *config_path = talloc_asprintf(ctx, "%s/.config/ikigai/credentials.json", home);
            if (load_credential_from_file(ctx, config_path, "api_key", out_api_key) != 0) {
                *out_api_key = NULL;
            }
        } else {
            *out_api_key = NULL;
        }
    }

    const char *engine_id_env = getenv("GOOGLE_SEARCH_ENGINE_ID");
    if (engine_id_env != NULL) {
        *out_engine_id = talloc_strdup(ctx, engine_id_env);
    } else {
        const char *home = getenv("HOME");
        if (home == NULL) {
            struct passwd *pw = getpwuid(getuid());
            if (pw != NULL) {
                home = pw->pw_dir;
            }
        }

        if (home != NULL) {
            char *config_path = talloc_asprintf(ctx, "%s/.config/ikigai/credentials.json", home);
            if (load_credential_from_file(ctx, config_path, "engine_id", out_engine_id) != 0) {
                *out_engine_id = NULL;
            }
        } else {
            *out_engine_id = NULL;
        }
    }

    if (*out_api_key == NULL || *out_engine_id == NULL) {
        return -1;
    }

    if (strlen(*out_api_key) == 0 || strlen(*out_engine_id) == 0) {
        return -1;
    }

    return 0;
}

static char *url_encode(void *ctx, const char *str)
{
    CURL *curl = curl_easy_init();
    if (curl == NULL) {
        return NULL;
    }

    char *encoded = curl_easy_escape(curl, str, (int32_t)strlen(str));
    if (encoded == NULL) {
        curl_easy_cleanup(curl);
        return NULL;
    }

    char *result = talloc_strdup(ctx, encoded);
    curl_free(encoded);
    curl_easy_cleanup(curl);

    return result;
}

static bool url_already_seen(yyjson_mut_val *results_arr, const char *url)
{
    size_t idx, max;
    yyjson_mut_val *item;
    yyjson_mut_arr_foreach(results_arr, idx, max, item) {
        yyjson_mut_val *url_val = yyjson_mut_obj_get(item, "url");
        if (url_val != NULL) {
            const char *existing_url = yyjson_mut_get_str(url_val);
            if (existing_url != NULL && strcmp(existing_url, url) == 0) {
                return true;
            }
        }
    }
    return false;
}

int32_t main(int32_t argc, char **argv)
{
    void *ctx = talloc_new(NULL);

    if (argc == 2 && strcmp(argv[1], "--schema") == 0) {
        printf("{\n");
        printf("  \"name\": \"web_search_google\",\n");
        printf("  \"description\": \"Search the web using Google Custom Search API and use the results to inform responses. Provides up-to-date information for current events and recent data. Returns search result information formatted as search result blocks, including links as markdown hyperlinks.\",\n");
        printf("  \"parameters\": {\n");
        printf("    \"type\": \"object\",\n");
        printf("    \"properties\": {\n");
        printf("      \"query\": {\n");
        printf("        \"type\": \"string\",\n");
        printf("        \"description\": \"The search query to use\",\n");
        printf("        \"minLength\": 2\n");
        printf("      },\n");
        printf("      \"num\": {\n");
        printf("        \"type\": \"integer\",\n");
        printf("        \"description\": \"Number of results to return (1-10)\",\n");
        printf("        \"minimum\": 1,\n");
        printf("        \"maximum\": 10,\n");
        printf("        \"default\": 10\n");
        printf("      },\n");
        printf("      \"start\": {\n");
        printf("        \"type\": \"integer\",\n");
        printf("        \"description\": \"Result index offset for pagination (1-based, max 91)\",\n");
        printf("        \"minimum\": 1,\n");
        printf("        \"maximum\": 91,\n");
        printf("        \"default\": 1\n");
        printf("      },\n");
        printf("      \"allowed_domains\": {\n");
        printf("        \"type\": \"array\",\n");
        printf("        \"items\": {\n");
        printf("          \"type\": \"string\"\n");
        printf("        },\n");
        printf("        \"description\": \"Only include search results from these domains\"\n");
        printf("      },\n");
        printf("      \"blocked_domains\": {\n");
        printf("        \"type\": \"array\",\n");
        printf("        \"items\": {\n");
        printf("          \"type\": \"string\"\n");
        printf("        },\n");
        printf("        \"description\": \"Never include search results from these domains\"\n");
        printf("      }\n");
        printf("    },\n");
        printf("    \"required\": [\"query\"]\n");
        printf("  }\n");
        printf("}\n");
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
        fprintf(stderr, "web_search_google: empty input\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(input, total_read, 0, &allocator, NULL);
    if (doc == NULL) {
        fprintf(stderr, "web_search_google: invalid JSON\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *query_val = yyjson_obj_get(root, "query");
    if (query_val == NULL || !yyjson_is_str(query_val)) {
        fprintf(stderr, "web_search_google: missing or invalid query field\n");
        talloc_free(ctx);
        return 1;
    }

    const char *query = yyjson_get_str(query_val);

    int64_t num = 10;
    yyjson_val *num_val = yyjson_obj_get(root, "num");
    if (num_val != NULL && yyjson_is_int(num_val)) {
        num = yyjson_get_int(num_val);
    }

    int64_t start = 1;
    yyjson_val *start_val = yyjson_obj_get(root, "start");
    if (start_val != NULL && yyjson_is_int(start_val)) {
        start = yyjson_get_int(start_val);
    }

    yyjson_val *allowed_domains_val = yyjson_obj_get(root, "allowed_domains");
    yyjson_val *blocked_domains_val = yyjson_obj_get(root, "blocked_domains");

    size_t allowed_count = 0;
    size_t blocked_count = 0;
    if (allowed_domains_val != NULL && yyjson_is_arr(allowed_domains_val)) {
        allowed_count = yyjson_arr_size(allowed_domains_val);
    }
    if (blocked_domains_val != NULL && yyjson_is_arr(blocked_domains_val)) {
        blocked_count = yyjson_arr_size(blocked_domains_val);
    }

    char *api_key = NULL;
    char *engine_id = NULL;
    if (load_credentials(ctx, &api_key, &engine_id) != 0) {
        const char *error_msg = "Web search requires API key configuration.\n\nGoogle Custom Search offers 100 free searches/day.\nGet API key: https://developers.google.com/custom-search/v1/overview\nGet Search Engine ID: https://programmablesearchengine.google.com/controlpanel/create\nAdd to: ~/.config/ikigai/credentials.json as 'web_search.google.api_key' and 'web_search.google.engine_id'";
        output_error_with_event(ctx, error_msg, "AUTH_MISSING");
        talloc_free(ctx);
        return 0;
    }

    char *encoded_query = url_encode(ctx, query);
    if (encoded_query == NULL) {
        output_error(ctx, "Failed to encode query", "NETWORK_ERROR");
        talloc_free(ctx);
        return 0;
    }

    char *encoded_api_key = url_encode(ctx, api_key);
    if (encoded_api_key == NULL) {
        output_error(ctx, "Failed to encode API key", "NETWORK_ERROR");
        talloc_free(ctx);
        return 0;
    }

    char *encoded_engine_id = url_encode(ctx, engine_id);
    if (encoded_engine_id == NULL) {
        output_error(ctx, "Failed to encode engine ID", "NETWORK_ERROR");
        talloc_free(ctx);
        return 0;
    }

    struct api_call *calls = NULL;
    size_t num_calls = 0;

    if (allowed_count > 1) {
        num_calls = allowed_count;
        calls = talloc_array(ctx, struct api_call, (unsigned int)num_calls);
        if (calls == NULL) {
            talloc_free(ctx);
            return 1;
        }

        int64_t per_domain = num / (int64_t)allowed_count;
        int64_t remainder = num % (int64_t)allowed_count;

        for (size_t i = 0; i < allowed_count; i++) {
            yyjson_val *domain_val = yyjson_arr_get(allowed_domains_val, i);
            if (domain_val == NULL || !yyjson_is_str(domain_val)) {
                continue;
            }

            const char *domain = yyjson_get_str(domain_val);
            int64_t num_for_domain = per_domain + ((int64_t)i < remainder ? 1 : 0);

            if (num_for_domain == 0) {
                continue;
            }

            char *encoded_domain = url_encode(ctx, domain);
            if (encoded_domain == NULL) {
                continue;
            }

            char *url = talloc_asprintf(ctx, "https://customsearch.googleapis.com/customsearch/v1?key=%s&cx=%s&q=%s&num=%" PRId64 "&start=%" PRId64 "&siteSearch=%s&siteSearchFilter=i", encoded_api_key, encoded_engine_id, encoded_query, num_for_domain, start, encoded_domain);
            if (url == NULL) {
                continue;
            }

            calls[i].domain = talloc_strdup(ctx, domain);
            calls[i].num_for_domain = num_for_domain;
            calls[i].url = url;
            calls[i].success = false;
            calls[i].response.ctx = ctx;
            calls[i].response.data = talloc_array(ctx, char, 1);
            if (calls[i].response.data == NULL) {
                continue;
            }
            calls[i].response.data[0] = '\0';
            calls[i].response.size = 0;

            calls[i].handle = curl_easy_init();
            if (calls[i].handle == NULL) {
                continue;
            }

            curl_easy_setopt(calls[i].handle, CURLOPT_URL, url);
            curl_easy_setopt(calls[i].handle, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt(calls[i].handle, CURLOPT_WRITEDATA, (void *)&calls[i].response);
            curl_easy_setopt(calls[i].handle, CURLOPT_TIMEOUT, 30L);
        }

        CURLM *multi_handle = curl_multi_init();
        if (multi_handle == NULL) {
            for (size_t i = 0; i < num_calls; i++) {
                if (calls[i].handle != NULL) {
                    curl_easy_cleanup(calls[i].handle);
                }
            }
            output_error(ctx, "Failed to initialize HTTP client", "NETWORK_ERROR");
            talloc_free(ctx);
            return 0;
        }

        for (size_t i = 0; i < num_calls; i++) {
            if (calls[i].handle != NULL) {
                curl_multi_add_handle(multi_handle, calls[i].handle);
            }
        }

        int32_t still_running = 0;
        curl_multi_perform(multi_handle, &still_running);

        while (still_running > 0) {
            int32_t numfds = 0;
            CURLMcode mc = curl_multi_wait(multi_handle, NULL, 0, 1000, &numfds);
            if (mc != CURLM_OK) {
                break;
            }
            curl_multi_perform(multi_handle, &still_running);
        }

        for (size_t i = 0; i < num_calls; i++) {
            if (calls[i].handle != NULL) {
                int64_t http_code = 0;
                curl_easy_getinfo(calls[i].handle, CURLINFO_RESPONSE_CODE, &http_code);
                if (http_code == 200) {
                    calls[i].success = true;
                }
                curl_multi_remove_handle(multi_handle, calls[i].handle);
                curl_easy_cleanup(calls[i].handle);
                calls[i].handle = NULL;
            }
        }

        curl_multi_cleanup(multi_handle);
    } else {
        num_calls = 1;
        calls = talloc_array(ctx, struct api_call, 1);
        if (calls == NULL) {
            talloc_free(ctx);
            return 1;
        }

        char *url = NULL;
        if (allowed_count == 1) {
            yyjson_val *domain_val = yyjson_arr_get(allowed_domains_val, 0);
            if (domain_val != NULL && yyjson_is_str(domain_val)) {
                const char *domain = yyjson_get_str(domain_val);
                char *encoded_domain = url_encode(ctx, domain);
                if (encoded_domain != NULL) {
                    url = talloc_asprintf(ctx, "https://customsearch.googleapis.com/customsearch/v1?key=%s&cx=%s&q=%s&num=%" PRId64 "&start=%" PRId64 "&siteSearch=%s&siteSearchFilter=i", encoded_api_key, encoded_engine_id, encoded_query, num, start, encoded_domain);
                }
            }
        } else if (blocked_count == 1) {
            yyjson_val *domain_val = yyjson_arr_get(blocked_domains_val, 0);
            if (domain_val != NULL && yyjson_is_str(domain_val)) {
                const char *domain = yyjson_get_str(domain_val);
                char *encoded_domain = url_encode(ctx, domain);
                if (encoded_domain != NULL) {
                    url = talloc_asprintf(ctx, "https://customsearch.googleapis.com/customsearch/v1?key=%s&cx=%s&q=%s&num=%" PRId64 "&start=%" PRId64 "&siteSearch=%s&siteSearchFilter=e", encoded_api_key, encoded_engine_id, encoded_query, num, start, encoded_domain);
                }
            }
        } else {
            url = talloc_asprintf(ctx, "https://customsearch.googleapis.com/customsearch/v1?key=%s&cx=%s&q=%s&num=%" PRId64 "&start=%" PRId64, encoded_api_key, encoded_engine_id, encoded_query, num, start);
        }

        if (url == NULL) {
            talloc_free(ctx);
            return 1;
        }

        calls[0].url = url;
        calls[0].success = false;
        calls[0].response.ctx = ctx;
        calls[0].response.data = talloc_array(ctx, char, 1);
        if (calls[0].response.data == NULL) {
            talloc_free(ctx);
            return 1;
        }
        calls[0].response.data[0] = '\0';
        calls[0].response.size = 0;

        CURL *curl = curl_easy_init();
        if (curl == NULL) {
            output_error(ctx, "Failed to initialize HTTP client", "NETWORK_ERROR");
            talloc_free(ctx);
            return 0;
        }

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&calls[0].response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

        fprintf(stderr, "DEBUG: Request URL: %s\n", url);

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            curl_easy_cleanup(curl);
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "HTTP request failed: %s", curl_easy_strerror(res));
            output_error(ctx, error_msg, "NETWORK_ERROR");
            talloc_free(ctx);
            return 0;
        }

        int64_t http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);

        if (http_code != 200) {
            yyjson_alc resp_allocator = ik_make_talloc_allocator(ctx);
            yyjson_doc *resp_doc = yyjson_read_opts(calls[0].response.data, calls[0].response.size, 0, &resp_allocator, NULL);
            if (resp_doc != NULL) {
                yyjson_val *resp_root = yyjson_doc_get_root(resp_doc);
                yyjson_val *error_obj = yyjson_obj_get(resp_root, "error");
                if (error_obj != NULL) {
                    yyjson_val *message_val = yyjson_obj_get(error_obj, "message");
                    const char *api_message = NULL;
                    if (message_val != NULL && yyjson_is_str(message_val)) {
                        api_message = yyjson_get_str(message_val);
                    }

                    yyjson_val *errors_arr = yyjson_obj_get(error_obj, "errors");
                    if (errors_arr != NULL && yyjson_is_arr(errors_arr)) {
                        yyjson_val *first_error = yyjson_arr_get_first(errors_arr);
                        if (first_error != NULL) {
                            yyjson_val *reason_val = yyjson_obj_get(first_error, "reason");
                            if (reason_val != NULL && yyjson_is_str(reason_val)) {
                                const char *reason = yyjson_get_str(reason_val);
                                if (strcmp(reason, "dailyLimitExceeded") == 0 || strcmp(reason, "quotaExceeded") == 0) {
                                    output_error(ctx, "Rate limit exceeded. You've used your free search quota (100/day).", "RATE_LIMIT");
                                    talloc_free(ctx);
                                    return 0;
                                }
                            }

                            if (api_message == NULL) {
                                yyjson_val *msg_val = yyjson_obj_get(first_error, "message");
                                if (msg_val != NULL && yyjson_is_str(msg_val)) {
                                    api_message = yyjson_get_str(msg_val);
                                }
                            }
                        }
                    }

                    if (api_message != NULL) {
                        char error_msg[512];
                        snprintf(error_msg, sizeof(error_msg), "API error (HTTP %ld): %s", (long)http_code, api_message);
                        output_error(ctx, error_msg, "API_ERROR");
                        talloc_free(ctx);
                        return 0;
                    }
                }
            }

            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "API request failed with HTTP %ld", (long)http_code);
            output_error(ctx, error_msg, "API_ERROR");
            talloc_free(ctx);
            return 0;
        }

        calls[0].success = true;
    }

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

    yyjson_mut_val *success_val = yyjson_mut_bool(output_doc, true);
    if (success_val == NULL) {
        talloc_free(ctx);
        return 1;
    }

    yyjson_mut_val *results_arr = yyjson_mut_arr(output_doc);
    if (results_arr == NULL) {
        talloc_free(ctx);
        return 1;
    }

    if (allowed_count > 1) {
        yyjson_val **items_arrays = talloc_array(ctx, yyjson_val *, (unsigned int)num_calls);
        if (items_arrays == NULL) {
            talloc_free(ctx);
            return 1;
        }

        size_t *indices = talloc_zero_array(ctx, size_t, (unsigned int)num_calls);
        if (indices == NULL) {
            talloc_free(ctx);
            return 1;
        }

        size_t *sizes = talloc_zero_array(ctx, size_t, (unsigned int)num_calls);
        if (sizes == NULL) {
            talloc_free(ctx);
            return 1;
        }

        for (size_t i = 0; i < num_calls; i++) {
            items_arrays[i] = NULL;
            if (calls[i].success) {
                yyjson_alc resp_allocator = ik_make_talloc_allocator(ctx);
                yyjson_doc *resp_doc = yyjson_read_opts(calls[i].response.data, calls[i].response.size, 0, &resp_allocator, NULL);
                if (resp_doc != NULL) {
                    yyjson_val *resp_root = yyjson_doc_get_root(resp_doc);
                    yyjson_val *items = yyjson_obj_get(resp_root, "items");
                    if (items != NULL && yyjson_is_arr(items)) {
                        items_arrays[i] = items;
                        sizes[i] = yyjson_arr_size(items);
                    }
                }
            }
        }

        int64_t count = 0;
        bool has_more = true;
        while (has_more && count < num) {
            has_more = false;
            for (size_t i = 0; i < num_calls; i++) {
                if (items_arrays[i] != NULL && indices[i] < sizes[i]) {
                    yyjson_val *item = yyjson_arr_get(items_arrays[i], indices[i]);
                    indices[i]++;

                    if (item != NULL) {
                        yyjson_val *title_val = yyjson_obj_get(item, "title");
                        yyjson_val *link_val = yyjson_obj_get(item, "link");
                        yyjson_val *snippet_val = yyjson_obj_get(item, "snippet");

                        if (title_val != NULL && yyjson_is_str(title_val) && link_val != NULL && yyjson_is_str(link_val)) {
                            const char *title = yyjson_get_str(title_val);
                            const char *link = yyjson_get_str(link_val);

                            if (url_already_seen(results_arr, link)) {
                                has_more = true;
                                continue;
                            }

                            const char *snippet = "";
                            if (snippet_val != NULL && yyjson_is_str(snippet_val)) {
                                snippet = yyjson_get_str(snippet_val);
                            }

                            yyjson_mut_val *result_item = yyjson_mut_obj(output_doc);
                            if (result_item == NULL) {
                                talloc_free(ctx);
                                return 1;
                            }

                            yyjson_mut_val *title_mut = yyjson_mut_str(output_doc, title);
                            if (title_mut == NULL) {
                                talloc_free(ctx);
                                return 1;
                            }

                            yyjson_mut_val *url_mut = yyjson_mut_str(output_doc, link);
                            if (url_mut == NULL) {
                                talloc_free(ctx);
                                return 1;
                            }

                            yyjson_mut_val *snippet_mut = yyjson_mut_str(output_doc, snippet);
                            if (snippet_mut == NULL) {
                                talloc_free(ctx);
                                return 1;
                            }

                            yyjson_mut_obj_add_val(output_doc, result_item, "title", title_mut);
                            yyjson_mut_obj_add_val(output_doc, result_item, "url", url_mut);
                            yyjson_mut_obj_add_val(output_doc, result_item, "snippet", snippet_mut);

                            yyjson_mut_arr_append(results_arr, result_item);
                            count++;

                            if (count >= num) {
                                break;
                            }
                        }
                    }

                    has_more = true;
                }
            }
        }
    } else {
        if (calls[0].success) {
            yyjson_alc resp_allocator = ik_make_talloc_allocator(ctx);
            yyjson_doc *resp_doc = yyjson_read_opts(calls[0].response.data, calls[0].response.size, 0, &resp_allocator, NULL);
            if (resp_doc == NULL) {
                output_error(ctx, "Failed to parse API response", "API_ERROR");
                talloc_free(ctx);
                return 0;
            }

            yyjson_val *resp_root = yyjson_doc_get_root(resp_doc);
            yyjson_val *items = yyjson_obj_get(resp_root, "items");

            if (items != NULL && yyjson_is_arr(items)) {
                size_t idx, max;
                yyjson_val *item;
                yyjson_arr_foreach(items, idx, max, item) {
                    yyjson_val *title_val = yyjson_obj_get(item, "title");
                    yyjson_val *link_val = yyjson_obj_get(item, "link");
                    yyjson_val *snippet_val = yyjson_obj_get(item, "snippet");

                    if (title_val != NULL && yyjson_is_str(title_val) && link_val != NULL && yyjson_is_str(link_val)) {
                        const char *title = yyjson_get_str(title_val);
                        const char *link = yyjson_get_str(link_val);
                        const char *snippet = "";
                        if (snippet_val != NULL && yyjson_is_str(snippet_val)) {
                            snippet = yyjson_get_str(snippet_val);
                        }

                        if (blocked_count > 1) {
                            bool blocked = false;
                            for (size_t i = 0; i < blocked_count; i++) {
                                yyjson_val *blocked_domain_val = yyjson_arr_get(blocked_domains_val, i);
                                if (blocked_domain_val != NULL && yyjson_is_str(blocked_domain_val)) {
                                    const char *blocked_domain = yyjson_get_str(blocked_domain_val);
                                    if (strstr(link, blocked_domain) != NULL) {
                                        blocked = true;
                                        break;
                                    }
                                }
                            }
                            if (blocked) {
                                continue;
                            }
                        }

                        yyjson_mut_val *result_item = yyjson_mut_obj(output_doc);
                        if (result_item == NULL) {
                            talloc_free(ctx);
                            return 1;
                        }

                        yyjson_mut_val *title_mut = yyjson_mut_str(output_doc, title);
                        if (title_mut == NULL) {
                            talloc_free(ctx);
                            return 1;
                        }

                        yyjson_mut_val *url_mut = yyjson_mut_str(output_doc, link);
                        if (url_mut == NULL) {
                            talloc_free(ctx);
                            return 1;
                        }

                        yyjson_mut_val *snippet_mut = yyjson_mut_str(output_doc, snippet);
                        if (snippet_mut == NULL) {
                            talloc_free(ctx);
                            return 1;
                        }

                        yyjson_mut_obj_add_val(output_doc, result_item, "title", title_mut);
                        yyjson_mut_obj_add_val(output_doc, result_item, "url", url_mut);
                        yyjson_mut_obj_add_val(output_doc, result_item, "snippet", snippet_mut);

                        yyjson_mut_arr_append(results_arr, result_item);
                    }
                }
            }
        }
    }

    int64_t count = (int64_t)yyjson_mut_arr_size(results_arr);
    yyjson_mut_val *count_val = yyjson_mut_int(output_doc, count);
    if (count_val == NULL) {
        talloc_free(ctx);
        return 1;
    }

    yyjson_mut_obj_add_val(output_doc, result_obj, "success", success_val);
    yyjson_mut_obj_add_val(output_doc, result_obj, "results", results_arr);
    yyjson_mut_obj_add_val(output_doc, result_obj, "count", count_val);

    yyjson_mut_doc_set_root(output_doc, result_obj);

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
