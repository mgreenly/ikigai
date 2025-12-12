# Mail Scoped to Session

## Description

Mail is scoped to the current ikigai session. Each session has isolated mail - messages from previous sessions are not visible. This keeps the inbox relevant to current work.

## Transcript

User in a new session checks inbox:

```text
───────── ↑- ←- [0/] →- ↓- ─────────────────────────
> /mail

Inbox for agent 0/:
  (no messages)

> _
```

Even if previous sessions had mail, this session starts fresh.

## Walkthrough

1. User starts ikigai, new session created

2. Session gets unique `session_id` in database

3. Agent inboxes initialized empty (no messages loaded)

4. All mail operations include `session_id` in queries:
   ```sql
   -- Send mail
   INSERT INTO mail (session_id, ...) VALUES (current_session_id, ...)

   -- List inbox
   SELECT * FROM mail
   WHERE session_id = ? AND to_agent_id = ?
   ORDER BY read ASC, timestamp DESC

   -- Read message
   SELECT * FROM mail
   WHERE session_id = ? AND id = ?
   ```

5. Previous session's mail exists in database but is not visible

6. Each session is a fresh workspace:
   - New agents start with empty inboxes
   - Mail accumulates during session
   - Session end leaves mail in database (not deleted)

7. Future extension (not in v1): cross-session mail could allow:
   - Persistent inboxes across restarts
   - Historical mail search
   - Session restoration with mail

8. Current scope: session isolation is simpler and matches conversation model

9. Database index optimizes session-scoped queries:
   ```sql
   CREATE INDEX idx_mail_recipient ON mail(session_id, to_agent_id, read);
   ```
