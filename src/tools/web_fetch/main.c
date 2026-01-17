#include "json_allocator.h"

#include "vendor/yyjson/yyjson.h"

#include <curl/curl.h>
#include <inttypes.h>
#include <libxml/HTMLparser.h>
#include <libxml/tree.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

struct response_buffer {
    char *data;
    size_t size;
    size_t capacity;
    void *ctx;
};

struct markdown_buffer {
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

static void append_markdown(struct markdown_buffer *buf, const char *str)
{
    if (str == NULL) return; // LCOV_EXCL_BR_LINE

    size_t len = strlen(str);
    while (buf->size + len + 1 > buf->capacity) {
        buf->capacity *= 2;
        buf->data = talloc_realloc(buf->ctx, buf->data, char, (unsigned int)buf->capacity);
        if (buf->data == NULL) { // LCOV_EXCL_BR_LINE
            exit(1); // LCOV_EXCL_LINE
        } // LCOV_EXCL_LINE
    }

    memcpy(buf->data + buf->size, str, len);
    buf->size += len;
    buf->data[buf->size] = '\0';
}

static void convert_text_node(xmlNode *node, struct markdown_buffer *buf)
{
    if (node->content == NULL) return; // LCOV_EXCL_BR_LINE

    const char *text = (const char *)node->content;
    append_markdown(buf, text);
}

static void convert_node_to_markdown(xmlNode *node, struct markdown_buffer *buf);

static void convert_children(xmlNode *node, struct markdown_buffer *buf)
{
    for (xmlNode *child = node->children; child; child = child->next) {
        convert_node_to_markdown(child, buf);
    }
}

static void convert_node_to_markdown(xmlNode *node, struct markdown_buffer *buf)
{
    if (node == NULL) return; // LCOV_EXCL_BR_LINE

    if (node->type == XML_TEXT_NODE) {
        convert_text_node(node, buf);
        return;
    }

    if (node->type != XML_ELEMENT_NODE) {
        return;
    }

    const char *name = (const char *)node->name;

    if (strcmp(name, "script") == 0 || strcmp(name, "style") == 0 || strcmp(name, "nav") == 0) {
        return;
    }

    if (strcmp(name, "h1") == 0) {
        append_markdown(buf, "# ");
        convert_children(node, buf);
        append_markdown(buf, "\n\n");
    } else if (strcmp(name, "h2") == 0) {
        append_markdown(buf, "## ");
        convert_children(node, buf);
        append_markdown(buf, "\n\n");
    } else if (strcmp(name, "h3") == 0) {
        append_markdown(buf, "### ");
        convert_children(node, buf);
        append_markdown(buf, "\n\n");
    } else if (strcmp(name, "h4") == 0) {
        append_markdown(buf, "#### ");
        convert_children(node, buf);
        append_markdown(buf, "\n\n");
    } else if (strcmp(name, "h5") == 0) {
        append_markdown(buf, "##### ");
        convert_children(node, buf);
        append_markdown(buf, "\n\n");
    } else if (strcmp(name, "h6") == 0) {
        append_markdown(buf, "###### ");
        convert_children(node, buf);
        append_markdown(buf, "\n\n");
    } else if (strcmp(name, "p") == 0) {
        convert_children(node, buf);
        append_markdown(buf, "\n\n");
    } else if (strcmp(name, "br") == 0) {
        append_markdown(buf, "\n");
    } else if (strcmp(name, "strong") == 0 || strcmp(name, "b") == 0) {
        append_markdown(buf, "**");
        convert_children(node, buf);
        append_markdown(buf, "**");
    } else if (strcmp(name, "em") == 0 || strcmp(name, "i") == 0) {
        append_markdown(buf, "*");
        convert_children(node, buf);
        append_markdown(buf, "*");
    } else if (strcmp(name, "code") == 0) {
        append_markdown(buf, "`");
        convert_children(node, buf);
        append_markdown(buf, "`");
    } else if (strcmp(name, "a") == 0) {
        xmlChar *href = xmlGetProp(node, (const xmlChar *)"href");
        append_markdown(buf, "[");
        convert_children(node, buf);
        append_markdown(buf, "](");
        if (href) { // LCOV_EXCL_BR_LINE
            append_markdown(buf, (const char *)href);
            xmlFree(href);
        }
        append_markdown(buf, ")");
    } else if (strcmp(name, "ul") == 0 || strcmp(name, "ol") == 0) {
        convert_children(node, buf);
        append_markdown(buf, "\n");
    } else if (strcmp(name, "li") == 0) {
        append_markdown(buf, "- ");
        convert_children(node, buf);
        append_markdown(buf, "\n");
    } else {
        convert_children(node, buf);
    }
}

static void output_error(void *ctx, const char *error, const char *error_code)
{
    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *doc = yyjson_mut_doc_new(&allocator);
    if (doc == NULL) { // LCOV_EXCL_BR_LINE
        exit(1); // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_val *obj = yyjson_mut_obj(doc);
    if (obj == NULL) { // LCOV_EXCL_BR_LINE
        exit(1); // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_val *success_val = yyjson_mut_bool(doc, false);
    if (success_val == NULL) { // LCOV_EXCL_BR_LINE
        exit(1); // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_val *error_val = yyjson_mut_str(doc, error);
    if (error_val == NULL) { // LCOV_EXCL_BR_LINE
        exit(1); // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_val *error_code_val = yyjson_mut_str(doc, error_code);
    if (error_code_val == NULL) { // LCOV_EXCL_BR_LINE
        exit(1); // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_obj_add_val(doc, obj, "success", success_val);
    yyjson_mut_obj_add_val(doc, obj, "error", error_val);
    yyjson_mut_obj_add_val(doc, obj, "error_code", error_code_val);
    yyjson_mut_doc_set_root(doc, obj);

    char *json_str = yyjson_mut_write(doc, 0, NULL);
    if (json_str == NULL) { // LCOV_EXCL_BR_LINE
        exit(1); // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    printf("%s\n", json_str);
    free(json_str);
}

int32_t main(int32_t argc, char **argv)
{
    void *ctx = talloc_new(NULL);

    if (argc == 2 && strcmp(argv[1], "--schema") == 0) { // LCOV_EXCL_BR_LINE
        printf("{\n");
        printf("  \"name\": \"web_fetch\",\n");
        printf("  \"description\": \"Fetches content from a specified URL and returns it as markdown. Converts HTML to markdown using libxml2. Supports pagination via offset and limit parameters similar to file_read.\",\n");
        printf("  \"parameters\": {\n");
        printf("    \"type\": \"object\",\n");
        printf("    \"properties\": {\n");
        printf("      \"url\": {\n");
        printf("        \"type\": \"string\",\n");
        printf("        \"format\": \"uri\",\n");
        printf("        \"description\": \"The URL to fetch content from\"\n");
        printf("      },\n");
        printf("      \"offset\": {\n");
        printf("        \"type\": \"integer\",\n");
        printf("        \"description\": \"Line number to start reading from (1-based)\",\n");
        printf("        \"minimum\": 1\n");
        printf("      },\n");
        printf("      \"limit\": {\n");
        printf("        \"type\": \"integer\",\n");
        printf("        \"description\": \"Maximum number of lines to return\",\n");
        printf("        \"minimum\": 1\n");
        printf("      }\n");
        printf("    },\n");
        printf("    \"required\": [\"url\"]\n");
        printf("  }\n");
        printf("}\n");
        talloc_free(ctx);
        return 0;
    }

    size_t buffer_size = 4096;
    size_t total_read = 0;
    char *input = talloc_array(ctx, char, (unsigned int)buffer_size);
    if (input == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    size_t bytes_read;
    while ((bytes_read = fread(input + total_read, 1, buffer_size - total_read, stdin)) > 0) {
        total_read += bytes_read;

        if (total_read >= buffer_size) {
            buffer_size *= 2;
            input = talloc_realloc(ctx, input, char, (unsigned int)buffer_size);
            if (input == NULL) { // LCOV_EXCL_BR_LINE
                talloc_free(ctx); // LCOV_EXCL_LINE
                return 1; // LCOV_EXCL_LINE
            } // LCOV_EXCL_LINE
        }
    }

    if (total_read < buffer_size) { // LCOV_EXCL_BR_LINE
        input[total_read] = '\0';
    } else { // LCOV_EXCL_LINE
        input = talloc_realloc(ctx, input, char, (unsigned int)(total_read + 1)); // LCOV_EXCL_LINE
        if (input == NULL) { // LCOV_EXCL_LINE
            talloc_free(ctx); // LCOV_EXCL_LINE
            return 1; // LCOV_EXCL_LINE
        } // LCOV_EXCL_LINE
        input[total_read] = '\0'; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    if (total_read == 0) {
        fprintf(stderr, "web_fetch: empty input\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_alc allocator = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(input, total_read, 0, &allocator, NULL);
    if (doc == NULL) {
        fprintf(stderr, "web_fetch: invalid JSON\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *root = yyjson_doc_get_root(doc); // LCOV_EXCL_BR_LINE
    yyjson_val *url_val = yyjson_obj_get(root, "url"); // LCOV_EXCL_BR_LINE
    if (url_val == NULL || !yyjson_is_str(url_val)) {
        fprintf(stderr, "web_fetch: missing or invalid url field\n");
        talloc_free(ctx);
        return 1;
    }

    const char *url = yyjson_get_str(url_val); // LCOV_EXCL_BR_LINE

    yyjson_val *offset_val = yyjson_obj_get(root, "offset"); // LCOV_EXCL_BR_LINE
    yyjson_val *limit_val = yyjson_obj_get(root, "limit"); // LCOV_EXCL_BR_LINE

    int64_t offset = 0;
    int64_t limit = 0;
    bool has_offset = false;
    bool has_limit = false;

    if (offset_val != NULL && yyjson_is_int(offset_val)) {
        offset = yyjson_get_int(offset_val);
        has_offset = true;
    }

    if (limit_val != NULL && yyjson_is_int(limit_val)) {
        limit = yyjson_get_int(limit_val);
        has_limit = true;
    }

    CURL *curl = curl_easy_init();
    if (curl == NULL) { // LCOV_EXCL_BR_LINE
        output_error(ctx, "Failed to initialize HTTP client", "NETWORK_ERROR"); // LCOV_EXCL_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 0; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    struct response_buffer response;
    response.data = talloc_array(ctx, char, 4096);
    if (response.data == NULL) { // LCOV_EXCL_BR_LINE
        curl_easy_cleanup(curl); // LCOV_EXCL_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE
    response.size = 0;
    response.capacity = 4096;
    response.ctx = ctx;
    response.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "Failed to fetch URL: %s", curl_easy_strerror(res));
        curl_easy_cleanup(curl);
        output_error(ctx, error_msg, "NETWORK_ERROR");
        talloc_free(ctx);
        return 0;
    }

    int64_t http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code >= 400) {
        char error_msg[512];
        snprintf(error_msg, sizeof(error_msg), "HTTP %" PRId64 ": Error", http_code);
        curl_easy_cleanup(curl);
        output_error(ctx, error_msg, "HTTP_ERROR");
        talloc_free(ctx);
        return 0;
    }

    char *effective_url = NULL;
    curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_url);
    char *final_url = talloc_strdup(ctx, effective_url ? effective_url : url); // LCOV_EXCL_BR_LINE
    if (final_url == NULL) { // LCOV_EXCL_BR_LINE
        curl_easy_cleanup(curl); // LCOV_EXCL_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    curl_easy_cleanup(curl);

    htmlDocPtr html_doc = htmlReadMemory(response.data, (int32_t)response.size, NULL, NULL, HTML_PARSE_NOERROR);
    if (html_doc == NULL) { // LCOV_EXCL_BR_LINE
        output_error(ctx, "Failed to parse HTML", "PARSE_ERROR"); // LCOV_EXCL_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 0; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    xmlNode *root_element = xmlDocGetRootElement(html_doc);

    xmlNode *title_node = NULL;
    if (root_element != NULL) { // LCOV_EXCL_BR_LINE
        for (xmlNode *node = root_element; node; node = node->next) { // LCOV_EXCL_BR_LINE
            if (node->type == XML_ELEMENT_NODE && strcmp((const char *)node->name, "html") == 0) { // LCOV_EXCL_BR_LINE
                for (xmlNode *child = node->children; child; child = child->next) { // LCOV_EXCL_BR_LINE
                    if (child->type == XML_ELEMENT_NODE && strcmp((const char *)child->name, "head") == 0) {
                        for (xmlNode *head_child = child->children; head_child; head_child = head_child->next) { // LCOV_EXCL_BR_LINE
                            if (head_child->type == XML_ELEMENT_NODE && strcmp((const char *)head_child->name, "title") == 0) { // LCOV_EXCL_BR_LINE
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
                xmlFreeDoc(html_doc); // LCOV_EXCL_LINE
                talloc_free(ctx); // LCOV_EXCL_LINE
                return 1; // LCOV_EXCL_LINE
            } // LCOV_EXCL_LINE
        }
    }

    struct markdown_buffer md_buf;
    md_buf.data = talloc_array(ctx, char, 4096);
    if (md_buf.data == NULL) { // LCOV_EXCL_BR_LINE
        xmlFreeDoc(html_doc); // LCOV_EXCL_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE
    md_buf.size = 0;
    md_buf.capacity = 4096;
    md_buf.ctx = ctx;
    md_buf.data[0] = '\0';

    if (root_element != NULL) { // LCOV_EXCL_BR_LINE
        convert_node_to_markdown(root_element, &md_buf);
    }

    xmlFreeDoc(html_doc);

    char *content = md_buf.data;
    if (has_offset || has_limit) {
        int64_t current_line = 1;
        size_t line_start = 0;
        size_t output_start = 0;
        size_t output_end = md_buf.size;
        int64_t lines_collected = 0;

        for (size_t i = 0; i <= md_buf.size; i++) {
            if (i == md_buf.size || md_buf.data[i] == '\n') {
                if (has_offset && current_line < offset) {
                    current_line++;
                    line_start = i + 1;
                    continue;
                }

                if (current_line == offset && has_offset) { // LCOV_EXCL_BR_LINE
                    output_start = line_start;
                }

                if (!has_offset && lines_collected == 0) {
                    output_start = 0;
                }

                lines_collected++;

                if (has_limit && lines_collected >= limit) {
                    output_end = i + 1;
                    break;
                }

                current_line++;
                line_start = i + 1;
            }
        }

        if (has_offset && current_line < offset) {
            content = talloc_strdup(ctx, "");
        } else {
            size_t content_len = output_end - output_start;
            content = talloc_array(ctx, char, (unsigned int)(content_len + 1));
            if (content == NULL) { // LCOV_EXCL_BR_LINE
                talloc_free(ctx); // LCOV_EXCL_LINE
                return 1; // LCOV_EXCL_LINE
            } // LCOV_EXCL_LINE
            memcpy(content, md_buf.data + output_start, content_len);
            content[content_len] = '\0';
        }
    }

    yyjson_alc output_allocator = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *output_doc = yyjson_mut_doc_new(&output_allocator);
    if (output_doc == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_val *result_obj = yyjson_mut_obj(output_doc);
    if (result_obj == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_val *success_val = yyjson_mut_bool(output_doc, true);
    if (success_val == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_val *url_result = yyjson_mut_str(output_doc, final_url ? final_url : url); // LCOV_EXCL_BR_LINE
    if (url_result == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_val *title_result = yyjson_mut_str(output_doc, title);
    if (title_result == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_val *content_result = yyjson_mut_str(output_doc, content);
    if (content_result == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_obj_add_val(output_doc, result_obj, "success", success_val);
    yyjson_mut_obj_add_val(output_doc, result_obj, "url", url_result);
    yyjson_mut_obj_add_val(output_doc, result_obj, "title", title_result);
    yyjson_mut_obj_add_val(output_doc, result_obj, "content", content_result);
    yyjson_mut_doc_set_root(output_doc, result_obj);

    char *json_str = yyjson_mut_write(output_doc, 0, NULL);
    if (json_str == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    printf("%s\n", json_str);
    free(json_str);

    talloc_free(ctx);
    return 0;
}
