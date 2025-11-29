# Task Order

## Story 01: Simple Greeting (No Tools)

1. tool-glob-schema.md
2. tool-all-schemas.md
3. tool-build-array.md
4. request-with-tools.md

## Story 02: Single Glob Call

5. tool-call-struct.md
6. tool-config-fields.md
7. tool-output-limit.md
8. parse-tool-calls.md
9. tool-argument-parser.md
10. glob-execute.md
11. tool-dispatcher.md
12. tool-result-msg.md
13. assistant-tool-calls-msg.md
14. tool-loop-finish-detection.md
15. tool-loop-state-mutation.md
16. tool-loop-continuation.md
17. scrollback-tool-display.md
18. db-tool-persist.md
19. db-tool-replay.md
20. conversation-rebuild.md

Note: conversation-tool-loop.md was split into three focused tasks (14-16) for better separation of concerns: finish detection, state mutation, and loop continuation.

Note: db-tool-replay.md immediately follows db-tool-persist.md to ensure replay works correctly after adding new message kinds. conversation-rebuild.md then ensures the replayed messages are transformed into the OpenAI conversation format so the LLM has full context after restart. Zero technical debt - every feature works end-to-end before moving to the next story.

## Story 03: Single File Read

21. file-read-execute.md

## Story 04: Glob Then Read File

22. multi-tool-loop.md

## Story 05: Grep Search

23. grep-execute.md

## Story 06: File Write

24. file-write-execute.md

## Story 07: Bash Command

25. bash-execute.md

## Story 08: File Not Found Error

26. file-read-error-e2e.md

## Story 09: Bash Command Fails

27. bash-command-error-e2e.md

## Story 10: Multi-Turn Tool Loop

No new tasks needed. The multi-tool conversation loop implemented in Story 04 (task 22: multi-tool-loop.md) already handles any number of sequential tool calls until finish_reason is "stop". Story 10's grep -> file_read -> file_write sequence works with the existing implementation.

## Story 11: Tool Loop Limit Reached

Note: The `max_tool_turns` config field was added in Story 02 (task 6: tool-config-fields.md).

28. tool-loop-counter.md
29. tool-result-limit-metadata.md
30. tool-choice-none-on-limit.md  *(uses hardcoded strings; refactored in Story 13)*
31. tool-loop-limit-e2e.md

## Story 12: Session Replay E2E Verification

32. replay-tool-e2e.md

Note: The core replay functionality for tool messages is implemented in Story 02 (tasks 19-20: db-tool-replay.md and conversation-rebuild.md). Story 12 provides end-to-end verification that a full tool conversation can be persisted, the application restarted, and the session correctly restored with the model maintaining context.

## Story 13: Tool Choice Control (4 variants: auto, none, required, specific)

33. tool-choice-config.md
34. tool-choice-serialize.md
35. request-with-tool-choice-param.md  *(refactors Story 11's hardcoded tool_choice)*
36. tool-choice-auto-e2e.md
37. tool-choice-none-e2e.md
38. tool-choice-required-e2e.md
39. tool-choice-specific-e2e.md
