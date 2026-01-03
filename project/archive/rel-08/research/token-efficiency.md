# Token Efficiency: External Tools at Zero Overhead

## The Problem

Every agent has built-in tools (efficient) and bash (for everything else).

Using bash to invoke external capabilities has overhead:
- LLM formulates shell command syntax
- No schema - LLM guesses parameters
- Text output requires parsing
- Error handling is ad-hoc

**Example: Database query via bash**
```json
{
  "tool": "bash",
  "command": "psql -h $DB_HOST -U $DB_USER -d customers -c 'SELECT * FROM users LIMIT 5' -t -A -F '|'"
}
```

LLM must know: psql syntax, connection parameters, SQL formatting flags. No validation. Text output.

## ikigai's Solution

External tools use the same protocol as built-in tools. To the LLM, they're indistinguishable.

**Same database query via ikigai protocol**
```json
{
  "tool": "db_query",
  "query": "SELECT * FROM users",
  "limit": 5
}
```

LLM sees: typed parameters, schema validation, structured JSON response. Identical to a built-in tool.

## Why This Matters

The token savings per call are small. The value is **extensibility without penalty**.

Any capability you want to add:
- Process messages from a queue
- Query a company database
- Call a proprietary API
- Run domain-specific analysis
- Integrate with internal systems

All become first-class tools. Same efficiency as if we'd written them in C and compiled them into ikigai.

## Comparison

| Capability | Other Agents | ikigai |
|------------|--------------|--------|
| Built-in tools | Efficient | Efficient |
| External via bash | Overhead (command syntax, text parsing) | N/A |
| External via protocol | Not available | **Efficient (same as built-in)** |

Other agents are stuck with a two-tier system. ikigai has one tier: everything is a first-class tool.

## Token Impact

Per-call overhead for bash-wrapped external tool vs native protocol:
- ~10-30 extra tokens for command formulation
- ~10-20 extra tokens for output parsing context
- Variable tokens for error handling without structure

Small per-call, but it adds up across a session with many tool calls. More importantly: **no architectural reason to avoid external tools**.

## Implication

We don't need to build capabilities into ikigai. If an external tool can do it, wrap it in the protocol and it's as good as built-in. This keeps ikigai simple while enabling unlimited extensibility.
