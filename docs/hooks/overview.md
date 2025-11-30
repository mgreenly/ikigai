# Claude Code Hooks Overview

Hooks are shell commands that execute in response to specific events during a Claude Code session. They enable customization and extension of Claude Code's behavior without modifying its core.

## Purpose

Hooks allow you to:
- Log session activity and conversation history
- Integrate with external tools and services
- Trigger custom workflows based on user interactions
- Monitor and audit Claude Code usage
- Implement custom automation

## Key Concepts

### Event-Driven Execution

Hooks are triggered by specific lifecycle events:
- **SessionStart**: When a new Claude Code session begins
- **UserPromptSubmit**: When the user submits a prompt
- **Stop**: When Claude finishes responding

### Non-Blocking by Default

Hook failures are non-blocking - if a hook fails, Claude Code continues operating normally. The error is logged but doesn't interrupt the user's workflow.

### Input via stdin

All hooks receive event data as JSON through stdin (not command-line arguments). This allows rich, structured data to be passed to hooks.

### Configuration in Settings

Hooks are configured in `.claude/settings.json` under the `hooks` key. Each hook type can have multiple hooks with matchers to control when they run.

## Example Use Cases

1. **Conversation Logging**: Capture all prompts and responses to a log file
2. **Analytics**: Track usage patterns and session metrics
3. **Integration**: Send notifications to Slack/Discord when sessions start
4. **Automation**: Trigger CI/CD pipelines based on specific prompts
5. **Compliance**: Audit logs for security and compliance requirements

## Architecture

```
User Action → Claude Code Event → Hook Matcher → Shell Command
                                        ↓
                                   JSON via stdin
                                        ↓
                                   Your Script
```

## Getting Started

See:
- `configuration.md` - How to configure hooks
- `hook-types.md` - Available hook types and their data
- `data-formats.md` - JSON schemas for hook input
