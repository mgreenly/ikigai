---
name: task-authoring
description: Task Authoring skill for the ikigai project
---

# Task Authoring

Guidance for creating task files from requirements (user stories, bugs, gaps).

## Process

1. **Review source material** - Read and understand the requirements thoroughly
2. **Think deeply** - Consider edge cases, dependencies, and implementation approaches
3. **Break down** - Split into smallest achievable, testable units
4. **Create order.json** - Define execution order and metadata

## File Naming

**DO NOT use numeric prefixes.** Order is defined in `order.json`, not filenames.

| Good | Bad |
|------|-----|
| `provider-types.md` | `01-provider-types.md` |
| `openai-adapter.md` | `02-openai-adapter.md` |
| `tests-integration.md` | `20-tests-integration.md` |

Use descriptive kebab-case names that reflect the task's purpose.

## order.json Structure

The `order.json` file is the **single source of truth** for task execution:

```json
{
  "todo": [
    {"task": "provider-types.md", "group": "Foundation", "model": "sonnet", "thinking": "none"},
    {"task": "openai-adapter.md", "group": "Migration", "model": "sonnet", "thinking": "thinking"},
    {"task": "tests-integration.md", "group": "Testing", "model": "sonnet", "thinking": "extended"}
  ]
}
```

| Field | Required | Description |
|-------|----------|-------------|
| `task` | Yes | Filename (must exist in same directory) |
| `group` | Yes | Logical grouping for reporting |
| `model` | Yes | `sonnet` or `opus` |
| `thinking` | Yes | `none`, `thinking`, or `extended` |

Array position determines execution order. The database tracks completion state.

## Task File Requirements

### Model/Thinking Selection

Choose minimum capability needed for each task:

| Complexity | Model | Thinking |
|------------|-------|----------|
| Straightforward | sonnet | none |
| Moderate | sonnet | thinking |
| Complex | sonnet | extended |
| Very complex | opus | extended |

Default to `sonnet/none` and escalate only when complexity demands it.

### Skill Loading

- Load all skills the sub-agent will need (err on the side of more)
- Include relevant developer skills for implementation tasks
- **Never load `align`** - sub-agents execute, they don't negotiate

### Source Code Links

- Provide specific file paths the agent will need
- Include both implementation files and test patterns
- Link to related existing code as examples

### Pre/Post Conditions

- Pre-conditions must be verifiable before starting
- Post-conditions must be testable after completion
- Chain logically: post(N) = pre(N+1)

### Context (What/How/Why)

Every task must provide:

- **What** - The specific goal
- **How** - Approach and patterns to follow
- **Why** - Business/technical rationale

This context enables agents to handle unforeseen issues intelligently.

### Sub-agent Guidance

Include in task instructions:

- Suggest sub-agent use where appropriate (research, parallel work)
- Encourage persistence - overcome obstacles to complete the goal
- Task success is the measure, not partial progress
