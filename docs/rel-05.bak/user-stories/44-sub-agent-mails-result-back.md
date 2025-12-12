# Sub-Agent Mails Result Back

## Description

After completing a task received via mail, an agent sends results back to the originating agent. This completes the async delegation round-trip.

## Transcript

Agent 1/ completes the build task and reports back:

```text
───────── ↑- ←0/ [1/] →- ↓- ─────────────────────────

[Tool use: bash]
  command: "make check"

[Tool result]:
"Build successful. 847 tests passed, 0 failed."

The build completed successfully. I'll report back to agent 0/.

[Tool use: mail]
  action: send
  to: 0/
  body: "Build complete!\n\nResults:\n- Build: SUCCESS\n- Tests: 847 passed, 0 failed\n- Time: 42 seconds\n\nNo issues found. Ready to proceed."

[Tool result]:
{"sent": true, "to": "0/", "id": 9}

I've sent the results to agent 0/. Task complete.

> _
```

User switches back to agent 0/ to see results:

```text
───────── ↑- ←- [0/] →1/ ↓- ─────── [mail:1] ───────

[Notification: You have 1 unread message in your inbox]

> /mail read 9

From: 1/
Time: just now

Build complete!

Results:
- Build: SUCCESS
- Tests: 847 passed, 0 failed
- Time: 42 seconds

No issues found. Ready to proceed.

> _
```

## Walkthrough

1. Agent 1/ completes the delegated task (build and test)

2. Agent 1/ uses `mail` tool to send results to agent 0/

3. Message delivered to agent 0/'s inbox

4. Agent 0/'s `unread_count` incremented

5. If agent 0/ is IDLE:
   - Notification injected
   - Agent (or user) can check mail

6. User switches to agent 0/ (Ctrl+Left)

7. Separator shows `[mail:1]` indicator

8. User runs `/mail read 9` to see full results

9. Async delegation cycle complete:
   - 0/ → mail task → 1/
   - 1/ → executes → 1/
   - 1/ → mail result → 0/
   - 0/ → receives result

10. This pattern enables parallel work:
    - User continues on 0/ while 1/ works
    - Multiple agents can work simultaneously
    - Results aggregate via inbox
