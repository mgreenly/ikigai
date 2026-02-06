# Commands

Slash commands are typed at the ikigai prompt and begin with `/`. They control agents, communication, and session state.

## Agent Management

| Command | Purpose |
|---------|---------|
| [/fork](commands/fork.md) | Create a child agent |
| [/kill](commands/kill.md) | Terminate an agent and its descendants |
| [/reap](commands/reap.md) | Remove all dead agents from nav rotation |
| [/capture](commands/capture.md) | Enter capture mode for composing a child's task |
| [/cancel](commands/capture.md#cancel) | Exit capture mode without forking |

## Inter-Agent Communication

| Command | Purpose |
|---------|---------|
| [/send](commands/send.md) | Send a message to another agent |
| [/wait](commands/wait.md) | Wait for messages (blocking, with fan-in support) |
