#include "response_processor.h"

#include "json_allocator.h"
#include "panic.h"
#include "result_utils.h"

#include "vendor/yyjson/yyjson.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

char *process_responses(void *ctx, struct api_call *calls, size_t num_calls, size_t allowed_count,
                        size_t blocked_count, yyjson_val *blocked_domains_val, int64_t num)
{
    yyjson_alc output_allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *output_doc = yyjson_mut_doc_new(&output_allocator);
    if (output_doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *result_obj = yyjson_mut_obj(output_doc);
    if (result_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *success_val = yyjson_mut_bool(output_doc, true);
    if (success_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *results_arr = yyjson_mut_arr(output_doc);
    if (results_arr == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    if (allowed_count > 1) {
        yyjson_val **items_arrays = talloc_array(ctx, yyjson_val *, (unsigned int)num_calls);
        if (items_arrays == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        size_t *indices = talloc_zero_array(ctx, size_t, (unsigned int)num_calls);
        if (indices == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        size_t *sizes = talloc_zero_array(ctx, size_t, (unsigned int)num_calls);
        if (sizes == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        for (size_t i = 0; i < num_calls; i++) {
            items_arrays[i] = NULL;
            if (calls[i].success) {
                yyjson_alc resp_allocator = ik_make_talloc_allocator(ctx);
                yyjson_doc *resp_doc = yyjson_read_opts(calls[i].response.data,
                                                        calls[i].response.size,
                                                        0,
                                                        &resp_allocator,
                                                        NULL);
                if (resp_doc != NULL) {
                    yyjson_val *resp_root = yyjson_doc_get_root(resp_doc);  // LCOV_EXCL_BR_LINE
                    yyjson_val *items = yyjson_obj_get(resp_root, "items");  // LCOV_EXCL_BR_LINE
                    if (items != NULL && yyjson_is_arr(items)) {
                        items_arrays[i] = items;
                        sizes[i] = yyjson_arr_size(items);  // LCOV_EXCL_BR_LINE
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
                    yyjson_val *item = yyjson_arr_get(items_arrays[i], indices[i]);  // LCOV_EXCL_BR_LINE
                    indices[i]++;

                    if (item != NULL) {  // LCOV_EXCL_BR_LINE
                        yyjson_val *title_val = yyjson_obj_get(item, "title");  // LCOV_EXCL_BR_LINE
                        yyjson_val *link_val = yyjson_obj_get(item, "link");  // LCOV_EXCL_BR_LINE
                        yyjson_val *snippet_val = yyjson_obj_get(item, "snippet");

                        if (title_val != NULL && yyjson_is_str(title_val) && link_val != NULL &&
                            yyjson_is_str(link_val)) {
                            const char *title = yyjson_get_str(title_val);  // LCOV_EXCL_BR_LINE
                            const char *link = yyjson_get_str(link_val);  // LCOV_EXCL_BR_LINE

                            if (url_already_seen(results_arr, link)) {
                                has_more = true;
                                continue;
                            }

                            const char *snippet = "";
                            if (snippet_val != NULL && yyjson_is_str(snippet_val)) {
                                snippet = yyjson_get_str(snippet_val);
                            }

                            yyjson_mut_val *result_item = yyjson_mut_obj(output_doc);
                            if (result_item == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

                            yyjson_mut_val *title_mut = yyjson_mut_str(output_doc, title);
                            if (title_mut == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

                            yyjson_mut_val *url_mut = yyjson_mut_str(output_doc, link);
                            if (url_mut == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

                            yyjson_mut_val *snippet_mut = yyjson_mut_str(output_doc, snippet);
                            if (snippet_mut == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

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
            yyjson_doc *resp_doc = yyjson_read_opts(calls[0].response.data,
                                                    calls[0].response.size,
                                                    0,
                                                    &resp_allocator,
                                                    NULL);
            if (resp_doc != NULL) {  // LCOV_EXCL_BR_LINE
                yyjson_val *resp_root = yyjson_doc_get_root(resp_doc);  // LCOV_EXCL_BR_LINE
                yyjson_val *items = yyjson_obj_get(resp_root, "items");  // LCOV_EXCL_BR_LINE

                if (items != NULL && yyjson_is_arr(items)) {  // LCOV_EXCL_BR_LINE
                    size_t idx, max;
                    yyjson_val *item;
                    yyjson_arr_foreach(items, idx, max, item) {  // LCOV_EXCL_BR_LINE
                        yyjson_val *title_val = yyjson_obj_get(item, "title");  // LCOV_EXCL_BR_LINE
                        yyjson_val *link_val = yyjson_obj_get(item, "link");  // LCOV_EXCL_BR_LINE
                        yyjson_val *snippet_val = yyjson_obj_get(item, "snippet");

                        if (title_val != NULL && yyjson_is_str(title_val) && link_val != NULL &&
                            yyjson_is_str(link_val)) {
                            const char *title = yyjson_get_str(title_val);  // LCOV_EXCL_BR_LINE
                            const char *link = yyjson_get_str(link_val);  // LCOV_EXCL_BR_LINE
                            const char *snippet = "";
                            if (snippet_val != NULL && yyjson_is_str(snippet_val)) {
                                snippet = yyjson_get_str(snippet_val);
                            }

                            if (blocked_count > 1) {
                                bool blocked = false;
                                for (size_t i = 0; i < blocked_count; i++) {
                                    yyjson_val *blocked_domain_val = yyjson_arr_get(blocked_domains_val, i);  // LCOV_EXCL_BR_LINE
                                    if (blocked_domain_val != NULL && yyjson_is_str(blocked_domain_val)) {  // LCOV_EXCL_BR_LINE
                                        const char *blocked_domain = yyjson_get_str(blocked_domain_val);  // LCOV_EXCL_BR_LINE
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
                            if (result_item == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

                            yyjson_mut_val *title_mut = yyjson_mut_str(output_doc, title);
                            if (title_mut == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

                            yyjson_mut_val *url_mut = yyjson_mut_str(output_doc, link);
                            if (url_mut == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

                            yyjson_mut_val *snippet_mut = yyjson_mut_str(output_doc, snippet);
                            if (snippet_mut == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

                            yyjson_mut_obj_add_val(output_doc, result_item, "title", title_mut);
                            yyjson_mut_obj_add_val(output_doc, result_item, "url", url_mut);
                            yyjson_mut_obj_add_val(output_doc, result_item, "snippet", snippet_mut);

                            yyjson_mut_arr_append(results_arr, result_item);
                        }
                    }
                }
            }
        }
    }

    int64_t count = (int64_t)yyjson_mut_arr_size(results_arr);  // LCOV_EXCL_BR_LINE
    yyjson_mut_val *count_val = yyjson_mut_int(output_doc, count);
    if (count_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_obj_add_val(output_doc, result_obj, "success", success_val);
    yyjson_mut_obj_add_val(output_doc, result_obj, "results", results_arr);
    yyjson_mut_obj_add_val(output_doc, result_obj, "count", count_val);

    yyjson_mut_doc_set_root(output_doc, result_obj);

    char *json_str = yyjson_mut_write_opts(output_doc, 0, &output_allocator, NULL, NULL);
    return json_str;
}
