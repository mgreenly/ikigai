# Config System Design

## Core Principle

Default config is defined as constants in code. There are no config files. All config is per-agent, managed at runtime via `/config` commands.

## Config Data Model

Config is a single data structure used in two contexts:

- **Agent config** — attached to a running agent instance
- **Named template** — a standalone config not attached to any agent

Both are the same structure. Templates are not a separate concept.

## Agent Lifecycle and Config

### Root Agents

- Initialize from code constants (compile-time defaults)
- Can be modified at runtime via `/config set`
- Never die — persist for session lifetime
- Modified config becomes the baseline for all future children

### Forking

Fork performs a deep copy of the source config and applies optional overrides atomically. The child starts immediately with its final config.

```
/agent fork                              # copy from parent (default)
/agent fork --from scanner-template      # copy from named template
/agent fork --from agent-7               # copy from another agent's config
/agent fork --from agent-7 --config x=y  # copy from agent + overrides
```

- **Default source** is the parent agent's current config
- **`--from`** accepts a template name or any agent id
- **`--config`** applies per-instance overrides on top of the source
- Child starts immediately — no paused/limbo state

### Config Inheritance Rules

- Fork = deep copy at fork time. No read-through, no shared state.
- Post-fork changes to the parent (or source) do not affect existing children.
- Children forked later get the parent's config as it exists at that later time.

## Commands

### `/config set <key> <value>`

Sets a key on the current agent's config. Scope is always "this agent." Does not cascade to existing children.

### `/config get <key>`

Reads a key from the current agent's config.

### `/config template create <name>`

Creates a named template, initialized as a copy of the current agent's config.

### `/config template set <name> <key> <value>`

Sets a key on a named template.

## Templates

- Belong to the agent that created them (talloc child of agent context)
- Die when the owning agent dies
- Not copied to children on fork — templates are about how you spawn your children, not inherited by them
- Mutable — changes affect future forks, not past ones
- Reusable — fork many children from the same template

## Visibility

Any agent can read any other agent's config and use it as a `--from` source. This is a single-user system with no trust boundaries between agents.

## Schema

Valid keys and types are defined by the code constants at compile time. `/config set` validates against this schema — unknown keys and type mismatches are rejected.

## What This Design Rules Out

- Config files on disk
- Cascading writes to existing children
- Paused/limbo agent states for pre-fork configuration
- Post-fork parent-to-child config mutation
- Template composition/inheritance (create separate templates instead)
- Config persistence beyond agent lifetime (except roots)
