# Context Window

Ikigai conversations run indefinitely — there is no hard limit where the conversation stops. Full history is always preserved in the database and survives restarts.

## How it works

LLM providers have finite context windows. As a conversation grows, ikigai maintains a token budget for the message history sent to the provider. When the conversation exceeds that budget, the oldest turns silently drop off the back of the active context. Nothing is deleted — old turns remain in the database and are always accessible.

The default budget is **100,000 tokens**.

## Visual indicator

A horizontal rule appears in the scrollback to mark where the LLM's visible context begins. Everything above the rule is archived; everything below is what the LLM currently sees.

```
...older messages...

────────────────────────────────────────────────────────

you: what's the next step?
assistant: Based on our earlier discussion...
```

The rule moves forward as older turns are pruned in subsequent exchanges.

## Nothing is lost

All messages — including those above the horizontal rule — remain in the database. They are excluded from the current request but are never deleted. The database is the permanent record.

## Switching models

Switching to a different provider or model may shift the context boundary. Each provider counts tokens differently, so the same conversation may fit more or less context depending on the active model.

## Configuration

The token budget can be set via environment variable:

```
IKIGAI_SLIDING_CONTEXT_TOKENS=200000
```

Set to `0` to disable the sliding window entirely (all messages are always sent).
