#include "web_fetch.h"

#include "html_to_markdown.h"
#include "json_allocator.h"
#include "panic.h"
#include "wrapper_web.h"

#include <curl/curl.h>
#include <inttypes.h>
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>

#include "vendor/yyjson/yyjson.h"

struct response_buffer {
    char *data;
    size_t size;
    size_t capacity;
    void *ctx;
};

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct response_buffer *buf = (struct response_buffer *)userp;

    while (buf->size + realsize + 1 > buf->capacity) {
        buf->capacity *= 2;
        buf->data = talloc_realloc(buf->ctx, buf->data, char, (unsigned int)buf->capacity);
        if (buf->data == NULL) { // LCOV_EXCL_BR_LINE
            return 0; // LCOV_EXCL_LINE
        } // LCOV_EXCL_LINE
    }

    memcpy(buf->data + buf->size, contents, realsize);
    buf->size += realsize;
    buf->data[buf->size] = '\0';

    return realsize;
}

static void output_error(void *ctx, const char *error, const char *error_code)
{
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *error_val = yyjson_mut_str(doc, error);
    if (error_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *error_code_val = yyjson_mut_str(doc, error_code);
    if (error_code_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_obj_add_val(doc, obj, "error", error_val);
    yyjson_mut_obj_add_val(doc, obj, "error_code", error_code_val);
    yyjson_mut_doc_set_root(doc, obj);

    char *json_str = yyjson_mut_write_opts(doc, 0, &allocator, NULL, NULL);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    printf("%s\n", json_str);
}

int32_t web_fetch_execute(void *ctx, const web_fetch_params_t *params)
{
    CURL *curl = curl_easy_init_();
    if (curl == NULL) { // LCOV_EXCL_BR_LINE
        output_error(ctx, "Failed to initialize HTTP client", "ERR_IO");
        return 0;
    }

    struct response_buffer response;
    response.data = talloc_array(ctx, char, 4096);
    if (response.data == NULL) { // LCOV_EXCL_BR_LINE
        curl_easy_cleanup_(curl); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE
    response.size = 0;
    response.capacity = 4096;
    response.ctx = ctx;
    response.data[0] = '\0';

    curl_easy_setopt_(curl, CURLOPT_URL, params->url);
    curl_easy_setopt_(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt_(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt_(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt_(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform_(curl);
    if (res != CURLE_OK) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to fetch URL: %s", curl_easy_strerror_(res));
        curl_easy_cleanup_(curl);
        output_error(ctx, error_msg, "ERR_IO");
        return 0;
    }

    int64_t http_code = 0;
    curl_easy_getinfo_(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code >= 400) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "HTTP %" PRId64 " error", http_code);
        curl_easy_cleanup_(curl);
        output_error(ctx, error_msg, "ERR_IO");
        return 0;
    }

    char *effective_url = NULL;
    curl_easy_getinfo_(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
    char *final_url = talloc_strdup(ctx, effective_url ? effective_url : params->url); // LCOV_EXCL_BR_LINE
    if (final_url == NULL) { // LCOV_EXCL_BR_LINE
        curl_easy_cleanup_(curl); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    curl_easy_cleanup_(curl);

    htmlDocPtr html_doc = htmlReadMemory_(response.data, (int32_t)response.size, NULL, NULL, HTML_PARSE_NOERROR);
    if (html_doc == NULL) { // LCOV_EXCL_BR_LINE
        output_error(ctx, "Failed to parse HTML", "ERR_PARSE");
        return 0;
    }

    xmlNode *root_element = xmlDocGetRootElement(html_doc);

    xmlNode *title_node = NULL;
    if (root_element != NULL) { // LCOV_EXCL_BR_LINE
        for (xmlNode *node = root_element; node; node = node->next) { // LCOV_EXCL_BR_LINE
            if (node->type == XML_ELEMENT_NODE && strcmp((const char *)node->name, "html") == 0) { // LCOV_EXCL_BR_LINE
                for (xmlNode *child = node->children; child; child = child->next) { // LCOV_EXCL_BR_LINE
                    if (child->type == XML_ELEMENT_NODE && strcmp((const char *)child->name, "head") == 0) {
                        for (xmlNode *head_child = child->children; head_child; head_child = head_child->next) { // LCOV_EXCL_BR_LINE
                            if (head_child->type == XML_ELEMENT_NODE &&
                                strcmp((const char *)head_child->name, "title") == 0) { // LCOV_EXCL_BR_LINE
                                title_node = head_child;
                                break;
                            }
                        }
                        break;
                    }
                }
                break;
            }
        }
    }

    char *title = talloc_strdup(ctx, "");
    if (title_node != NULL && title_node->children != NULL) { // LCOV_EXCL_BR_LINE
        xmlNode *text_node = title_node->children;
        if (text_node->type == XML_TEXT_NODE && text_node->content != NULL) { // LCOV_EXCL_BR_LINE
            title = talloc_strdup(ctx, (const char *)text_node->content);
            if (title == NULL) { // LCOV_EXCL_BR_LINE
                xmlFreeDoc_(html_doc); // LCOV_EXCL_LINE
                PANIC("Out of memory"); // LCOV_EXCL_LINE
            } // LCOV_EXCL_LINE
        }
    }

    struct markdown_buffer md_buf;
    md_buf.data = talloc_array(ctx, char, 4096);
    if (md_buf.data == NULL) { // LCOV_EXCL_BR_LINE
        xmlFreeDoc_(html_doc); // LCOV_EXCL_LINE
        PANIC("Out of memory"); // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE
    md_buf.size = 0;
    md_buf.capacity = 4096;
    md_buf.ctx = ctx;
    md_buf.data[0] = '\0';

    if (root_element != NULL) { // LCOV_EXCL_BR_LINE
        convert_node_to_markdown(root_element, &md_buf);
    }

    xmlFreeDoc_(html_doc);

    char *content = md_buf.data;
    if (params->has_offset || params->has_limit) {
        int64_t current_line = 1;
        size_t line_start = 0;
        size_t output_start = 0;
        size_t output_end = md_buf.size;
        int64_t lines_collected = 0;

        for (size_t i = 0; i <= md_buf.size; i++) {
            if (i == md_buf.size || md_buf.data[i] == '\n') {
                if (params->has_offset && current_line < params->offset) {
                    current_line++;
                    line_start = i + 1;
                    continue;
                }

                if (current_line == params->offset && params->has_offset) { // LCOV_EXCL_BR_LINE
                    output_start = line_start;
                }

                if (!params->has_offset && lines_collected == 0) {
                    output_start = 0;
                }

                lines_collected++;

                if (params->has_limit && lines_collected >= params->limit) {
                    output_end = i + 1;
                    break;
                }

                current_line++;
                line_start = i + 1;
            }
        }

        if (params->has_offset && current_line < params->offset) {
            content = talloc_strdup(ctx, "");
        } else {
            size_t content_len = output_end - output_start;
            content = talloc_array(ctx, char, (unsigned int)(content_len + 1));
            if (content == NULL) { // LCOV_EXCL_BR_LINE
                PANIC("Out of memory"); // LCOV_EXCL_LINE
            } // LCOV_EXCL_LINE
            memcpy(content, md_buf.data + output_start, content_len);
            content[content_len] = '\0';
        }
    }

    // Output success JSON
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    if (doc == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (obj == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *url_val = yyjson_mut_str(doc, final_url);
    if (url_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *title_val = yyjson_mut_str(doc, title);
    if (title_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_val *content_val = yyjson_mut_str(doc, content);
    if (content_val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    yyjson_mut_obj_add_val(doc, obj, "url", url_val);
    yyjson_mut_obj_add_val(doc, obj, "title", title_val);
    yyjson_mut_obj_add_val(doc, obj, "content", content_val);
    yyjson_mut_doc_set_root(doc, obj);

    char *json_str = yyjson_mut_write_opts(doc, 0, &allocator, NULL, NULL);
    if (json_str == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    printf("%s\n", json_str);

    return 0;
}
