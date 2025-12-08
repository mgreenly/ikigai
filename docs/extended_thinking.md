# Extended Thinking in Claude Code

Extended thinking allows Claude to reason through complex problems before responding. In Claude Code, this reasoning is visible as italic gray text above responses.

## How LLM Reasoning Works

Standard LLMs reason implicitly—computation happens in hidden layers with no visible step-by-step process. Extended thinking makes this explicit by allocating tokens for the model to "think out loud" before answering.

### API Visibility

| Provider | Reasoning Visibility |
|----------|---------------------|
| Anthropic (standard) | None - just the response |
| Anthropic (extended thinking) | `thinking` blocks returned in response |
| OpenAI (GPT-4, etc.) | None - just the response |
| OpenAI (o1/o3) | Summary only, not full chain-of-thought |

## Thinking Keywords

Claude Code preprocesses these keywords to trigger extended thinking with different token budgets:

| Keyword | Token Budget | Use Case |
|---------|--------------|----------|
| `think` | ~4,000 tokens | Basic extended reasoning |
| `think hard` / `megathink` | ~10,000 tokens | Deeper consideration |
| `think more` / `think deeply` | ~10,000 tokens | Intermediate reasoning |
| `think harder` / `ultrathink` | ~32,000 tokens | Maximum depth reasoning |

**Note:** These keywords only work in Claude Code's terminal interface, not in the web chat or direct API calls. Claude Code intercepts these keywords and sets the appropriate API parameters.

## API Details

Under the hood, extended thinking is enabled via a `thinking` parameter in the Anthropic API request:

### Request Format

```json
{
  "model": "claude-sonnet-4-20250514",
  "max_tokens": 16000,
  "thinking": {
    "type": "enabled",
    "budget_tokens": 10000
  },
  "messages": [
    {
      "role": "user",
      "content": "Analyze this architecture..."
    }
  ]
}
```

**Parameters:**
- `budget_tokens` - Token budget for thinking (minimum ~1024, maximum varies by model)
- `max_tokens` - Must be greater than `budget_tokens`
- `type` - Set to `"enabled"` to activate extended thinking

### Response Format

The thinking output comes back in a separate block before the main response:

```json
{
  "content": [
    {
      "type": "thinking",
      "thinking": "Let me work through this step by step. First, I need to consider the current architecture..."
    },
    {
      "type": "text",
      "text": "Here's my analysis of the architecture..."
    }
  ]
}
```

### How Claude Code Keywords Map to API

The model doesn't inherently understand keywords like `ultrathink`. Claude Code's preprocessing layer intercepts them and sets `budget_tokens` accordingly:

| Keyword | API budget_tokens |
|---------|-------------------|
| `think` | ~4,000 |
| `think hard` / `megathink` | ~10,000 |
| `ultrathink` / `think harder` | ~32,000 |

## Enabling Extended Thinking

### Method 1: Keywords in Prompts

Include a thinking keyword in your prompt:

```
ultrathink about how to refactor this authentication system
```

### Method 2: Tab Key Toggle

Press `Tab` to toggle thinking on/off for the current session.

### Method 3: Environment Variable

Set a permanent token budget:

```bash
export MAX_THINKING_TOKENS=10000
```

### Method 4: Settings Configuration

Add to your `settings.json`:

```json
{
  "alwaysThinkingEnabled": true
}
```

## When to Use Extended Thinking

Extended thinking is most valuable for:

- Planning complex architectural changes
- Debugging intricate issues
- Creating implementation plans for new features
- Understanding complex codebases
- Evaluating tradeoffs between approaches
- Critical system design decisions

## When NOT to Use Ultrathink

Reserve maximum thinking depth for genuinely complex tasks. Overusing it on simple queries:

- Generates unnecessary costs
- Increases latency without benefit
- Doesn't improve response quality for simple questions

## Best Practices

1. **Start with high-level instructions** rather than prescriptive step-by-step guidance
2. **Let the model determine its own reasoning approach**
3. **Use reflection and verification** - ask Claude to verify work and test solutions
4. **Match thinking depth to task complexity** - simple tasks don't need ultrathink
5. **Review the thinking output** - it helps you understand Claude's approach and catch potential issues early
