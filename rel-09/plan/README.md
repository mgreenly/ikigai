# rel-08 Plan

Module-level architecture plans describing what modules exist and how they interact.

## Key Decisions

**Implementation Phases:**
- **Phase 1**: Brave Search only (`web_search_brave` tool)
- **Phase 2**: Add Google Search (`web_search_google` tool)
- Architecture supports multiple providers from start
- Implement one completely before adding next

**Tool Advertising:**
- Only enabled tools advertised to LLM (prevent confusion)
- Brave enabled by default, Google disabled by default
- Tool returns helpful error with signup URL if unconfigured
- Makes capability discoverable

**Multiple Providers:**
- Two independent tools: `web_search_brave` and `web_search_google`
- No shared code between them
- Each has own source file (tool_web_search_brave.c, tool_web_search_google.c)
- Each reads own config/credentials sections
- Each makes own HTTP API calls
- No coordination or abstraction layer
- Hierarchical config structure: `web_search.brave.*`, `web_search.google.*`

**Tool Parameters:**
- Start minimal: `query` (required), `count`/`num` (optional)
- Provider-specific response formats (described in tool descriptions)
- Defer freshness/country/language until requested

**Display:**
- Use existing tool_call/tool_result pattern (dimmed in scrollback)
- Both shown to user and sent to LLM
- New `config_required` event kind for actionable errors (dim yellow)
- Triggers: missing credentials, invalid credentials (401), rate limits
- Tool decides when to emit, renderer just styles
- Defer fancy formatting for success cases

**Naming:**
- Provider-specific: `web_search_brave`, `web_search_google`
- Descriptions mention provider
- Tool names grouped by namespace prefix

**Configuration:**
- Hierarchical structure ready for multiple providers
- Credentials: `~/.config/ikigai/credentials.json`
  - `{"web_search": {"brave": {"api_key": "..."}}}`
  - `{"web_search": {"google": {"api_key": "...", "engine_id": "..."}}}`
- Config: `~/.config/ikigai/config.json`
  - `{"web_search": {"brave": {"enabled": true, "default_count": 10}}}`
  - `{"web_search": {"google": {"enabled": false, "default_count": 10}}}`
- Pattern established by rel-07, extended for hierarchical search tools

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
