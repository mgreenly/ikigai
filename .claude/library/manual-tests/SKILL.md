---
name: manual-tests
description: Running and creating manual verification tests against a live ikigai instance
---

# Manual Tests

Manual verification tests live in `tests/manual/*-test.md`. Each test describes preconditions, steps using `ikigai-ctl`, and expected results to verify against the framebuffer.

Test execution order is defined in `tests/manual/index.md`.

## Running Tests

**Run all tests:** Read `tests/manual/index.md` and launch a background Task agent for each test file. Each agent reads its test file, executes the steps, and reports PASS/FAIL for each test within. Collect results from all agents and report a summary table.

**Run a single test file:** Launch a background Task agent for the specified file.

Each agent follows this procedure per test:

1. Read the test file for preconditions, steps, and expected results
2. Execute `ikigai-ctl` commands from the steps
3. After `send_keys`, wait 1 second before `read_framebuffer` to allow UI to update
4. After sending a prompt to the LLM, wait several seconds for the response
5. Compare framebuffer content against expected results
6. Report **PASS** or **FAIL** with evidence (cite the relevant rows)

## Key Rules

- **Never start ikigai** — the user manages the instance
- **If no instance is running**, report the precondition failure and stop
- **If multiple sockets exist**, try each or use `--socket PATH`
- **User messages** sent to the LLM are prefixed with `❯` in the framebuffer
- **LLM responses** are prefixed with `●` in the framebuffer

## Creating Tests

Test files use this format:

```markdown
# Test Name

## Preconditions
- Required state before the test

## Steps
1. `ikigai-ctl` commands to execute

## Expected
- What the framebuffer should contain
```

A single test file can contain multiple `## Test:` sections, each with its own Steps and Expected.

**Conventions:**
- Filename: `descriptive-name-test.md` (no numeric prefix)
- Add new tests to `tests/manual/index.md` to define run order
- Steps use `ikigai-ctl send_keys` and `ikigai-ctl read_framebuffer`
- Expected results reference specific text that should appear on screen

## Verifying Framebuffer Content

The `read_framebuffer` response contains a `lines` array. Each line has `spans` with `text` fields. Concatenate span texts per row to reconstruct what's on screen. Match expected strings against this reconstructed text.
