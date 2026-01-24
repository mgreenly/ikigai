# Context Management (v2)

## Overview

Ikigai conversations run indefinitely - across hours, days, or weeks, surviving restarts. Context persists until explicitly modified. This document describes the minimal toolkit for managing conversation context.

**Core principle**: Context changes only through explicit action, never implicitly.

---

## Context vs Database

**Database**: Immutable event log. Every message, tool call, and command is recorded permanently.

**Context**: The filtered view sent to the LLM. Modified through context commands.

Commands modify context, not the database. Full history remains queryable.

---

## Commands

| Command | Effect |
|---------|--------|
| `/clear` | Wipe everything |
| `/clear MARK` | Clear after mark (rewind) |
| `/mark NAME` | Create checkpoint |
| `/fork` | Child inherits full context |
| `/fork MARK` | Child inherits from mark onward |

Three commands. Two take an optional mark argument.

---

## /mark NAME

Create a named checkpoint in the conversation.

```
> /mark BEFORE_REFACTOR
Checkpoint 'BEFORE_REFACTOR' created.
```

**Names**: Case-sensitive. Descriptive names recommended: `TASK_START`, `WORKING_STATE`, `RISKY_CHANGE`.

**Uniqueness**: One mark per name per agent. Re-using a name updates the position.

**Storage**: Mark events stored in database. Never sent to LLM - invisible boundary markers.

---

## /clear

Two modes based on whether a mark is provided.

### Without Mark: Nuclear Reset

Wipe everything. Conversation begins fresh.

```
> /clear
Context cleared.
```

**Use when**: Starting unrelated work, or context is beyond recovery.

### With Mark: Rewind

Clear everything after the named mark. Context returns to that checkpoint.

```
> /clear BEFORE_REFACTOR
Rewound to 'BEFORE_REFACTOR'.
```

**Use when**: Experimental approach failed, want to try differently.

```
Timeline: [A] [B] [MARK] [C] [D] [E]

/clear MARK  →  [A] [B]
```

---

## /fork

Two modes based on whether a mark is provided.

### Without Mark: Full Inheritance

Child receives complete parent context. Both agents continue from same point.

```
> /fork
Forked. Child: abc123
```

**Use when**: Exploring two approaches in parallel. Divergence point.

### With Mark: Trimmed Inheritance

Child receives only context from mark onward. Parent history before mark is not inherited.

```
> /fork TASK_START
Forked. Child: abc123 (from TASK_START)
```

**Use when**: Delegating work to sub-agent. Clean handoff.

```
Timeline: [A] [B] [MARK] [C] [D]

/fork MARK  →  Child gets: [C] [D]
               Parent keeps: [A] [B] [MARK] [C] [D]
```

---

## Sub-Agent Pattern

The minimal sub-agent creation:

```
/mark RESEARCH
Investigate authentication approaches. Compare OAuth2, JWT, and session-based.
Report findings via /send when complete.
/fork RESEARCH
```

Three operations:
1. Mark the boundary
2. Write the goal
3. Fork from the mark

Child is born with exactly the goal. No parent history, no cleanup step.

### Why This Works

The parent must articulate the goal anyway - that's irreducible cognitive work. The system just needs to:
- Know where the goal starts (`/mark`)
- Create the child with that context (`/fork MARK`)

No complex tool calls. No elaborate setup. Just mark, goal, fork.

### Example: Parallel Research

```
Parent:
> /mark AUTH_RESEARCH
> Research OAuth2 implementation patterns. Report via /send.
> /fork AUTH_RESEARCH
Forked. Child: abc123

> /mark SESSION_RESEARCH
> Research session-based auth patterns. Report via /send.
> /fork SESSION_RESEARCH
Forked. Child: def456

> [continues other work while children research]
> /check-mail
```

Each child gets only its specific task. Parent coordinates results.

---

## Mark Lifecycle

### Creation

```
/mark NAME
```

Creates or updates the named mark at current position.

### Scope

Marks are agent-scoped. Each agent maintains its own marks.

### Inheritance

When `/fork MARK` executes:
- Child inherits messages from mark onward
- Child does NOT inherit parent's marks
- Child can create its own marks

### Persistence

Marks survive restarts. Stored in database, replayed on session load.

---

## Command Interactions

### Mark Then Clear (Rewind)

```
/mark CHECKPOINT
[experimental work]
/clear CHECKPOINT
```

Returns to checkpoint. Experimental work removed from context.

### Mark Then Fork (Delegation)

```
/mark TASK
[goal description]
/fork TASK
```

Child gets goal only. Parent keeps everything.

### Multiple Marks

```
/mark PHASE_1
[work]
/mark PHASE_2
[work]
/clear PHASE_2    # rewind to PHASE_2
/clear PHASE_1    # rewind further to PHASE_1
```

Marks are independent. Clear to any mark.

---

## Design Rationale

### Why No "Clear Before"?

Early designs included `/clear --before MARK` to keep only content after a mark. But `/fork MARK` handles the primary use case (sub-agent handoff) more elegantly - the trimming happens at birth, not as a cleanup step.

### Why Marks Instead of Message IDs?

Names are semantic. `BEFORE_RISKY_CHANGE` communicates intent. Message ID 4523 does not.

### Why Fork Takes a Mark?

Alternative: child runs `/clear --before MARK` after fork. But this wastes tokens - child inherits full context only to immediately discard it. Better to trim at fork time.

---

## LLM Tool Interface

LLMs access slash commands through a single `slash` tool. This provides a controlled subset of commands - human-only commands like `/exit` are not exposed.

### Schema

```json
{
  "name": "slash",
  "description": "Execute a slash command",
  "parameters": {
    "type": "object",
    "properties": {
      "command": {
        "type": "string",
        "enum": ["mark", "clear", "fork", "send", "check-mail", "kill"],
        "description": "The command to execute"
      },
      "args": {
        "type": "string",
        "description": "Command arguments as a single string"
      }
    },
    "required": ["command"]
  }
}
```

### Usage Examples

```json
{"tool": "slash", "command": "mark", "args": "TASK_START"}
{"tool": "slash", "command": "fork", "args": "TASK_START"}
{"tool": "slash", "command": "send", "args": "abc123 task complete"}
{"tool": "slash", "command": "check-mail"}
{"tool": "slash", "command": "clear", "args": "CHECKPOINT"}
{"tool": "slash", "command": "clear"}
{"tool": "slash", "command": "kill", "args": "abc123"}
```

### Command Access Split

**LLM-accessible (via `slash` tool):**

| Command | Args | Purpose |
|---------|------|---------|
| `mark` | `NAME` | Create checkpoint |
| `clear` | `[MARK]` | Reset context (optionally to mark) |
| `fork` | `[MARK]` | Create child agent |
| `send` | `UUID MESSAGE` | Message another agent |
| `check-mail` | none | Read inbox |
| `kill` | `UUID` | Terminate child agent |

**Human-only (not exposed to LLM):**

| Command | Purpose |
|---------|---------|
| `/exit` | End session |
| `/help` | Get help |
| `/model` | Switch model |
| `/system` | View system prompt |

### Design Rationale

**Single tool**: LLM has one interface for all agent operations. Minimal cognitive load.

**Enumerated commands**: Schema lists exactly what's available. No discovery by trial and error.

**String args**: Simple for LLM to construct. Tool parses per-command. Keeps schema minimal.

**Controlled subset**: Human-only commands excluded. LLM can't accidentally exit or change models.

---

## Summary

| Goal | Command |
|------|---------|
| Start fresh | `/clear` |
| Rewind to checkpoint | `/clear MARK` |
| Create checkpoint | `/mark NAME` |
| Duplicate agent (explore two paths) | `/fork` |
| Delegate to sub-agent | `/mark` + goal + `/fork MARK` |

Three commands with optional mark arguments. LLM accesses via `slash` tool with enumerated command set. Each operation exists exactly once. No redundant paths.
