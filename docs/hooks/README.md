# Claude Code Hooks Documentation

This directory contains comprehensive documentation on Claude Code's hook system, gathered from analyzing a working implementation and Claude Code's behavior.

## Purpose

These docs will guide ikigai's implementation of Claude Code-compatible hooks, enabling users to extend and customize ikigai's behavior without modifying its core.

## Documentation Structure

### [overview.md](overview.md)
Introduction to hooks, their purpose, and high-level architecture. Start here to understand what hooks are and why they exist.

### [configuration.md](configuration.md)
Complete reference for configuring hooks in `.claude/settings.json`. Covers:
- Configuration file structure
- Hook type definitions
- Matcher patterns
- Multiple hook setup
- Best practices

### [hook-types.md](hook-types.md)
Detailed specification of each hook type:
- **SessionStart**: Triggered when sessions begin
- **UserPromptSubmit**: Triggered on user input
- **Stop**: Triggered after responses complete

Includes lifecycle diagrams, timing, and usage examples.

### [data-formats.md](data-formats.md)
JSON schemas and data structures passed to hooks:
- Input format for each hook type
- Transcript JSONL structure
- Parsing examples with `jq`
- Error handling patterns
- Testing approaches

### [implementation-guide.md](implementation-guide.md)
Technical guide for implementing hooks in ikigai:
- Architecture components
- Code examples (TypeScript)
- Security considerations
- Performance optimization
- Testing strategy
- Migration path

## Quick Start

### For Hook Users

1. Read [overview.md](overview.md) to understand hooks
2. Follow [configuration.md](configuration.md) to set up hooks
3. Reference [hook-types.md](hook-types.md) for specific hook behavior
4. Use [data-formats.md](data-formats.md) when writing hook scripts

### For Ikigai Developers

1. Read [overview.md](overview.md) for context
2. Study [hook-types.md](hook-types.md) for behavior specification
3. Review [data-formats.md](data-formats.md) for data contracts
4. Follow [implementation-guide.md](implementation-guide.md) for implementation

## Example Hook

Here's a minimal working hook that logs user prompts:

**Configuration** (`.claude/settings.json`):
```json
{
  "hooks": {
    "UserPromptSubmit": [
      {
        "matcher": "*",
        "hooks": [
          {
            "type": "command",
            "command": "hooks/log_prompt.sh",
            "timeout": 5000
          }
        ]
      }
    ]
  }
}
```

**Hook Script** (`hooks/log_prompt.sh`):
```bash
#!/bin/bash
set -e

# Read JSON from stdin
INPUT=$(cat)

# Extract prompt
PROMPT=$(echo "$INPUT" | jq -r '.prompt // empty')

# Log it
if [ -n "$PROMPT" ]; then
    echo "[$(date)] $PROMPT" >> session.log
fi

exit 0
```

**Make it executable**:
```bash
chmod +x hooks/log_prompt.sh
```

## Key Learnings

### From Analyzing Claude Code

1. **Non-blocking by design**: Hook failures never interrupt sessions
2. **JSON via stdin**: All data passed as JSON to stdin, not args
3. **JSONL transcripts**: Conversation history in JSON Lines format
4. **No hot reload**: Settings loaded once at session start
5. **Timeout enforcement**: Hooks killed if they exceed timeout
6. **Simple matchers**: Currently only "*" wildcard supported

### Design Principles

- **Reliability**: System continues even if hooks fail
- **Simplicity**: Easy to configure and understand
- **Performance**: Fast hooks, reasonable timeouts
- **Security**: Validated paths, permission checks
- **Compatibility**: Match Claude Code behavior exactly

## Implementation Status

This documentation is based on analysis of Claude Code's hook system. ikigai's implementation is planned but not yet complete.

## Contributing

When implementing hooks in ikigai:
- Follow the specifications in these docs exactly
- Maintain Claude Code compatibility
- Add tests for all behavior
- Update docs if behavior differs

## See Also

- `.claude/settings.json` - Hook configuration file
- `agents/logs/hooks/` - Example hook implementations
- Claude Code documentation - Official reference
