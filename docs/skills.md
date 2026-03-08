# Skills

Skills are chunks of working knowledge that you inject into the LLM's system prompt on demand. Use `!load` to add a skill, `!unload` to remove it, and the LLM will follow that knowledge for the rest of the session.

## What are skills?

A skill is a markdown file stored on disk. When you load it, its content is resolved and appended to the system prompt — the LLM sees it as background context, not as part of the conversation.

Skills are **ephemeral working knowledge**. They are dropped when you `/clear` or `/rewind` past the point where you loaded them. They are tied to the current session, not to the agent's identity.

This contrasts with [`/pin`](commands.md#pin), which is **durable identity**. Pinned documents survive restarts and `/clear` — they are part of who the agent is. Skills are what the agent knows right now.

| Property | `/pin` | `!load` |
|----------|--------|---------|
| Lifecycle | Durable — survives `/clear` and restarts | Ephemeral — dropped by `/clear` and `/rewind` |
| Purpose | Agent identity and permanent context | Session working knowledge |
| Template variables | Re-evaluated each turn | Frozen at load time |
| Input | File path | Skill name |

## Using skills

### !load

```
!load <skill-name> [arg1 arg2 ...]
```

Loads a skill by name. Optional positional arguments are substituted into the skill file as `${1}`, `${2}`, etc.

```
!load database
!load database users
!load deploy staging us-east-1
```

If the skill is already loaded, `!load` replaces it in place with the new arguments (if any).

### !unload

```
!unload <skill-name>
```

Removes a previously loaded skill from the system prompt.

```
!unload database
```

If the skill is not currently loaded, a warning is displayed and nothing changes.

### Common workflows

Load a general-purpose skill for the session:

```
!load database
```

Load a parameterized skill targeting a specific resource:

```
!load deploy staging us-east-1
```

Replace a loaded skill with updated arguments:

```
!load database orders
```

Remove a skill you no longer need:

```
!unload deploy
```

## Lifecycle behavior

### /clear — skills are dropped

`/clear` wipes the conversation and all loaded skills. After a clear, `loaded_skills` starts fresh from zero. Skills and conversation messages share the same lifetime — both are wiped together.

### /rewind — skills trimmed to target

`/rewind` rolls the conversation back to a mark. Any skill loaded after that mark is dropped. Skills loaded before the mark are kept.

For example, if you loaded a skill after mark `A` and then rewound to mark `A`, that skill would be gone. A skill loaded before `A` would survive.

Rewinding past an `!unload` restores the skill — the `skill_load` event is still in history and the `skill_unload` event gets dropped by the rewind.

### Sliding window pruning — skills survive

As conversations grow, old turns slide off the back of the active context. This does **not** affect loaded skills. Skills are in a separate data structure that the sliding window never touches.

Skills remain in the system prompt regardless of how far the conversation window has advanced. A skill you loaded 200 turns ago is still active.

See [Context Window](context-window.md) for how the sliding window works.

### Session restore

When you restart ikigai, the session is restored by replaying events from the database. Skills are restored automatically — `skill_load` and `skill_unload` events are replayed in order, and the final state matches what it was before the restart. No special handling needed.

### /fork

When you fork a child agent:

- **`/fork` (bare fork, no prompt):** Loaded skills are copied to the child. This creates an identical clone — the child has the same skills active from the start.
- **`/fork <prompt>` (prompted fork):** The child starts with no loaded skills. The child loads whatever skills it needs for its own task.

## Token budget impact

Loaded skills increase the system prompt size. Each active skill uses tokens that would otherwise be available for conversation history. Heavy skill loading means more aggressive sliding window pruning.

There is no cap on the number of skills or their total size. If you load very large skills, the conversation window shrinks proportionally to stay within the total token budget.

## Authoring skills

### Directory structure

Skills live under `$IKIGAI_STATE_DIR/skills/`:

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

In local development, `IKIGAI_STATE_DIR` is typically `$PWD/state`, so skills live at `state/skills/`.

Each skill is a directory named after the skill, containing a single `SKILL.md` file. The skill name used in `!load` matches the directory name.

### File format

`SKILL.md` is plain markdown. No frontmatter required. Write whatever context the LLM needs.

```markdown
# Database Patterns

Use parameterized queries. Never interpolate user input into SQL strings.

Connection pooling is managed by the application layer — do not open raw connections.
```

### Template variables

Skill files support template substitution. Variables are resolved at load time.

**Positional arguments** — passed as `!load` arguments:

```
!load deploy staging us-east-1
```

| Variable | Value |
|----------|-------|
| `${1}` | `staging` |
| `${2}` | `us-east-1` |

**Standard variables:**

| Namespace | Examples | Description |
|-----------|---------|-------------|
| `${agent.*}` | `${agent.uuid}`, `${agent.name}` | Agent properties |
| `${config.*}` | `${config.provider}` | Configuration values |
| `${env.*}` | `${env.HOME}` | Environment variables |
| `${func.*}` | `${func.now}`, `${func.cwd}` | Computed values |

**Example skill using positional arguments:**

```markdown
# Deploy Skill

Environment: ${1}
Region: ${2}

Use the ${1} environment for all operations. Target region is ${2}.
Deploy via the standard pipeline — no manual server access.
```

Invoked as:

```
!load deploy staging us-east-1
```

Resolves to:

```markdown
# Deploy Skill

Environment: staging
Region: us-east-1

Use the staging environment for all operations. Target region is us-east-1.
Deploy via the standard pipeline — no manual server access.
```

Unreferenced arguments are ignored. Unreplaced placeholders (e.g., `${2}` when only one arg given) remain as literal text in the skill.

### Template freeze

All template variables — including `${func.now}` and `${env.*}` — are resolved **at load time**. The content is captured once and stays frozen for the rest of the session. The system prompt does not re-evaluate skill templates on each turn.

This means:
- `${func.now}` captures the timestamp when you typed `!load`, not the current time on each request.
- `${env.FOO}` captures the environment variable's value at load time.

If you need updated values, `!unload` and `!load` again.

### Best practices

**Keep skills focused.** A skill named `database` should cover database patterns, not the entire codebase. Load multiple targeted skills rather than one large catch-all.

**Use positional arguments for specifics.** If the same skill applies to different tables, regions, or environments, parameterize it with `${1}` rather than duplicating the file.

**Name skills after their purpose.** Short, lowercase names matching the directory: `database`, `deploy`, `style`, `testing`.

**Plain prose works best.** Write for the LLM, not for humans skimming a README. State constraints directly: "Always use parameterized queries." "Never catch errors silently."

## Error messages

| Situation | Message |
|-----------|---------|
| Skill file not found | `Skill not found: <name>` |
| File read failure | Warning with error detail |
| `!load` with no name | `Usage: !load <skill-name> [args...]` |
| `!unload` skill not loaded | `Skill not loaded: <name>` |
| `!unload` with no name | `Usage: !unload <skill-name>` |

Errors appear in the scrollback. No database event is created. Nothing is sent to the LLM.

## See also

[Context Window](context-window.md), [Commands](commands.md)
