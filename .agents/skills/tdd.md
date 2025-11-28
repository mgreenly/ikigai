# Test-Driven Development

## Description
Strict TDD methodology and Red/Green/Verify cycle for ikigai development.

## Details

**ABSOLUTE RULE: NEVER WRITE CODE BEFORE YOU HAVE A TEST THAT NEEDS IT**

Follow strict Red/Green/Verify cycle:

### 1. Red: Write a failing test first

- Write the test code that calls the new function
- Add function declaration to header file
- Add stub implementation that compiles but does nothing (e.g., `return OK(NULL);`)
- **IMPORTANT**: A compilation error is NOT a failing test - you need a stub that compiles and runs
- Verify the test actually fails with assertion failures (e.g., wrong output)
- NO CODE exists until a test demands it

### 2. Green: Write minimal code to make the test pass

- Implement ONLY what the test requires
- STOP immediately when the test passes
- DO NOT write "helper functions" before they're called
- DO NOT write code "because you'll need it later"
- DO NOT refactor for complexity until `make lint` actually fails

### 3. Verify: Run quality checks

- `make check` - All tests must pass
- `make lint` - Code complexity under threshold

**WARNING**: Writing code before tests wastes tokens, time, and money by:
- Generating premature code
- Debugging unnecessary coverage gaps
- Reverting unused code
- Violating the core methodology

**The test MUST come first. No exceptions.**

If writing a helper function, ask: "Does a passing test call this right now?" If no, DELETE IT.
