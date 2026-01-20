#include "web_search_google.h"

#include "../../credentials.h"
#include "../../error.h"
#include "error_output.h"
#include "http_utils.h"
#include "json_allocator.h"
#include "panic.h"
#include "response_processor.h"
#include "result_utils.h"
#include "wrapper_web.h"

#include "vendor/yyjson/yyjson.h"

#include <curl/curl.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

int32_t web_search_google_execute(void *ctx, const web_search_google_params_t *params)
{
    ik_credentials_t *creds = NULL;
    res_t load_res = ik_credentials_load(ctx, NULL, &creds);
    if (is_err(&load_res)) {
        const char *error_msg =
            "Web search requires API key configuration.\n\nGoogle Custom Search offers 100 free searches/day.\nGet API key: https://developers.google.com/custom-search/v1/overview\nGet Search Engine ID: https://programmablesearchengine.google.com/controlpanel/create\nAdd to: ~/.config/ikigai/credentials.json as 'GOOGLE_SEARCH_API_KEY' and 'GOOGLE_SEARCH_ENGINE_ID'";
        output_error_with_event(ctx, error_msg, "AUTH_MISSING");
        return 0;
    }

    const char *api_key = ik_credentials_get(creds, "GOOGLE_SEARCH_API_KEY");
    const char *engine_id = ik_credentials_get(creds, "GOOGLE_SEARCH_ENGINE_ID");
    if (api_key == NULL || engine_id == NULL) {
        const char *error_msg =
            "Web search requires API key configuration.\n\nGoogle Custom Search offers 100 free searches/day.\nGet API key: https://developers.google.com/custom-search/v1/overview\nGet Search Engine ID: https://programmablesearchengine.google.com/controlpanel/create\nAdd to: ~/.config/ikigai/credentials.json as 'GOOGLE_SEARCH_API_KEY' and 'GOOGLE_SEARCH_ENGINE_ID'";
        output_error_with_event(ctx, error_msg, "AUTH_MISSING");
        return 0;
    }

    char *encoded_query = url_encode(ctx, params->query);
    if (encoded_query == NULL) {  // LCOV_EXCL_LINE
        output_error(ctx, "Failed to encode query", "NETWORK_ERROR");  // LCOV_EXCL_LINE
        return 0;  // LCOV_EXCL_LINE
    }

    char *encoded_api_key = url_encode(ctx, api_key);
    if (encoded_api_key == NULL) {  // LCOV_EXCL_LINE
        output_error(ctx, "Failed to encode API key", "NETWORK_ERROR");  // LCOV_EXCL_LINE
        return 0;  // LCOV_EXCL_LINE
    }

    char *encoded_engine_id = url_encode(ctx, engine_id);
    if (encoded_engine_id == NULL) {  // LCOV_EXCL_LINE
        output_error(ctx, "Failed to encode engine ID", "NETWORK_ERROR");  // LCOV_EXCL_LINE
        return 0;  // LCOV_EXCL_LINE
    }

    struct api_call *calls = NULL;
    size_t num_calls = 0;

    if (params->allowed_count > 1) {
        num_calls = params->allowed_count;
        calls = talloc_zero_array(ctx, struct api_call, (unsigned int)num_calls);
        if (calls == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        int64_t per_domain = params->num / (int64_t)params->allowed_count;
        int64_t remainder = params->num % (int64_t)params->allowed_count;

        for (size_t i = 0; i < params->allowed_count; i++) {  // LCOV_EXCL_BR_LINE
            yyjson_val *domain_val = yyjson_arr_get(params->allowed_domains_val, i);  // LCOV_EXCL_BR_LINE
            if (domain_val == NULL || !yyjson_is_str(domain_val)) {  // LCOV_EXCL_BR_LINE
                continue;
            }

            const char *domain = yyjson_get_str(domain_val);
            int64_t num_for_domain = per_domain + ((int64_t)i < remainder ? 1 : 0);

            if (num_for_domain == 0) {
                continue;
            }

            char *encoded_domain = url_encode(ctx, domain);
            if (encoded_domain == NULL) {  // LCOV_EXCL_LINE
                continue;  // LCOV_EXCL_LINE
            }

            char *url = talloc_asprintf(ctx,
                                        "https://customsearch.googleapis.com/customsearch/v1?key=%s&cx=%s&q=%s&num=%"
                                        PRId64 "&start=%" PRId64 "&siteSearch=%s&siteSearchFilter=i",
                                        encoded_api_key,
                                        encoded_engine_id,
                                        encoded_query,
                                        num_for_domain,
                                        params->start,
                                        encoded_domain);
            if (url == NULL) {  // LCOV_EXCL_BR_LINE
                continue;  // LCOV_EXCL_LINE
            }  // LCOV_EXCL_LINE

            calls[i].domain = talloc_strdup(ctx, domain);
            calls[i].num_for_domain = num_for_domain;
            calls[i].url = url;
            calls[i].success = false;
            calls[i].response.ctx = ctx;
            calls[i].response.data = talloc_array(ctx, char, 1);
            if (calls[i].response.data == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            calls[i].response.data[0] = '\0';
            calls[i].response.size = 0;

            calls[i].handle = curl_easy_init_();
            if (calls[i].handle == NULL) {
                continue;
            }

            curl_easy_setopt_(calls[i].handle, CURLOPT_URL, url);
            curl_easy_setopt_(calls[i].handle, CURLOPT_WRITEFUNCTION, write_callback);
            curl_easy_setopt_(calls[i].handle, CURLOPT_WRITEDATA, (void *)&calls[i].response);
            curl_easy_setopt_(calls[i].handle, CURLOPT_TIMEOUT, 30L);
        }

        CURLM *multi_handle = curl_multi_init_();
        if (multi_handle == NULL) {
            for (size_t i = 0; i < num_calls; i++) {  // LCOV_EXCL_BR_LINE
                if (calls[i].handle != NULL) {  // LCOV_EXCL_BR_LINE
                    curl_easy_cleanup_(calls[i].handle);
                }
            }
            output_error(ctx, "Failed to initialize HTTP client", "NETWORK_ERROR");
            return 0;
        }

        for (size_t i = 0; i < num_calls; i++) {
            if (calls[i].handle != NULL) {
                curl_multi_add_handle_(multi_handle, calls[i].handle);
            }
        }

        int32_t still_running = 0;
        curl_multi_perform_(multi_handle, &still_running);

        /* LCOV_EXCL_START */
        while (still_running > 0) {
            int32_t numfds = 0;
            CURLMcode mc = curl_multi_wait_(multi_handle, NULL, 0, 1000, &numfds);
            if (mc != CURLM_OK) {
                break;
            }
            curl_multi_perform_(multi_handle, &still_running);
        }
        /* LCOV_EXCL_STOP */

        for (size_t i = 0; i < num_calls; i++) {  // LCOV_EXCL_BR_LINE
            if (calls[i].handle != NULL) {  // LCOV_EXCL_BR_LINE
                int64_t http_code = 0;
                curl_easy_getinfo_(calls[i].handle, CURLINFO_RESPONSE_CODE, &http_code);
                if (http_code == 200) {  // LCOV_EXCL_BR_LINE
                    calls[i].success = true;
                }
                curl_multi_remove_handle_(multi_handle, calls[i].handle);
                curl_easy_cleanup(calls[i].handle);
                calls[i].handle = NULL;
            }
        }

        curl_multi_cleanup_(multi_handle);
    } else {
        num_calls = 1;
        calls = talloc_array(ctx, struct api_call, 1);
        if (calls == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        char *url = NULL;
        if (params->allowed_count == 1) {
            yyjson_val *domain_val = yyjson_arr_get(params->allowed_domains_val, 0);  // LCOV_EXCL_BR_LINE
            if (domain_val != NULL && yyjson_is_str(domain_val)) {  // LCOV_EXCL_BR_LINE
                const char *domain = yyjson_get_str(domain_val);
                char *encoded_domain = url_encode(ctx, domain);
                if (encoded_domain != NULL) {  // LCOV_EXCL_BR_LINE
                    url = talloc_asprintf(ctx,
                                          "https://customsearch.googleapis.com/customsearch/v1?key=%s&cx=%s&q=%s&num=%"
                                          PRId64 "&start=%" PRId64 "&siteSearch=%s&siteSearchFilter=i",
                                          encoded_api_key,
                                          encoded_engine_id,
                                          encoded_query,
                                          params->num,
                                          params->start,
                                          encoded_domain);
                }
            }
        } else if (params->blocked_count == 1) {
            yyjson_val *domain_val = yyjson_arr_get(params->blocked_domains_val, 0);  // LCOV_EXCL_BR_LINE
            if (domain_val != NULL && yyjson_is_str(domain_val)) {  // LCOV_EXCL_BR_LINE
                const char *domain = yyjson_get_str(domain_val);
                char *encoded_domain = url_encode(ctx, domain);
                if (encoded_domain != NULL) {  // LCOV_EXCL_BR_LINE
                    url = talloc_asprintf(ctx,
                                          "https://customsearch.googleapis.com/customsearch/v1?key=%s&cx=%s&q=%s&num=%"
                                          PRId64 "&start=%" PRId64 "&siteSearch=%s&siteSearchFilter=e",
                                          encoded_api_key,
                                          encoded_engine_id,
                                          encoded_query,
                                          params->num,
                                          params->start,
                                          encoded_domain);
                }
            }
        } else {
            url = talloc_asprintf(ctx,
                                  "https://customsearch.googleapis.com/customsearch/v1?key=%s&cx=%s&q=%s&num=%" PRId64
                                  "&start=%" PRId64,
                                  encoded_api_key,
                                  encoded_engine_id,
                                  encoded_query,
                                  params->num,
                                  params->start);
        }

        if (url == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        calls[0].url = url;
        calls[0].success = false;
        calls[0].response.ctx = ctx;
        calls[0].response.data = talloc_array(ctx, char, 1);
        if (calls[0].response.data == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        calls[0].response.data[0] = '\0';
        calls[0].response.size = 0;

        CURL *curl = curl_easy_init_();
        if (curl == NULL) {
            output_error(ctx, "Failed to initialize HTTP client", "NETWORK_ERROR");
            return 0;
        }

        curl_easy_setopt_(curl, CURLOPT_URL, url);
        curl_easy_setopt_(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt_(curl, CURLOPT_WRITEDATA, (void *)&calls[0].response);
        curl_easy_setopt_(curl, CURLOPT_TIMEOUT, 30L);

        CURLcode res = curl_easy_perform_(curl);

        if (res != CURLE_OK) {
            curl_easy_cleanup_(curl);
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "HTTP request failed: %s", curl_easy_strerror_(res));
            output_error(ctx, error_msg, "NETWORK_ERROR");
            return 0;
        }

        int64_t http_code = 0;
        curl_easy_getinfo_(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup_(curl);

        if (http_code != 200) {
            yyjson_alc resp_allocator = ik_make_talloc_allocator(ctx);
            yyjson_doc *resp_doc = yyjson_read_opts(calls[0].response.data,
                                                    calls[0].response.size,
                                                    0,
                                                    &resp_allocator,
                                                    NULL);
            if (resp_doc != NULL) {
                yyjson_val *resp_root = yyjson_doc_get_root(resp_doc);  // LCOV_EXCL_BR_LINE
                yyjson_val *error_obj = yyjson_obj_get(resp_root, "error");
                if (error_obj != NULL) {
                    yyjson_val *message_val = yyjson_obj_get(error_obj, "message");
                    const char *api_message = NULL;
                    if (message_val != NULL && yyjson_is_str(message_val)) {  // LCOV_EXCL_BR_LINE
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
                                    output_error(ctx,
                                                 "Rate limit exceeded. You've used your free search quota (100/day).",
                                                 "RATE_LIMIT");
                                    return 0;
                                }
                            }

                            if (api_message == NULL) {
                                yyjson_val *msg_val = yyjson_obj_get(first_error, "message");
                                if (msg_val != NULL && yyjson_is_str(msg_val)) {  // LCOV_EXCL_BR_LINE
                                    api_message = yyjson_get_str(msg_val);
                                }
                            }
                        }
                    }

                    if (api_message != NULL) {  // LCOV_EXCL_BR_LINE
                        char error_msg[512];
                        snprintf(error_msg, sizeof(error_msg), "API error (HTTP %ld): %s", (long)http_code,
                                 api_message);
                        output_error(ctx, error_msg, "API_ERROR");
                        return 0;
                    }
                }
            }

            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), "API request failed with HTTP %ld", (long)http_code);
            output_error(ctx, error_msg, "API_ERROR");
            return 0;
        }

        calls[0].success = true;
    }

    char *json_str = process_responses(ctx,
                                       calls,
                                       num_calls,
                                       params->allowed_count,
                                       params->blocked_count,
                                       params->blocked_domains_val,
                                       params->num);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    printf("%s\n", json_str);

    return 0;
}
