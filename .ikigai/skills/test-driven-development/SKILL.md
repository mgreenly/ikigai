---
name: test-driven-development
description: Test-Driven Development methodology using Red/Green/Verify cycle. Use when implementing new features, writing code, or developing functionality that requires testing.
---

# Test-Driven Development

## Overview

**Core principle: Write tests for the code you write.**

This skill provides TDD methodology for the development phase, focusing on testing what you build through the Red/Green/Verify cycle.

## Instructions

Follow the Red/Green/Verify cycle for all new development:

### 1. Red: Write a failing test first

- Write the test code that calls the new function
- Add function declaration to header file
- Add stub implementation that compiles but does nothing (e.g., `return OK(NULL);`)
- A compilation error is NOT a failing test - you need a stub that compiles and runs
- Verify the test actually fails with assertion failures

### 2. Green: Write minimal code to make the test pass

- Implement ONLY what the test requires
- STOP immediately when the test passes
- DO NOT write "helper functions" before they're called
- DO NOT write code "because you'll need it later"

### 3. Verify: Run quality checks

- `make check` - All tests must pass
- `make lint` - Code complexity under threshold

## Best Practices

### Development Phase Focus

**Test what you build.** During development:

- Write tests for the happy path
- Write tests for obvious error cases
- Cover the main functionality thoroughly
- Keep momentum - don't get stuck on edge cases

### Coverage Philosophy

**Coverage gaps are OK during development.** They will be closed in a dedicated coverage phase before release.

**The test should come first** - but don't let perfect coverage slow down feature development.
