# Diagnosis: Model Configuration Not Persisted Across Restarts

**Status: FIXED**

## Symptoms

- `/model gpt-5-nano` works and displays confirmation
- Quitting and restarting shows "No model configured"

## Root Cause

The `/model` command persisted its output to the `messages` table but the replay code only rendered the output to the scrollback - it didn't re-apply the command's side effects (setting `agent->provider` and `agent->model`).

## Fix

Added `replay_command_effects()` in `src/repl/agent_restore_replay.c` that is called during scrollback population. When a "command" message is encountered with `"command":"model"`, it:

1. Parses the model name from `data_json.args`
2. Infers the provider via `ik_infer_provider()`
3. Sets `agent->provider` and `agent->model`

This is consistent with how other commands like `/mark` and `/rewind` work during replay.

## Files Changed

| File | Change |
|------|--------|
| src/repl/agent_restore_replay.c | Added `replay_command_effects()` function |
| src/repl/agent_restore_replay.c | Call `replay_command_effects()` in `ik_agent_restore_populate_scrollback()` |
