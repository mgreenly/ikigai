# Mail Commands

## NAME

/mail-send, /mail-check, /mail-read, /mail-delete, /mail-filter - inter-agent messaging

## DESCRIPTION

Agents communicate through a simple mailbox system. Each agent has an inbox. Messages are persisted in the database and survive restarts.

Mail is the primary coordination mechanism between parent and child agents. A parent forks a child with a task, the child sends results back via mail, and the parent reads them when ready.

All mail commands are available both as slash commands (human-initiated) and as internal tools (agent-initiated). The only difference is rendering: slash commands display in the scrollback, tools return JSON.

### Tool Names

| Slash command | Tool name |
|---|---|
| `/mail-send` | `mail_send` |
| `/mail-check` | `mail_check` |
| `/mail-read` | `mail_read` |
| `/mail-delete` | `mail_delete` |
| `/mail-filter` | `mail_filter` |

---

## /mail-send

### SYNOPSIS

```
/mail-send UUID "MESSAGE"
```

### DESCRIPTION

Send a message to another agent. The recipient must exist and be running (not killed). The message body must be non-empty and enclosed in quotes.

### ARGUMENTS

**UUID**
: The recipient agent's UUID or unique prefix.

**MESSAGE**
: The message body, enclosed in quotes.

### EXAMPLES

Send a result to the parent:

```
> /mail-send e5f6 "Analysis complete. Found 3 unchecked error paths in provider.c"
Message sent to e5f6g7h8.
```

Send follow-up instructions to a child:

```
> /mail-send a1b2 "Good findings. Now fix the unchecked returns you identified."
Message sent to a1b2c3d4.
```

---

## /mail-check

### SYNOPSIS

```
/mail-check
```

### DESCRIPTION

List all messages in the current agent's inbox. Shows sender, timestamp, read status, and a preview of each message. Unread messages are shown first.

Takes no arguments.

### EXAMPLES

```
> /mail-check
Inbox (3 messages, 1 unread):
  1. [unread] from a1b2c3d4 (2m ago): Analysis complete. Found 3 unchecked...
  2. from e5f6g7h8 (15m ago): Schema review done. No issues found...
  3. from a1b2c3d4 (1h ago): Starting work on the coverage analysis...
```

---

## /mail-read

### SYNOPSIS

```
/mail-read ID
```

### DESCRIPTION

Read a specific message by its inbox index number (1-based, as shown by `/mail-check`). Displays the full message content with sender information. Marks the message as read.

### ARGUMENTS

**ID**
: The 1-based index of the message in the inbox, as listed by `/mail-check`.

### EXAMPLES

```
> /mail-read 1
From: a1b2c3d4 (2m ago)

Analysis complete. Found 3 unchecked error paths in provider.c:
- line 142: ik_provider_start_stream() return ignored
- line 287: ik_curl_perform() return ignored
- line 355: ik_json_parse() NULL check missing
```

---

## /mail-delete

### SYNOPSIS

```
/mail-delete ID
```

### DESCRIPTION

Permanently delete a message from the inbox by its index number (1-based, as shown by `/mail-check`).

### ARGUMENTS

**ID**
: The 1-based index of the message to delete.

### EXAMPLES

```
> /mail-delete 3
Message deleted.
```

---

## /mail-filter

### SYNOPSIS

```
/mail-filter --from UUID
```

### DESCRIPTION

Filter the inbox to show only messages from a specific sender. Output format is the same as `/mail-check`. Partial UUID matches are accepted.

### ARGUMENTS

**--from UUID**
: The sender's UUID or unique prefix. Required.

### EXAMPLES

Show only messages from one child:

```
> /mail-filter --from a1b2
Inbox (2 messages from a1b2c3d4, 1 unread):
  1. [unread] from a1b2c3d4 (2m ago): Analysis complete. Found 3 unchecked...
  2. from a1b2c3d4 (1h ago): Starting work on the coverage analysis...
```

---

## WORKFLOW EXAMPLE

A typical parent-child coordination:

```
> /capture
(capturing) > Analyze src/repl.c for memory leaks.
(capturing) > Report findings via /mail-send when complete.
> /fork
Forked. Child: a1b2c3d4

  [parent continues other work...]

> /mail-check
Inbox (1 message, 1 unread):
  1. [unread] from a1b2c3d4 (5m ago): Found 2 potential leaks...

> /mail-read 1
From: a1b2c3d4 (5m ago)

Found 2 potential leaks:
1. line 450: talloc_strdup result not freed on early return
2. line 612: ik_agent_ctx allocated but not freed in error path

> /mail-send a1b2 "Fix both leaks and confirm with tests."
Message sent to a1b2c3d4.

  [later...]

> /mail-check
Inbox (2 messages, 1 unread):
  1. [unread] from a1b2c3d4 (1m ago): Both leaks fixed. Tests passing...
  2. from a1b2c3d4 (5m ago): Found 2 potential leaks...

> /kill a1b2
Agent a1b2c3d4 killed.
```

## SEE ALSO

[/fork](fork.md), [/kill](kill.md), [/capture](capture.md), [Commands](../commands.md)
