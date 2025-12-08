# Agents Layered Resolution

## Summary

Commands, skills, and personas resolve through a three-layer precedence chain with namespace prefixes for explicit access to overridden items.

## Motivation

- Ikigai ships with built-in commands, skills, and personas
- Users need to customize per-project (`.agents/`) and globally (`~/.config/ikigai/`)
- User customizations should override built-ins by default
- Overridden built-ins must remain accessible when needed
- Composition should allow extending built-ins rather than fully replacing them

## Design

### Resolution Layers (highest to lowest priority)

| Priority | Layer | Path |
|----------|-------|------|
| 1 | Project-local | `.agents/` |
| 2 | User global | `~/.config/ikigai/` |
| 3 | Built-in | `<exe_dir>/../share/ikigai/agents/` |

### Directory Structure

Each layer contains the same structure:

```
commands/
skills/
personas/
```

### Built-in Path Resolution

Built-ins are located relative to the executable:

```
<exe_dir>/../share/ikigai/agents/
```

Resolved at runtime via `/proc/self/exe` (Linux-only; cross-platform support deferred).

### Install Layout

```
<prefix>/
  bin/
    ikigai
  share/
    ikigai/
      agents/
        commands/
        skills/
        personas/
```

### Namespace Prefixes

Explicit access to specific layers when overrides exist:

- `/persona architect` — resolves using priority order (project → global → built-in)
- `/persona builtin:architect` — forces built-in layer
- `/persona global:architect` — forces user global layer

### Composition

Personas and skills can reference items from specific layers:

```json
["builtin:default", "builtin:ddd", "my-custom-skill"]
```

This allows extending built-ins rather than fully replacing them.

## Rationale

- **Relative path for built-ins**: Enables relocatable installs without compile-time path baking or environment variables
- **Three layers**: Matches common patterns (git config, shell PATH, CSS cascade) users already understand
- **Namespace prefixes**: Simple, explicit disambiguation without complex override syntax
- **Linux-only initially**: `/proc/self/exe` is straightforward; cross-platform can be added when needed
