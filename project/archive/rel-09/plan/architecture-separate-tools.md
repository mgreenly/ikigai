# Architecture Decision: Separate Tools vs. Generic Interface

## Decision

**Chosen**: Separate tools (`web_search_brave`, `web_search_google`, etc.)
**Rejected**: Generic interface (`web_search()` with provider dispatch)

## Implementation Phases

- **Phase 1**: Brave Search (`web_search_brave`) - Complete implementation
- **Phase 2**: Google Search (`web_search_google`) - Follow Phase 1 pattern
- Architecture and config structure support both from start
- Only enabled tools advertised to LLM

## Rationale

### Why Separate Tools?

1. **Simplicity**
   - No dispatching logic needed
   - No provider enum
   - Each tool is self-contained and independent

2. **Transparency**
   - LLM sees exactly which providers are enabled
   - Tool descriptions explain provider characteristics
   - Single enabled tool avoids confusion (default: Brave only)

3. **Future-Ready**
   - Perfect alignment with future Tool Sets feature
   - Tool Sets will control which search tools are active per task
   - No architecture changes needed

4. **Flexibility**
   - Users can enable multiple providers via config
   - Each tool is independent
   - Phased implementation: prove pattern with Brave, extend to Google

5. **Maintainability**
   - Add provider: Register new tool
   - Remove provider: Don't register tool

### Why Not Generic Interface?

**Generic interface would require**:
- Provider enum and config parsing
- Dispatch logic based on config
- Hidden provider choice (LLM doesn't know which it's using)
- More complex configuration structure

**Key insight**: Search is different from LLM provider choice
- LLM provider (OpenAI/Anthropic) is **system-level** - can't mix in one conversation
- Search provider is **request-level** - can use different providers per search

## Comparison

| Aspect | Separate Tools | Generic Interface |
|--------|----------------|-------------------|
| Simplicity | No dispatch logic | Dispatch + enum |
| Transparency | LLM knows provider | Hidden from LLM |
| Flexibility | Mix providers | One per config |
| Tool Sets | Natural fit | Awkward |
| Extensibility | Add tool | Modify dispatch |
| Token cost | Multiple tools | Single tool |
| LLM choice | Must decide | Automatic |

**Token cost mitigation**: Tool Sets filter which tools are sent to LLM.

## LLM Perspective

### Default Configuration (Brave Only)
- LLM sees one tool: `web_search_brave`
- No confusion, simple choice
- Uses it automatically when web search needed

### User Enables Both Providers
User sets `web_search.google.enabled: true` in config:
- LLM sees both `web_search_brave` and `web_search_google`
- Can choose based on context or requirements
- Can use both in sequence for comprehensive research

### Implementation Sequence
- **Phase 1**: Only `web_search_brave` exists and is enabled
- **Phase 2**: Add `web_search_google` (disabled by default)
- Users can enable Google when needed

## Future: Tool Sets Integration

Tool Sets feature (deferred) will filter available tools per task:
- `default` set: Only Brave (conserve Google quota)
- `research` set: Both providers (LLM chooses)

Separate tools architecture requires no changes for this.

## Decision Status

**Approved**: 2025-12-21
**Applies to**: rel-08
