# Commands

Slash commands are typed at the ikigai prompt and begin with `/`. They control agents, communication, and session state.

## Agent Management

| Command | Purpose |
|---------|---------|
| [/fork](commands/fork.md) | Create a child agent |
| [/kill](commands/kill.md) | Terminate an agent |
| [/capture](commands/capture.md) | Enter capture mode for composing a child's task |
| [/cancel](commands/capture.md#cancel) | Exit capture mode without forking |

## Inter-Agent Mail

| Command | Purpose |
|---------|---------|
| [/mail-send](commands/mail.md#mail-send) | Send a message to another agent |
| [/mail-check](commands/mail.md#mail-check) | List inbox messages |
| [/mail-read](commands/mail.md#mail-read) | Read a message |
| [/mail-delete](commands/mail.md#mail-delete) | Delete a message |
| [/mail-filter](commands/mail.md#mail-filter) | Filter inbox by sender |
