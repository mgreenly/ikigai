---
name: ralph-goal-authoring
description: How to write effective Ralph goals for Ikigai-driven workflows
---

# Ralph Goal Authoring

Write goals in terms of **what must be true when the work is complete**, not the exact implementation recipe.

Ralph can iterate, inspect the repo, read documents, and recover from failures. Good goals give it a clear objective, rich references, measurable outcomes, and unambiguous acceptance criteria.

## Default Structure

Use this shape:

```markdown
## Objective
What should be accomplished.

## Reference
Relevant files, documents, examples, commands, and surrounding context.

## Outcomes
Concrete observable results Ralph should produce.

## Acceptance
How to verify success.
```

## Rules

1. Specify **what**, not step-by-step **how**
2. Reference liberally
3. Keep one cohesive objective per goal
4. Make outcomes measurable
5. Make acceptance testable
6. Be explicit about discovery work when discovery is required

## Discovery Rule

If the work requires finding all occurrences of something, say so explicitly.

Good:

```markdown
## Objective
Discover all hardcoded config paths in src/ and replace them with CONFIG_DIR-based resolution.
```

Bad:

```markdown
## Objective
Fix config paths.
```

Ralph does better when the discovery step is named directly in the objective and outcomes.

## Good Example

```markdown
## Objective
Implement configuration loading with environment variable overrides.

## Reference
- docs/config.md
- docs/environment.md
- src/main.c
- tests/unit/config/

## Outcomes
- Configuration loading implemented per docs/config.md
- Environment variable overrides are supported
- Default values are applied when overrides are absent
- Unit tests covering config behavior pass

## Acceptance
- All relevant tests pass
- Running the app with CONFIG_DIR set uses the expected config directory
```

## Bad Example

```markdown
## Objective
Add config struct.

## Reference
docs/config.md

## Outcomes
- Struct added
```

Why this is weak:

- too small and under-specified
- little context
- no success criteria
- no validation path

## Authoring Guidance

Do:

- describe the end state
- include relevant files and docs
- name constraints explicitly
- include user-visible or test-visible outcomes
- include acceptance checks that can actually be run

Do not:

- write pseudocode as outcomes
- overspecify edit order
- omit validation
- assume Ralph will infer missing discovery requirements

## When Used With the Goal Scripts

Typical Ikigai flow:

1. Draft the body using this structure
2. Pipe it to `../ralph-pipeline/scripts/goal-create`
3. Queue with `../ralph-pipeline/scripts/goal-queue`

Example:

```bash
cat <<'EOF' | ../ralph-pipeline/scripts/goal-create --title "Implement configuration loading"
## Objective
Implement configuration loading with environment variable overrides.

## Reference
- docs/config.md
- docs/environment.md
- src/main.c
- tests/unit/config/

## Outcomes
- Configuration loading implemented per docs/config.md
- Environment variable overrides are supported
- Default values are applied when overrides are absent
- Unit tests covering config behavior pass

## Acceptance
- All relevant tests pass
- Running the app with CONFIG_DIR set uses the expected config directory
EOF
```
