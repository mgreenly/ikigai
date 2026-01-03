# Startup Experience

User starts ikigai and can immediately begin typing while tools load in the background.

**Phase 6 behavior.** Phases 2-5 use blocking discovery (terminal appears after tools are loaded).

## Description

When ikigai starts, the terminal appears instantly. Tool discovery runs in the background. Users can compose their message while tools load. If they submit before discovery completes, a brief spinner appears.

## Transcript

### Normal Case (tools ready before submit)

```text
$ ikigai

> list all markdown files in this directory
                                              [user types for 3 seconds]

I'll find all markdown files for you.

Found 12 markdown files:
  README.md
  CONTRIBUTING.md
  docs/setup.md
  ...
```

User sees no loading indicator. By the time they finish typing, tools are ready.

### Fast Submit (tools still loading)

```text
$ ikigai

> ls
   [user types immediately and hits Enter]

Loading tools...                              [spinner, ~0.5 seconds]

Here's the directory listing:
  src/
  tests/
  Makefile
  ...
```

User submitted before discovery finished. Brief spinner, then request proceeds.

## Walkthrough

1. User runs `ikigai` command
2. Terminal enters alternate buffer, input prompt appears immediately
3. Background: ikigai spawns `--schema` for all tools in parallel
4. User begins typing their message
5. Background: tool schemas collected, registry populated
6. User presses Enter to submit
7. If `tool_scan_state == TOOL_SCAN_COMPLETE`: build request with tools, send to LLM
8. If `tool_scan_state == TOOL_SCAN_IN_PROGRESS`: show "Loading tools..." spinner, wait for completion, then proceed
9. If `tool_scan_state == TOOL_SCAN_FAILED`: proceed with empty tool registry (no tools available)

## Timing

- Tool discovery: ~1 second (6 tools, 1s timeout each, parallel)
- Typical user typing: 5-10 seconds
- Result: tools ready before user submits in vast majority of cases

## Notes

- Input buffer is fully functional during discovery (cursor movement, editing, paste)
- Discovery failure does not block startup - ikigai works without tools
- Failed tools logged as debug messages, not shown to user unless they look
- `/refresh` always blocks (explicit user action, expects immediate feedback)
