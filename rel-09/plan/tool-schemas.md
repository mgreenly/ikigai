# Tool Schemas

Complete JSON schema specifications for all three web-related external tools.

**Research References:**
- `../research/brave.md` - Brave Search API specification
- `../research/google.md` - Google Custom Search API specification
- `../research/html-to-markdown.md` - HTML to markdown conversion guidance

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
      "count": {
        "type": "integer",
        "description": "Number of results to return (1-20)",
        "minimum": 1,
        "maximum": 20,
        "default": 10
      },
      "offset": {
        "type": "integer",
        "description": "Result offset for pagination",
        "minimum": 0,
        "default": 0
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
  "count": 10,
  "offset": 0,
  "allowed_domains": ["example.com", "another.com"],
  "blocked_domains": ["spam.com"]
}
```

**Fields:**
- `query` (string, required): Search query, minimum 2 characters
- `count` (integer, optional): Number of results (1-20, default: 10)
- `offset` (integer, optional): Pagination offset (default: 0)
- `allowed_domains` (array of strings, optional): Whitelist of domains
- `blocked_domains` (array of strings, optional): Blacklist of domains

**Mapping to Brave API** (see `../research/brave.md`):
- `query` → `q` parameter
- `count` → `count` parameter (max 20)
- `offset` → `offset` parameter
- Domain filtering implemented via post-processing (Brave doesn't have native domain filters)

### Response Format (stdout)

Success response:

```json
{
  "success": true,
  "results": [
    {
      "title": "Result title",
      "url": "https://example.com/page",
      "snippet": "Snippet or description of the result"
    }
  ],
  "count": 3
}
```

**Fields:**
- `success` (boolean): Always `true` for successful responses
- `results` (array): Array of search result objects
  - `title` (string): Page title
  - `url` (string): Full URL
  - `snippet` (string): Result snippet or description
- `count` (integer): Number of results returned

**Mapping from Brave API** (see `../research/brave.md`):
- Brave's `web.results[]` → `results[]`
- Brave's `web.results[].title` → `results[].title`
- Brave's `web.results[].url` → `results[].url`
- Brave's `web.results[].description` → `results[].snippet`
- Array length → `count`

### Error Response

When credentials are missing or invalid:

```json
{
  "success": false,
  "error": "Web search requires API key configuration.\n\nBrave Search offers 2,000 free searches/month.\nGet your key: https://brave.com/search/api/\nAdd to: ~/.config/ikigai/credentials.json as 'web_search.brave.api_key'"
}
```

When rate limit exceeded (HTTP 429 from Brave):

```json
{
  "success": false,
  "error": "Rate limit exceeded. You've used your free search quota (2,000/month).",
  "error_code": "RATE_LIMIT"
}
```

**Fields:**
- `success` (boolean): Always `false` for errors
- `error` (string): Human-readable error message
- `error_code` (string, optional): Machine-readable error code

**Error codes:**
- `AUTH_MISSING` - Credentials not configured
- `AUTH_INVALID` - Credentials rejected by API
- `RATE_LIMIT` - Quota exhausted
- `NETWORK_ERROR` - Connection or timeout failure
- `API_ERROR` - Provider API returned error

Non-zero exit code indicates failure.

**config_required Event:**

When credentials are missing, the tool writes a `config_required` event to **stderr** (see user-stories/first-time-discovery.md):

```json
{
  "kind": "config_required",
  "content": "⚠ Configuration Required\n\nWeb search needs an API key. Brave Search offers 2,000 free searches/month.\n\nGet your key: https://brave.com/search/api/\nAdd to: ~/.config/ikigai/credentials.json\n\nExample:\n{\n  \"web_search\": {\n    \"brave\": {\n      \"api_key\": \"your-api-key-here\"\n    }\n  }\n}",
  "data_json": "{\"tool\": \"web_search_brave\", \"credential\": \"api_key\", \"signup_url\": \"https://brave.com/search/api/\"}"
}
```

**Important:**
- Error response goes to **stdout** (sent to LLM)
- config_required event goes to **stderr** (captured by ikigai, stored in database, displayed to user)
- This separation allows LLM to see error while user gets detailed setup instructions

This event is stored in the database and displayed to users in dim yellow.

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
      "num": {
        "type": "integer",
        "description": "Number of results to return (1-10)",
        "minimum": 1,
        "maximum": 10,
        "default": 10
      },
      "start": {
        "type": "integer",
        "description": "Result index offset for pagination (1-based, max 91)",
        "minimum": 1,
        "maximum": 91,
        "default": 1
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
  "num": 10,
  "start": 1,
  "allowed_domains": ["example.com", "another.com"],
  "blocked_domains": ["spam.com"]
}
```

**Fields:**
- `query` (string, required): Search query, minimum 2 characters
- `num` (integer, optional): Number of results (1-10, default: 10)
- `start` (integer, optional): 1-based result index (default: 1, max: 91)
- `allowed_domains` (array of strings, optional): Whitelist of domains
- `blocked_domains` (array of strings, optional): Blacklist of domains

**Note:** Unlike Brave (which uses `count`/`offset`), Google uses `num`/`start` parameters.

**Mapping to Google API** (see `../research/google.md`):
- `query` → `q` parameter
- `num` → `num` parameter (max 10)
- `start` → `start` parameter (1-based, max 91)
- `allowed_domains` → `siteSearch` parameter with `siteSearchFilter=i`
- `blocked_domains` → `siteSearch` parameter with `siteSearchFilter=e` (only supports single domain)

### Response Format (stdout)

Success response (identical to Brave):

```json
{
  "success": true,
  "results": [
    {
      "title": "Result title",
      "url": "https://example.com/page",
      "snippet": "Snippet or description of the result"
    }
  ],
  "count": 3
}
```

**Fields:**
- `success` (boolean): Always `true` for successful responses
- `results` (array): Array of search result objects
  - `title` (string): Page title
  - `url` (string): Full URL
  - `snippet` (string): Result snippet or description
- `count` (integer): Number of results returned

**Mapping from Google API** (see `../research/google.md`):
- Google's `items[]` → `results[]`
- Google's `items[].title` → `results[].title`
- Google's `items[].link` → `results[].url`
- Google's `items[].snippet` → `results[].snippet`
- Array length → `count`

### Error Response

When credentials are missing or invalid:

```json
{
  "success": false,
  "error": "Web search requires API key configuration.\n\nGoogle Custom Search offers 100 free searches/day.\nGet API key: https://developers.google.com/custom-search/v1/overview\nGet Search Engine ID: https://programmablesearchengine.google.com/controlpanel/create\nAdd to: ~/.config/ikigai/credentials.json as 'web_search.google.api_key' and 'web_search.google.engine_id'"
}
```

When rate limit exceeded (HTTP 403 with `dailyLimitExceeded`):

```json
{
  "success": false,
  "error": "Rate limit exceeded. You've used your free search quota (100/day).",
  "error_code": "RATE_LIMIT"
}
```

**Fields:**
- `success` (boolean): Always `false` for errors
- `error` (string): Human-readable error message
- `error_code` (string, optional): Machine-readable error code

**Error codes:** Same as Brave (AUTH_MISSING, AUTH_INVALID, RATE_LIMIT, NETWORK_ERROR, API_ERROR)

Non-zero exit code indicates failure.

**config_required Event:**

When credentials are missing, writes a `config_required` event to **stderr** (similar to Brave but with Google-specific instructions):

```json
{
  "kind": "config_required",
  "content": "⚠ Configuration Required\n\nWeb search needs an API key and Search Engine ID. Google Custom Search offers 100 free searches/day.\n\nGet API key: https://developers.google.com/custom-search/v1/overview\nGet Search Engine ID: https://programmablesearchengine.google.com/controlpanel/create\nAdd to: ~/.config/ikigai/credentials.json\n\nExample:\n{\n  \"web_search\": {\n    \"google\": {\n      \"api_key\": \"your-api-key-here\",\n      \"engine_id\": \"your-search-engine-id\"\n    }\n  }\n}",
  "data_json": "{\"tool\": \"web_search_google\", \"credentials\": [\"api_key\", \"engine_id\"]}"
}
```

Event written to stderr, captured by ikigai, and displayed to user in dim yellow.

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

Success response:

```json
{
  "success": true,
  "url": "https://example.com/rust-async-guide",
  "title": "Asynchronous Programming in Rust",
  "content": "# Asynchronous Programming in Rust\n\nMarkdown content here..."
}
```

**Fields:**
- `success` (boolean): Always `true` for successful responses
- `url` (string): The fetched URL (may differ from requested if redirected)
- `title` (string): Page title extracted from `<title>` tag
- `content` (string): Markdown-converted HTML content

**Processing:**
1. Fetch URL via HTTP/HTTPS
2. Parse HTML using libxml2 (`htmlReadMemory()`)
3. Extract title from `<title>` tag
4. Convert HTML DOM to markdown (see `../research/html-to-markdown.md`)
5. Apply offset/limit to markdown lines (not raw HTML)
6. Return result

**Behavior with offset/limit:**
- If `offset` specified: skip first N lines of markdown
- If `limit` specified: return at most N lines
- If `offset` exceeds total lines: return empty content
- If `limit` exceeds remaining lines: return all remaining
- Lines are counted after markdown conversion

**HTML to Markdown Conversion** (see `../research/html-to-markdown.md`):
- `<h1>` → `# Heading`
- `<p>` → Plain text with blank line
- `<a href="url">` → `[text](url)`
- `<strong>`, `<b>` → `**text**`
- `<em>`, `<i>` → `*text*`
- `<code>` → `` `text` ``
- `<ul>`, `<li>` → Markdown lists
- Strip `<script>`, `<style>`, comments

### Error Response

Various error conditions:

```json
{
  "success": false,
  "error": "Failed to fetch URL: Connection timeout",
  "error_code": "NETWORK_ERROR"
}
```

```json
{
  "success": false,
  "error": "Failed to parse HTML: Invalid document structure",
  "error_code": "PARSE_ERROR"
}
```

```json
{
  "success": false,
  "error": "HTTP 404: Not Found",
  "error_code": "HTTP_ERROR"
}
```

**Fields:**
- `success` (boolean): Always `false` for errors
- `error` (string): Human-readable error message
- `error_code` (string): Machine-readable error code

**Error codes:**
- `NETWORK_ERROR` - Connection failure, timeout, DNS resolution failure
- `HTTP_ERROR` - HTTP status >= 400 (404, 500, etc.)
- `PARSE_ERROR` - HTML parsing failed
- `INVALID_URL` - Malformed URL

Non-zero exit code indicates failure.

**Note:** web-fetch-tool requires no credentials and does not emit `config_required` events.

---

## Implementation Notes

### Schema Consistency

Both `web-search-*-tool` schemas are **byte-for-byte identical** except for:
- `name` field: `web_search_brave` vs `web_search_google`
- `description` field: Provider name in description

This ensures LLM can use either tool interchangeably.

### Credential Discovery

Each tool implements this precedence independently:

1. **Environment variables** (highest precedence)
   - `BRAVE_API_KEY` for web-search-brave-tool
   - `GOOGLE_SEARCH_API_KEY` and `GOOGLE_SEARCH_ENGINE_ID` for web-search-google-tool

2. **Credentials file** `~/.config/ikigai/credentials.json`:
   ```json
   {
     "web_search": {
       "brave": {
         "api_key": "your-brave-api-key"
       },
       "google": {
         "api_key": "your-google-api-key",
         "engine_id": "your-search-engine-id"
       }
     }
   }
   ```

3. **Return authentication error** with helpful message and emit `config_required` event

**Note:** User stories reference `credentials.json` with nested structure, not individual credential files.

### Output Format Mapping

Tools must map provider-specific API responses to the common format:

**Brave Search API** → Common format (see `../research/brave.md`):
- API returns: `{type: "search", web: {results: [...]}}`
- Extract: `web.results[]` array
- Map each result: `{title, url, description}` → `{title, url, snippet}`
- Add: `success: true` and `count: results.length`

**Google Custom Search API** → Common format (see `../research/google.md`):
- API returns: `{kind: "customsearch#search", items: [...]}`
- Extract: `items[]` array
- Map each result: `{title, link, snippet}` → `{title, url, snippet}`
- Add: `success: true` and `count: items.length`

The common format allows LLM to use either tool interchangeably.

### HTML to Markdown Conversion

The `web-fetch-tool` uses libxml2 for HTML parsing (see `../research/html-to-markdown.md`):

1. **Parse HTML into DOM tree**
   - Use `htmlReadMemory(content, length, NULL, NULL, HTML_PARSE_NOERROR)`
   - Get root: `xmlDocGetRootElement(doc)`

2. **Walk tree and convert to markdown**
   - Recursive traversal using `node->children` and `node->next` pointers
   - Element mapping (see research doc for complete table):
     - `<h1>` → `# Heading`
     - `<h2>` → `## Heading`
     - `<p>` → Plain text with blank line
     - `<a href="url">` → `[text](url)`
     - `<strong>`, `<b>` → `**text**`
     - `<em>`, `<i>` → `*text*`
     - `<code>` → `` `text` ``
     - `<ul>`, `<li>` → Markdown lists
   - Strip: `<script>`, `<style>`, `<nav>`, comments

3. **Count lines in final markdown**

4. **Apply offset/limit to markdown lines**

All line counting and limiting happens on markdown output, not raw HTML.

**Recommended approach:** Visitor pattern with context tracking for list nesting and parent element types.
