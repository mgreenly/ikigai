# Task Order

## Story 01: Simple Greeting (No Tools)

1. tool-glob-schema.md
2. tool-all-schemas.md
3. tool-build-array.md
4. request-with-tools.md

## Story 02: Single Glob Call

5. tool-call-struct.md
6. parse-tool-calls.md
7. glob-execute.md
8. tool-result-msg.md
9. assistant-tool-calls-msg.md
10. conversation-tool-loop.md
11. scrollback-tool-display.md
12. db-tool-persist.md

## Story 03: Single File Read

13. file-read-schema.md
14. file-read-execute.md

## Story 04: Glob Then Read File

15. multi-tool-loop.md

## Story 05: Grep Search

16. grep-execute.md

## Story 06: File Write

17. file-write-execute.md

## Story 07: Bash Command

18. bash-execute.md

## Story 08: File Not Found Error

19. file-read-error-e2e.md

## Story 09: Bash Command Fails

20. bash-command-error-e2e.md

## Story 10: Multi-Turn Tool Loop

No new tasks needed. The multi-tool conversation loop implemented in Story 04 (task 15: multi-tool-loop.md) already handles any number of sequential tool calls until finish_reason is "stop". Story 10's grep → file_read → file_write sequence works with the existing implementation.

## Story 11: Tool Loop Limit Reached

21. tool-loop-limit-config.md
22. tool-loop-counter.md
23. tool-result-limit-metadata.md
24. tool-choice-none-on-limit.md
25. tool-loop-limit-e2e.md

## Story 12: Session Replay With Tools

26. replay-tool-messages.md

## Story 13: Tool Choice Control (4 variants: auto, none, required, specific)

27. tool-choice-config.md
28. tool-choice-serialize.md
29. request-with-tool-choice-param.md
30. tool-choice-auto-e2e.md
31. tool-choice-none-e2e.md
32. tool-choice-required-e2e.md
33. tool-choice-specific-e2e.md
