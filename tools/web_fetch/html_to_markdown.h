#ifndef HTML_TO_MARKDOWN_H
#define HTML_TO_MARKDOWN_H

#include <libxml/tree.h>
#include <stddef.h>

struct markdown_buffer {
    char *data;
    size_t size;
    size_t capacity;
    void *ctx;
};

void append_markdown(struct markdown_buffer *buf, const char *str);
void convert_node_to_markdown(xmlNode *node, struct markdown_buffer *buf);

#endif
