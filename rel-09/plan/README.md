# rel-09 Plan

Module-level architecture plans describing what modules exist and how they interact.

## Key Decisions

**External Tool Architecture:**
- All tools are external executables following ikigai's external tool pattern
- Written in C, compiled from source, built by `make tools` target
- Installed to `libexec/ikigai/` (system) or `~/.ikigai/tools/` (user) or `.ikigai/tools/` (project)
- Each tool implements `--schema` flag and JSON stdin/stdout protocol
- Tool discovery and execution via external tool framework (implemented in rel-08)

**Tools:**
- `web-search-brave-tool` - Brave Search API integration
- `web-search-google-tool` - Google Custom Search API integration
- `web-fetch-tool` - URL content fetching with HTML→markdown conversion

**Tool Availability:**
- All tools advertised to LLM even without credentials configured
- Tools return authentication error when credentials missing
- Tools include `_event` field for config_required metadata (dim yellow warning)
- Credential precedence: environment variable → credentials.json → error
- Environment variables: BRAVE_API_KEY, GOOGLE_SEARCH_API_KEY, GOOGLE_SEARCH_ENGINE_ID

**config_required Events:**
- When credentials missing, tools include `_event` field in JSON output
- ikigai extracts `_event`, stores in messages table, removes from LLM result
- Event displayed to user in dim yellow (not sent to LLM)
- Event includes setup instructions, signup URLs, and credential paths
- See user-stories/first-time-discovery.md for example

**Multiple Providers:**
- Three independent external executables: `web-search-brave-tool`, `web-search-google-tool`, `web-fetch-tool`
- All written in C, compiled from source
- Each is separate executable built by `make tools`
- Each manages own config/credentials (not centralized in ikigai)
- Each makes own HTTP API calls
- No coordination or abstraction layer
- Credential discovery: environment variable → credential file

**Tool Parameters:**
- `web-search-brave-tool`: Brave Search with pagination
  - `query` (required): Search query string
  - `count` (optional): Results to return (1-20, default: 10)
  - `offset` (optional): Pagination offset (default: 0)
  - `allowed_domains` (optional): Array of domains to include
  - `blocked_domains` (optional): Array of domains to exclude
- `web-search-google-tool`: Google Custom Search with pagination
  - `query` (required): Search query string
  - `num` (optional): Results to return (1-10, default: 10)
  - `start` (optional): 1-based result index (default: 1, max: 91)
  - `allowed_domains` (optional): Array of domains to include
  - `blocked_domains` (optional): Array of domains to exclude
- `web-fetch-tool`: URL fetching with offset/limit
  - `url` (required): URL to fetch
  - `offset` (optional): Line offset to start from
  - `limit` (optional): Maximum lines to return
  - Uses libxml2 to convert HTML→markdown before applying limits

**Response Format:**
- Both `web-search-*-tool` tools return identical JSON structure:
  - Success: `{success: true, results: [{title, url, snippet}], count: N}`
  - Error: `{success: false, error: "message", error_code: "CODE"}`
- `web-fetch-tool` returns:
  - Success: `{success: true, url: "...", title: "...", content: "markdown..."}`
  - Error: `{success: false, error: "message", error_code: "CODE"}`
- All tools include `success` boolean field
- Errors include machine-readable `error_code` field

**Naming:**
- External tool naming: hyphen-separated ending in `-tool`
- Examples: `web-search-brave-tool`, `web-search-google-tool`, `web-fetch-tool`
- Tool names in API: `web_search_brave`, `web_search_google`, `web_fetch`
- Descriptions mention provider capabilities

**Build Dependencies:**
- libxml2 required for HTML parsing in web-fetch-tool
- Makefile must be updated to link against libxml2
- HTTP client library needed for API calls (decide which)

**Configuration:**
- Credential precedence: environment variable → credentials.json → error
- Environment variables:
  - `BRAVE_API_KEY`
  - `GOOGLE_SEARCH_API_KEY`
  - `GOOGLE_SEARCH_ENGINE_ID`
- Credentials file: `~/.config/ikigai/credentials.json`
  ```json
  {
    "web_search": {
      "brave": {"api_key": "..."},
      "google": {"api_key": "...", "engine_id": "..."}
    }
  }
  ```
- Tools always load and advertise to LLM
- Missing credentials result in authentication error and `config_required` event

## Plan Documents

| Document | Description | Status |
|----------|-------------|--------|
| `architecture-separate-tools.md` | Rationale for separate tools vs generic interface | Updated |
| `tool-schemas.md` | Exact JSON schemas for all three tools | New |
| `build-integration.md` | Makefile changes and build dependencies | New |
| `tool-implementation.md` | Internal C implementation patterns (memory, errors, structure) | New |
| `tool-list-sorting.md` | Alphabetical sorting of tool list after discovery | New |

## Research References

Plan documents reference research for implementation details:

- `../research/brave.md` - Brave Search API endpoints, parameters, response format
- `../research/google.md` - Google Custom Search API endpoints, parameters, response format
- `../research/all-providers-comparison.md` - Why Brave and Google were selected
- `../research/html-to-markdown.md` - libxml2 usage, DOM traversal, element mapping

## Abstraction Level

Plan documents describe:
- Module names and responsibilities
- Major structs (names and purpose, not full definitions)
- Key function signatures (declaration only)
- Data flow between modules

**DO NOT include:** Full struct definitions, implementation code, detailed algorithms.

See `.claude/library/task-authoring/SKILL.md` for guidance on abstraction levels.
