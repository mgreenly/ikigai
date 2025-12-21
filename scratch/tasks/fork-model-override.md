# Task: Update /fork Command for Model Override

**Model:** sonnet/none
**Depends on:** agent-provider-fields.md, model-command.md

## Context

**Working directory:** Project root (where `Makefile` lives)
**All paths are relative to project root**, not to this task file.

## Pre-Read

**Skills:**
- `/load source-code` - Map of source files by functional area

**Source:**
- `src/commands_fork.c` - Fork command implementation
- `src/commands_basic.c` - Model command for reference

**Plan:**
- `scratch/plan/README.md` - Fork Command Integration section

## Objective

Update `/fork` command to support `--model MODEL/THINKING` flag that overrides the child agent's provider, model, and thinking level. When no override is specified, child inherits parent's configuration. Store the override in database so child agents start with correct settings.

## Interface

Functions to implement:

| Function | Purpose |
|----------|---------|
| `res_t cmd_fork_parse_args(const char *input, char **model, char **prompt)` | Parse /fork command arguments, extract --model flag and prompt |
| `res_t cmd_fork_apply_override(agent_t *child, const char *model_spec)` | Apply model override to child agent, parse MODEL/THINKING |
| `res_t cmd_fork_inherit_config(agent_t *child, const agent_t *parent)` | Copy parent's provider/model/thinking to child |

## Behaviors

**Argument Parsing:**
- Detect `--model` flag in fork command
- Extract MODEL/THINKING value after `--model`
- Extract remaining prompt text if provided
- Support both orders: `--model X "prompt"` and `"prompt" --model X`
- Return ERR_INVALID_ARG for malformed flags

**Child Agent Creation - Inheritance:**
- If no `--model` flag, copy from parent:
  - child->provider = parent->provider
  - child->model = parent->model
  - child->thinking_level = parent->thinking_level
- Preserve parent's message history reference
- Create child's own database record

**Child Agent Creation - Override:**
- If `--model` flag provided, parse MODEL/THINKING
- Infer provider from model name (reuse `ik_infer_provider()`)
- Set child->provider to inferred provider
- Set child->model to specified model
- Set child->thinking_level to specified level
- Do not inherit parent's provider/model/thinking

**Database Persistence:**
- Store child agent with provider/model/thinking_level
- Ensure child can be restored with correct configuration
- Link child to parent via parent_id

**User Feedback:**
- Display what child agent will use:
  - Inheritance: "Forked child with parent's model (provider/model/thinking)"
  - Override: "Forked child with model/thinking"
- Show any warnings if overridden model doesn't support thinking

## Test Scenarios

**Inheritance (no override):**
- Parent: provider="anthropic", model="claude-sonnet-4-5", thinking="medium"
- Execute: `/fork`
- Child: provider="anthropic", model="claude-sonnet-4-5", thinking="medium"
- Database has child record with same values

**Override with model only:**
- Parent: provider="anthropic", model="claude-sonnet-4-5", thinking="medium"
- Execute: `/fork --model gpt-4o`
- Child: provider="openai", model="gpt-4o", thinking="medium" (inherits thinking)
- Database has child record with new provider/model

**Override with model and thinking:**
- Parent: provider="anthropic", model="claude-sonnet-4-5", thinking="medium"
- Execute: `/fork --model o3-mini/high`
- Child: provider="openai", model="o3-mini", thinking="high"
- Database has child record with new values

**Override with prompt:**
- Parent: provider="anthropic", model="claude-sonnet-4-5", thinking="none"
- Execute: `/fork --model gemini-2.5/med "Analyze this data"`
- Child: provider="google", model="gemini-2.5", thinking="medium", has prompt task
- Database has child with prompt and config

**Argument ordering:**
- `/fork --model gpt-4o "prompt"` works
- `/fork "prompt" --model gpt-4o` works
- Both produce same result

**Invalid model:**
- Execute: `/fork --model unknown-model/high`
- Returns: ERR_NOT_FOUND (unknown provider)
- Child not created

**Warning for non-thinking model:**
- Execute: `/fork --model gpt-4o/high`
- Child created with: provider="openai", model="gpt-4o", thinking="high"
- Warning displayed: "gpt-4o does not support extended thinking"

## Postconditions

- [ ] `/fork` without --model inherits parent config
- [ ] `/fork --model X` overrides child's provider and model
- [ ] `/fork --model X/Y` overrides child's provider, model, and thinking
- [ ] `--model` flag works before or after prompt
- [ ] Database stores child agent with correct values
- [ ] Child agent can be restored from database with correct config
- [ ] User feedback shows inheritance vs override
- [ ] Warnings shown for non-thinking models
- [ ] All tests compile without warnings
- [ ] All tests pass
- [ ] `make check` passes
