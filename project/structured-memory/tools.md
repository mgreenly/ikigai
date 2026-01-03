# Tool Interface

Structured Memory uses a unified tool interface where operations work identically on filesystem and StoredAssets.

## Unified File Operations

From the LLM's perspective, these are identical:

```json
// Read filesystem file
{"tool": "file_read", "path": "src/main.c"}

// Read StoredAsset
{"tool": "file_read", "path": "ikigai:///blocks/decisions.md"}

// Write filesystem file
{"tool": "file_write", "path": "docs/api.md", "content": "..."}

// Write StoredAsset
{"tool": "file_write", "path": "ikigai:///blocks/patterns.md", "content": "..."}

// Edit filesystem file
{"tool": "file_edit", "path": "config.json", "old": "...", "new": "..."}

// Edit StoredAsset
{"tool": "file_edit", "path": "ikigai:///blocks/prefs.md", "old": "...", "new": "..."}
```

**The URI scheme determines storage backend. Everything else is identical.**

## Tool Implementation

### URI Routing Pattern

```c
// src/tool_file_read.c
ik_result_t tool_file_read(ik_agent_ctx_t *agent, yyjson_val *params) {
    const char *path = get_string_param(params, "path");

    // Route based on URI scheme
    if (str_starts_with(path, "ikigai://")) {
        return storage_db_read(agent, path);
    } else {
        return storage_fs_read(path);
    }
}

// src/tool_file_write.c
ik_result_t tool_file_write(ik_agent_ctx_t *agent, yyjson_val *params) {
    const char *path = get_string_param(params, "path");
    const char *content = get_string_param(params, "content");

    if (str_starts_with(path, "ikigai://")) {
        // Check block budget before writing
        TRY(storage_db_check_budget(agent, path, content));
        return storage_db_write(agent, path, content);
    } else {
        return storage_fs_write(path, content);
    }
}

// src/tool_file_edit.c
ik_result_t tool_file_edit(ik_agent_ctx_t *agent, yyjson_val *params) {
    const char *path = get_string_param(params, "path");
    const char *old_string = get_string_param(params, "old");
    const char *new_string = get_string_param(params, "new");

    if (str_starts_with(path, "ikigai://")) {
        return storage_db_edit(agent, path, old_string, new_string);
    } else {
        return storage_fs_edit(path, old_string, new_string);
    }
}
```

**One tool. Two backends. Zero special cases.**

## Storage Backends

### Filesystem Backend

```c
// src/storage/fs_read.c
ik_result_t storage_fs_read(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        return ERR("Failed to open %s: %s", path, strerror(errno));
    }

    // Read entire file
    char *content = read_file_contents(fp);
    fclose(fp);

    return OK(content);
}

// src/storage/fs_write.c
ik_result_t storage_fs_write(const char *path, const char *content) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        return ERR("Failed to write %s: %s", path, strerror(errno));
    }

    fprintf(fp, "%s", content);
    fclose(fp);

    return OK_VOID;
}
```

Standard filesystem operations.

### Database Backend

```c
// src/storage/db_read.c
ik_result_t storage_db_read(ik_agent_ctx_t *agent, const char *uri) {
    // Strip ikigai:// prefix -> blocks/decisions.md
    const char *path = uri + strlen("ikigai://");

    // Query database
    ik_pg_result_t *res = db_query(
        "SELECT content FROM stored_assets WHERE path = $1",
        path
    );

    if (PQntuples(res) == 0) {
        return ERR("StoredAsset not found: %s", path);
    }

    const char *content = PQgetvalue(res, 0, 0);
    return OK(talloc_strdup(agent, content));
}

// src/storage/db_write.c
ik_result_t storage_db_write(ik_agent_ctx_t *agent,
                              const char *uri,
                              const char *content) {
    const char *path = uri + strlen("ikigai://");

    // Upsert (insert or update)
    ik_pg_result_t *res = db_query(
        "INSERT INTO stored_assets (path, content, updated_at) "
        "VALUES ($1, $2, NOW()) "
        "ON CONFLICT (path) DO UPDATE "
        "SET content = $2, updated_at = NOW()",
        path, content
    );

    if (PQresultStatus(res) != PGRES_COMMAND_OK) {
        return ERR("Failed to write StoredAsset: %s",
                   PQerrorMessage(agent->db));
    }

    return OK_VOID;
}
```

PostgreSQL-backed operations.

## Block Budget Enforcement

```c
// src/storage/db_check_budget.c
ik_result_t storage_db_check_budget(ik_agent_ctx_t *agent,
                                     const char *uri,
                                     const char *new_content) {
    const char *path = uri + strlen("ikigai://");

    // Get current total pinned block size
    int current_total = db_get_total_pinned_size(agent->id);

    // Get size of this document if it exists
    int existing_size = db_get_document_size(path);

    // Calculate new total if write succeeds
    int new_doc_size = estimate_tokens(new_content);
    int new_total = current_total - existing_size + new_doc_size;

    if (new_total > MAX_BLOCK_BUDGET) {
        return ERR(
            "Block budget exceeded: %dk/%dk tokens. "
            "This write would use %dk tokens. "
            "Use /compact or /unpin to free space.",
            current_total / 1000,
            MAX_BLOCK_BUDGET / 1000,
            new_doc_size / 1000
        );
    }

    return OK_VOID;
}
```

**Budget check happens before write**. Error returned to agent if exceeded.

## Tool Error Handling

When a StoredAsset write fails due to budget:

```json
// Agent tries write
{
  "tool": "file_write",
  "path": "ikigai:///blocks/large-doc.md",
  "content": "[80k tokens]"
}

// Tool returns error
{
  "error": "Block budget exceeded: 95k/100k tokens. This write would use 80k tokens. Use /compact or /unpin to free space."
}

// Agent sees error, adapts behavior
{
  "tool": "file_read",
  "path": "ikigai:///blocks/old-block.md"
}
{
  "tool": "slash_command",
  "command": "/compact blocks/old-block"
}
// [Compresses old block, frees 20k]

// Retry original write
{
  "tool": "file_write",
  "path": "ikigai:///blocks/large-doc.md",
  "content": "[80k tokens]"
}
// Success
```

**Error provides back-pressure**. Agent must actively manage budget.

## System Prompt Guidance

```markdown
## File Operations

You have file_read, file_write, and file_edit tools that work on paths.

**Filesystem paths**: Regular paths
- src/main.c
- tests/test_db.c
- README.md

**StoredAssets**: ikigai:// URIs
- ikigai:///blocks/decisions.md
- ikigai:///skills/ddd.md
- ikigai:///blocks/user-preferences.md

Operations work identically on both storage backends.

**StoredAsset budget**: 100k tokens total
- Writes fail with error if budget exceeded
- Use /compact to compress blocks
- Use /unpin to remove from context

**When to use StoredAssets vs filesystem:**
- StoredAssets: Cross-project patterns, frequently referenced, want pinned
- Filesystem: Project code/docs, version controlled, tool integration

When in doubt, ask the user where to save.
```

## Cross-References

StoredAssets can reference files, files can reference StoredAssets:

**StoredAsset referencing file**:
```markdown
<!-- ikigai:///blocks/architecture.md -->
# Architecture Decisions

## Database Layer
Implementation: src/db/connection.c
Schema: src/db/schema.sql

See also: ikigai:///blocks/error-patterns.md
```

**File referencing StoredAsset**:
```markdown
<!-- README.md (filesystem) -->
# Project Guidelines

Coding standards: ikigai:///blocks/coding-standards.md
Architecture decisions: ikigai:///blocks/architecture.md
Error patterns: ikigai:///blocks/error-patterns.md
```

Tools handle references transparently.

## Prompt Expansion

User input supports expansion markers:

```
User typing:
> Implement auth following @@auth-patterns and update @src/auth.c

Expanded before sending to LLM:
> Implement auth following [CONTENT FROM ikigai:///blocks/auth-patterns.md]
  and update [CONTENT FROM src/auth.c]
```

**Expansion rules**:
- `@path` → Filesystem file content
- `@@path` → StoredAsset content (strips ikigai:// prefix)
- Both expand transparently at submit time

**Fuzzy finders**:
- `@` triggers filesystem file finder
- `@@` triggers StoredAsset finder
- User selects from matches, path inserted

## Agent Workflows

### Creating New StoredAsset

```json
// Agent decides to create block
{
  "tool": "file_write",
  "path": "ikigai:///blocks/new-patterns.md",
  "content": "# New Patterns\n\n..."
}

// Block created, not auto-pinned yet
// Agent can suggest pinning:
"I've created a new patterns block at ikigai:///blocks/new-patterns.md.
Would you like me to pin it so it's always available?"
```

### Appending to Existing Block

```json
// Option 1: Read + modify + write
{
  "tool": "file_read",
  "path": "ikigai:///blocks/patterns.md"
}
// [Agent modifies in memory]
{
  "tool": "file_write",
  "path": "ikigai:///blocks/patterns.md",
  "content": "[modified content]"
}

// Option 2: Use file_edit
{
  "tool": "file_edit",
  "path": "ikigai:///blocks/patterns.md",
  "old": "[END]",
  "new": "\n## New Pattern\n...\n[END]"
}
```

Both approaches work. Agent chooses based on modification complexity.

### Compacting a Block

```json
// 1. Read current content
{
  "tool": "file_read",
  "path": "ikigai:///blocks/decisions.md"
}

// 2. Analyze and compress (in agent reasoning)

// 3. Write compressed version
{
  "tool": "file_write",
  "path": "ikigai:///blocks/decisions.md",
  "content": "[compressed content]"
}
```

Same tools. Just different intent.

## Implementation Benefits

**For LLM**:
- No special cases to learn
- Paths are just strings
- Same operations everywhere

**For tools**:
- One code path per operation
- Simple URI routing
- Clean separation of concerns

**For users**:
- Consistent mental model
- Operations transfer between storage types
- No surprises

**For implementation**:
- Single tool set maintains
- New storage backends easy to add
- Testing simplified (mock both backends same way)

---

The unified interface makes StoredAssets feel like "just another filesystem" to the agent, while providing entirely different backend capabilities (budget enforcement, pinning, auto-summary integration).
