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

struct resp_buf {
    char *data;
    size_t size;
    size_t cap;
    void *ctx;
};

struct md_buf {
    char *data;
    size_t size;
    size_t cap;
    void *ctx;
};

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct resp_buf *buf = (struct resp_buf *)userp;

    while (buf->size + realsize + 1 > buf->cap) {
        buf->cap *= 2;
        buf->data = talloc_realloc(buf->ctx, buf->data, char, (unsigned int)buf->cap);
        if (buf->data == NULL) { // LCOV_EXCL_BR_LINE
            return 0; // LCOV_EXCL_LINE
        } // LCOV_EXCL_LINE
    }

    memcpy(buf->data + buf->size, contents, realsize);
    buf->size += realsize;
    buf->data[buf->size] = '\0';

    return realsize;
}

static void append_markdown(struct md_buf *buf, const char *str)
{
    if (str == NULL) return; // LCOV_EXCL_BR_LINE

    size_t len = strlen(str);
    while (buf->size + len + 1 > buf->cap) {
        buf->cap *= 2;
        buf->data = talloc_realloc(buf->ctx, buf->data, char, (unsigned int)buf->cap);
        if (buf->data == NULL) { // LCOV_EXCL_BR_LINE
            exit(1); // LCOV_EXCL_LINE
        } // LCOV_EXCL_LINE
    }

    memcpy(buf->data + buf->size, str, len);
    buf->size += len;
    buf->data[buf->size] = '\0';
}

static void convert_text_node(xmlNode *node, struct md_buf *buf)
{
    if (node->content == NULL) return; // LCOV_EXCL_BR_LINE

    const char *text = (char *)node->content;
    append_markdown(buf, text);
}

static void convert_node_to_markdown(xmlNode *node, struct md_buf *buf);

static void convert_children(xmlNode *node, struct md_buf *buf)
{
    for (xmlNode *child = node->children; child; child = child->next) {
        convert_node_to_markdown(child, buf);
    }
}

static void convert_node_to_markdown(xmlNode *node, struct md_buf *buf)
{
    if (node == NULL) return; // LCOV_EXCL_BR_LINE

    if (node->type == XML_TEXT_NODE) {
        convert_text_node(node, buf);
        return;
    }

    if (node->type != XML_ELEMENT_NODE) {
        return;
    }

    const char *n = (const char *)node->name;

    if (strcmp(n, "script") == 0 || strcmp(n, "style") == 0 || strcmp(n, "nav") == 0) {
        return;
    }

    if (strcmp(n, "h1") == 0) {
        append_markdown(buf, "# ");
        convert_children(node, buf);
        append_markdown(buf, "\n\n");
    } else if (strcmp(n, "h2") == 0) {
        append_markdown(buf, "## ");
        convert_children(node, buf);
        append_markdown(buf, "\n\n");
    } else if (strcmp(n, "h3") == 0) {
        append_markdown(buf, "### ");
        convert_children(node, buf);
        append_markdown(buf, "\n\n");
    } else if (strcmp(n, "h4") == 0) {
        append_markdown(buf, "#### ");
        convert_children(node, buf);
        append_markdown(buf, "\n\n");
    } else if (strcmp(n, "h5") == 0) {
        append_markdown(buf, "##### ");
        convert_children(node, buf);
        append_markdown(buf, "\n\n");
    } else if (strcmp(n, "h6") == 0) {
        append_markdown(buf, "###### ");
        convert_children(node, buf);
        append_markdown(buf, "\n\n");
    } else if (strcmp(n, "p") == 0) {
        convert_children(node, buf);
        append_markdown(buf, "\n\n");
    } else if (strcmp(n, "br") == 0) {
        append_markdown(buf, "\n");
    } else if (strcmp(n, "strong") == 0 || strcmp(n, "b") == 0) {
        append_markdown(buf, "**");
        convert_children(node, buf);
        append_markdown(buf, "**");
    } else if (strcmp(n, "em") == 0 || strcmp(n, "i") == 0) {
        append_markdown(buf, "*");
        convert_children(node, buf);
        append_markdown(buf, "*");
    } else if (strcmp(n, "code") == 0) {
        append_markdown(buf, "`");
        convert_children(node, buf);
        append_markdown(buf, "`");
    } else if (strcmp(n, "a") == 0) {
        xmlChar *href = xmlGetProp(node, (const xmlChar *)"href");
        append_markdown(buf, "[");
        convert_children(node, buf);
        append_markdown(buf, "](");
        if (href) { // LCOV_EXCL_BR_LINE
            append_markdown(buf, (char *)href);
            xmlFree(href);
        }
        append_markdown(buf, ")");
    } else if (strcmp(n, "ul") == 0 || strcmp(n, "ol") == 0) {
        convert_children(node, buf);
        append_markdown(buf, "\n");
    } else if (strcmp(n, "li") == 0) {
        append_markdown(buf, "- ");
        convert_children(node, buf);
        append_markdown(buf, "\n");
    } else {
        convert_children(node, buf);
    }
}

static void output_error(void *ctx, const char *error, const char *error_code)
{
    yyjson_alc a = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *d = yyjson_mut_doc_new(&a);
    if (d == NULL) { // LCOV_EXCL_BR_LINE
        exit(1); // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_val *o = yyjson_mut_obj(d);
    if (o == NULL) { // LCOV_EXCL_BR_LINE
        exit(1); // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_val *s = yyjson_mut_bool(d, false);
    if (s == NULL) { // LCOV_EXCL_BR_LINE
        exit(1); // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_val *e = yyjson_mut_str(d, error);
    if (e == NULL) { // LCOV_EXCL_BR_LINE
        exit(1); // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_val *ec = yyjson_mut_str(d, error_code);
    if (ec == NULL) { // LCOV_EXCL_BR_LINE
        exit(1); // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_obj_add_val(d, o, "success", s);
    yyjson_mut_obj_add_val(d, o, "error", e);
    yyjson_mut_obj_add_val(d, o, "error_code", ec);
    yyjson_mut_doc_set_root(d, o);

    char *js = yyjson_mut_write(d, 0, NULL);
    if (js == NULL) { // LCOV_EXCL_BR_LINE
        exit(1); // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    printf("%s\n", js);
    free(js);
}

int32_t main(int32_t argc, char **argv)
{
    void *ctx = talloc_new(NULL);

    if (argc == 2 && strcmp(argv[1], "--schema") == 0) { // LCOV_EXCL_BR_LINE
        printf("{\n"
"  \"name\": \"web_fetch\",\n"
"  \"description\": \"Fetches content from URL and returns markdown. Converts HTML to markdown. Supports offset/limit pagination.\",\n"
"  \"parameters\": {\n"
"    \"type\": \"object\",\n"
"    \"properties\": {\n"
"      \"url\": {\"type\": \"string\", \"format\": \"uri\", \"description\": \"URL to fetch\"},\n"
"      \"offset\": {\"type\": \"integer\", \"description\": \"Line to start from (1-based)\", \"minimum\": 1},\n"
"      \"limit\": {\"type\": \"integer\", \"description\": \"Max lines to return\", \"minimum\": 1}\n"
"    },\n"
"    \"required\": [\"url\"]\n"
"  }\n"
"}\n");
        talloc_free(ctx);
        return 0;
    }

    size_t bufsz = 4096;
    size_t total = 0;
    char *in = talloc_array(ctx, char, (unsigned int)bufsz);
    if (in == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    size_t nr;
    while ((nr = fread(in + total, 1, bufsz - total, stdin)) > 0) {
        total += nr;

        if (total >= bufsz) {
            bufsz *= 2;
            in = talloc_realloc(ctx, in, char, (unsigned int)bufsz);
            if (in == NULL) { // LCOV_EXCL_BR_LINE
                talloc_free(ctx); // LCOV_EXCL_LINE
                return 1; // LCOV_EXCL_LINE
            } // LCOV_EXCL_LINE
        }
    }

    if (total < bufsz) { // LCOV_EXCL_BR_LINE
        in[total] = '\0';
    } else { // LCOV_EXCL_LINE
        in = talloc_realloc(ctx, in, char, (unsigned int)(total + 1)); // LCOV_EXCL_LINE
        if (in == NULL) { // LCOV_EXCL_LINE
            talloc_free(ctx); // LCOV_EXCL_LINE
            return 1; // LCOV_EXCL_LINE
        } // LCOV_EXCL_LINE
        in[total] = '\0'; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    if (total == 0) {
        fprintf(stderr, "e\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_alc a = ik_make_talloc_allocator(ctx);
    yyjson_doc *doc = yyjson_read_opts(in, total, 0, &a, NULL);
    if (doc == NULL) {
        fprintf(stderr, "j\n");
        talloc_free(ctx);
        return 1;
    }

    yyjson_val *rt = yyjson_doc_get_root(doc); // LCOV_EXCL_BR_LINE
    yyjson_val *uv = yyjson_obj_get(rt, "url"); // LCOV_EXCL_BR_LINE
    if (uv == NULL || !yyjson_is_str(uv)) {
        fprintf(stderr, "u\n");
        talloc_free(ctx);
        return 1;
    }

    const char *url = yyjson_get_str(uv); // LCOV_EXCL_BR_LINE

    yyjson_val *ov = yyjson_obj_get(rt, "offset"); // LCOV_EXCL_BR_LINE
    yyjson_val *lv = yyjson_obj_get(rt, "limit"); // LCOV_EXCL_BR_LINE

    int64_t off = 0;
    int64_t lim = 0;
    bool has_off = false;
    bool has_lim = false;

    if (ov != NULL && yyjson_is_int(ov)) {
        off = yyjson_get_int(ov);
        has_off = true;
    }

    if (lv != NULL && yyjson_is_int(lv)) {
        lim = yyjson_get_int(lv);
        has_lim = true;
    }

    CURL *cu = curl_easy_init();
    if (cu == NULL) { // LCOV_EXCL_BR_LINE
        output_error(ctx, "HTTP init failed", "NETWORK_ERROR"); // LCOV_EXCL_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 0; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    struct resp_buf resp;
    resp.data = talloc_array(ctx, char, 4096);
    if (resp.data == NULL) { // LCOV_EXCL_BR_LINE
        curl_easy_cleanup(cu); // LCOV_EXCL_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE
    resp.size = 0;
    resp.cap = 4096;
    resp.ctx = ctx;
    resp.data[0] = '\0';

    curl_easy_setopt(cu, CURLOPT_URL, url);
    curl_easy_setopt(cu, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(cu, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(cu, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(cu, CURLOPT_TIMEOUT, 10L);

    CURLcode res = curl_easy_perform(cu);
    if (res != CURLE_OK) {
        char err[64];
        snprintf(err, sizeof(err), "Fetch: %s", curl_easy_strerror(res));
        curl_easy_cleanup(cu);
        output_error(ctx, err, "NETWORK_ERROR");
        talloc_free(ctx);
        return 0;
    }

    int64_t hcode = 0;
    curl_easy_getinfo(cu, CURLINFO_RESPONSE_CODE, &hcode);
    if (hcode >= 400) {
        char err[64];
        snprintf(err, sizeof(err), "%" PRId64, hcode);
        curl_easy_cleanup(cu);
        output_error(ctx, err, "HTTP_ERROR");
        talloc_free(ctx);
        return 0;
    }

    char *eurl = NULL;
    curl_easy_getinfo(cu, CURLINFO_EFFECTIVE_URL, &eurl);
    char *furl = talloc_strdup(ctx, eurl ? eurl : url); // LCOV_EXCL_BR_LINE
    if (furl == NULL) { // LCOV_EXCL_BR_LINE
        curl_easy_cleanup(cu); // LCOV_EXCL_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    curl_easy_cleanup(cu);

    htmlDocPtr hdoc = htmlReadMemory(resp.data, (int32_t)resp.size, NULL, NULL, HTML_PARSE_NOERROR);
    if (hdoc == NULL) { // LCOV_EXCL_BR_LINE
        output_error(ctx, "HTML parse failed", "PARSE_ERROR"); // LCOV_EXCL_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 0; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    xmlNode *xr = xmlDocGetRootElement(hdoc);

    xmlNode *tn = NULL;
    if (xr != NULL) { // LCOV_EXCL_BR_LINE
        for (xmlNode *n = xr; n; n = n->next) { // LCOV_EXCL_BR_LINE
            if (n->type == XML_ELEMENT_NODE && strcmp((const char *)n->name, "html") == 0) { // LCOV_EXCL_BR_LINE
                for (xmlNode *c = n->children; c; c = c->next) { // LCOV_EXCL_BR_LINE
                    if (c->type == XML_ELEMENT_NODE && strcmp((const char *)c->name, "head") == 0) {
                        for (xmlNode *h = c->children; h; h = h->next) { // LCOV_EXCL_BR_LINE
                            if (h->type == XML_ELEMENT_NODE && strcmp((const char *)h->name, "title") == 0) { // LCOV_EXCL_BR_LINE
                                tn = h;
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

    char *t = talloc_strdup(ctx, "");
    if (tn != NULL && tn->children != NULL) { // LCOV_EXCL_BR_LINE
        xmlNode *tx = tn->children;
        if (tx->type == XML_TEXT_NODE && tx->content != NULL) { // LCOV_EXCL_BR_LINE
            t = talloc_strdup(ctx, (char *)tx->content);
            if (t == NULL) { // LCOV_EXCL_BR_LINE
                xmlFreeDoc(hdoc); // LCOV_EXCL_LINE
                talloc_free(ctx); // LCOV_EXCL_LINE
                return 1; // LCOV_EXCL_LINE
            } // LCOV_EXCL_LINE
        }
    }

    struct md_buf mb;
    mb.data = talloc_array(ctx, char, 4096);
    if (mb.data == NULL) { // LCOV_EXCL_BR_LINE
        xmlFreeDoc(hdoc); // LCOV_EXCL_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_BR_LINE
    mb.size = 0;
    mb.cap = 4096;
    mb.ctx = ctx;
    mb.data[0] = '\0';

    if (xr != NULL) { // LCOV_EXCL_BR_LINE
        convert_node_to_markdown(xr, &mb);
    }

    xmlFreeDoc(hdoc);

    char *con = mb.data;
    if (has_off || has_lim) {
        int64_t c = 1;
        size_t s = 0;
        size_t os = 0;
        size_t oe = mb.size;
        int64_t col = 0;

        for (size_t i = 0; i <= mb.size; i++) {
            if (i == mb.size || mb.data[i] == '\n') {
                if (has_off && c < off) {
                    c++;
                    s = i + 1;
                    continue;
                }

                if (c == off && has_off) { // LCOV_EXCL_BR_LINE
                    os = s;
                }

                if (!has_off && col == 0) {
                    os = 0;
                }

                col++;

                if (has_lim && col >= lim) {
                    oe = i + 1;
                    break;
                }

                c++;
                s = i + 1;
            }
        }

        if (has_off && c < off) {
            con = talloc_strdup(ctx, "");
        } else {
            size_t len = oe - os;
            con = talloc_array(ctx, char, (unsigned int)(len + 1));
            if (con == NULL) { // LCOV_EXCL_BR_LINE
                talloc_free(ctx); // LCOV_EXCL_LINE
                return 1; // LCOV_EXCL_LINE
            } // LCOV_EXCL_LINE
            memcpy(con, mb.data + os, len);
            con[len] = '\0';
        }
    }

    yyjson_alc oa = ik_make_talloc_allocator(ctx);
    yyjson_mut_doc *od = yyjson_mut_doc_new(&oa);
    if (od == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_val *obj = yyjson_mut_obj(od);
    if (obj == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_val *su = yyjson_mut_bool(od, true);
    if (su == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_val *ur = yyjson_mut_str(od, furl ? furl : url); // LCOV_EXCL_BR_LINE
    if (ur == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_val *tr = yyjson_mut_str(od, t);
    if (tr == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_val *cr = yyjson_mut_str(od, con);
    if (cr == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    yyjson_mut_obj_add_val(od, obj, "success", su);
    yyjson_mut_obj_add_val(od, obj, "url", ur);
    yyjson_mut_obj_add_val(od, obj, "title", tr);
    yyjson_mut_obj_add_val(od, obj, "content", cr);
    yyjson_mut_doc_set_root(od, obj);

    char *js = yyjson_mut_write(od, 0, NULL);
    if (js == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(ctx); // LCOV_EXCL_LINE
        return 1; // LCOV_EXCL_LINE
    } // LCOV_EXCL_LINE

    printf("%s\n", js);
    free(js);

    talloc_free(ctx);
    return 0;
}
