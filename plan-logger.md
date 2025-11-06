# Plan: Logger Module Implementation

## Overview

Implement the logger module (`src/logger.c/h`) following systemd conventions. This module has no dependencies and is used by all other modules.

## Prerequisites

- Review AGENT.md for development methodology
- Review docs/phase-1.md and docs/phase-1-details.md for logger specifications

## Task List

### Task 1: Review and Verify Prerequisites

**Description**: Review previous work decisions and verify that no architectural changes have been made that would affect the logger module implementation.

**Actions**:
- Read through docs/decisions.md to check for any logging-related decisions
- Verify docs/phase-1-details.md logger section is current
- Confirm logger API matches specification (no changes)
- Verify no dependencies have been added to logger module

**Acceptance**:
- No conflicts found with previous decisions
- Logger specification is clear and unchanged
- Ready to proceed with implementation

---

### Task 2: Create Logger Header (Red Phase)

**Description**: Write failing tests for logger header file declarations.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Create `tests/unit/logger_test.c`
- Write test that attempts to call `ik_log_info()` function
- Test should fail to compile (function doesn't exist)
- Run `make check` to verify test fails

**GREEN (Implement Minimum Code)**:
- Create `include/logger.h`
- Add function declarations:
  - `void ik_log_debug(const char *fmt, ...)`
  - `void ik_log_info(const char *fmt, ...)`
  - `void ik_log_warn(const char *fmt, ...)`
  - `void ik_log_error(const char *fmt, ...)`
  - `void ik_log_fatal(const char *fmt, ...) __attribute__((noreturn))`
- Add include guards
- Update Makefile if needed to include logger in build
- Run `make check` to verify test now compiles (may still fail at runtime)

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Add logger.h function declarations"

---

### Task 3: Implement Info/Debug/Warn to Stdout (Red Phase)

**Description**: Implement functions that write to stdout with proper formatting.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Add test `test_logger_info_stdout()` that:
  - Captures stdout using freopen or similar
  - Calls `ik_log_info("test message")`
  - Verifies output is "INFO: test message\n"
- Add test `test_logger_debug_stdout()` that verifies DEBUG output format
- Add test `test_logger_warn_stdout()` that verifies WARN output format
- Run `make check` to verify tests fail

**GREEN (Implement Minimum Code)**:
- Create `src/logger.c`
- Implement `ik_log_info()`:
  - Write "INFO: " to stdout
  - Use `vfprintf()` for formatted output
  - Add newline
- Implement `ik_log_debug()` (similar, with "DEBUG: ")
- Implement `ik_log_warn()` (similar, with "WARN: ")
- Run `make check` to verify tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Implement logger info/debug/warn functions"

---

### Task 4: Implement Error/Fatal to Stderr (Red Phase)

**Description**: Implement error and fatal functions that write to stderr.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Add test `test_logger_error_stderr()` that:
  - Captures stderr
  - Calls `ik_log_error("error message")`
  - Verifies output is "ERROR: error message\n"
- Add test `test_logger_fatal_calls_abort()` that:
  - Uses fork() or signal catching to test fatal behavior
  - Calls `ik_log_fatal("fatal error")`
  - Verifies stderr output and that abort() is called
- Run `make check` to verify tests fail

**GREEN (Implement Minimum Code)**:
- Implement `ik_log_error()`:
  - Write "ERROR: " to stderr
  - Use `vfprintf()` for formatted output
  - Add newline
- Implement `ik_log_fatal()`:
  - Write "FATAL: " to stderr
  - Use `vfprintf()` for formatted output
  - Add newline
  - Call `abort()`
- Run `make check` to verify tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Implement logger error and fatal functions"

---

### Task 5: Test Printf-Style Formatting (Red Phase)

**Description**: Verify that printf-style formatting works correctly for all log levels.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Add test `test_logger_formatting()` that:
  - Calls `ik_log_info("value=%d string=%s", 42, "test")`
  - Verifies output is "INFO: value=42 string=test\n"
  - Tests multiple format specifiers (%s, %d, %x, etc.)
- Run `make check` to verify tests pass (should already work)

**GREEN (Verify Implementation)**:
- Code should already support this via `vfprintf()`
- If tests fail, fix implementation
- Run `make check` to verify tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Add tests for logger printf-style formatting"

---

### Task 6: Integration Test - Use Logger in Test Program (Red Phase)

**Description**: Create a simple integration test that uses all logger functions.

**TDD Cycle**:

**RED (Write Failing Test)**:
- Create `tests/integration/logger_integration_test.c`
- Write test that:
  - Calls all log levels in sequence
  - Verifies output order and formatting
  - Tests long messages
  - Tests special characters
- Run `make check` to verify test runs

**GREEN (Fix Any Issues)**:
- Fix any issues discovered during integration testing
- Run `make check` to verify all tests pass

**VERIFY**:
- Run `make ci` (must pass: check + lint + coverage)
- Coverage must be 100% for Lines, Functions, and Branches

**Commit**: "Add logger integration tests"

---

### Task 7: Manual Verification and Documentation Review

**Description**: Human verification of logger functionality and code quality.

**Manual Tests**:
1. Build the test suite: `make check`
2. Run logger tests with verbose output
3. Verify stdout/stderr separation is correct
4. Review logger.c code for clarity and simplicity
5. Verify no talloc or other dependencies added
6. Check that fatal behavior is correct (abort called)
7. Verify all function signatures match phase-1-details.md specification

**Code Inspection**:
- Review logger.c implementation for:
  - No memory leaks
  - Thread safety not required but implementation should be simple
  - No global state beyond standard streams
  - Proper use of `__attribute__((noreturn))` on fatal

**Documentation**:
- Verify logger.h has appropriate comments
- Ensure implementation matches specification exactly

**Acceptance**:
- All manual tests pass
- Code quality is high
- Implementation matches specification
- Ready to proceed to next module (config)

---

## Success Criteria

- All tests pass (`make check`)
- Code complexity passes (`make lint`)
- Coverage is 100% for Lines, Functions, and Branches
- All commits follow format: "Brief description of change"
- No architectural changes made without explicit approval
- Logger module ready for use by other modules

## Notes

- Logger has no dependencies - can be implemented first
- Keep implementation simple - just format and print
- No configuration or runtime state needed
- Follow systemd conventions strictly (stdout for info, stderr for errors)
