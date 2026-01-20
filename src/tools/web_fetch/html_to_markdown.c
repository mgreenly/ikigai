#include "html_to_markdown.h"

#include <libxml/tree.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

void append_markdown(struct markdown_buffer *buf, const char *str)
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

static void convert_children(xmlNode *node, struct markdown_buffer *buf)
{
    for (xmlNode *child = node->children; child; child = child->next) {
        convert_node_to_markdown(child, buf);
    }
}

void convert_node_to_markdown(xmlNode *node, struct markdown_buffer *buf)
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
