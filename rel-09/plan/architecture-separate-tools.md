# Architecture Decision: Separate External Tools vs. Generic Interface

## Decision

**Chosen**: Separate external tools (`web-search-brave-tool`, `web-search-google-tool`, `web-fetch-tool`)
**Rejected**: Generic interface (`web-search()` with provider dispatch)

## External Tool Architecture

All web-related tools are external executables, consistent with ikigai's external tool architecture (implemented in rel-08):

- **External executables**: Not compiled into ikigai core, separate binaries
- **C implementation**: Written in C, compiled from source, built by `make tools`
- **Standard protocol**: Each implements `--schema` flag and JSON stdin/stdout interface
- **Installation**: Built to `libexec/ikigai/` (system), can be overridden in `~/.ikigai/tools/` (user) or `.ikigai/tools/` (project)
- **Discovery**: Automatically discovered by ikigai's external tool framework
- **Self-contained**: Each manages own credentials, HTTP requests, error handling

## Tools

Three external executables:

- `web-search-brave-tool` - Brave Search API integration
- `web-search-google-tool` - Google Custom Search API integration
- `web-fetch-tool` - URL fetching with HTML→markdown conversion via libxml2

All tools advertised to LLM regardless of credential availability. Missing credentials result in authentication error when tool is called.

## Rationale

### Why Separate External Tools?

1. **Simplicity**
   - No dispatching logic needed in ikigai core
   - No provider enum in ikigai core
   - Each tool is self-contained and independent
   - Web functionality isolated in external executables

2. **Transparency**
   - LLM sees exactly which providers are available
   - Tool descriptions explain provider characteristics
   - All tools visible to LLM (authentication errors guide credential setup)

3. **Future-Ready**
   - Perfect alignment with future Tool Sets feature
   - Tool Sets will control which search tools are active per task
   - No architecture changes needed

4. **Flexibility**
   - Each tool is independent
   - Users can override system tools with custom implementations in `~/.ikigai/tools/`
   - Project-specific overrides in `.ikigai/tools/`

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

### Default Configuration
- LLM sees all three tools: `web_search_brave`, `web_search_google`, `web_fetch`
- Tools return authentication errors if credentials missing
- Errors guide user to configure credentials via environment variables or files

### With Credentials Configured
- LLM can choose between providers based on context
- Can use multiple tools in sequence for comprehensive research

### Tool Override Examples
- Override Brave with DuckDuckGo: Place custom `web-search-brave-tool` in `~/.ikigai/tools/`
- Add Tavily search: Create `web-search-tavily-tool` in `~/.ikigai/tools/`
- Project-specific search: Place custom tool in `.ikigai/tools/`

## External Tool Benefits

### Credential Management
- Each tool manages own credentials independently
- Credential precedence: environment variable → credential file → error
- Environment variables:
  - `BRAVE_API_KEY` (overrides `~/.config/ikigai/brave-api-key`)
  - `GOOGLE_SEARCH_API_KEY` (overrides `~/.config/ikigai/google-api-key`)
  - `GOOGLE_SEARCH_ENGINE_ID` (overrides `~/.config/ikigai/google-engine-id`)
- All tools load and advertise regardless of credential availability
- Missing credentials result in authentication error with helpful message

### Implementation Language
- All tools written in C, compiled from source
- Built by `make tools` target, installed to `libexec/ikigai/`
- libxml2 dependency for HTML→markdown conversion in web-fetch-tool
- HTTP client library needed (to be decided)

### User Customization Examples
- Override Brave with DuckDuckGo: Place `web-search-brave-tool` (DDG impl) in `~/.ikigai/tools/`
- Add Tavily search: Create `web-search-tavily-tool` in `~/.ikigai/tools/`
- Project-specific search: Place custom tool in `.ikigai/tools/` for specialized searches

## Schema Alignment

Both `web-search-*-tool` tools implement identical schemas matching Claude Code's WebSearch tool:

**Input Schema:**
- `query` (string, required): Search query
- `allowed_domains` (array of strings, optional): Only include results from these domains
- `blocked_domains` (array of strings, optional): Exclude results from these domains

**Output Schema:**
- Identical structure for both providers
- Matches Claude Code WebSearch response format
- See `tool-schemas.md` for complete specification

The `web-fetch-tool` follows the file_read pattern:

**Input Schema:**
- `url` (string, required): URL to fetch
- `limit` (integer, optional): Maximum lines to return
- `offset` (integer, optional): Line offset to start from

**Output Schema:**
- `content` (string): Markdown-converted HTML content
- `lines_read` (integer): Number of lines returned
- Limits and offsets apply to markdown, not raw HTML

## Future: Tool Sets Integration

Tool Sets feature (deferred) will filter available tools per task:
- `default` set: Conservative defaults
- `research` set: Both Brave and Google (LLM chooses)
- `web` set: All three (search + fetch)

Separate external tools architecture requires no changes for this.

## Decision Status

**Approved**: 2025-12-21
**Updated**: 2026-01-16 (C implementation, schema alignment)
**Applies to**: rel-09
