# Mail Persisted to Database

## Description

All mail messages are persisted to the database immediately on send. This ensures mail survives agent switches and is recoverable within the session.

## Transcript

User sends mail and switches agents:

```text
───────── ↑- ←- [0/] →1/ ↓- ─────────────────────────
> /mail send 1/ "Research sorting algorithms"

Mail sent to agent 1/

> _
```

User switches to agent 1/:

```text
───────── ↑- ←0/ [1/] →- ↓- ─────── [mail:1] ───────
> /mail

Inbox for agent 1/:
  #3 [unread] from 0/ - "Research sorting algorithms"

> _
```

Mail is there because it was persisted to database.

## Walkthrough

1. User sends mail with `/mail send 1/ "Research..."`

2. Command handler creates `ik_mail_msg_t` in memory

3. Handler immediately writes to database:
   ```sql
   INSERT INTO mail (session_id, from_agent_id, to_agent_id, body, timestamp, read)
   VALUES (?, '0/', '1/', 'Research...', ?, 0)
   ```

4. Database write must succeed before confirmation displayed

5. If database write fails, error shown, mail not delivered

6. Handler adds message to recipient's in-memory inbox

7. User sees confirmation: "Mail sent to agent 1/"

8. When user switches to agent 1/:
   - Agent 1/'s inbox already has message (was added in step 6)
   - Even if inbox wasn't pre-loaded, could query database

9. Database schema:
   ```sql
   CREATE TABLE mail (
       id INTEGER PRIMARY KEY AUTOINCREMENT,
       session_id INTEGER NOT NULL,
       from_agent_id TEXT NOT NULL,
       to_agent_id TEXT NOT NULL,
       body TEXT NOT NULL,
       timestamp INTEGER NOT NULL,
       read INTEGER DEFAULT 0,
       FOREIGN KEY (session_id) REFERENCES sessions(id)
   );
   ```

10. Mail persists for duration of session
