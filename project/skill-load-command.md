# !load / !unload — Skill Loading Commands

## Overview

`!load` reads a skill file from disk, applies template processing (including positional argument substitution), and injects the content as a system prompt block. `!unload` removes a previously loaded skill. Skills are stored in a separate `loaded_skills[]` array on the agent struct (parallel to `pinned_paths[]`), with a `load_position` tracking where in the conversation the load occurred. Skills survive sliding window pruning but are dropped by `/clear` and `/rewind`.

## Syntax

```
!load <skill-name> [arg1 arg2 ...]
```

**Examples:**

```
!load database
!load database users
!load deploy staging us-east-1
```

## Behavior

### !load

1. Parse skill name and positional arguments from input
2. Construct URI: `ik://skills/<skill-name>/SKILL.md`
3. Resolve URI to filesystem path via `ik_paths_translate_ik_uri_to_path()` — all URI resolution goes through this single function so future changes to path resolution are picked up by all callers
4. Read file content via doc cache (`ik_doc_cache_get()`)
5. Apply template processing via `ik_template_process_()` with positional args injected as `${1}`, `${2}`, etc. alongside standard template variables (`${agent.*}`, `${config.*}`, `${env.*}`, `${func.*}`)
6. Check for duplicate: if a skill with the same name is already in `loaded_skills[]`, replace its `content` and `load_position` in place
7. Store the **instantiated content** in a `skill_load` database event — the fully resolved content is captured at creation time so the skill survives even if the source file changes or disappears later
8. Add to (or update in) `agent->loaded_skills[]` with `load_position = agent->message_count`
9. Call `ik_token_cache_invalidate_system()` to force token recount
10. Display confirmation in scrollback

**Template freeze note:** Both positional substitution and template variables are resolved at load time. The fully resolved content is stored in the database event. Template variables (e.g., `${func.now}`) reflect values at the time of loading, not at the time of each LLM request. Per-turn re-evaluation is a potential future enhancement.

### !unload

1. Parse skill name from input
2. Scan `loaded_skills[]` by name
3. If not found: display warning "Skill not loaded: <name>" in scrollback, no DB event
4. If found: remove entry (shift remaining entries down), store `skill_unload` database event
5. Call `ik_token_cache_invalidate_system()` to force token recount
6. Display confirmation in scrollback

Rewind past an `!unload` restores the skill — the original `skill_load` event is still in history and the `skill_unload` event gets dropped by the rewind.

## Skill Resolution

Skills are identified by short name. Resolution constructs a `ik://` URI and resolves through the standard path translation:

```
!load database  →  ik://skills/database/SKILL.md
                →  ik_paths_translate_ik_uri_to_path()
                →  $IKIGAI_STATE_DIR/skills/database/SKILL.md
```

In local development, `IKIGAI_STATE_DIR=$PWD/state`, so skills live at `state/skills/`.

Single search location via `ik://skills/` for now. Future releases may add additional URI namespaces or search paths — all callers use the same resolution function, so new behavior propagates automatically.

### Existing URI Resolution Infrastructure

The `ik://` URI system already exists and is fully implemented:

- **`ik_paths_translate_ik_uri_to_path()`** — `paths.c:174-298`, declared in `paths.h:40`
  ```c
  res_t ik_paths_translate_ik_uri_to_path(TALLOC_CTX *ctx, ik_paths_t *paths,
                                           const char *input, char **out);
  ```
- **Reverse function:** `ik_paths_translate_path_to_ik_uri()` — `paths.c:300-419`, `paths.h:47`
- **Mockable wrappers:** `shared/wrapper_internal.h:26-34` — `ik_paths_translate_ik_uri_to_path_()` and `ik_paths_translate_path_to_ik_uri_()`

**Two URI namespaces exist today:**

| Namespace | Pattern | Resolves to |
|-----------|---------|-------------|
| Generic | `ik://path/to/file` | `$IKIGAI_STATE_DIR/path/to/file` |
| System | `ik://system/path` | `$IKIGAI_DATA_DIR/system/path` |

Skill URIs (`ik://skills/database/SKILL.md`) use the generic namespace, resolving to `$IKIGAI_STATE_DIR/skills/database/SKILL.md`. No new namespace needed.

**Existing callers:** tool executor (`tool_executor.c:42,58`), doc cache (`doc_cache.c:49,103`), pin commands (`commands_pin.c`).

**Tests:** `tests/unit/apps/ikigai/paths/paths_translate_test.c` (22 cases), `paths_translate_round_trip_test.c`.

**Full module docs:** `project/modules/paths.md`

## Template Processing

Skill files are processed through the existing template engine (`ik_template_process_()` at `agent_system_prompt.c:52-70`), the same engine used for pinned documents. Positional arguments from `!load` are injected as numeric template variables (`${1}`, `${2}`, etc.) alongside the standard namespaces.

**Available variables:**
- `${1}`, `${2}`, `${3}`, ... — positional arguments from `!load`
- `${agent.*}` — agent properties (uuid, name, etc.)
- `${config.*}` — configuration values
- `${env.*}` — environment variables
- `${func.*}` — computed values (now, cwd, hostname, etc.)

**Skill file (`SKILL.md`):**
```markdown
# Database Skill

## Table: ${1}

Schema and patterns for the ${1} table...
Agent: ${agent.uuid}
```

**Invocation:**
```
!load database users
```

**Result after substitution:**
```markdown
# Database Skill

## Table: users

Schema and patterns for the users table...
Agent: a1b2c3d4-...
```

Unreferenced arguments are ignored. Unreplaced placeholders (e.g., `${2}` when only one arg given) remain as literal text.

## In-Memory Representation

Skills are stored in a separate array on the agent struct, parallel to `pinned_paths[]` and `marks[]`:

```c
typedef struct {
    char *name;           // Skill name (e.g., "database")
    char *content;        // Fully resolved content (captured at load time)
    size_t load_position; // agent->message_count at time of load
} ik_loaded_skill_t;

// On ik_agent_ctx_t:
ik_loaded_skill_t **loaded_skills;  // Array of pointers (like marks[], pinned_paths[])
size_t loaded_skill_count;
```

Growth via `talloc_realloc(agent, agent->loaded_skills, ik_loaded_skill_t *, count + 1)` — linear, matching the pins/marks pattern. Each `ik_loaded_skill_t *` is talloc-allocated with agent as parent.

The `load_position` uses the same coordinate system as marks (`mark->message_index = agent->message_count`). This allows direct comparison with mark positions for rewind decisions.

Skills are NOT stored in `agent->messages[]`. The messages array contains only conversation messages (user, assistant, tool_call, tool_result). Skills sit alongside it as a parallel data structure.

## Conversation Lifecycle

### /clear — Drops All Skills

`/clear` wipes the conversation and all loaded skills. Same lifetime, same boundaries. After clear, both `messages[]` and `loaded_skills[]` start fresh from zero.

```c
// In /clear handler:
agent->loaded_skill_count = 0;
// (talloc frees skill entries when agent context is cleaned)
```

The `load_position` values from before the clear have no meaning after clear — they referenced positions in an array that no longer exists. This is fine because all skills are dropped.

### /rewind — Drops Skills After Target

Rewind truncates messages to the mark position. Skills loaded after that position are dropped:

```c
// After truncating messages to target_mark->message_index:
while (agent->loaded_skill_count > 0 &&
       agent->loaded_skills[agent->loaded_skill_count - 1].load_position >= target_mark->message_index) {
    agent->loaded_skill_count--;
}
```

Since skills are loaded sequentially, they're naturally ordered by `load_position`. Iterate from the end and trim. Each trimmed entry is freed via `talloc_free()`.

### Sliding Window Pruning — Skills Survive

The sliding window advances `context_start_index` past old conversation turns. This does NOT affect `loaded_skills[]` — skills are in a separate data structure that the pruning logic never touches. Skills remain in the system prompt regardless of how far the conversation window slides.

This is by design: skills are working knowledge the user explicitly loaded. Silently dropping them because old conversation turns were pruned would be surprising and unhelpful.

**Token accounting note:** Loaded skills increase the system prompt token count (via `ik_token_cache_get_system_tokens()`), which squeezes the conversation token budget. Heavy skill loading means more aggressive conversation pruning. This is correct behavior — the total token budget is finite, and system prompt tokens are part of it. No limit on loaded skill count or total token size — if skills alone consume most of the budget, the conversation window shrinks proportionally.

### Session Restore — Replay Plays Out Naturally

On session restore, events are replayed from the database in chronological order. No special replay logic is needed for skills — the event stream plays out naturally:

1. Replay encounters `skill_load` event → adds to (or replaces in) `loaded_skills[]` (same as live operation)
2. Replay encounters `skill_unload` event → removes from `loaded_skills[]` (same as live operation)
3. Replay encounters `rewind` event → rewind handler trims `loaded_skills[]` past target position (same code path as live rewind)
4. Replay encounters `clear` event → replay starts after the most recent clear (existing behavior), so skills before the clear are never replayed

The data structures may yo-yo during replay (skills added then removed by a subsequent rewind), but they end up containing the correct final state.

**Key invariant:** The rewind handler that trims `loaded_skills[]` is the same code whether running live or during replay. No separate "replay-aware" skill logic.

### Lifecycle Summary

| Event | Effect on loaded skills |
|-------|------------------------|
| `/clear` | All skills dropped (same lifetime as messages) |
| `/rewind` past load point | Skill dropped (`load_position >= target`) |
| `/rewind` before load point | Skill retained (`load_position < target`) |
| Sliding window pruning | Skills survive (separate data structure) |
| Session restore | Events replay naturally; rewind handler trims as needed |

### Child Agents (Fork)

Pinned documents (`/pin`) are inherited by child agents — see `commands_fork.c:40-55`.

Loaded skills follow a split rule based on fork type:

- **`/fork` (no prompt):** Skills are copied to the child. This is a "clone me" operation — the child should feel identical. Inherited skills have `load_position` reset to 0 (child's conversation starts fresh), so no `/rewind` in the child can accidentally drop them.
- **`/fork <prompt>`:** Child starts with empty `loaded_skills[]`. This is a "start a new specialist" operation — the child loads its own skills as needed.

The fork DB event includes the skill snapshot (like it does for `pinned_paths`) so replay reconstructs correctly.

### Contrast with `/pin`

| Property | `/pin` | `!load` |
|----------|--------|---------|
| Lifecycle | Durable — survives `/clear`, restarts | Ephemeral — dropped by `/clear` and `/rewind` |
| Survives pruning | Yes (not in messages[]) | Yes (not in messages[]) |
| Identity | Part of agent identity | Session working knowledge |
| Replay | Independent of clear boundaries | Respects clear boundaries |
| Storage | `pinned_paths[]` (paths, content resolved on demand) | `loaded_skills[]` (content captured at load time) |
| Input | File path | Skill name (resolved via `ik://` URI) |
| Template processing | `${...}` variables resolved on demand (every turn) | `${...}` variables resolved at load time (frozen) |
| Prefix | `/` (system operation) | `!` (context operation) |

## System Prompt Assembly

### Existing Block Architecture

The system prompt is **not a flat string** — it's a structured array of blocks, each with a cache control flag:

```c
// provider.h:176-182
struct ik_system_block {
    char *text;       /* System prompt text */
    bool cacheable;   /* Enable prompt caching for this block */
};

// In ik_request_t (provider.h:187-200):
ik_system_block_t *system_blocks;
size_t system_block_count;
```

Blocks are appended via `ik_request_add_system_block()` (`request.h:92-102`). Each provider serializes them differently — Anthropic uses `cache_control: {"type": "ephemeral"}` on cacheable blocks, OpenAI uses separate system messages, Google uses system parts.

**`ik_agent_build_system_blocks()`** — `agent_system_prompt.c:143-218` — currently builds:

1. Base system prompt (not cacheable)
2. Pinned documents (cacheable, durable) — each pin is a separate block via `ik_doc_cache_get()`
3. Previous-session summaries (cacheable)
4. Recent summary (not cacheable)

**`ik_agent_get_effective_system_prompt()`** — `agent_system_prompt.c:76-141` — returns a single flattened string for token counting. Priority: pinned files → `ik://system/prompt.md` → config → hardcoded default.

### With `!load`

`ik_agent_build_system_blocks()` becomes:

1. Base system prompt (not cacheable)
2. Pinned documents (cacheable, durable)
3. **Loaded skills (cacheable, ephemeral)** — iterates `agent->loaded_skills[]`
4. Previous-session summaries (cacheable)
5. Recent summary (not cacheable)

Each loaded skill becomes a separate cacheable system block. The LLM sees them as part of the system prompt, not as conversation messages.

**Token accounting:** `ik_token_cache_get_system_tokens()` (`token_cache.c:196-225`) calls `ik_agent_get_effective_system_prompt()`, which returns a single flattened string for token counting. With `!load`, this function must also concatenate loaded skill content (same `talloc_asprintf` pattern used for pinned docs at `agent_system_prompt.c:88-101`). The token cache code itself (`token_cache.c`) requires no changes. Both `!load` and `!unload` call `ik_token_cache_invalidate_system()` after modifying `loaded_skills[]` to force a recount on next access.

## Doc Cache

The doc cache already exists and is used by pinned documents. `!load` uses it to read skill files before applying positional substitution.

- **`ik_doc_cache_get()`** — `doc_cache.c:41-94`, declared in `doc_cache.h`
  ```c
  res_t ik_doc_cache_get(ik_doc_cache_t *cache, const char *path, char **out_content);
  ```
  Translates `ik://` URIs internally, caches file content, returns pointer owned by cache (caller must NOT free). Returns `ERR` on file read failure.

- **Structure** — `doc_cache.c:18-23` (opaque type, `doc_cache.h:10`):
  ```c
  struct ik_doc_cache {
      ik_paths_t *paths;
      ik_doc_cache_entry_t **entries;  // {path, content} pairs
      size_t count;
      size_t capacity;                 // Starts at 4, grows 2x
  };
  ```

- **Lifetime:** Per-agent, allocated with agent as talloc parent (`agent.c:122,206`). Stored at `agent->doc_cache` (`agent.h:166`). Freed automatically with agent.

- **Pinned doc usage:** Pin validation (`commands_pin.c:98`), system prompt assembly (`agent_system_prompt.c:91,192`).

- **Other API:** `ik_doc_cache_invalidate()` (single path), `ik_doc_cache_clear()` (all entries) — `doc_cache.h:26,29`.

- **Tests:** `tests/unit/apps/ikigai/doc_cache_test.c`
- **Full docs:** `project/modules/doc_cache.md`

## Event Storage

### Existing Event Infrastructure

Messages are stored in a `messages` table (`share/migrations/001-initial-schema.sql:41-48`):

```sql
CREATE TABLE messages (
    id BIGSERIAL PRIMARY KEY,
    session_id BIGINT NOT NULL REFERENCES sessions(id),
    kind TEXT NOT NULL,
    content TEXT,           -- Message text (nullable)
    data JSONB,             -- Structured metadata (nullable)
    created_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
);
```

Agent UUID column added in `003-messages-agent-uuid.sql:24-28`.

**Insert function** — `db/message.c:53-105`:
```c
res_t ik_db_message_insert(ik_db_ctx_t *db, int64_t session_id,
                           const char *agent_uuid, const char *kind,
                           const char *content, const char *data_json);
```

`data_json` is a plain text JSON string — PostgreSQL stores it in the `JSONB` column. Built with yyjson or `talloc_asprintf()`.

**Current VALID_KINDS[]** — `db/message.c:20-35`:
`clear`, `system`, `user`, `assistant`, `tool_call`, `tool_result`, `mark`, `rewind`, `agent_killed`, `command`, `fork`, `usage`, `interrupted`

**Conversation vs metadata** — `msg.c:13-31`: `ik_msg_is_conversation_kind()` returns true for `system`, `user`, `assistant`, `tool_call`, `tool_result`, `tool`. All others are metadata (not sent to LLM).

**Message struct** — `msg.h:28-34`:
```c
typedef struct {
    int64_t id;
    char *kind;
    char *content;
    char *data_json;
    bool interrupted;
} ik_msg_t;
```

**Metadata event payload patterns** (all use `data_json`, most have NULL `content`):
- `mark`: `{"label":"<string>"}` — `commands_mark.c:91-97`
- `rewind`: `{"target_message_id":<int64>,"target_label":"<string>"|null}` — `commands_mark.c:152-159`
- `agent_killed`: `{"killed_by":"user","target":"<uuid>","cascade":<bool>,"count":<int>}` — `commands_kill.c:85-98`
- `command`: `{"command":"<name>","args":"<string>"|null,"echo":"<input>"}` — `commands.c:244-259`
- `usage`: `{"input_tokens":<int>,"output_tokens":<int>,"thinking_tokens":<int>}` — `repl_event_handlers.c:187-200`
- `clear`, `interrupted`: NULL data_json — `commands_basic.c:204-205`, `repl_event_handlers.c:314-315`

### skill_load Event

The `!load` event follows the same pattern — kind `skill_load` (added to `VALID_KINDS[]`), metadata classification in `msg.c`, structured JSON payload in `data_json`:

```json
{
  "skill": "database",
  "args": ["users"],
  "content": "# Database Skill\n\n## Table: users\n..."
}
```

The fully resolved content (positional args + template variables) is captured at creation time. If the source file changes or disappears after loading, the skill content in the system prompt remains as it was when loaded.

### skill_unload Event

The `!unload` event uses kind `skill_unload` (also added to `VALID_KINDS[]` and classified as metadata in `msg.c`):

```json
{
  "skill": "database"
}
```

No content field — the unload just records which skill was removed. During replay, this event triggers removal from `loaded_skills[]`.

## Input Dispatch

The `!` prefix is a new input category. The dispatch point in `repl_actions_llm.c` (`ik_repl_handle_newline_action()`, currently at line ~219) checks for `/` to detect slash commands. A new check for `!` routes to the bang command dispatcher.

```
User input
  ├── starts with '/' → slash command dispatch (ik_cmd_dispatch)
  ├── starts with '!' → bang command dispatch (new)
  └── anything else   → send to LLM
```

The bang dispatcher parses the command name and routes to the appropriate handler. Initially `!load` and `!unload` exist; `!skillset` and others come later.

The bang command dispatcher follows the same mechanical patterns as the existing slash command dispatcher (`commands.c`): null-terminated input copy before buffer clear, echo to scrollback within the dispatcher, whitespace skipping after prefix, and error message for empty command (`!` alone). See `repl_actions_llm.c:~212-232` and `commands.c:96-107` for the patterns to mirror.

## Error Handling

| Error | Behavior |
|-------|----------|
| Skill not found | Warning in scrollback: "Skill not found: database" |
| File read failure | Warning in scrollback with error detail |
| No skill name given (`!load` alone) | Warning: "Usage: !load <skill-name> [args...]" |
| `!unload` skill not loaded | Warning in scrollback: "Skill not loaded: database" |
| No skill name given (`!unload` alone) | Warning: "Usage: !unload <skill-name>" |
| Empty bang command (`!` alone) | Warning in scrollback |

Errors are displayed in scrollback. No event is created in the database. No error is sent to the LLM.

## File Structure (Implementation)

New files:
- `apps/ikigai/bang_commands.c` — Bang command dispatcher, `!load` and `!unload` handlers
- `apps/ikigai/bang_commands.h` — Public interface

Modified files:
- `apps/ikigai/repl_actions_llm.c` — Add `!` prefix detection in input dispatch
- `apps/ikigai/agent.h` — Add `ik_loaded_skill_t` struct, `loaded_skills[]` (pointer array) and `loaded_skill_count` to `ik_agent_ctx_t`
- `apps/ikigai/agent_system_prompt.c` — Iterate `loaded_skills[]` in both `ik_agent_build_system_blocks()` (between pinned docs and session summaries) and `ik_agent_get_effective_system_prompt()` (for token counting)
- `apps/ikigai/marks.c` — Trim `loaded_skills[]` in rewind handler (`load_position >= target`)
- `apps/ikigai/commands_basic.c` — Clear `loaded_skills[]` in `/clear` handler
- `apps/ikigai/commands_fork.c` — Copy `loaded_skills[]` to child on bare `/fork` (no prompt); skip on `/fork <prompt>`
- `apps/ikigai/db/message.c` — Add `"skill_load"` and `"skill_unload"` to `VALID_KINDS[]`
- `apps/ikigai/msg.c` — Classify `"skill_load"` and `"skill_unload"` as metadata (not conversation)
- `apps/ikigai/event_render.c` — Render `skill_load` and `skill_unload` events during replay
- `apps/ikigai/repl/agent_restore_replay.c` — Handle `skill_load` and `skill_unload` events during session restore

## Skill File Format

A skill is a directory containing `SKILL.md`:

```
$IKIGAI_STATE_DIR/
  skills/
    database/
      SKILL.md
    deploy/
      SKILL.md
    style/
      SKILL.md
```

The `SKILL.md` file is plain markdown. No frontmatter required. Template placeholders (`${1}`, `${2}`, `${agent.*}`, etc.) are optional.

## Not In Scope

- `!skillset` (composite bundles) — future command
- Skill discovery / listing (`!skills`) — future command
- Per-turn re-evaluation of template variables in loaded skills — future enhancement (currently frozen at load time)
- Multiple search paths (project-local, user-global) — future enhancement
- Tab completion for skill names — future enhancement
