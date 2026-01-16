# Architecture Decision: Separate External Tools vs. Generic Interface

## Decision

**Chosen**: Separate external tools (`web-search-brave-tool`, `web-search-google-tool`, `web-fetch-tool`)
**Rejected**: Generic interface (`web-search()` with provider dispatch)

## External Tool Architecture

All web-related tools are external executables, consistent with ikigai's external tool architecture (implemented in rel-08):

- **No internal C code**: These tools are not compiled into ikigai
- **External executables**: Independent programs in any language (Python, Go, Rust, etc.)
- **Standard protocol**: Each implements `--schema` flag and JSON stdin/stdout interface
- **Installation**: Placed in `libexec/ikigai/` (system), `~/.ikigai/tools/` (user), or `.ikigai/tools/` (project)
- **Discovery**: Automatically discovered by ikigai's external tool framework
- **Self-contained**: Each manages own credentials, HTTP requests, error handling

## Implementation Phases

- **Phase 1**: Brave Search (`web-search-brave-tool`) - Complete implementation
- **Phase 2**: Google Search (`web-search-google-tool`) - Follow Phase 1 pattern
- **Phase 3**: Web fetch (`web-fetch-tool`) - URL fetching and content extraction
- Architecture and config structure support all from start
- Only enabled tools advertised to LLM

## Rationale

### Why Separate External Tools?

1. **Simplicity**
   - No dispatching logic needed in ikigai
   - No provider enum in C code
   - Each tool is self-contained and independent
   - Zero internal C code for web functionality

2. **Transparency**
   - LLM sees exactly which providers are available
   - Tool descriptions explain provider characteristics
   - Single enabled tool avoids confusion (default: Brave only)

3. **Future-Ready**
   - Perfect alignment with future Tool Sets feature
   - Tool Sets will control which search tools are active per task
   - No architecture changes needed

4. **Flexibility**
   - Users can enable multiple providers by installing tools
   - Each tool is independent
   - Phased implementation: prove pattern with Brave, extend to others
   - Users can override system tools with custom implementations

5. **Maintainability**
   - Add provider: Create new external executable
   - Remove provider: Delete/disable tool
   - Update provider: Modify single external tool
   - No ikigai recompilation needed

6. **Extensibility**
   - External tool architecture enables user customization
   - Users can create custom search tools (DuckDuckGo, Tavily, etc.)
   - Project-specific overrides for specialized search behavior
   - Any language can be used for implementation

### Why Not Generic Interface?

**Generic interface would require**:
- Provider enum and config parsing in C code
- Dispatch logic compiled into ikigai
- Hidden provider choice (LLM doesn't know which it's using)
- More complex configuration structure
- Recompilation to add new providers

**Key insight**: Search is different from LLM provider choice
- LLM provider (OpenAI/Anthropic) is **system-level** - can't mix in one conversation
- Search provider is **request-level** - can use different providers per search
- External tools enable request-level flexibility without internal complexity

## Comparison

| Aspect | Separate External Tools | Generic Interface |
|--------|-------------------------|-------------------|
| Simplicity | No dispatch logic in ikigai | Dispatch + enum in C |
| Implementation | External executables | Internal C code |
| Transparency | LLM knows provider | Hidden from LLM |
| Flexibility | Mix providers | One per config |
| Tool Sets | Natural fit | Awkward |
| Extensibility | Add external tool | Modify ikigai C code |
| User Override | Place in ~/.ikigai/tools/ | Recompile ikigai |
| Token cost | Multiple tools | Single tool |
| LLM choice | Must decide | Automatic |
| Language | Any (Python/Go/Rust) | C only |

**Token cost mitigation**: Tool Sets filter which tools are sent to LLM.

## LLM Perspective

### Default Configuration (Brave Only)
- LLM sees one tool: `web_search_brave`
- No confusion, simple choice
- Uses it automatically when web search needed

### User Installs Multiple Providers
User places `web-search-google-tool` in `~/.ikigai/tools/`:
- LLM sees both `web_search_brave` and `web_search_google`
- Can choose based on context or requirements
- Can use both in sequence for comprehensive research
- Can also see `web_fetch` for URL content extraction

### Implementation Sequence
- **Phase 1**: Only `web-search-brave-tool` exists in libexec/ikigai/
- **Phase 2**: Add `web-search-google-tool` to libexec/ikigai/
- **Phase 3**: Add `web-fetch-tool` to libexec/ikigai/
- Users can enable/disable via tool installation
- Users can override with custom implementations

## External Tool Benefits

### Credential Management
- Each tool manages own credentials independently
- No centralized ikigai credentials.json for these tools
- Tools read from standard locations (e.g., `~/.config/web-search-brave/`)
- Follows Unix principle: each tool responsible for own config

### Implementation Language
- `web-search-brave-tool`: Can be Python with requests library
- `web-search-google-tool`: Can be Go with net/http
- `web-fetch-tool`: Can be Rust with reqwest
- No C code in ikigai for HTTP, JSON parsing, rate limiting, etc.

### User Customization Examples
- Override Brave with DuckDuckGo: Place `web-search-brave-tool` (DDG impl) in `~/.ikigai/tools/`
- Add Tavily search: Create `web-search-tavily-tool` in `~/.ikigai/tools/`
- Project-specific search: Place custom tool in `.ikigai/tools/` for specialized searches

## Future: Tool Sets Integration

Tool Sets feature (deferred) will filter available tools per task:
- `default` set: Only Brave (conserve Google quota)
- `research` set: Both Brave and Google (LLM chooses)
- `web` set: All three (search + fetch)

Separate external tools architecture requires no changes for this.

## Decision Status

**Approved**: 2025-12-21
**Updated**: 2026-01-16 (External tool architecture alignment)
**Applies to**: rel-09
