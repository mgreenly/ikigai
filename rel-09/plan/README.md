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
- Credential precedence: environment variable → credential file → error
- Environment variables: BRAVE_API_KEY, GOOGLE_SEARCH_API_KEY, GOOGLE_SEARCH_ENGINE_ID

**Multiple Providers:**
- Three independent external executables: `web-search-brave-tool`, `web-search-google-tool`, `web-fetch-tool`
- All written in C, compiled from source
- Each is separate executable built by `make tools`
- Each manages own config/credentials (not centralized in ikigai)
- Each makes own HTTP API calls
- No coordination or abstraction layer
- Credential discovery: environment variable → credential file

**Tool Parameters:**
- `web-search-*-tool`: Identical schema matching Claude Code's WebSearch tool
  - `query` (required): Search query string
  - `allowed_domains` (optional): Array of domains to include
  - `blocked_domains` (optional): Array of domains to exclude
- `web-fetch-tool`: URL fetching with limit/offset like file_read
  - `url` (required): URL to fetch
  - `limit` (optional): Maximum lines to return
  - `offset` (optional): Line offset to start from
  - Uses libxml2 to convert HTML→markdown before applying limits

**Response Format:**
- Both `web-search-*-tool` tools return identical JSON structure
- Matches Claude Code WebSearch response format exactly
- `web-fetch-tool` returns markdown content with line count (like file_read)

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
- Credential precedence: environment variable → credential file → error
- Environment variables:
  - BRAVE_API_KEY (overrides ~/.config/ikigai/brave-api-key)
  - GOOGLE_SEARCH_API_KEY (overrides ~/.config/ikigai/google-api-key)
  - GOOGLE_SEARCH_ENGINE_ID (overrides ~/.config/ikigai/google-engine-id)
- Tools always load and advertise to LLM
- Missing credentials result in authentication error when tool is called

## Plan Documents

| Document | Description | Status |
|----------|-------------|--------|
| `architecture-separate-tools.md` | Rationale for separate tools vs generic interface | Updated |
| `tool-schemas.md` | Exact JSON schemas for all three tools | New |
| `build-integration.md` | Makefile changes and build dependencies | New |

## Abstraction Level

Plan documents describe:
- Module names and responsibilities
- Major structs (names and purpose, not full definitions)
- Key function signatures (declaration only)
- Data flow between modules

**DO NOT include:** Full struct definitions, implementation code, detailed algorithms.

See `.claude/library/task-authoring/SKILL.md` for guidance on abstraction levels.
