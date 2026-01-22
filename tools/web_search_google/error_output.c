#include "error_output.h"

#include "json_allocator.h"
#include "panic.h"

#include "vendor/yyjson/yyjson.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *AUTH_MISSING_EVENT_CONTENT =
    "⚠ Configuration Required\n\nWeb search needs an API key and Search Engine ID. Google Custom Search offers 100 free searches/day.\n\nGet API key: https://developers.google.com/custom-search/v1/overview\nGet Search Engine ID: https://programmablesearchengine.google.com/controlpanel/create\nAdd to: ~/.config/ikigai/credentials.json\n\nExample:\n{\n  \"web_search\": {\n    \"google\": {\n      \"api_key\": \"your-api-key-here\",\n      \"engine_id\": \"your-search-engine-id\"\n    }\n  }\n}";

void output_error_with_event(void *ctx, const char *error, const char *error_code)
{
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *success_val = yyjson_mut_bool(doc, false);
    if (success_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *error_val = yyjson_mut_str(doc, error);
    if (error_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *error_code_val = yyjson_mut_str(doc, error_code);
    if (error_code_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_obj_add_val(doc, obj, "success", success_val);
    yyjson_mut_obj_add_val(doc, obj, "error", error_val);
    yyjson_mut_obj_add_val(doc, obj, "error_code", error_code_val);

    if (strcmp(error_code, "AUTH_MISSING") == 0) {
        yyjson_mut_val *event_obj = yyjson_mut_obj(doc);
        if (event_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *kind_val = yyjson_mut_str(doc, "config_required");
        if (kind_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        // LCOV_EXCL_BR_START
        yyjson_mut_val *content_val = yyjson_mut_str(doc, AUTH_MISSING_EVENT_CONTENT);
        if (content_val == NULL) PANIC("Out of memory");
        // LCOV_EXCL_BR_STOP

        yyjson_mut_val *data_obj = yyjson_mut_obj(doc);
        if (data_obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *tool_val = yyjson_mut_str(doc, "web_search_google");
        if (tool_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *creds_arr = yyjson_mut_arr(doc);
        if (creds_arr == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *cred1 = yyjson_mut_str(doc, "api_key");
        if (cred1 == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

        yyjson_mut_val *cred2 = yyjson_mut_str(doc, "engine_id");
        if (cred2 == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

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

    char *json_str = yyjson_mut_write_opts(doc, 0, &allocator, NULL, NULL);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    printf("%s\n", json_str);
}

void output_error(void *ctx, const char *error, const char *error_code)
{
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *success_val = yyjson_mut_bool(doc, false);
    if (success_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *error_val = yyjson_mut_str(doc, error);
    if (error_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *error_code_val = yyjson_mut_str(doc, error_code);
    if (error_code_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_obj_add_val(doc, obj, "success", success_val);
    yyjson_mut_obj_add_val(doc, obj, "error", error_val);
    yyjson_mut_obj_add_val(doc, obj, "error_code", error_code_val);

    yyjson_mut_doc_set_root(doc, obj);

    char *json_str = yyjson_mut_write_opts(doc, 0, &allocator, NULL, NULL);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    printf("%s\n", json_str);
}
