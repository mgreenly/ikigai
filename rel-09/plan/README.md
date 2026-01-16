# rel-09 Plan

Module-level architecture plans describing what modules exist and how they interact.

## Key Decisions

**External Tool Architecture:**
- All tools are external executables (no internal C code in ikigai)
- Installed to `libexec/ikigai/` (system) or `~/.ikigai/tools/` (user) or `.ikigai/tools/` (project)
- Each tool implements `--schema` flag and JSON stdin/stdout protocol
- Tool discovery and execution via external tool framework (implemented in rel-08)

**Implementation Phases:**
- **Phase 1**: Brave Search only (`web-search-brave-tool`)
- **Phase 2**: Add Google Search (`web-search-google-tool`)
- **Phase 3**: Add web fetch capability (`web-fetch-tool`)
- Architecture supports multiple providers from start
- Implement one completely before adding next

**Tool Advertising:**
- Only enabled tools advertised to LLM (prevent confusion)
- Brave enabled by default, Google disabled by default
- Tool returns helpful error with signup URL if unconfigured
- Makes capability discoverable

**Multiple Providers:**
- Three independent external executables: `web-search-brave-tool`, `web-search-google-tool`, `web-fetch-tool`
- No shared code between them
- Each is separate executable (any language: Python, Go, Rust, etc.)
- Each manages own config/credentials (not centralized in ikigai)
- Each makes own HTTP API calls
- No coordination or abstraction layer
- Each tool discovers its own credentials from standard locations

**Tool Parameters:**
- Start minimal: `query` (required), `count`/`num` (optional)
- Provider-specific response formats (described in tool descriptions)
- Defer freshness/country/language until requested
- web-fetch-tool: `url` (required), `prompt` (optional) for content extraction

**Display:**
- Use existing tool_call/tool_result pattern (dimmed in scrollback)
- Both shown to user and sent to LLM
- New `config_required` event kind for actionable errors (dim yellow)
- Triggers: missing credentials, invalid credentials (401), rate limits
- Tool decides when to emit, renderer just styles
- Defer fancy formatting for success cases

**Naming:**
- External tool naming: hyphen-separated ending in `-tool`
- Examples: `web-search-brave-tool`, `web-search-google-tool`, `web-fetch-tool`
- Tool names in API: `web_search_brave`, `web_search_google`, `web_fetch`
- Descriptions mention provider capabilities

**Configuration:**
- Each tool manages own credentials independently
- Tools read from standard config locations (e.g., `~/.config/web-search-brave/`, `~/.config/web-search-google/`)
- No centralized ikigai credentials.json for these tools
- Each tool implements own config/credential discovery logic
- Tools may provide `--configure` flag for interactive setup

**Response Format:**
- JSON envelopes for all tools
- Double-escaping accepted as current reality
- Provider-specific result structures

## Plan Documents

| Document | Description | Status |
|----------|-------------|--------|
| `architecture-separate-tools.md` | Rationale for separate tools vs generic interface | Complete |

## Abstraction Level

Plan documents describe:
- Module names and responsibilities
- Major structs (names and purpose, not full definitions)
- Key function signatures (declaration only)
- Data flow between modules

**DO NOT include:** Full struct definitions, implementation code, detailed algorithms.

See `.claude/library/task-authoring/SKILL.md` for guidance on abstraction levels.
