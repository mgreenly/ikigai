# Agent Sends Mail

## Description

An agent uses the `mail` tool with `action: send` to send a message to another agent. This enables agent-to-agent communication for task delegation and result reporting.

## Transcript

```text
I've completed the OAuth research. I'll send my findings back to agent 0/.

[Tool use: mail]
  action: send
  to: 0/
  body: "OAuth research complete. Recommend PKCE flow with retry-on-401 pattern. Key points:\n1. Use authorization code flow\n2. Store tokens in httpOnly cookies\n3. Implement silent refresh before expiry"

[Tool result]:
{
  "sent": true,
  "to": "0/",
  "id": 7
}

I've sent my research findings to agent 0/. They can review and proceed with implementation.
```

## Walkthrough

1. Agent calls `mail` tool with `action: send`, `to: "0/"`, and `body`

2. Tool handler validates recipient agent exists

3. If recipient not found, returns error:
   ```json
   {"error": "Agent 99/ not found"}
   ```

4. Handler validates body is non-empty

5. If body empty, returns error:
   ```json
   {"error": "Message body cannot be empty"}
   ```

6. Handler creates `ik_mail_msg_t`:
   - `from_agent_id`: current agent's ID
   - `to_agent_id`: recipient
   - `body`: message content
   - `timestamp`: current time
   - `read`: false

7. Handler inserts message into database

8. Handler adds message to recipient's inbox

9. Handler increments recipient's `unread_count`

10. Tool result returned with confirmation:
    - `sent`: true
    - `to`: recipient ID
    - `id`: new message ID

11. Recipient will get notification when they next become IDLE

---

## Reference

### Tool Call

```json
{
  "name": "mail",
  "arguments": "{\"action\": \"send\", \"to\": \"0/\", \"body\": \"OAuth research complete...\"}"
}
```

### Tool Result (Success)

```json
{
  "sent": true,
  "to": "0/",
  "id": 7
}
```

### Tool Result (Agent Not Found)

```json
{
  "error": "Agent 99/ not found"
}
```

### Tool Result (Empty Body)

```json
{
  "error": "Message body cannot be empty"
}
```
