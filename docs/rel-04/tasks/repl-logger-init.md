# Task: REPL Logger Initialization

## Target
Infrastructure: JSONL logging system integration

## Agent
model: haiku

### Pre-read Skills
- .agents/skills/default.md
- .agents/skills/tdd.md
- .agents/skills/di.md

### Pre-read Docs
- docs/naming.md

### Pre-read Source (patterns)
- src/repl_init.c (REPL initialization)
- src/main.c (main entry point)
- src/logger.h (new JSONL logger API)

### Pre-read Tests (patterns)
- tests/unit/repl/repl_init_test.c

## Pre-conditions
- `make check` passes
- Task `jsonl-logger-reinit.md` completed
- Logger can be initialized and reinitialized

## Task
Integrate JSONL logger initialization into REPL startup. Call `ik_log_init()` in `main()` before REPL starts, and call `ik_log_reinit()` when /clear command is executed.

Changes:
1. Add `ik_log_init(getcwd())` in `main()` after config load
2. Add `ik_log_shutdown()` in `main()` cleanup
3. Update `/clear` command to call `ik_log_reinit(getcwd())`

The working directory ($PWD) is captured at startup and reset on /clear per requirements.

## TDD Cycle

### Red
1. Add test in `tests/integration/logger/repl_logger_test.c`:
   - Start REPL
   - Trigger action that generates log
   - Verify `.ikigai/logs/current.log` exists in working dir
   - Execute /clear command
   - Verify previous log rotated
   - Verify new current.log created
2. Run `make check` - expect failure (no integration yet)

### Green
1. Update `src/main.c`:
   - Add `char cwd[PATH_MAX]` to capture working directory
   - Call `getcwd(cwd, sizeof(cwd))` before REPL init
   - Call `ik_log_init(cwd)` after config load
   - Call `ik_log_shutdown()` before exit
2. Update `src/commands.c` in `/clear` handler:
   - Call `ik_log_reinit(getcwd())`
3. Run `make check` - expect pass

### Refactor
1. Verify error handling (getcwd can fail)
2. Ensure proper cleanup order
3. Run `make check` - verify still green

## Post-conditions
- `make check` passes
- Logger initializes on REPL startup
- Logger reinitializes on /clear
- Logs written to `.ikigai/logs/current.log` in working directory
