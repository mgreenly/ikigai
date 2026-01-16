# Tool Schemas

Complete JSON schema specifications for all three web-related external tools.

## web-search-brave-tool

External executable: `web-search-brave-tool`
Tool name in API: `web_search_brave`
Provider: Brave Search API

### Schema Output (--schema flag)

```json
{
  "name": "web_search_brave",
  "description": "Search the web using Brave Search API and use the results to inform responses. Provides up-to-date information for current events and recent data. Returns search result information formatted as search result blocks, including links as markdown hyperlinks.",
  "parameters": {
    "type": "object",
    "properties": {
      "query": {
        "type": "string",
        "description": "The search query to use",
        "minLength": 2
      },
      "allowed_domains": {
        "type": "array",
        "items": {
          "type": "string"
        },
        "description": "Only include search results from these domains"
      },
      "blocked_domains": {
        "type": "array",
        "items": {
          "type": "string"
        },
        "description": "Never include search results from these domains"
      }
    },
    "required": ["query"]
  }
}
```

### Request Format (stdin)

```json
{
  "query": "search query string",
  "allowed_domains": ["example.com", "another.com"],
  "blocked_domains": ["spam.com"]
}
```

**Fields:**
- `query` (string, required): Search query, minimum 2 characters
- `allowed_domains` (array of strings, optional): Whitelist of domains
- `blocked_domains` (array of strings, optional): Blacklist of domains

### Response Format (stdout)

Match Claude Code WebSearch response format exactly. The tool maps Brave Search API responses to this format.

```json
{
  "results": [
    {
      "title": "Result title",
      "url": "https://example.com/page",
      "description": "Snippet or description of the result",
      "published_date": "2026-01-16T12:00:00Z"
    }
  ]
}
```

**Fields:**
- `results` (array): Array of search result objects
  - `title` (string): Page title
  - `url` (string): Full URL
  - `description` (string): Snippet or summary
  - `published_date` (string, optional): ISO 8601 timestamp if available

### Error Response

When credentials are missing or invalid:

```json
{
  "error": "Authentication required: BRAVE_API_KEY not found. Set environment variable or create ~/.config/ikigai/brave-api-key file. Get API key from https://brave.com/search/api/"
}
```

Non-zero exit code indicates failure.

---

## web-search-google-tool

External executable: `web-search-google-tool`
Tool name in API: `web_search_google`
Provider: Google Custom Search API

### Schema Output (--schema flag)

```json
{
  "name": "web_search_google",
  "description": "Search the web using Google Custom Search API and use the results to inform responses. Provides up-to-date information for current events and recent data. Returns search result information formatted as search result blocks, including links as markdown hyperlinks.",
  "parameters": {
    "type": "object",
    "properties": {
      "query": {
        "type": "string",
        "description": "The search query to use",
        "minLength": 2
      },
      "allowed_domains": {
        "type": "array",
        "items": {
          "type": "string"
        },
        "description": "Only include search results from these domains"
      },
      "blocked_domains": {
        "type": "array",
        "items": {
          "type": "string"
        },
        "description": "Never include search results from these domains"
      }
    },
    "required": ["query"]
  }
}
```

### Request Format (stdin)

**IDENTICAL to web-search-brave-tool** - see above.

### Response Format (stdout)

**IDENTICAL to web-search-brave-tool** - see above.

The tool maps Google Custom Search API responses to the same format as Brave.

### Error Response

When credentials are missing or invalid:

```json
{
  "error": "Authentication required: GOOGLE_SEARCH_API_KEY or GOOGLE_SEARCH_ENGINE_ID not found. Set environment variables or create ~/.config/ikigai/google-api-key and ~/.config/ikigai/google-engine-id files. Get credentials from https://developers.google.com/custom-search"
}
```

Non-zero exit code indicates failure.

---

## web-fetch-tool

External executable: `web-fetch-tool`
Tool name in API: `web_fetch`
Provider: HTTP client + libxml2 HTML parser

### Schema Output (--schema flag)

```json
{
  "name": "web_fetch",
  "description": "Fetches content from a specified URL and returns it as markdown. Converts HTML to markdown using libxml2. Supports pagination via offset and limit parameters similar to file_read.",
  "parameters": {
    "type": "object",
    "properties": {
      "url": {
        "type": "string",
        "format": "uri",
        "description": "The URL to fetch content from"
      },
      "offset": {
        "type": "integer",
        "description": "Line number to start reading from (1-based)",
        "minimum": 1
      },
      "limit": {
        "type": "integer",
        "description": "Maximum number of lines to return",
        "minimum": 1
      }
    },
    "required": ["url"]
  }
}
```

### Request Format (stdin)

```json
{
  "url": "https://example.com/page",
  "offset": 50,
  "limit": 100
}
```

**Fields:**
- `url` (string, required): Full URL to fetch (HTTP/HTTPS)
- `offset` (integer, optional): Line number to start from (1-based, default: 1)
- `limit` (integer, optional): Maximum lines to return (default: all)

### Response Format (stdout)

```json
{
  "content": "# Page Title\n\nMarkdown content here...",
  "lines_read": 100
}
```

**Fields:**
- `content` (string): Markdown-converted HTML content
- `lines_read` (integer): Number of lines returned

**Processing:**
1. Fetch URL via HTTP/HTTPS
2. Parse HTML using libxml2
3. Convert HTML to markdown
4. Apply offset/limit to markdown lines (not raw HTML)
5. Return result

**Behavior matching file_read:**
- If `offset` exceeds total lines: return empty content, `lines_read: 0`
- If `limit` exceeds remaining lines: return all remaining, `lines_read` = actual count
- Lines are counted after markdown conversion

### Error Response

Various error conditions:

```json
{
  "error": "Failed to fetch URL: Connection timeout"
}
```

```json
{
  "error": "Failed to parse HTML: Invalid document structure"
}
```

Non-zero exit code indicates failure.

---

## Implementation Notes

### Schema Consistency

Both `web-search-*-tool` schemas are **byte-for-byte identical** except for:
- `name` field: `web_search_brave` vs `web_search_google`
- `description` field: Provider name in description

This ensures LLM can use either tool interchangeably.

### Credential Discovery

Each tool implements this precedence independently:

1. Check environment variable (tool-specific)
2. Check credential file in `~/.config/ikigai/`
3. Return authentication error with helpful message

### Output Format Mapping

Tools must map provider-specific API responses to the common format:

**Brave Search API** → Common format
**Google Custom Search API** → Common format

The common format matches Claude Code's WebSearch tool response structure exactly.

### HTML to Markdown Conversion

The `web-fetch-tool` uses libxml2 for HTML parsing:

1. Parse HTML into DOM tree
2. Walk tree and convert to markdown:
   - `<h1>` → `# Heading`
   - `<p>` → Plain text with blank line
   - `<a>` → `[text](url)`
   - `<ul>/<li>` → Markdown lists
   - Strip scripts, styles, nav elements
3. Count lines in final markdown
4. Apply offset/limit

All line counting and limiting happens on markdown output, not raw HTML.
