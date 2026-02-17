#include "tests/helpers/framebuffer_inspect_helper.h"

#include "vendor/yyjson/yyjson.h"

#include <string.h>

bool ik_fb_is_valid(const char *framebuffer_json)
{
    if (framebuffer_json == NULL) {
        return false;
    }

    yyjson_doc *doc = yyjson_read(framebuffer_json, strlen(framebuffer_json), 0);
    if (doc == NULL) {
        return false;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *type_val = yyjson_obj_get(root, "type");
    const char *type = yyjson_get_str(type_val);

    bool valid = (type != NULL && strcmp(type, "framebuffer") == 0);

    yyjson_val *lines = yyjson_obj_get(root, "lines");
    valid = valid && yyjson_is_arr(lines);

    yyjson_doc_free(doc);
    return valid;
}

bool ik_fb_contains_text(const char *framebuffer_json, const char *text)
{
    if (framebuffer_json == NULL || text == NULL) {
        return false;
    }

    yyjson_doc *doc = yyjson_read(framebuffer_json, strlen(framebuffer_json), 0);
    if (doc == NULL) {
        return false;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *lines = yyjson_obj_get(root, "lines");
    if (!yyjson_is_arr(lines)) {
        yyjson_doc_free(doc);
        return false;
    }

    size_t idx;
    size_t max;
    yyjson_val *line;
    yyjson_arr_foreach(lines, idx, max, line) {
        yyjson_val *spans = yyjson_obj_get(line, "spans");
        if (!yyjson_is_arr(spans)) {
            continue;
        }

        size_t sidx;
        size_t smax;
        yyjson_val *span;
        yyjson_arr_foreach(spans, sidx, smax, span) {
            yyjson_val *text_val = yyjson_obj_get(span, "text");
            const char *span_text = yyjson_get_str(text_val);
            if (span_text != NULL && strstr(span_text, text) != NULL) {
                yyjson_doc_free(doc);
                return true;
            }
        }
    }

    yyjson_doc_free(doc);
    return false;
}

char *ik_fb_get_row_text(TALLOC_CTX *ctx, const char *framebuffer_json, int row)
{
    if (framebuffer_json == NULL || ctx == NULL) {
        return NULL;
    }

    yyjson_doc *doc = yyjson_read(framebuffer_json, strlen(framebuffer_json), 0);
    if (doc == NULL) {
        return NULL;
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    yyjson_val *lines = yyjson_obj_get(root, "lines");
    if (!yyjson_is_arr(lines)) {
        yyjson_doc_free(doc);
        return NULL;
    }

    yyjson_val *line = yyjson_arr_get(lines, (size_t)row);
    if (line == NULL) {
        yyjson_doc_free(doc);
        return NULL;
    }

    yyjson_val *spans = yyjson_obj_get(line, "spans");
    if (!yyjson_is_arr(spans)) {
        yyjson_doc_free(doc);
        return NULL;
    }

    char *result = talloc_strdup(ctx, "");
    if (result == NULL) {
        yyjson_doc_free(doc);
        return NULL;
    }

    size_t sidx;
    size_t smax;
    yyjson_val *span;
    yyjson_arr_foreach(spans, sidx, smax, span) {
        yyjson_val *text_val = yyjson_obj_get(span, "text");
        const char *span_text = yyjson_get_str(text_val);
        if (span_text != NULL) {
            result = talloc_asprintf_append(result, "%s", span_text);
            if (result == NULL) {
                yyjson_doc_free(doc);
                return NULL;
            }
        }
    }

    yyjson_doc_free(doc);
    return result;
}
