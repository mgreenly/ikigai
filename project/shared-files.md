# Shared Files

## Problem

Agents need access to shared knowledge, templates, and reference material. The original rel-11 design proposed database-backed storage with `ikigai:///` URIs, but this approach has a critical flaw: URIs break bash tool compatibility.

Since ikigai heavily uses shell scripts (make, test runners, jj commands, etc.), those scripts need plain file paths. A database-backed URI scheme would force us to build adapters for every tool, creating unnecessary complexity and friction.

## Solution

Shared filesystem at `$IKIGAI_DATA_DIR/files/` with automatic versioning to protect against agent corruption.

**Key insight**: Agents are unreliable and will corrupt files. Shared files MUST have automatic versioning for recovery, but the versioning must be completely transparent to agents—they should just write files normally.

## Architecture

### File Structure

```
$IKIGAI_DATA_DIR/files/
├── system/          # Owned by ikigai (installed files)
│   ├── prompt.md    # Default system prompt
│   └── ...          # Other system resources
├── knowledge/       # Shared reference material (user/agent editable)
├── templates/       # File templates (user/agent editable)
└── .versions/       # Automatic versioning (hidden)
    └── knowledge_auth-patterns.md.1736252000
```

### Versioning

**Mechanism**: inotify background thread in ikigai process

**Storage format**: Flat file copies with timestamp
- Pattern: `path_to_file.timestamp`
- Example: `knowledge_auth-patterns.md.1736252000`

**Watch strategy**: Recursive inotify on `files/`, filter in event handler
```c
inotify_add_watch(fd, "$IKIGAI_DATA_DIR/files",
                  IN_CLOSE_WRITE | IN_CREATE | IN_MOVED_TO);

void handle_file_event(struct inotify_event *event) {
    // Skip versioning directory
    if (strstr(filepath, "/.versions/")) return;

    // Skip temp/swap files
    if (strstr(event->name, ".swp") ||
        strstr(event->name, ".tmp") ||
        event->name[0] == '.') return;

    // Version the file
    version_file(filepath);
}
```

**Delete handling**: Versions persist after deletion. If file gets deleted and recreated with same path, versions accumulate chronologically. No special delete tracking needed.

**Why flat files, not git**:
- Dead simple implementation (just copy on write)
- No git dependency for core functionality
- Transparent debugging (`ls .versions/` shows everything)
- Easy to migrate to git later if storage becomes a problem
- Start simple, add compression if needed

### Tool Integration

**URI scheme**: `ik://` prefix for concise file references

All file tools expand `ik://` URIs before execution:
```
ik://system/prompt.md        → $IKIGAI_DATA_DIR/files/system/prompt.md
ik://knowledge/auth.md       → $IKIGAI_DATA_DIR/files/knowledge/auth.md
ik://templates/task.md       → $IKIGAI_DATA_DIR/files/templates/task.md
```

**Expansion implementation**:
```c
char* expand_ik_uri(const char *path) {
    if (strncmp(path, "ik://", 5) == 0) {
        return format_string("%s/files/%s", data_dir, path + 5);
    }
    return strdup(path);  // Not an ik:// URI, return as-is
}
```

**Tool support**: Read, Write, Edit, Glob all expand `ik://` paths before execution

**Agent interface**:
- Agents use `ik://` URIs exclusively (e.g., `ik://knowledge/auth.md`)
- Underlying filesystem location is implementation detail (not exposed to agents)
- URI expansion happens in C code, not in prompts or model logic
- Works consistently across all tools
- Bash tools receive expanded paths transparently

**Lifecycle guarantee**: Agents can only run when ikigai is running, therefore ALL agent file modifications are captured by the versioning system. Zero coverage gaps.

### System Prompt

**Default location**: `ik://system/prompt.md`

**Per-agent override**:
- Agents can use `/system-prompt <path>` command to override
- Custom path stored in agents table (supports `ik://` URIs)
- Falls back to default on error loading custom path

**Database schema**:
```sql
ALTER TABLE agents ADD COLUMN system_prompt TEXT
    DEFAULT 'ik://system/prompt.md';
```

**Loading logic**:
```c
char* load_system_prompt(struct agent *agent) {
    char *prompt_path = expand_ik_uri(agent->system_prompt);
    char *content = read_file(prompt_path);

    if (!content) {
        // Fallback to default
        char *default_path = expand_ik_uri("ik://system/prompt.md");
        content = read_file(default_path);
    }

    return content;
}
```

### System Prompt Documentation

Agents receive minimal documentation in their system prompt:

```markdown
## Shared Files

You have access to shared files via the `ik://` URI scheme:

- `ik://system/` - System files (prompt, configuration)
- `ik://knowledge/` - Shared reference documentation and patterns
- `ik://templates/` - File templates

Use `ik://` paths in any file tool:
- Read: `ik://knowledge/auth-patterns.md`
- Edit: `ik://templates/task.md`
- Glob: `ik://knowledge/*.md`

Bash tools also work with `ik://` paths.
```

**Key point**: No mention of `$IKIGAI_DATA_DIR` or filesystem structure. Pure abstraction.

## Implementation

### C Module Structure

**New module**: `src/file_versioning.c`

**Components**:
- Background thread with inotify event loop
- Recursive directory watch on `$IKIGAI_DATA_DIR/files/`
- Event filter (skip `.versions/`, temp files)
- File copy on write events

**Integration points**:
- Initialize at ikigai startup
- Add inotify fd to main event loop
- Handle events in background thread
- Expand `ik://` URIs in all tool implementations (Read, Write, Edit, Glob)
- Expand `ik://` URIs in bash command parameters before execution

### Installation

**Initial setup**:
```makefile
install:
    mkdir -p $(DESTDIR)$(PREFIX)/share/ikigai/files/system
    mkdir -p $(DESTDIR)$(PREFIX)/share/ikigai/files/knowledge
    mkdir -p $(DESTDIR)$(PREFIX)/share/ikigai/files/templates
```

**System file installation**:
```makefile
ifdef FORCE
    # Overwrite system files (for recovery)
    install -m 644 data/system/*.md \
        $(DESTDIR)$(PREFIX)/share/ikigai/files/system/
else
    # Preserve existing files (normal upgrade)
    [ -f $(DESTDIR)$(PREFIX)/share/ikigai/files/system/prompt.md ] || \
        install -m 644 data/system/*.md \
            $(DESTDIR)$(PREFIX)/share/ikigai/files/system/
endif
```

**Upgrade workflow**:
- `make install` - Preserves user customizations to system files
- `make install FORCE=1` - Restores defaults (recovery from corruption)

**Note**: System files are versioned like any other file. If agent corrupts `system/prompt.md`, user can restore from `.versions/` or use `make install FORCE=1`.

## URI and Path Expansion

**`ik://` URI expansion**: All tools expand `ik://` URIs before execution
```c
char* expand_ik_uri(const char *path) {
    if (strncmp(path, "ik://", 5) == 0) {
        return format_string("%s/files/%s", data_dir, path + 5);
    }
    return strdup(path);  // Not an ik:// URI, return as-is
}
```

**File tools** (Read, Write, Edit, Glob):
```c
char *resolved_path = expand_ik_uri(user_provided_path);
// Use resolved_path for actual file operations
```

**Bash tool**: Expand `ik://` in command string before execution
```c
// Agent provides: cat ik://knowledge/auth.md
char *expanded_cmd = expand_ik_uris_in_string(command);
// Bash executes: cat /home/user/.local/share/ikigai/files/knowledge/auth.md
execvp("bash", ["-c", expanded_cmd]);
```

**Internal storage**: Use `$IKIGAI_DATA_DIR` in database strings and config files
- Enables different PREFIX install locations
- Expansion happens at runtime
- Paths remain valid across reinstalls

## Future Considerations

The initial implementation deliberately does NOT address these concerns. They are documented here for future reference:

### 1. Recovery/Cleanup Tools

Versions accumulate indefinitely. Future releases may need:
- `ikigai files versions <path>` - List all versions of a file
- `ikigai files restore <path> <timestamp>` - Restore specific version
- `ikigai files cleanup --keep=N` - Keep last N versions per file
- `ikigai files cleanup --days=N` - Keep last N days of versions

### 2. Error Handling

**Disk full scenario**: What happens when `.versions/` fills the disk?
- Current: Copy fails silently
- Future: Log warning, alert user, stop versioning gracefully

**inotify failures**: System out of watches, thread crashes, etc.
- Current: No detection
- Future: Monitor thread health, warn user if versioning stops

### 3. Initial Content

What ships in `files/` on first install?
- Current: Empty directories (maybe README.md placeholders)
- Future: Useful default content (example patterns, task templates, reference docs)

### 4. Concurrent Writes

Two agents modify same file simultaneously:
- Current: Both events captured, both versioned (different timestamps), last write wins
- Future: Consider file locking, conflict detection, or accept current behavior

### 5. Performance

**Versioning overhead**: Copy on every write could be slow for large files or high churn
- Current: Synchronous copy in event handler
- Future: Async copying, throttling (max 1 version per file per second), size limits

### 6. Documentation Location

Where should shared files be documented for end users?
- `project/` (developer docs) ✓
- `man ikigai-files` (user manual)
- `$IKIGAI_DATA_DIR/files/system/README.md` (self-documenting)

### 7. Storage Optimization

If flat file storage becomes problematic:
- Migrate to git (compression + deduplication)
- Add gzip compression to `.versions/` files
- Implement copy-on-write deduplication
- Add cleanup policies (keep last N versions)

### 8. System File Protection

Should `files/system/` be read-only for agents?
- Current: No protection, rely on versioning for recovery
- Future: Permission-based protection, or keep current approach

## Design Rationale

**Filesystem over database**:
- Bash tools work natively (cat, grep, ls)
- No URI parsing or adapters needed
- Simpler implementation
- Easier to debug and inspect

**Flat files over git**:
- Simpler initial implementation
- No external dependencies
- Transparent storage
- Easy to migrate later if needed

**inotify over manual versioning**:
- Completely transparent to agents
- No code changes in agent logic
- Catches all modifications (even from bash)
- Background thread, zero agent overhead

**Filter in handler over selective watches**:
- Handles new directories automatically
- Single code path for all events
- Simpler logic, fewer edge cases

**Environment variables over absolute paths**:
- Supports different install prefixes
- Runtime configuration flexibility
- Standard Unix pattern

**`ik://` URIs over full env var paths**:
- Concise and clear (`ik://knowledge/auth.md` vs `$IKIGAI_DATA_DIR/files/knowledge/auth.md`)
- Unambiguous (clearly not a regular filesystem path)
- Code-level expansion (no model/prompt awareness needed)
- Familiar URI pattern for users
- Easy to detect and expand in C code

## Tradeoffs

| Decision | Cost | Benefit |
|----------|------|---------|
| Flat files vs git | Higher storage use | Simpler, no dependencies |
| Auto-version everything | Disk space grows | Complete protection, zero agent awareness |
| Background thread | Minor resource use | Zero latency for agents |
| No cleanup in v1 | Storage grows unbounded | Simpler implementation, optimize later |
| System files unprotected | Agents can corrupt | Versioning provides recovery, simpler permissions |
