## Objective

Complete the pluribus multi-agent conversation harness. The scaffold exists in `.claude/harness/pluribus/` - refine and test it until it works.

## Reference

- `.claude/harness/pluribus/run` - main script (exists, needs testing/refinement)
- `.claude/harness/pluribus/personas.json` - persona definitions (exists)
- `.claude/harness/pluribus/system.md.erb` - agent system prompt template (exists)
- `.claude/harness/ralph/run` - reference implementation for streaming I/O patterns

## Current State

Scaffold created with:
- `Pluribus` class: orchestrates conversation, tracks rounds, manages master log
- `Agent` class: persistent `IO.popen` process per agent, tracks `last_seen_index`, retry logic for structured output
- CLI with `--goal`, `--agents`, `--model`, `--duration`, `--debug` flags
- Debug mode writes per-agent JSONL logs plus master conversation log

## Key Design Decisions

1. **No threads** - sequential round-robin, one agent responds at a time
2. **Persistent processes** - each agent keeps its Claude process open via `IO.popen(cmd, 'r+')`
3. **Incremental context** - agents only receive unseen messages each turn (not full history replay)
4. **Structured output** - agents return `{"response": "..."}`, with retry logic if they fail
5. **UNDECIPHERABLE** - after 3 retries without valid structured output, record as agent's response

## Outcomes

1. `pluribus --help` shows usage
2. Basic 2-agent conversation completes without errors
3. Debug mode produces valid JSONL logs per agent
4. Agents correctly see only new messages each turn
5. PASS/DONE handling works (agent passes or leaves conversation)
6. Retry logic triggers when structured output is missing

## Acceptance

Run a test conversation that completes without crashes:
```bash
.claude/harness/pluribus/run \
  --goal="Design a simple key-value store API" \
  --agents=white,blonde \
  --model=haiku \
  --duration=5m \
  --debug
```

Verify debug logs show proper message flow.
