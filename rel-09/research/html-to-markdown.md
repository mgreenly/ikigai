# HTML to Markdown Conversion Using libxml2

Research for implementing a custom HTML-to-markdown converter using libxml2 for the `web-fetch-tool`.

## Overview

The `web-fetch-tool` needs to convert fetched HTML content into markdown for LLM consumption. We're implementing a simple custom solution using libxml2 for HTML parsing and DOM traversal.

## libxml2 HTML Parsing

### Parsing HTML into DOM

libxml2 provides HTML-specific parsing functions with error tolerance for malformed HTML:

```c
htmlDocPtr htmlReadMemory(const char *buffer, int size,
                          const char *URL, const char *encoding,
                          int options);
```

**Example:**
```c
htmlDocPtr doc = htmlReadMemory(html_content, content_length,
                                NULL, NULL, HTML_PARSE_NOERROR);
```

**Options:**
- `HTML_PARSE_NOERROR` - Suppress error messages
- `HTML_PARSE_NOWARNING` - Suppress warnings
- `HTML_PARSE_RECOVER` - Recover from errors (default for HTML)

### Getting Root Element

```c
xmlNodePtr root = xmlDocGetRootElement(doc);
```

### Memory Cleanup

```c
xmlFreeDoc(doc);
xmlCleanupParser();
```

## DOM Tree Traversal

### Node Structure

Each `xmlNode` has key members:
- `name` - Element name (e.g., "p", "div", "a")
- `type` - Node type (XML_ELEMENT_NODE, XML_TEXT_NODE, etc.)
- `content` - Text content (for text nodes)
- `children` - First child node
- `next` - Next sibling node
- `properties` - Attributes

### Basic Traversal Pattern

Recursive traversal using `children` and `next` pointers:

```c
static void traverse_nodes(xmlNode *node) {
    xmlNode *cur = NULL;

    for (cur = node; cur; cur = cur->next) {
        if (cur->type == XML_ELEMENT_NODE) {
            // Process element node
            printf("Element: %s\n", cur->name);
        }
        else if (cur->type == XML_TEXT_NODE) {
            // Process text node
            printf("Text: %s\n", cur->content);
        }

        // Recurse into children
        traverse_nodes(cur->children);
    }
}
```

### Node Types

Common node types to handle:
- `XML_ELEMENT_NODE` (1) - HTML elements (div, p, a, etc.)
- `XML_TEXT_NODE` (3) - Text content
- `XML_COMMENT_NODE` (8) - HTML comments
- `XML_CDATA_SECTION_NODE` (4) - CDATA sections

### Accessing Attributes

```c
xmlChar *href = xmlGetProp(node, (const xmlChar *)"href");
if (href) {
    // Use attribute value
    xmlFree(href);
}
```

## HTML to Markdown Mapping

### Conversion Strategy

Use a visitor pattern during DOM traversal:
1. Traverse DOM tree recursively
2. For each element node, check element name
3. Apply appropriate markdown conversion
4. Accumulate output into buffer

### Element Mapping Table

| HTML Element | Markdown Syntax | Notes |
|--------------|-----------------|-------|
| `<h1>` | `# text` | Add newlines before/after |
| `<h2>` | `## text` | Add newlines before/after |
| `<h3>` | `### text` | Add newlines before/after |
| `<h4>` | `#### text` | Add newlines before/after |
| `<h5>` | `##### text` | Add newlines before/after |
| `<h6>` | `###### text` | Add newlines before/after |
| `<p>` | `text\n\n` | Double newline after |
| `<br>` | `\n` | Single newline |
| `<strong>`, `<b>` | `**text**` | Inline bold |
| `<em>`, `<i>` | `*text*` | Inline italic |
| `<code>` | `` `text` `` | Inline code |
| `<pre>` | `` ```\ntext\n``` `` | Code block |
| `<a href="url">` | `[text](url)` | Links |
| `<img src="url" alt="text">` | `![alt](url)` | Images |
| `<ul>` | List container | Track nesting level |
| `<ol>` | Numbered list | Track item number |
| `<li>` | `- text` or `1. text` | Depends on parent |
| `<blockquote>` | `> text` | Quote prefix |
| `<hr>` | `---\n` | Horizontal rule |

### Handling Nested Elements

Key considerations:
- Maintain context stack for list nesting
- Track parent element type (ul vs ol)
- Preserve inline formatting within block elements
- Handle whitespace normalization

### Text Content Extraction

For text nodes:
1. Extract `node->content`
2. Trim leading/trailing whitespace
3. Collapse multiple spaces to single space
4. Preserve intentional line breaks

### Elements to Skip

Common elements that don't map to markdown:
- `<script>` - Skip entirely
- `<style>` - Skip entirely
- `<nav>` - Skip or process children only
- `<header>`, `<footer>` - Process children only
- `<div>`, `<span>` - Process children only (no markup)

## Implementation Approach

### Recommended Structure

1. **Parser Module** - HTML parsing with libxml2
   - `html_parse(const char *html, size_t len)` → `htmlDocPtr`

2. **Converter Module** - DOM to markdown conversion
   - `convert_node(xmlNode *node, struct buffer *out, struct context *ctx)`
   - Recursive traversal with context tracking

3. **Context Structure** - Track conversion state
   - List nesting level
   - Inside code block flag
   - Parent element type
   - Output buffer

4. **Buffer Management** - talloc-based string building
   - Append markdown fragments
   - Efficient reallocation

### Minimal Viable Conversion

For initial implementation, focus on:
1. Headings (h1-h6)
2. Paragraphs (p)
3. Links (a)
4. Basic formatting (strong, em, code)
5. Line breaks (br)
6. Text content extraction

Can defer:
- Tables (complex formatting)
- Lists (nesting complexity)
- Images (optional for text extraction)
- Blockquotes (less common)

## Existing Libraries (Reference Only)

Several HTML-to-markdown libraries exist but we're implementing custom:

- **html2md** (C++) - Uses custom parser, supports tables
- **turndown** (JavaScript) - Popular, visitor pattern
- **html-to-markdown** (Python) - Visitor callbacks per element
- **Rust implementations** - High performance, complex

**Why custom implementation:**
- Simpler dependencies (libxml2 already available)
- Full control over output format
- Tailored to LLM consumption use case
- Learning opportunity for libxml2 usage

## Sources

- [The Complete Libxml2 C++ Cheatsheet](https://proxiesapi.com/articles/the-complete-libxml2-c-cheatsheet)
- [libxml2 HTMLparser.h Reference](https://gnome.pages.gitlab.gnome.org/libxml2/devhelp/libxml2-HTMLparser.html)
- [libxml2 Examples](https://opensource.netapp.com/Data_ONTAP_9/9.6P12/libxml2-2.9.10/doc/examples/index.html)
- [html2md Library (C++)](https://github.com/tim-gromeyer/html2md)
- [turndown (JavaScript)](https://github.com/mixmark-io/turndown)
- [html-to-markdown (Python)](https://pypi.org/project/html-to-markdown/)
- [libxml2 DOM Traversal Examples](https://www.codeguru.com/database/libxml2-everything-you-need-in-an-xml-library/)

## Notes

- libxml2's HTML parser is error-tolerant, handles malformed HTML gracefully
- DOM API is consistent between XML and HTML parsing
- Recursive traversal with `children` and `next` pointers is the standard pattern
- Consider using visitor/replacement pattern for extensibility
- Focus on common elements first, add edge cases as needed
