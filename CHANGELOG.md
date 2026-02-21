# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),

## [rel-12] - 2026-02-22

### Added

#### Comprehensive Model Support & Unified Thinking Display (Complete)
- **17 models across 3 providers**: Full model registry coverage — OpenAI (o1, o3, o3-mini, o3-pro, o4-mini, gpt-5, gpt-5-mini, gpt-5-nano, gpt-5-pro, gpt-5.1, gpt-5.1-chat-latest, gpt-5.1-codex, gpt-5.1-codex-mini, gpt-5.2, gpt-5.2-chat-latest, gpt-5.2-codex, gpt-5.2-pro), Anthropic (claude-haiku-4-5, claude-sonnet-4-5, claude-opus-4-5, claude-opus-4-6, claude-sonnet-4-6), Google (gemini-2.5-flash-lite, gemini-2.5-flash, gemini-2.5-pro, gemini-3-flash-preview, gemini-3-pro-preview, gemini-3.1-pro-preview)
- **Unified thinking effort display**: All providers show a consistent `Thinking: LEVEL (effort: EFFORT)` status line, normalizing each provider's native thinking/reasoning representation into a single UI
- **Per-model thinking level tables**: Each provider maps the four abstract levels (min/low/med/high) to its native API parameters — OpenAI reasoning effort strings, Anthropic budget ranges and adaptive mode, Google per-model thinking level strings
- **New models**: gpt-5.1-codex-mini (standard reasoning), gpt-5.2-pro (medium/high/xhigh reasoning), gemini-3.1-pro-preview (per-model thinking mapping), claude-sonnet-4-6 (adaptive thinking), claude-opus-4-5 (budget table)

#### End-to-End Test Suite (Complete)
- **32 live-verified e2e tests**: Every registered model has a live end-to-end test that switches to the model, sends a prompt to the real provider API, waits for a response, and asserts correct UI state — model name in status bar, thinking effort indicator, and response prefix
- **3 tool-use e2e tests**: One per provider (Anthropic, OpenAI, Google) verifying bash tool execution round-trips through the real API with tool request/response indicators
- **Mock + live dual-mode format**: Each test includes `mock_expect` steps for deterministic CI and runs against real providers in live mode with the same assertions
- **Automated e2e runner**: `check-e2e` script for CI; live mode executed via ikigai-ctl for human-observable verification
- **Mock provider coverage**: Mock HTTP server implements OpenAI Responses, Anthropic Messages, and Google GenerateContent APIs

#### Control Socket & Headless Mode (Complete)
- **Control socket**: Unix domain socket with stateless one-connection-per-message protocol; `read_framebuffer`, `send_keys`, `wait_idle` commands
- **ikigai-ctl**: Ruby client for programmatic interaction — auto-discovers socket, per-character key injection with escape sequence support
- **Headless mode**: `--headless` flag initializes terminal layer without TTY for background/CI operation
- **Framebuffer serialization**: ANSI-to-JSON serialization for programmatic screen reading

#### Documentation & Skills
- **Provider documentation**: Per-provider docs split into `docs/anthropic.md`, `docs/openai.md`, `docs/gemini.md`
- **e2e-testing skill**: JSON-based test format with step types, assertion types, and execution rules
- **Mock providers design doc**: Architecture document for the mock provider system

### Changed

#### Code Quality
- **Header guard normalization**: All header guards in `apps/ikigai/` use the `IK_` prefix
- **Namespace compliance**: 7 public functions in the commands layer renamed to add the missing `ik_` prefix
- **Memory safety**: Replaced bare `malloc` with `talloc_zero_size` in OpenAI serializers; `talloc_zero_array` used in `key_inject.c` and `serialize_framebuffer.c` escape buffers
- **Dead code cleanup**: Stale TODOs and dead commented-out code removed

#### Skills & Pipeline
- **Pipeline skill**: `--model`/`--reasoning` flags omitted by default; only set when user explicitly requests them

### Fixed
- **Framebuffer serializer**: Row-0 collapse bug — all framebuffer content was being written to row 0
- **ikigai-ctl send_keys**: Escape sequence handling corrected; `jq --compact` flag added to produce valid single-line JSON
- **ikigai-ctl protocol**: Changed from line-oriented `gets` to EOF-oriented `read` to match server's close-after-response design
- **gpt-5.1/5.2-chat-latest reasoning**: Both models use fixed adaptive reasoning — the API rejects any effort value other than `"medium"`; all four effort levels now map to `"medium"` for these models
- **Gemini 3 thinking level mapping**: Replaced hardcoded LOW/MED/HIGH strings with a per-model table; gemini-3-flash-preview, gemini-3-pro-preview, and gemini-3.1-pro-preview each have their own correct mappings
- **Anthropic adaptive model detection**: Replaced single-model hardcoded check with a proper `ADAPTIVE_MODELS[]` table; claude-sonnet-4-6 now correctly identified as adaptive

### Technical Metrics
- **Changes**: 166 files changed, +10,471/-551 lines
- **Commits**: 43 commits over development cycle
- **Quality gates**: All standard checks pass (compile, link, filesize, unit, integration, complexity)
- **E2e verification**: 32/32 tests pass in live mode against real provider APIs

## [rel-11] - 2026-02-15

### Added

#### Agent Orchestration Primitives (Complete)
- **Internal tool infrastructure**: Tool registry supports both external (fork/exec) and internal (in-process C function) tools
- **fork internal tool**: Extract fork logic into standalone internal tool for programmatic agent creation
- **send internal tool**: Direct inter-agent message delivery, replacing mail-based messaging
- **wait internal tool**: Fan-in semantics — wait for multiple sub-agents with structured per-agent results, PG LISTEN/NOTIFY wake-up, and status layer in UI
- **kill internal tool**: Agent termination with protection against killing root agents or own parent
- **/reap command**: Human-only command for explicit dead agent cleanup (kill marks dead, reap removes)
- **Agent name resolution**: Wait fan-in results display actual agent names from database instead of hardcoded placeholders
- **Sub-agent lifecycle**: System prompt guidance ensures children complete work, send results, and go idle — parents manage lifecycle through fork/wait/kill
- **PostgreSQL NOTIFY/LISTEN**: Event-driven agent communication via `db/notify.c`
- **Idle tracking**: `idle` column in agents table (migration 006) to track agents waiting for external events
- **Session ID tracking**: Agent session tracking and reaped status (migrations 007, 008)

#### Template Processing for Pinned Documents
- **Template processor module**: `${variable}` syntax in pinned documents (`apps/ikigai/template.c`)
- **Namespace support**: `${agent.*}`, `${config.*}`, `${env.*}`, `${func.*}` variable namespaces
- **Unresolved variable warnings**: Display `IK_OUTPUT_WARNING` for unresolved variables (preserved as literal text)
- **System prompt integration**: Template processing integrated with `ik_agent_get_effective_system_prompt()`

#### Source Tree Reorganization
- **New directory structure**: Reorganized source into `apps/ikigai/`, `shared/`, and `tests/` mirroring structure
- **Include path update**: All includes use full paths from project root with `-I.`

#### Claude Opus 4.6 Support
- **Model registration**: claude-opus-4-6 with 128K thinking budget
- **Adaptive thinking**: `{"thinking": {"type": "adaptive"}}` with effort parameters in output_config

#### Ralph Pipeline (External Service Migration)
- **ralph-plans API integration**: 11 goal scripts (`scripts/goal-*/run`) backed by external ralph-plans service
- **Goal commands**: goal-create, goal-list, goal-get, goal-queue, goal-start, goal-done, goal-stuck, goal-retry, goal-cancel, goal-comment, goal-comments
- **FIFO scheduling**: Fair scheduling with priority for untried goals
- **Goal dependencies**: Goals can declare `Depends: #N, #M` for ordered execution
- **Story auto-close**: Automatic story completion when all goals finish
- **Orchestrator improvements**: Completed count in status line, queued/running counts, clone cleanup, directory restructuring to org/repo/goal-num
- **watch-ralphs**: Terminal watcher script with wrap-aware log tail display and flicker-free rendering
- **Goal approval workflow**: goal-approve harness and goal-queue validation
- **Ralph stats**: Commit hashes and diff stats in stats records, --name flag for tracking

#### UI Improvements
- **Interrupted message styling**: Render interrupted/cancelled messages with `✗` prefix and gray color, excluded from LLM context
- **Streaming display fix**: Skip empty/whitespace-only text blocks in streaming display

#### Documentation
- **AGENTS.md**: Canonical project context document with agent guidance
- **Config system design**: Architecture design document for future config system
- **rel-11 internal tools design**: Design documentation and command reference

### Changed

#### Inter-Agent Communication
- **Mail system replaced**: Removed 5 mail commands (mail_send, mail_check, mail_read, mail_delete, mail_filter) in favor of 2 primitives (send, wait) — total internal tools reduced from 7 to 4

#### Configuration
- **Config file loading removed**: Removed config file loading infrastructure; configuration via environment variables only
- **OpenAI model config**: Refactored into unified table

#### Testing Strategy
- **Three-tier checks**: Restructured into inner loop (`--file=PATH`), standard checks (exit gate), and deep checks (on request)
- **/reset-repo**: Renamed from /jj-reset, moved to deterministic Ruby script

#### Pipeline Infrastructure
- **Goals-first workflow**: Default workflow creates and queues goals via ralph-plans API; local changes are rare exceptions
- **Spot-check workflow**: Moved to PR-based verification with clone cleanup

### Fixed

- **text_delta handling**: Restored Anthropic text_delta handling dropped by dead code pruning and PR merge
- **Quality checks**: Added wrappers and resolved valgrind failures
- **Orchestrator process exit**: Fixed detection with Open3.popen2e
- **Ralph log buffering**: Fixed buffering in log-mode
- **watch-ralphs flicker**: Clear to end of line and screen

### Removed

#### Config File Infrastructure
- Config file loading and parsing code

#### Mail Commands
- /check-mail, /read-mail, /delete-mail, /filter-mail (replaced by /send and /wait)

#### Pipeline (Old Internal Infrastructure)
- Old internal pipeline scripts (harness goal/story/orchestrator/ralph scripts)
- Auto-merge GitHub workflow
- Trigger-ci-on-merge workflow
- project/pseudo-code directory

#### Dead Code Pruning (73 functions)
- anthropic_cleanup, anthropic_fdset, anthropic_timeout
- build_completion_for_success, categorize_http_response
- extract_basic_fields, extract_error_strings, extract_model_if_needed
- find_and_process_completed_request, http_write_callback
- ik_anthropic_process_content_block_start, ik_anthropic_process_content_block_stop
- ik_anthropic_process_message_delta, ik_anthropic_process_message_start
- ik_array_set, ik_config_get_default_provider
- ik_db_mail_mark_read, ik_debug_log_write
- ik_error_is_retryable, ik_event_renders_visible
- ik_google_build_headers, ik_google_extract_thought_signature_from_response
- ik_google_handle_error, ik_google_parse_error
- ik_google_stream_completion_cb, ik_google_stream_end_tool_call_if_needed
- ik_google_stream_get_finish_reason, ik_google_stream_get_usage
- ik_layer_cake_get_total_height, ik_log_error_json, ik_log_init, ik_log_shutdown
- ik_msg_create_tool_result, ik_openai_chat_maybe_emit_start
- ik_openai_chat_process_delta, ik_openai_chat_stream_get_finish_reason
- ik_openai_handle_error, ik_openai_http_completion_handler
- ik_openai_maybe_emit_start, ik_openai_responses_handle_reasoning_summary_text_delta
- ik_openai_responses_handle_response_created, ik_openai_responses_stream_ctx_create
- ik_openai_stream_completion_handler, ik_openai_validate_thinking
- ik_pp_int32, ik_render_scrollback, ik_repl_handle_history_prev_action
- ik_request_add_message_blocks, ik_response_add_content
- ik_separator_layer_set_debug, ik_tool_arg_get_int, ik_tool_arg_get_string
- openai_cleanup, openai_info_read, openai_perform, openai_timeout
- parse_chat_usage, process_choices_array, process_completed_request
- process_input_json_delta, process_message_content, process_output_text
- process_refusal, process_text_delta, process_text_part
- process_thinking_delta, replay_unpin_command
- add_text_content, add_tool_result_content, google_cancel

### Technical Metrics
- **Changes**: 2,101 files changed, +218,138/-230,106 lines
- **Commits**: 217 commits over development cycle
- **New files**: 969 files added
- **Removed files**: 1,021 files deleted
- **Dead code pruned**: 73 functions removed
- **Quality gates**: All 11 checks pass (compile, link, filesize, unit, integration, complexity, sanitize, tsan, valgrind, helgrind, coverage)
- **Test coverage**: 90%+ lines, functions, and branches

## [rel-10] - 2026-02-01

### Added

#### Multi-Provider AI Support
- **Status layer**: Shows current model and thinking level in REPL UI
- **Gemini 3 support**: Updated Google AI provider for Gemini 3 API
- **AI providers documentation**: User-facing documentation for configuring supported providers
- **Gemini 2.5 strict validation**: Strict model validation with power-of-2 thinking budgets
- **Anthropic thinking budgets**: Improved thinking budget handling for Anthropic provider

#### /toolset Command
- **Tool set management**: New `/toolset` command for managing which tools are offered to AI providers

#### /fix-checks Command
- **Quality check repairs**: New `/fix-checks` slash command replaces fix-checks harness script

#### /jj-reset Command
- **Fresh working copy**: New `/jj-reset` command to reset jj working copy to fresh state from main@origin

#### XDG-Aware Configuration (Complete)
- **Directory configuration**: IKIGAI_STATE_DIR, IKIGAI_DATA_DIR, IKIGAI_CACHE_DIR with compiled defaults
- **XDG compliance**: Follows XDG Base Directory specification
- **Database environment variables**: Configure PostgreSQL connection via PGHOST, PGDATABASE, PGUSER
- **User-facing configuration docs**: Comprehensive documentation for all configuration options

#### System Prompt System (Complete)
- **File-based loading**: System prompt loaded from `$IKIGAI_DATA_DIR/prompts/system.md`
- **Effective prompt display**: Display and style effective system prompt in REPL
- **Secret field**: Added secret to system prompt for verification

#### Pin/Unpin Commands (Complete)
- **/pin and /unpin commands**: Manage pinned documents that form the system prompt
  - `/pin` (no args) lists pinned paths in FIFO order
  - `/pin PATH` adds document to pinned list with event persistence
  - `/unpin PATH` removes document from pinned list with event persistence
  - Both filesystem paths and `ik://` URIs supported
  - Missing file at pin time: warn in subdued text, do not add to list
  - Already pinned/not pinned: warn in subdued text, record event anyway
- **Agent struct extension**: Added `pinned_paths` (char**) and `pinned_count` (size_t) to `ik_agent_ctx_t`
- **Document cache module**: Global talloc-based cache for pinned document content
  - `doc_cache_get(path)` - returns content, loads and caches if not present
  - `doc_cache_invalidate(path)` - invalidates specific path
  - `doc_cache_clear()` - invalidates entire cache
  - Handles both filesystem and `ik://` paths via `ik_paths_translate_ik_uri_to_path()`
- **Pin replay mechanism**: Rebuild pinned_paths from events during agent restoration
  - Replay walks back to agent's fork/creation event only
  - Fork events include `pinned_paths` snapshot of parent's pins
  - `replay_command_effects()` extended for pin/unpin commands
- **Root agent bootstrap**: Synthetic event with default `$IKIGAI_DATA_DIR/prompts/system.md` pin
- **System prompt assembly**: Build system prompt by iterating pinned documents in FIFO order
- **/refresh integration**: Invalidates document cache when refresh command executed
- **Fork behavior**: Child agents inherit parent's pinned_paths via snapshot in fork event

#### ik:// URI Translation (Complete)
- **Bidirectional path translation**: Clean ik:// URIs for models, filesystem paths for tools
  - Model requests: ik:// → filesystem path ($IKIGAI_STATE_DIR/path)
  - Tool results: filesystem path → ik://
  - Transparent translation at tool execution boundaries
- **ik://system namespace**: Translation for system-level paths
- **Edge case handling**: Multiple paths, trailing slashes, false positive avoidance

#### List Tool (Complete)
- **`list` external tool**: Redis-style deque operations for per-agent task management
  - Operations: lpush, rpush, lpop, rpop, lpeek, rpeek, list, count
  - Per-agent scope via `IKIGAI_AGENT_ID` environment variable
  - Stores data in `$IKIGAI_STATE_DIR/agents/<agent_id>/list.json`
  - O(1) token cost per operation (vs TodoWrite's O(n))

#### Keyboard Improvements
- **Home/End keys**: Support for Home/End key navigation in input
- **Arrow keys with NumLock**: Fixed arrow key handling when NumLock is enabled

#### UI Improvements
- **Braille spinner**: Replaced ASCII spinner (|/-\\) with braille animation (⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏), slowed animation rate
- **Flicker elimination**: Screen clear moved from render path to terminal initialization
- **Cursor visibility fix**: Explicit cursor state at end of each render frame
- **Hidden cursor during spinner**: Cursor hidden while spinner is active

#### Documentation
- **Development setup guide**: Comprehensive installation and development guide
- **AI providers guide**: User-facing documentation for configuring AI providers
- **Module documentation**: doc_cache, paths, and file_utils modules documented
- **make-as-query-engine**: Documentation for using `make -n` to query file dependencies

#### Developer Tools
- **Dev framebuffer dump**: Compile-time conditional for debugging render issues
- **dev-dump skill**: Documentation for debug buffer dumps
- **ralph-stats**: Skill and script for tracking Ralph execution statistics
- **Time tracking**: Ralph logs execution time and statistics

#### Security & Quality
- **Banned functions enforcement**: Compile-time enforcement of banned unsafe C functions
- **Dead code detection**: Improved detection to catch circular test dependencies

### Changed

#### REPL Behavior
- **History navigation disabled**: Keeps readline commands (Ctrl+A/E/K/U/W) but disables Up/Down history navigation
- **Readline in CSI u mode**: Fixed readline command support in CSI u keyboard protocol mode
- **Hidden system messages**: System messages no longer displayed in UI
- **Thinking level display**: Changed from 'disabled' to 'min'
- **Banner alignment**: Banner text columns aligned for cleaner display

#### Memory Safety
- **Atomic agent state**: Agent state made atomic to eliminate mutex contention in spinner render path
- **Zeroing allocations**: Convert all talloc allocations to zeroing variants

#### Ralph Harness
- **Ralph v0.4.0**: Upgraded to Ralph version 0.4.0
- **Gemini thinkingLevel**: Uses uppercase LOW/HIGH values

#### List Tool
- **Directory change**: Uses IKIGAI_STATE_DIR instead of IKIGAI_CACHE_DIR

#### jj Skill
- **Track-then-push workflow**: Updated jj skill to use `jj bookmark track` before first push
- **Selective commit requirement**: Added prohibition on restoring others' work

#### Infrastructure
- **Database scripts**: Use environment variables for PostgreSQL connection
- **Build artifacts**: Cleaned up build artifacts and CI paths

### Fixed

#### Multi-Provider Fixes
- **Gemini 3 tool calls**: Fixed crashes and HTTP 400 errors in tool call handling
- **Gemini 3 tool call bugs**: Fixed various tool call handling issues
- **OpenAI Responses API strict mode**: Fixed compatibility with strict mode validation
- **OpenAI Responses API serialization**: Fixed serialization for multi-turn conversations
- **Responses API tool format**: Fixed tool serialization format

#### UI Fixes
- **Cursor during spinner**: Hide cursor during spinner display
- **Cursor visibility control**: Fixed cursor visibility in REPL viewport
- **Live streaming output**: Fixed prefix and blank line spacing
- **Render test loops**: Fixed off-by-one error in search loops
- **Buffer overflow**: Fixed ik_render_scrollback (13 bytes needed, not 7)

#### CI Fixes
- **Container git**: Added safe.directory for git in container
- **CI badge**: Fixed workflow_dispatch and trigger for full CI run
- **Paths-filter**: Install git before paths-filter step

#### Other Fixes
- **Tool result persistence**: Fixed database persistence for conversation replay
- **Web-search tool bugs**: Various bug fixes and clean target
- **XML output**: Added to render_text_test
- **PostgreSQL callback**: Added test for notice processor callback

### Removed

- **/debug command**: Removed debug pipe infrastructure (use dev framebuffer dump instead)
- **CDD workflow**: Removed Context-Driven Development workflow, keeping Ralph execution harness
- **note.md**: Removed temporary note file
- **Old task scripts**: Cleaned up obsolete task scripts
- **structured-memory docs**: Superseded by simpler pin implementation

#### Dead Code Pruning
- ik_db_messages_load, ensure_capacity
- ik_repl_handle_tool_completion
- ik_paths_get_cache_dir, ik_paths_get_bin_dir
- ik_line_array module
- ik_openai_responses_stream_get_usage
- ik_log_fatal_json
- ik_history_save

### Technical Metrics
- **Quality gates**: All 11 checks pass (compile, link, filesize, unit, integration, complexity, sanitize, tsan, valgrind, helgrind, coverage)
- **Test coverage**: 90%+ lines, functions, and branches

## [rel-09] - 2026-01-22

### Added

#### Tools Refactoring (Complete)
- **Minimal main pattern**: Refactored all existing tools to separate logic from main()
  - bash, file_edit, file_read, file_write, glob, grep all refactored
  - Logic extracted to .c/.h files for direct unit testing
  - main.c files reduced to argument parsing and entry point
- **Direct unit tests**: Comprehensive test suites for all tool logic
  - bash_direct_test, file_edit_direct_test, file_read_direct_test
  - file_write_direct_test, glob_direct_test, grep_direct_test
  - Tests call logic functions directly without subprocess overhead
- **Wrapper infrastructure**: Enhanced wrapper system for testability
  - Added wrapper_web.c/h for curl/libxml2 operations
  - Extended wrapper_posix.h with additional POSIX wrappers
  - All wrappers mockable for deterministic testing

#### Web Tools (Complete)
- **web-fetch-tool**: URL content fetching with HTML→markdown conversion
  - HTTP fetching via libcurl with redirect support
  - HTML parsing using libxml2
  - DOM-to-markdown conversion (headings, paragraphs, links, lists, formatting)
  - Script/style/nav tag stripping for clean output
  - Pagination support (offset/limit on markdown lines)
  - Title extraction from HTML
  - No credentials required (works out of the box)
  - 100% test coverage with comprehensive unit tests
  - HTML test fixtures for deterministic testing
- **web-search-brave-tool**: Brave Search API integration
  - Pagination support (count/offset parameters)
  - Domain filtering (allowed_domains/blocked_domains)
  - Post-processing domain filter implementation
  - Comprehensive unit tests with 100% coverage
  - Credential discovery: BRAVE_API_KEY → credentials.json → config_required event
- **web-search-google-tool**: Google Custom Search API integration
  - Pagination support (num/start parameters, 1-based indexing)
  - Domain filtering via parallel multi-domain calls
  - Parallel request handling for multiple allowed_domains
  - Comprehensive unit tests with 100% coverage
  - Credential discovery: GOOGLE_SEARCH_API_KEY, GOOGLE_SEARCH_ENGINE_ID
- **Common features**:
  - Identical JSON response format across providers
  - Tools always advertised to LLM (even without credentials)
  - Helpful config_required events with setup instructions
  - Memory-safe talloc implementation
  - Error handling with machine-readable error codes

#### Ralph Harness Improvements
- **Better error visibility**: Output unhandled message types in subdued text
  - Unknown message types logged with "?" prefix
  - Consistent formatting with tool output
  - Improved debugging for protocol changes
- **Improved error handling**: Enhanced retry logic for structured output
- **Better documentation**: Updated ralph skill with complete flag documentation
- **Quality gate enforcement**: Added quality gate requirement to Ralph prompt
- **Environment isolation**: Clear ANTHROPIC_API_KEY when invoking claude -p (pluribus)

#### Quality Infrastructure (Complete)
- **Rubyified check scripts**: Converted all harness check scripts to Ruby
  - Consistent output formatting and error handling
  - Better JSON output structure
  - Improved parallel execution support
- **Fix harnesses**: Added automated repair scripts for all quality checks
  - fix-compile, fix-filesize, fix-unit, fix-integration
  - fix-complexity, fix-sanitize, fix-tsan, fix-valgrind
  - fix-helgrind, fix-coverage
  - Each fix-* script launches Ralph with appropriate goal file
- **Goal files**: Comprehensive goal files for all fix harnesses
  - Detailed acceptance criteria
  - Reference to relevant skills
  - Clear outcomes definition

#### Tool System Improvements
- **Alphabetical tool sorting**: Tools displayed in sorted order via `/tool` command
- **Environment variable support**: Added BRAVE_API_KEY, GOOGLE_SEARCH_API_KEY, GOOGLE_SEARCH_ENGINE_ID
- **Tool registry**: Added tool registry sorting and display functions

#### Documentation
- **External tool architecture**: Comprehensive documentation for external tool system
  - Tool discovery and registration patterns
  - Subprocess communication protocols
  - JSON schema definitions
- **Pull-request skill**: Updated for jj repository workflow
  - Correct `gh pr create --repo` usage
  - Fixed examples for jj bookmarks
- **Goal authoring skill**: Comprehensive guide for writing Ralph goals
- **New build system**: Architecture and patterns documentation for Phase 1 Makefile
- **Debian Docker**: Documentation for Debian 13.2 container environment

### Changed

#### Build System
- **Makefile rewrite**: Complete Phase 1 rewrite with elegant architecture
  - Replaced complex Makefile with clean, maintainable version
  - New check-compile target with parallel compilation and clean output
  - New check-link target with race condition fixes
  - New check-unit target with XML parsing for test results
  - New check-complexity target for cyclomatic complexity enforcement
  - New check-coverage target with consistency improvements
  - New check-sanitize target for ASan/UBSan validation
  - Added web tool targets (web_search_brave_tool, web_search_google_tool, web_fetch_tool)
  - Added direct test targets for all tools
  - libxml2 integration via pkg-config
  - All tests use srunner_set_xml() for XML output
- **CI infrastructure**: Enhanced continuous integration
  - Switched to Debian 13.2 container for coverage check compatibility
  - Run all 11 check-* targets in order
  - Run tests as non-root user with VCR debug support
  - Fixed workspace paths and API key secrets
  - Added provider API keys to development and CI environments
- **Test organization**: Reorganized unit tests into tools/ subdirectory
  - All tool tests moved to tests/unit/tools/
  - Clearer separation of concerns
  - Better discoverability

#### Code Quality
- **Coverage threshold**: Lowered from 100% to 90% for pragmatic coverage
- **Test coverage**: Maintained 90%+ across all modules
- **File size compliance**: All files within 16KB limit
- **Complexity compliance**: All functions within cyclomatic complexity limits

### Fixed
- **Memory leaks**: Fixed various memory leaks in web tools
- **Buffer overflows**: Fixed heap-buffer-overflow in file_edit.c
- **Test reliability**: Improved test determinism and reliability
- **Ralph output**: Fixed structured output handling with retry logic
- **VCR test failures**: Removed duplicate curl wrappers from wrapper_web causing VCR failures
- **CI coverage**: Fixed coverage checks by switching to Debian 13.2 container
- **Makefile warnings**: Fixed duplicate target warnings

### Technical Metrics
- **Changes**: 302 files modified, +19,294/-11,989 lines
- **New files**: 126 files added (tools, tests, harnesses, documentation)
- **Removed files**: 36 files deleted (obsolete CDD artifacts)
- **Test coverage**: 90%+ lines, functions, and branches
- **Code quality**: All 11 quality checks pass
- **Quality gates**: compile, link, filesize, unit, integration, complexity, sanitize, tsan, valgrind, helgrind, coverage

## [rel-08] - 2026-01-15

### Added

#### External Tool System Infrastructure (Complete)
- **Tool discovery**: Automatic tool detection and registration system
- **Tool command**: `/tool` command infrastructure for tool management
- **External tools**: Offer tools to AI providers for execution
- **Bash tool**: Full bash tool implementation with integration tests

#### Ralph Automation Harness (Complete)
- **Ralph v0.3.0**: Bidirectional streaming with retry logic for automated task execution
- **Ralph loop harness**: Iterative requirement completion with history tracking
- **Work agent pattern**: Moved history writing from ralph loop to work agent
- **Goal tracking**: Ralph goal file for self-improvement TODOs

#### Quality Harnesses
- **Fix-checks**: Automated check fixing with sub-agent orchestration
- **Notify**: Push notifications via ntfy.sh for long-running tasks
- **Pluribus**: Additional quality harness for multi-agent coordination
- **Check-? **: Documented 60-minute timeout, foreground execution, JSON output format

#### CDD Pipeline Improvements
- **Gap verification**: Checklists for gap command
- **TDD enforcement**: Enforced TDD across CDD pipeline with streamlined verification
- **$CDD_DIR requirement**: Environment variable required for workspace path
- **Pull-request skill**: Concise PR template for consistent pull requests

#### Documentation
- **Test refactoring strategy**: Comprehensive document for test improvement approach
- **Paths module planning**: Phase 0 planning for paths module foundation
- **Meta README**: Rewritten .claude README with meta-level architecture overview

### Changed

#### Version Control Migration
- **Git to jj**: Migrated from git to Jujutsu (jj) for version control
- **jj workflow**: Updated all VCS operations to use jj commands

#### Build System
- **Install target**: Removed all dependency from install target for cleaner builds
- **Secrets management**: Moved NTFY_TOPIC to secrets file for security

#### Agent System Simplification
- **Task skill removed**: Removed task skill and associated scripts
- **CDD skill simplified**: Streamlined cdd skill after task removal
- **Logging skills**: Reorganized into debug-log and event-log skills
- **Harness --file support**: Added --file flag to harness scripts for targeted execution

### Fixed
- **Segfault on exit**: Fixed crash during application shutdown
- **Tool discovery crash**: Fixed crash in tool discovery initialization
- **Quality harness bugs**: Various bugfixes in quality harness execution

### Removed

#### Dead Code Pruning
- ik_error_category_name
- ik_content_block_thinking
- Database layer dead code
- Various unused functions identified by prune harness

### Development

#### Testing & Quality Gates
- **Test infrastructure**: Comprehensive test infrastructure improvements
- **Coverage**: Maintained 100% (lines, functions, branches)
- **Sanitizers**: Clean runs through ASan, UBSan, TSan
- **Memory checkers**: All tests pass under Valgrind and Helgrind

### Technical Metrics
- **Changes**: 1,683 files modified, +188,063/-39,589 lines
- **Commits**: 433 commits over development cycle
- **Test coverage**: 100% lines, functions, and branches
- **Code quality**: All lint, format, and sanitizer checks pass
- **Quality gates**: 7 gates (lint, check, sanitize, tsan, valgrind, helgrind, coverage)

## [rel-07] - 2026-01-02

### Added

#### Extended Thinking Support (Complete)
- **Thinking signatures**: Added signature field to thinking content blocks for verification
- **Redacted thinking type**: New IK_CONTENT_REDACTED_THINKING for encrypted thinking data
- **Streaming capture**: Capture thinking signatures during streaming responses
- **Serialization**: Full serialization support for thinking signatures across all providers
- **Deep copy support**: Updated deep copy utilities to handle thinking signature fields
- **Response builder**: Include thinking blocks in response construction
- **Tool call integration**: Include thinking data in tool call messages
- **Database persistence**: Store and restore thinking blocks with database replay
- **Content block API**: Updated builder API for signature and redacted thinking types

#### Dead Code Pruning System (Complete)
- **/prune command**: Automated dead code removal with sub-agent orchestration
- **Prune harness**: Two-phase approach with classification and deletion
- **Model escalation**: Automatic escalation through model ladder on failures
- **Per-test flow**: Process test files individually for safer deletion
- **Debug mode**: --debug flag for troubleshooting with preserved workspace
- **Function targeting**: --function flag to target specific functions
- **False positives tracking**: dead-code-false-positives.txt for known exceptions

#### Coverage Infrastructure
- **Per-test coverage reports**: Individual coverage reports alongside XML test results
- **Coverage map**: Source→test mapping in `.claude/data/source_tests.json`
- **Early-commit strategy**: /coverage command prioritizes incremental commits

### Changed

#### Multi-Provider Improvements
- **OpenAI tool calls**: Fixed handling when role appears in same delta chunk
- **Streaming tool calls**: Implemented streaming tool call support for all providers
- **OpenAI strict mode**: Add all properties to required array for schema compliance
- **Tool schemas**: Added additionalProperties:false for stricter validation
- **Google models**: Added Gemini 2.5 models with standardized naming
- **SSE parsing**: Fixed CRLF support in Server-Sent Events parsing

#### Code Quality & Refactoring
- **CDD rename**: Renamed RPI (Release Process Infrastructure) to CDD (Context Driven Development)
- **File splits**: Split 50+ large files to comply with 16KB limit
- **Test organization**: Reorganized provider tests into focused modules
- **Cyclomatic complexity**: Extracted helper functions to reduce complexity violations
- **Skill compression**: Compressed skill files by ~40% while preserving content

#### Bug Fixes
- **Memory safety**: Use talloc_zero_array to prevent uninitialized memory access
- **Use-after-free**: Fixed bugs detected by sanitizers in Google error parsing
- **Thread leaks**: Fixed thread leak errors in repl_tool_completion_test
- **Race conditions**: Fixed data races in test mock state and fork_pending flag
- **Test timeouts**: Added 30-second timeouts to database tests

### Development

#### Testing & Quality Gates
- **Test coverage**: Achieved and maintained 100% (lines, functions, branches)
- **Branch coverage**: Improved from 99.6% to 100% across all source files
- **Coverage exclusions**: Updated LCOV limit to 3405 for legitimate exclusions
- **Sanitizer compliance**: Clean runs through ASan, UBSan, TSan
- **Memory checkers**: All tests pass under Valgrind and Helgrind
- **Test infrastructure**: Improved harness scripts with elapsed time and alignment

#### Dead Code Removed
- ik_anthropic_serialize_request, ik_anthropic_build_headers
- ik_anthropic_validate_thinking, ik_anthropic_stream_get_usage
- ik_anthropic_start_stream, ik_anthropic_parse_response
- ik_anthropic_parse_error, ik_anthropic_get_retry_after
- ik_config_get_default_provider, ik_completion_matches_prefix
- ik_completion_clear, ik_byte_array_set, ik_byte_array_capacity
- ik_ansi_init, ik_agent_apply_defaults
- And many other unused functions

### Technical Metrics
- **Changes**: 1,433 files modified, +188,693/-37,528 lines
- **Commits**: 817 commits over development cycle
- **Test coverage**: 100% lines (12,578), functions (690), and branches (4,704)
- **Code quality**: All lint, format, and sanitizer checks pass
- **Quality gates**: 7 gates (lint, check, sanitize, tsan, valgrind, helgrind, coverage)

## [rel-06] - 2025-12-19

### Added

#### Multi-Agent System (Complete)
- **Agent registry**: Database-backed agent tracking with UUID-based identification
- **Agent lifecycle**: Creation, state management, and termination of multiple agents
- **/fork command**: Create child agents with optional prompt argument
  - History inheritance via fork_message_id
  - Sync barrier for running tools
  - Parent scrollback population
- **/kill command**: Agent termination with multiple modes
  - Self termination (/kill self)
  - Targeted termination (/kill \<uuid\>)
  - Cascade termination (--cascade flag)
- **Agent navigation**: Keyboard shortcuts for agent switching
  - Ctrl+Arrow navigation events
  - Parent/child navigation
  - Sibling navigation
  - UUID prefix matching for agent lookup
- **/agents command**: Display agent hierarchy tree with visual indentation
- **Agent persistence**: Fork and command events stored in database
- **Agent restoration**: Range-based replay algorithm for multi-agent startup

#### Inter-Agent Mailbox System (Complete)
- **Mail database schema**: Migration 004 with inbox/outbox tables
- **Mail message structure**: ik_mail_t with sender/recipient tracking
- **/send command**: Inter-agent message delivery
- **/check-mail command**: List inbox contents with sender information
- **/read-mail command**: View individual messages
- **/delete-mail command**: Permanent message deletion using inbox position
- **/filter-mail command**: Inbox management and filtering

#### Infrastructure Improvements
- **JSONL logger integration**: Replaced debug pipe with structured logging
  - Lifecycle management (first in, last out)
  - Panic event logging
  - Request/response logging
- **UUID generation module**: Standalone base64url UUID creation (22 chars)
- **Database transactions**: begin/commit/rollback support for atomic operations
- **Agent state switching**: Preserve state when switching between agents
- **Message structure**: Added agent_uuid and id fields to ik_msg_t
- **Response builder pattern**: Standardized tool response JSON construction
- **Schema builder pattern**: Data-driven tool schema definitions
- **Temporary context helper**: tmp_ctx_create() for simplified memory management

### Changed

#### Code Quality & Refactoring
- **Namespace standardization**: Applied ik_MODULE_ prefix to public functions
  - Event handlers: ik_repl_ prefix
  - Commands: ik_cmd_ prefix
  - Tools: ik_tool_ prefix
- **Parameter naming**: Standardized TALLOC_CTX parameters to 'ctx' across codebase
- **Function clarity**: Expanded abbreviated names in debug_pipe and tool modules
- **Message consolidation**: Migrated from ik_message_t to unified ik_msg_t structure
- **Tool system refactoring**:
  - Response builders for all tools (bash, file_read, file_write, glob, grep)
  - Declarative schema builders with ik_tool_schema_def_t
  - Data-driven parameter definitions with ik_tool_param_def_t
- **Creator functions**: Changed OOM-only creators to return pointers directly
- **Static function policy**: Updated with pragmatic guidelines and LCOV support
- **Code organization**: Deleted obsolete session_restore module and msg_from_db code

#### Bug Fixes
- **Bug 6**: Fixed /delete-mail to use inbox position instead of database ID
- **Bug 5**: Fixed agent restore to filter metadata events correctly
- **Bug 4**: Filter metadata events from OpenAI API serialization
- **Bug 2**: Fixed separator width calculation to handle ANSI escape codes
- **Initialization fixes**:
  - Random seed initialization in main() for UUID generation
  - Agent created_at timestamp initialization
  - Session_id initialization before agent restoration
  - Proper handling of created_at=0 in child agent navigation
- **Race conditions**:
  - Fixed fork_pending flag data race
  - Fixed helgrind race in test mock state
  - Fixed database connection parallel test issues
- **Error handling**:
  - Display slash command errors to user
  - NULL pointer checks after talloc_steal
  - Unchecked database connection return values

#### Quality Gate Fixes
- **Helgrind**: Serialized test execution to fix flaky database tests (Makefile -P 1)
- **Coverage**: Wrapped yyjson inline mutation functions to achieve 100% branch coverage
- **Lint**: Split large files to comply with 16KB size limit
- **File splits**: Divided oversized modules for better maintainability

### Development

#### Testing & Quality Gates
- **Test coverage**: Maintained 100% (lines, functions, branches)
- **Quality pipeline**: Clean run through lint, check, sanitize, tsan, valgrind, helgrind, coverage
- **Comprehensive testing**:
  - Agent restoration tests
  - Mail system integration tests
  - Database error path coverage
  - Command execution tests
  - Navigation event tests
- **Build improvements**:
  - Added build-tests target (build without execution)
  - Fixed parallel test execution issues
  - Enhanced error path testing

#### Documentation
- **Multi-agent architecture**: Complete documentation for agent registry and lifecycle
- **Gap analysis**: Systematic identification and resolution of architectural gaps
- **Refactoring recommendations**: Comprehensive plan for code quality improvements
- **Task system**: 247 TDD task files with detailed specifications
- **Static function policy**: Guidelines for static vs public function decisions
- **Mocking patterns**: Updated documentation for test infrastructure

### Technical Metrics
- **Changes**: 450 files modified, +53,594/-5,362 lines
- **Commits**: 247 commits over development cycle
- **Test coverage**: 100% lines, functions, and branches
- **Code quality**: All lint, format, and sanitizer checks pass
- **Quality gates**: 7 gates (lint, check, sanitize, tsan, valgrind, helgrind, coverage)

## [rel-05] - 2025-12-14

### Added

#### Agent Process Model - Phase 0 (Complete)
- **Agent context structure**: Created `ik_agent_ctx_t` with UUID-based identity (base64url, 22 chars)
- **State extraction**: Migrated per-agent state from monolithic repl_ctx to agent_ctx:
  - Display state (scrollback, viewport, marks)
  - Input state (input_buffer, saved input)
  - Conversation state (messages, history)
  - LLM state (connection, streaming)
  - Tool state (execution context)
  - Completion state (tab completion)
  - Spinner state (activity indicator)
- **Agent integration**: Added `current` agent pointer to repl_ctx
- **Architecture ready**: Single agent works, code organized for multi-agent support

#### Shared Context Architecture (Complete)
- **Shared context structure**: Created `ik_shared_ctx_t` for global/process-wide state
- **State migration**: Moved shared state from repl_ctx to shared_ctx:
  - Configuration (cfg)
  - Terminal interface (term)
  - Render context (render)
  - Database context (db_ctx, session_id)
  - Command history
  - Debug infrastructure
- **Clear separation**: Isolated per-agent state (agent_ctx) from shared state (shared_ctx)
- **Multi-agent ready**: Foundation for multiple agents sharing common infrastructure

#### Task Orchestration Infrastructure (Complete)
- **Script-based orchestration**: Deno scripts in `.ikigai/scripts/tasks/` for task management
- **Task queue management**: `next.ts`, `done.ts`, `escalate.ts`, `session.ts` scripts
- **Model/thinking escalation**: Automatic capability escalation on task failure with progress
- **Orchestration command**: `/orchestrate PATH` for automated task execution
- **Task refactoring**: Removed Agent sections, streamlined task file format

#### CSI u Keyboard Protocol (Kitty Protocol)
- **Protocol support**: Parse CSI u escape sequences for enhanced keyboard input
- **XKB integration**: libxkbcommon for keyboard layout-aware shifted character translation
- **Modified keys**: Support for modified Enter sequences and other special keys

#### Scroll Improvements
- **Scroll detector**: State machine with burst absorption for smooth scrolling
- **Mouse wheel**: Increased scroll lines from 1 to 3 for faster navigation
- **Burst threshold**: Reduced from 20ms to 10ms for more responsive mouse wheel

### Changed

#### Code Quality & Refactoring
- **Tool test helpers**: JSON test helpers for cleaner, more maintainable tool tests
- **Response builders**: Extracted tool response builder utilities to eliminate duplication
- **File reader extraction**: Created `ik_file_read_all` utility for consistent file reading
- **Logger dependency injection**: Added DI pattern to logger for better testability
- **Circular dependency fix**: Broke msg.h → db/replay.h circular dependency
- **Unused code removal**: Removed conversation.h and other unused modules
- **Test refactoring**: Applied JSON helpers across all tool execution tests

#### Build System
- **Release builds**: Fixed MOCKABLE wrapper issues for NDEBUG builds
- **Wrapper headers**: Added inline implementations for pthread, stdlib wrappers in release mode
- **Format warnings**: Fixed format-truncation and format-nonliteral warnings

#### Documentation Organization
- **Directory restructure**: Renamed `docs/` to `project/` for internal design documentation
- **User documentation**: Created new `docs/` directory for user-facing content
- **Reference updates**: Updated all internal references from `docs/` to `project/`
- **Infrastructure docs**: Moved iki-genba from `.agents/` to `.ikigai/` with documentation

#### Code Cleanup
- **Legacy logger**: Removed functions that break alternate buffer mode
- **Scroll debug**: Removed debug logging from viewport actions
- **File organization**: Split test files to comply with 16KB size limit

### Development

#### Testing & Quality Gates
- **Test coverage**: Maintained 100% (lines, functions, branches)
- **Helgrind fixes**: Fixed mutex destroy false positives and double-destroy issues
- **Thread safety**: Improved thread handling for sanitizer compliance
- **New test suites**: scroll_detector, terminal_csi_u, shared context tests
- **Coverage improvements**: Added LCOV exclusions for defensive branches, achieved clean coverage
- **Valgrind fixes**: Fixed log rotation race conditions and talloc false positives

#### Documentation
- **Agent process model**: Comprehensive design document for multi-agent architecture
- **Shared context design**: Documentation for global state architecture
- **Task orchestration**: Complete orchestration system documentation with script APIs
- **Phase 0 tasks**: Complete TDD task files for agent context extraction
- **Refactor tasks**: Task files for code quality improvements and technical debt reduction
- **Skill system**: Updated personas and added git, database, and other skill modules
- **Worktree management**: Documentation for git worktree usage and transaction logs

### Technical Metrics
- **Changes**: 700+ files modified, +40,000/-15,000 lines (estimated)
- **Commits**: 187 commits over development cycle
- **Test coverage**: 100% lines, functions, and branches
- **Code quality**: All lint, format, and sanitizer checks pass

## [rel-04] - 2025-12-07

### Added

#### Local Tool Execution (Complete)
- **Tool dispatcher**: Route tool calls by name to execution handlers
- **File operations**: file_read and file_write tools with output truncation
- **Code search**: glob and grep tools for codebase exploration
- **Shell execution**: bash tool with command execution and error handling
- **Tool loop**: Multi-tool iteration with finish_reason detection and loop limits
- **Conversation integration**: Tool messages added to conversation state with DB persistence
- **Replay support**: Tool message transformation for session replay

#### Tool Choice API
- **Configuration type**: tool_choice field with auto, none, required, and specific modes
- **JSON serialization**: Proper serialization of all tool_choice variants
- **Loop limit behavior**: Automatic tool_choice=none when iteration limit reached
- **E2E test suite**: Comprehensive tests for all tool_choice modes

#### Command History
- **File persistence**: JSONL load/save/append operations in ~/.ikigai directory
- **Navigation**: Up/Down arrow key integration for history traversal
- **Deduplication**: Consecutive duplicate command prevention
- **Configuration**: history_size field in config with sensible defaults

#### Tab Completion (Complete)
- **Completion layer**: Dropdown display for completion candidates
- **Fuzzy matching**: fzy algorithm integration with talloc-aware wrapper
- **Prefix enforcement**: Strict prefix matching for command completion
- **Argument matching**: Context-aware completion based on current input
- **Navigation**: Arrow/Tab/Escape key interaction for selection
- **State machine**: Tab cycling through candidates with wrap-around
- **Accept behavior**: Tab accepts and dismisses, cursor positioned at end

#### JSONL Logger
- **File output**: Structured logging to ~/.ikigai/logs directory
- **Timestamps**: ISO 8601 timestamp formatting
- **Log rotation**: Atomic file rotation with configurable limits
- **Thread safety**: Mutex-protected logging for concurrent access
- **Log levels**: Debug, info, warning, error with filtering
- **HTTP logging**: Request/response logging for OpenAI API calls

#### Mouse Scroll Support
- **SGR protocol**: Parse SGR mouse escape sequences
- **Terminal integration**: Enable SGR mouse reporting mode
- **Scroll actions**: IK_INPUT_SCROLL_UP/DOWN action types
- **Handler**: Mouse scroll event handling in REPL
- **Alternate scroll mode**: Arrow keys scroll when viewing scrollback

#### UI Improvements
- **Unicode box drawing**: Replaced ASCII separators with Unicode characters
- **Input layer newline**: Fixed trailing newline handling in input layer

#### ANSI Color System
- **Escape skip**: Function to skip over ANSI CSI escape sequences
- **Width calculation**: ANSI-aware width for scrollback, input, and cursor
- **Color constants**: ANSI color codes and SGR sequence builders
- **SGR stripping**: Remove SGR sequences from pasted input
- **Message styling**: Apply colors to different message kinds

### Changed

#### Async Tool Execution
- **Thread infrastructure**: Background thread pool for tool execution
- **Non-blocking**: Tools execute without blocking the event loop
- **Debug output**: Request/response metadata with >> prefix convention

#### Terminal Improvements
- **Cursor restoration**: Proper cursor visibility on exit and panic
- **Terminal reset**: Test utility for consistent terminal state
- **HTTP/2 filtering**: Filter debug noise and redact secrets from logs

#### Code Organization
- **Layer modules**: Split layer_wrappers.c into per-layer files (completion, input, scrollback, separator, spinner)
- **Action modules**: Split repl_actions.c into focused modules (completion, history, llm, viewport)
- **Wrapper headers**: Split wrapper.h into domain-specific headers (curl, json, posix, postgres, pthread, stdlib, talloc)

### Development

#### Testing & Quality Gates
- **Test coverage**: Maintained 100% (6,374 lines, 384 functions, 2,144 branches)
- **Sanitizer fixes**: Thread safety improvements for ASan/UBSan/TSan compliance
- **Valgrind/Helgrind**: All 286 tests pass under memory and thread checkers
- **File size enforcement**: 16KB limit with automated splitting

#### Documentation
- **Task specifications**: TDD task files for all tool execution user stories
- **Architecture docs**: Canonical message format and extended thinking documentation
- **Agent skills**: Modernized skill system with composable personas

### Technical Metrics
- **Changes**: 703 files modified, +87,581/-9,083 lines
- **Commits**: 174 commits over development cycle
- **Test coverage**: 100% lines (6,374), functions (384), and branches (2,144)
- **Code quality**: All lint, format, and sanitizer checks pass

## [rel-03] - 2025-11-28

### Added

#### Database Integration (Complete)
- **PostgreSQL backend**: Full libpq integration for persistent conversation storage
- **Session management**: Create, restore, and manage sessions across application restarts
- **Message persistence**: Store and retrieve user/assistant messages with complete metadata
- **Replay system**: Core algorithm for conversation playback with mark/rewind support
- **Mark/Rewind implementation**: Checkpoint conversation state and restore to any mark
- **Migration system**: Automated database schema versioning with migration files
- **Error handling**: Comprehensive database error handling with res_t pattern

#### Event-Based Rendering
- **Unified render path**: Consistent rendering for both live streaming and replay modes
- **Event system**: Clean separation between data events and display logic
- **Replay display**: Seamless playback of historical conversations with proper formatting

#### Agent Task System
- **Hierarchical tasks**: Multi-level task execution with sub-agent optimization
- **Personas**: Composable skill sets (coverage, developer, task-runner, task-strategist, security, meta)
- **Manifest system**: Automated skill and persona discovery with .claude symlinks
- **Task state tracking**: Persistent task status with verification workflow
- **Agent scripts**: Deno-based tooling with standardized JSON output format

### Changed

#### Code Quality & Testing
- **Test coverage**: Maintained 100% coverage (4,061 lines, 257 functions, 1,466 branches)
- **Database testing**: Comprehensive integration tests with per-file database isolation
- **MOCKABLE wrappers**: Enhanced mocking infrastructure for database function testing
- **Test utilities**: Added configurable PostgreSQL connection helpers (PGHOST support)
- **Error injection**: Complete coverage of database error paths

#### Bug Fixes
- **Bug 9**: Fixed database error dangling pointer crash in error handling
- **Bug 8**: Fixed mark stack rebuild on session restoration for checkpoint recovery
- **Bug 7**: Fixed /clear command to properly display system message after clearing
- **Defensive checks**: Added NULL validation for db_connection_string in configuration

#### Code Organization
- **Agent system**: Reorganized .agents/prompts to .agents/skills for clarity
- **Documentation**: Restructured docs to reflect v0.3.0 database integration
- **Naming**: Updated to release numbering (rel-XX) from semantic versioning

### Development

#### Testing & Quality Gates
- **Docker Compose**: Local distro-check with PostgreSQL service for isolated testing
- **GitHub CI**: PostgreSQL service container integration for automated testing
- **PGHOST support**: Environment-based database host configuration
- **Build system**: Enhanced Makefile targets for docker-compose workflows

#### Documentation
- **Design documentation**: Comprehensive product vision and multi-agent architecture
- **Task system docs**: Complete documentation of hierarchical task execution
- **Protocol comparison**: Anthropic vs OpenAI streaming protocol documentation
- **Database ADRs**: Architecture decisions for PostgreSQL integration patterns

### Technical Metrics
- **Changes**: 246 files modified, +33,029/-6,446 lines
- **Commits**: 71 commits over development cycle
- **Test coverage**: 100% lines, functions, and branches
- **Code quality**: All lint, format, and sanitizer checks pass

## [rel-02] - 2025-11-22

### Added

#### OpenAI API Integration (Complete)
- **Streaming client**: Full OpenAI API integration with Server-Sent Events (SSE) parsing
- **Async I/O**: libcurl multi-handle for non-blocking HTTP requests integrated with event loop
- **GPT-5 compatibility**: Updated to support latest OpenAI API format
- **HTTP handler**: Modular request/response handling with completion callbacks
- **Error handling**: Comprehensive HTTP error handling with res_t pattern
- **State management**: REPL state machine for request/response lifecycle

#### REPL Command System
- **Command registry**: Extensible infrastructure for slash commands
- **Built-in commands**:
  - `/help` - Display available commands and usage
  - `/clear` - Clear conversation history and reset state
  - `/model` - Runtime LLM model switching
  - `/system` - Configure system message at runtime
  - `/mark` and `/rewind` - Conversation checkpoint and restore (architecture defined)

#### Configuration & Infrastructure
- **Config system**: JSON-based runtime configuration (Phase 1.1)
- **Layer abstraction**: Clean interface for terminal operations (Phase 1.2)
- **Debug pipe**: Development tool for inspecting internal state

### Changed

#### Code Quality & Testing
- **File size limits**: Refactored large files to comply with 16KB limit for maintainability
- **Test coverage**: Maintained 100% coverage (3,178 lines, 219 functions, 1,018 branches)
- **Test suite**: 129 test files with comprehensive unit and integration coverage
- **Mock infrastructure**: Link seams pattern with weak symbol wrappers (zero overhead in release builds)
- **Error injection**: Added tests for previously untestable error paths
- **LCOV refinement**: Reduced invalid exclusions through better error injection testing

#### Naming & Conventions
- **Pointer semantics**: Changed `_ref` suffix to `_ptr` for raw pointers
- **Return values**: Simplified OOM-only creator functions to return pointers directly
- **Completion callbacks**: Standardized to return `res_t` for consistent error handling

#### Code Organization
- **Module restructuring**: Reorganized input_buffer into subdirectory with full symbol prefixes
- **Wrapper functions**: Renamed to clarify external library seams
- **File splits**: Divided oversized test files for better maintainability

### Development

#### Testing & Quality Gates
- **Thread safety**: Added Helgrind testing for race condition detection
- **Code formatting**: Integrated uncrustify with automated style enforcement
- **Coverage refinement**: Improved coverage metrics accuracy through error injection
- **CI improvements**: Better visibility and reliability in GitHub Actions

#### Documentation
- **Vision docs**: Comprehensive multi-agent and mark/rewind architecture
- **ADRs**: Link seams mocking strategy and API design decisions
- **Return values**: Documented patterns and tracked technical debt
- **Organization**: Streamlined documentation structure and removed obsolete ADRs

### Technical Metrics
- **Changes**: 275 files modified, +28,676/-7,331 lines
- **Commits**: 71 commits over development cycle
- **Test coverage**: 100% lines, functions, and branches
- **Code quality**: All lint, format, and sanitizer checks pass

## [rel-01] - 2025-11-16

### Added

#### REPL Terminal Foundation (Complete)
- **Direct terminal rendering**: Single write per frame, 52× syscall reduction
- **UTF-8 support**: Full emoji, CJK, and combining character support
- **Multi-line input**: Cursor navigation and line editing
- **Readline shortcuts**: Ctrl+A/E/K/U/W for text navigation and manipulation
- **Text wrapping**: Automatic word wrapping with clean terminal restoration
- **Scrollback buffer**: O(1) arithmetic reflow (0.003-0.009 ms for 1000 lines), 726× faster than target
- **Viewport scrolling**: Page Up/Down navigation with auto-scroll on submit
- **Pretty-print infrastructure**: Format module with pp_helpers for debugging

#### Core Infrastructure
- **Error handling system**: Result types, assertions, FATAL() macros with comprehensive testing
- **Memory management**: talloc-based ownership with clear patterns and documentation
- **Generic arrays**: Type-safe wrapper pattern with base implementation
- **Build system**: Comprehensive warnings, sanitizers (ASan, UBSan, TSan), Valgrind, and coverage support
- **Testing framework**: 100% test coverage requirement (1,807 lines, 131 functions, 600 branches)

#### JSON Library Migration
- **yyjson integration**: Migrated from jansson to yyjson
- **Custom allocator**: talloc integration eliminates reference counting complexity
- **Performance**: 3× faster parsing for streaming LLM responses
- **Memory safety**: Better integration with project memory management patterns

### Changed
- Removed server/protocol code in favor of direct terminal client approach
- Restructured architecture for v1.0 desktop client vision
- Standardized on K&R code style with 120-character line width
- Adopted explicit sized integer types (<inttypes.h>) throughout codebase

### Documentation
- Comprehensive REPL documentation in docs/repl/
- Architecture decision records (ADRs) for key design choices
- Memory management, error handling, and naming convention guides
- Build system documentation with multi-distro support
- Security analysis of input parsing and terminal control

### Development
- Test-driven development with strict Red/Green/Verify cycle
- 100% coverage requirement for lines, functions, and branches
- Quality gates: fmt, check, lint, coverage, check-dynamic
- Parallel test execution support (up to 32 concurrent tests)

[rel-12]: https://github.com/mgreenly/ikigai/releases/tag/rel-12
[rel-11]: https://github.com/mgreenly/ikigai/releases/tag/rel-11
[rel-10]: https://github.com/mgreenly/ikigai/releases/tag/rel-10
[rel-09]: https://github.com/mgreenly/ikigai/releases/tag/rel-09
[rel-08]: https://github.com/mgreenly/ikigai/releases/tag/rel-08
[rel-07]: https://github.com/mgreenly/ikigai/releases/tag/rel-07
[rel-06]: https://github.com/mgreenly/ikigai/releases/tag/rel-06
[rel-05]: https://github.com/mgreenly/ikigai/releases/tag/rel-05
[rel-04]: https://github.com/mgreenly/ikigai/releases/tag/rel-04
[rel-03]: https://github.com/mgreenly/ikigai/releases/tag/rel-03
[rel-02]: https://github.com/mgreenly/ikigai/releases/tag/rel-02
[rel-01]: https://github.com/mgreenly/ikigai/releases/tag/rel-01
