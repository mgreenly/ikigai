# Task Order

## Story 01: Simple Greeting (No Tools)

- ~~[tool-glob-schema.md](tool-glob-schema.md)~~
- ~~[tool-all-schemas.md](tool-all-schemas.md)~~
- ~~[tool-build-array.md](tool-build-array.md)~~
- ~~[request-with-tools.md](request-with-tools.md)~~

## Story 02: Single Glob Call

- ~~[tool-call-struct.md](tool-call-struct.md)~~
- ~~[tool-config-fields.md](tool-config-fields.md)~~
- ~~[tool-output-limit.md](tool-output-limit.md)~~
- ~~[parse-tool-calls.md](parse-tool-calls.md)~~
- ~~[tool-argument-parser.md](tool-argument-parser.md)~~
- ~~[glob-execute.md](glob-execute.md)~~
- ~~[tool-dispatcher.md](tool-dispatcher.md)~~
- ~~[tool-result-msg.md](tool-result-msg.md)~~
- ~~[assistant-tool-calls-msg.md](assistant-tool-calls-msg.md)~~
- ~~[tool-loop-finish-detection.md](tool-loop-finish-detection.md)~~
- ~~[tool-loop-state-mutation.md](tool-loop-state-mutation.md)~~
- ~~[tool-loop-continuation.md](tool-loop-continuation.md)~~
- ~~[scrollback-tool-display.md](scrollback-tool-display.md)~~
- ~~[db-tool-persist.md](db-tool-persist.md)~~
- ~~[db-persist-failure-resilience.md](db-persist-failure-resilience.md)~~
- ~~[db-tool-replay.md](db-tool-replay.md)~~
- ~~[conversation-rebuild.md](conversation-rebuild.md)~~

Note: conversation-tool-loop.md was split into three focused tasks (tool-loop-finish-detection, tool-loop-state-mutation, tool-loop-continuation) for better separation of concerns.

Note: db-persist-failure-resilience.md verifies the independent event model - DB failures don't break sessions (memory is authoritative). db-tool-replay.md then ensures replay works correctly after adding new message kinds. conversation-rebuild.md ensures the replayed messages are transformed into the OpenAI conversation format so the LLM has full context after restart. Zero technical debt - every feature works end-to-end before moving to the next story.

## Story 03: Single File Read

- ~~[file-read-execute.md](file-read-execute.md)~~

## Story 04: Glob Then Read File

- ~~[multi-tool-loop.md](multi-tool-loop.md)~~

## Story 05: Grep Search

- ~~[grep-execute.md](grep-execute.md)~~

## Story 06: File Write

- ~~[file-write-execute.md](file-write-execute.md)~~

## Story 07: Bash Command

- ~~[bash-execute.md](bash-execute.md)~~

## Story 08: File Not Found Error

- ~~[file-read-error-e2e.md](file-read-error-e2e.md)~~

## Story 09: Bash Command Fails

- ~~[bash-command-error-e2e.md](bash-command-error-e2e.md)~~

## Story 10: Multi-Turn Tool Loop

No new tasks needed. The multi-tool conversation loop implemented in Story 04 (multi-tool-loop.md) already handles any number of sequential tool calls until finish_reason is "stop". Story 10's grep -> file_read -> file_write sequence works with the existing implementation.

## Story 11: Tool Loop Limit Reached

Note: The `max_tool_turns` config field was added in Story 02 (tool-config-fields.md).

- ~~[tool-loop-counter.md](tool-loop-counter.md)~~
- ~~[tool-result-limit-metadata.md](tool-result-limit-metadata.md)~~
- ~~[tool-choice-none-on-limit.md](tool-choice-none-on-limit.md)~~  *(uses hardcoded strings; refactored in Story 13)*
- ~~[tool-loop-limit-e2e.md](tool-loop-limit-e2e.md)~~

## Story 12: Session Replay E2E Verification

- ~~[replay-tool-e2e.md](replay-tool-e2e.md)~~

Note: The core replay functionality for tool messages is implemented in Story 02 (db-tool-replay.md and conversation-rebuild.md). Story 12 provides end-to-end verification that a full tool conversation can be persisted, the application restarted, and the session correctly restored with the model maintaining context.

## Story 13: Tool Choice Control (4 variants: auto, none, required, specific)

- ~~[tool-choice-config.md](tool-choice-config.md)~~
- ~~[tool-choice-serialize.md](tool-choice-serialize.md)~~
- ~~[request-with-tool-choice-param.md](request-with-tool-choice-param.md)~~  *(refactors Story 11's hardcoded tool_choice)*
- ~~[tool-choice-auto-e2e.md](tool-choice-auto-e2e.md)~~
- ~~[tool-choice-none-e2e.md](tool-choice-none-e2e.md)~~
- ~~[tool-choice-required-e2e.md](tool-choice-required-e2e.md)~~
- ~~[tool-choice-specific-e2e.md](tool-choice-specific-e2e.md)~~

## JSONL Logging Infrastructure

- ~~[jsonl-logger-core.md](jsonl-logger-core.md)~~
- ~~[jsonl-logger-timestamp.md](jsonl-logger-timestamp.md)~~
- ~~[jsonl-logger-file-output.md](jsonl-logger-file-output.md)~~
- ~~[jsonl-logger-thread-safety.md](jsonl-logger-thread-safety.md)~~
- ~~[jsonl-logger-rotation.md](jsonl-logger-rotation.md)~~
- ~~[jsonl-logger-levels.md](jsonl-logger-levels.md)~~
- ~~[jsonl-logger-reinit.md](jsonl-logger-reinit.md)~~
- ~~[repl-logger-init.md](repl-logger-init.md)~~
- ~~[openai-remove-debug-pipe.md](openai-remove-debug-pipe.md)~~
- ~~[openai-jsonl-http-logging.md](openai-jsonl-http-logging.md)~~

## Mouse Wheel Scrolling

- ~~[mouse-scroll-action-types.md](mouse-scroll-action-types.md)~~
- ~~[mouse-terminal-enable.md](mouse-terminal-enable.md)~~
- ~~[mouse-sgr-parse.md](mouse-sgr-parse.md)~~
- ~~[mouse-scroll-handler.md](mouse-scroll-handler.md)~~
- ~~[mouse-scroll-e2e.md](mouse-scroll-e2e.md)~~

## ANSI Color Support

Adds 256-color support for styled message display. User messages and commands use default terminal foreground. Assistant responses are slightly subdued (gray 249). Tool calls/results and system messages are very subdued (gray 242).

See `src/ansi.h` for implementation.

### Phase 1: Foundation
- ~~[ansi-escape-skip.md](ansi-escape-skip.md)~~ - CSI escape sequence skip function
- ~~[ansi-color-constants.md](ansi-color-constants.md)~~ - Color constants and SGR builders

### Phase 2: Width Calculation
- ~~[scrollback-ansi-width.md](scrollback-ansi-width.md)~~ - Scrollback ANSI-aware width
- ~~[input-layout-ansi-width.md](input-layout-ansi-width.md)~~ - Input buffer ANSI-aware width
- ~~[render-cursor-ansi-width.md](render-cursor-ansi-width.md)~~ - Cursor position ANSI-aware

### Phase 3: Input Handling
- ~~[input-strip-sgr.md](input-strip-sgr.md)~~ - Strip SGR from pasted input

### Phase 4: Configuration
- ~~[color-config-detection.md](color-config-detection.md)~~ - NO_COLOR and TERM=dumb detection

### Phase 5: Styling
- ~~[event-render-styling.md](event-render-styling.md)~~ - Apply colors to message kinds

## Readline Features

Adds command history and tab completion for enhanced interactive experience.

See `docs/backlog/readline-features.md` for design details.

### Feature 1: Command History

Persistent command history with navigation, deduplication, and JSONL storage in `$PWD/.ikigai/history`.

- ~~[history-config-field.md](history-config-field.md)~~ - Add history_size to config
- ~~[history-data-structures.md](history-data-structures.md)~~ - Core history data structures and operations
- ~~[history-directory-init.md](history-directory-init.md)~~ - Ensure .ikigai directory exists
- ~~[history-file-io.md](history-file-io.md)~~ - JSONL load/save/append operations
- ~~[history-navigation.md](history-navigation.md)~~ - Up/Down arrow integration
- ~~[history-deduplication.md](history-deduplication.md)~~ - Consecutive duplicate prevention
- ~~[history-integration.md](history-integration.md)~~ - End-to-end REPL integration

### Feature 2: Tab Completion

Context-aware tab completion for slash commands and arguments with interactive selection.

- ~~[completion-tab-action.md](completion-tab-action.md)~~ - Add IK_INPUT_TAB action
- ~~[completion-data-structures.md](completion-data-structures.md)~~ - Completion matching logic
- ~~[completion-layer.md](completion-layer.md)~~ - Completion display layer
- ~~[completion-navigation.md](completion-navigation.md)~~ - Arrow/tab/escape interaction
- ~~[completion-argument-matching.md](completion-argument-matching.md)~~ - Context-aware argument completion
- ~~[completion-integration.md](completion-integration.md)~~ - End-to-end integration and polish

### Feature 2b: Tab Completion Bug Fixes and fzy Integration

Fix HR rendering bug and integrate fzy fuzzy matching with correct Tab cycling behavior.

#### Bug Fixes
- ~~[completion-hr-bug-fix.md](completion-hr-bug-fix.md)~~ - Fix horizontal rule not rendering above input

#### fzy Integration
- ~~[completion-fzy-vendor.md](completion-fzy-vendor.md)~~ - Vendor fzy library source files
- ~~[completion-fzy-wrapper.md](completion-fzy-wrapper.md)~~ - Create talloc-aware fzy wrapper
- ~~[completion-fzy-integrate.md](completion-fzy-integrate.md)~~ - Replace strncmp with fzy matching

#### UI Layout
- ~~[completion-second-hr.md](completion-second-hr.md)~~ - Add second HR below input, above completions

#### Behavior Changes
- ~~[completion-display-trigger.md](completion-display-trigger.md)~~ - Typing `/` triggers display (not Tab)
- ~~[completion-state-machine.md](completion-state-machine.md)~~ - Tab cycles, ESC reverts, Space/Enter commits
- ~~[completion-layer-highlight.md](completion-layer-highlight.md)~~ - Verify highlight follows Tab cycling

#### Verification
- ~~[completion-e2e-test.md](completion-e2e-test.md)~~ - End-to-end behavior tests

## Error Context UAF Bug Fixes

Fix use-after-free bugs where errors are allocated on a context that gets freed before return. See `fix.md` for detailed analysis of the bug pattern.

### Documentation
- ~~[error-context-docs-update.md](error-context-docs-update.md)~~ - Document error context lifetime rules

### Code Fixes
- ~~[repl-init-uaf-fix.md](repl-init-uaf-fix.md)~~ - Refactor repl_init.c to "allocate late" pattern
- ~~[session-restore-uaf-fix.md](session-restore-uaf-fix.md)~~ - Fix UAF bugs in session_restore.c

## UI Polish and Completion Bug Fixes

Visual improvements and completion behavior fixes identified during manual testing.

### UI Visual Polish
- ~~[separator-unicode-box-drawing.md](separator-unicode-box-drawing.md)~~ - Replace ASCII `-` with Unicode box drawing `─` (U+2500)
- ~~[input-layer-newline.md](input-layer-newline.md)~~ - Fix input layer to append trailing `\r\n`

### Completion Behavior Fixes
- ~~[completion-prefix-matching.md](completion-prefix-matching.md)~~ - Enforce prefix matching (not subsequence)
- ~~[completion-dismiss-on-accept.md](completion-dismiss-on-accept.md)~~ - Dismiss completion menu after Tab accept
- ~~[completion-accept-cursor-position.md](completion-accept-cursor-position.md)~~ - Position cursor at end after Tab accept
