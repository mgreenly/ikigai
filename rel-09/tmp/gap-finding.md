# Primary Gap Finding: 2026-01-16

## Most Critical Gap: #8 - Domain Filtering Specification

**Why this is the highest priority:**

1. **Directly impacts user-facing behavior** - Users will see inconsistent filtering
2. **No obvious "right answer"** - Requires a decision, not just documentation
3. **Affects both tools differently** - Brave (post-processing) vs Google (API parameter)
4. **Breaks contract** - Schema promises arrays, but Google only supports single domain

### The Problem

**Brave Search:**
- tool-schemas.md:86 says "post-processing" but doesn't specify algorithm
- Questions unresolved:
  - Does "example.com" match "www.example.com"? "blog.example.com"?
  - Case sensitive? "Example.com" vs "example.com"?
  - Match against URL hostname only or full URL?

**Google Search:**
- tool-schemas.md:262 says "only supports single domain" for blocked_domains
- Schema promises array of strings: `"blocked_domains": {"type": "array", "items": {"type": "string"}}`
- Questions unresolved:
  - What happens if user passes multiple blocked_domains?
  - Error? Use first only? Ignore extras?
  - What about multiple allowed_domains? Same issue.

### Why This Blocks Implementation

Without specification, implementer must guess:
- Subdomain matching logic (affects correctness of filtering)
- Multiple domain handling (affects whether schema is honest)
- Edge case behavior (affects whether tests can be written)

### Recommendation

Add section to tool-schemas.md or new plan/domain-filtering.md specifying:

**Brave post-processing:**
```
Domain matching algorithm:
1. Extract hostname from result URL
2. Normalize: lowercase, strip "www." prefix
3. Compare: exact match only (no wildcard subdomains)
4. Example: "example.com" matches "example.com", "www.example.com"
5. Does NOT match: "blog.example.com", "example.co.uk"
```

**Google API limitations:**
```
Google API only supports single domain filtering:
- allowed_domains: Use first element only, ignore rest
- blocked_domains: Use first element only, ignore rest
- Document this limitation in tool schema description
- OR: Return error if multiple domains provided
```

**Decision needed:** Fail loudly (error) vs fail silently (use first only)

---

## Secondary Gap: #9 - Database Integration (Easy to Resolve)

**Evidence:** messages table already exists (share/ikigai/migrations/001-initial-schema.sql:41-48)

Schema supports arbitrary event types:
- `kind TEXT` - event type discriminator
- `content TEXT` - formatted message
- `data JSONB` - structured metadata

**What's needed:** Specify integration point in tool_wrapper.c

```c
// Pseudo-code for tool_wrapper.c integration
res_t ik_tool_wrap_success(ctx, tool_result, wrapped_result) {
    // 1. Parse tool JSON output
    // 2. Extract optional _event field
    // 3. If _event present:
    //    - Store in messages table:
    //      INSERT INTO messages (session_id, kind, content, data)
    //      VALUES (current_session, 'config_required', event.content, event.data)
    // 4. Remove _event from result
    // 5. Wrap remaining result for LLM
}
```

**This is straightforward** - just needs specification in plan, not a complex decision.

---

## Gap Priority Ranking

1. **Gap #8 (Domain Filtering)** - CRITICAL, requires decisions, blocks implementation
2. **Gap #9 (Database Integration)** - MEDIUM, needs specification, straightforward
3. **Gap #7 (External Tool Framework)** - LOW, mostly documented in rel-08
4. **Gap #5 (Test Strategy)** - RESOLVED (80%), minor HTTP mocking gap
5. **Gap #1 (Internal Specifications)** - RESOLVED by tool-implementation.md

**Next action:** Resolve Gap #8 by making domain filtering decisions and documenting them.
