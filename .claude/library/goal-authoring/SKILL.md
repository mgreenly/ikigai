---
name: goal-authoring
description: Writing effective goal files for Ralph execution
---

# Goal Authoring

Writing effective goal files for Ralph loop execution.

## Creating Goals

Goals are created via the ralph-plans API:

```bash
echo "<body>" | goal-create --org "$RALPH_ORG" --repo "$RALPH_REPO" --title "Title"
```

## Core Principle

**Ralph has unlimited context through iteration.** Don't artificially limit goals or references - Ralph can read all plan documents, iterate through failures, and persist until outcomes are achieved.

## Goal File Format

```markdown
## Objective
[Complete objective - not tiny steps]

## Reference
[All relevant docs + codebase examples]

## Outcomes
[Measurable, verifiable outcomes]

## Acceptance
[Success criteria: make check, specific tests, etc.]
```

## Key Principles

1. **Specify WHAT, never HOW** - Outcomes, not steps/order
2. **Reference liberally** - All relevant docs, Ralph reads across iterations
3. **One cohesive objective** - Not artificially small, not entire release
4. **Complete acceptance criteria** - Ralph needs to know when done
5. **Trust Ralph to iterate** - Discovers path, learns from failures
6. **Be explicit about discovery** - If work requires finding all instances of X, state it clearly in both objective and outcomes. Ralph has gotten stuck when discovery wasn't explicit. Write "Discover and fix all hardcoded paths" not "Fix hardcoded paths" or "Fix paths at lines 10, 25, 40"

## Example: Bad vs Good

**Bad (context-limited thinking):**
```markdown
## Objective
Create the web_fetch_t struct.

## Reference
plan/web-fetch.md section 2.1

## Outcomes
- Struct defined in src/web_fetch.h
```

Problems: Artificially small, minimal reference, no acceptance criteria.

**Good (leverages unlimited context):**
```markdown
## Objective
Implement web-fetch tool with HTML-to-markdown conversion per project/plan/web-fetch.md.

## Reference
- project/plan/web-fetch.md - Interface and behavior spec
- project/plan/tool-integration.md - Registry integration
- project/research/html-to-markdown.md - Library rationale
- src/tool_registry.c - Registration pattern
- tests/unit/tool_bash/ - Test structure

## Outcomes
- web_fetch tool implemented per project/plan/web-fetch.md
- HTML-to-markdown conversion working
- Integrated with tool registry
- Unit tests in tests/unit/web_fetch/ pass

## Acceptance
- All quality checks pass
- Manual: `./ikigai` → `/web-fetch https://example.com` returns markdown
```

Why better: Complete objective, comprehensive references, measurable outcomes, clear acceptance.

**Discovery example:**
```markdown
## Objective
Discover all hardcoded ~/.config/ikigai path references in tests/ directory and update them to use IKIGAI_CONFIG_DIR environment variable.

## Reference
- tests/test_utils.c:692-730 - test_paths_setup_env() sets IKIGAI_CONFIG_DIR
- .envrc:3 - Shows IKIGAI_CONFIG_DIR=$PWD/etc/ikigai

## Outcomes
- All hardcoded ~/.config/ikigai references in tests/ discovered via grep
- All discovered test files updated to use getenv("IKIGAI_CONFIG_DIR")
- No hardcoded ~/.config/ikigai paths remain in tests directory

## Acceptance
- All quality checks pass
- grep -r "\.config/ikigai" tests/ returns no hardcoded paths
```

Why explicit discovery matters: Objective starts with "Discover", first outcome confirms discovery happened. Ralph won't skip the grep step.

## Model and Reasoning Selection

Goals can optionally specify `--model` and `--reasoning` when created:

```bash
echo "..." | goal-create --title "..." --org ORG --repo REPO --model opus --reasoning high < body.md
```

**When to specify:**

- **Default (no flags):** Most goals use server defaults. Don't specify unless there's a clear reason.
- **--model opus:** Complex architectural decisions, multi-file refactors, unfamiliar domain
- **--model haiku:** Simple, well-defined tasks (rare; server usually picks correctly)
- **--reasoning high:** Deep debugging, architectural design, security analysis, complex tradeoffs
- **--reasoning med:** Moderate complexity requiring multi-step planning
- **--reasoning low/none:** Simple implementation tasks

**Example:**
```bash
# Architectural redesign - needs high capability and reasoning
echo "..." | goal-create --title "Redesign authentication module" --org "$RALPH_ORG" --repo "$RALPH_REPO" --model opus --reasoning high

# Standard implementation - use defaults
echo "..." | goal-create --title "Add validation to parse_input" --org "$RALPH_ORG" --repo "$RALPH_REPO"
```

## Anti-Patterns

❌ **Step-by-step instructions** - "First do X, then Y" (Ralph discovers path)
❌ **Minimal references** - "Save context" (Ralph iterates, include all relevant)
❌ **Vague outcomes** - "Feature implemented" (Be specific and measurable)
❌ **Tiny goals** - Breaking cohesive work into artificial steps
❌ **Pre-discovered work** - Listing specific file:line locations to fix (Ralph should discover)

✅ **Do this:** Comprehensive objective, all references, measurable outcomes, trust Ralph to iterate and discover.
