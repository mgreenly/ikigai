# Ticket Tool

External tool wrapping GitHub Issues to provide persistent goal/task tracking for agents.

## Purpose

1. **Token efficiency** - Demonstrates external tool + skill pattern
2. **Persistent history** - Planning artifacts survive context resets
3. **Audit trail** - Links commits to goals via issue references

## API

All operations return JSON.

### create

Create a new ticket.

**Input:**
- `title` (required) - Short description
- `body` (required) - Full details

**Output:**
```json
{"number": 123}
```

### get

Read a single ticket with comments.

**Input:**
- `number` (required) - Issue number

**Output:**
```json
{
  "number": 123,
  "title": "Implement ticket tool",
  "body": "Full description here...",
  "state": "open",
  "comments": [
    {"body": "Progress update..."}
  ]
}
```

### list

List tickets with optional filtering.

**Input:**
- `state` (optional) - "open", "closed", or "all" (default: "open")
- `limit` (optional) - Max results (default: 20)

**Output:**
```json
[
  {"number": 123, "title": "Implement ticket tool", "state": "open"},
  {"number": 122, "title": "Design ticket API", "state": "open"}
]
```

### update

Modify a ticket's title or body.

**Input:**
- `number` (required) - Issue number
- `title` (optional) - New title
- `body` (optional) - New body

**Output:**
```json
{"success": true}
```

### close

Mark a ticket as complete.

**Input:**
- `number` (required) - Issue number

**Output:**
```json
{"success": true}
```

### delete

Permanently remove a ticket.

**Input:**
- `number` (required) - Issue number

**Output:**
```json
{"success": true}
```

### comment

Add a comment to a ticket.

**Input:**
- `number` (required) - Issue number
- `body` (required) - Comment text

**Output:**
```json
{"success": true}
```

## Implementation Notes

- Labels are internal (not exposed to model)
- Milestones and assignees not supported in v1
- Tool handles GitHub API authentication and rate limiting
