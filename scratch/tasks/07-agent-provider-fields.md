# Task: Add Provider Fields to Agent Context

**Phase:** 3 - Agent Updates
**Depends on:** 01-provider-types, 06-database-migration

## Objective

Update `ik_agent_ctx_t` to store provider/model/thinking configuration.

## Deliverables

1. Update `src/agent.h` / `src/agent.c`:
   - Add `char *provider` field
   - Add `char *model` field
   - Add `ik_thinking_level_t thinking_level` field
   - Add `ik_provider_t *provider_instance` (cached, lazy-loaded)

2. Implement lazy provider loading:
   - `ik_agent_get_provider()` - Get or create provider instance
   - Cache provider on first use
   - Error if no credentials

3. Add provider inference from model name:
   - `ik_infer_provider()` - Map model prefix to provider
   - `claude-*` → anthropic
   - `gpt-*`, `o1-*`, `o3-*` → openai
   - `gemini-*` → google

## Reference

- `scratch/plan/architecture.md` - Agent context section
- `scratch/README.md` - Model Assignment section

## Verification

- Agent stores provider/model/thinking
- Provider lazy-loads on first use
- Inference works for all model prefixes
