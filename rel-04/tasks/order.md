# Task Order

## Story 01: Simple Greeting (No Tools)

- ~~tool-glob-schema.md~~
- ~~tool-all-schemas.md~~
- ~~tool-build-array.md~~
- ~~request-with-tools.md~~

## Story 02: Single Glob Call

- ~~tool-call-struct.md~~
- tool-config-fields.md
- tool-output-limit.md
- parse-tool-calls.md
- tool-argument-parser.md
- glob-execute.md
- tool-dispatcher.md
- tool-result-msg.md
- assistant-tool-calls-msg.md
- tool-loop-finish-detection.md
- tool-loop-state-mutation.md
- tool-loop-continuation.md
- scrollback-tool-display.md
- db-tool-persist.md
- db-persist-failure-resilience.md
- db-tool-replay.md
- conversation-rebuild.md

Note: conversation-tool-loop.md was split into three focused tasks (tool-loop-finish-detection, tool-loop-state-mutation, tool-loop-continuation) for better separation of concerns.

Note: db-persist-failure-resilience.md verifies the independent event model - DB failures don't break sessions (memory is authoritative). db-tool-replay.md then ensures replay works correctly after adding new message kinds. conversation-rebuild.md ensures the replayed messages are transformed into the OpenAI conversation format so the LLM has full context after restart. Zero technical debt - every feature works end-to-end before moving to the next story.

## Story 03: Single File Read

- file-read-execute.md

## Story 04: Glob Then Read File

- multi-tool-loop.md

## Story 05: Grep Search

- grep-execute.md

## Story 06: File Write

- file-write-execute.md

## Story 07: Bash Command

- bash-execute.md

## Story 08: File Not Found Error

- file-read-error-e2e.md

## Story 09: Bash Command Fails

- bash-command-error-e2e.md

## Story 10: Multi-Turn Tool Loop

No new tasks needed. The multi-tool conversation loop implemented in Story 04 (multi-tool-loop.md) already handles any number of sequential tool calls until finish_reason is "stop". Story 10's grep -> file_read -> file_write sequence works with the existing implementation.

## Story 11: Tool Loop Limit Reached

Note: The `max_tool_turns` config field was added in Story 02 (tool-config-fields.md).

- tool-loop-counter.md
- tool-result-limit-metadata.md
- tool-choice-none-on-limit.md  *(uses hardcoded strings; refactored in Story 13)*
- tool-loop-limit-e2e.md

## Story 12: Session Replay E2E Verification

- replay-tool-e2e.md

Note: The core replay functionality for tool messages is implemented in Story 02 (db-tool-replay.md and conversation-rebuild.md). Story 12 provides end-to-end verification that a full tool conversation can be persisted, the application restarted, and the session correctly restored with the model maintaining context.

## Story 13: Tool Choice Control (4 variants: auto, none, required, specific)

- tool-choice-config.md
- tool-choice-serialize.md
- request-with-tool-choice-param.md  *(refactors Story 11's hardcoded tool_choice)*
- tool-choice-auto-e2e.md
- tool-choice-none-e2e.md
- tool-choice-required-e2e.md
- tool-choice-specific-e2e.md
