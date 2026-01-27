---
name: dev-dump
description: Debug buffer dumps for inspecting terminal rendering state
---

# Dev Dump

Debug buffer dumps written to `.ikigai/debug/` for inspecting terminal rendering state.

## Enabling

Two conditions must be met:

1. **Compile with `IKIGAI_DEV=1`** - Compiles in dump code
2. **Create `.ikigai/debug/` directory** - Runtime opt-in

Without either, no dumps occur and no overhead exists.

## Available Buffers

| File | Source | Description |
|------|--------|-------------|
| `repl_viewport.framebuffer` | `src/repl_viewport.c` | Final composed screen with escape sequences |

## File Format

```
# rows=N cols=N cursor=R,C len=BYTES
<raw buffer bytes>
```

Header is a single line starting with `#`, then newline, then exact buffer bytes.

## Interpreting Escape Sequences

Common ANSI sequences in framebuffer:

| Sequence | Meaning |
|----------|---------|
| `\x1b[?25l` | Hide cursor |
| `\x1b[?25h` | Show cursor |
| `\x1b[H` | Home cursor (1,1) |
| `\x1b[R;CH` | Position cursor at row R, col C |
| `\x1b[K` | Clear to end of line |
| `\x1b[J` | Clear to end of screen |
| `\x1b[0m` | Reset attributes |
| `\x1b[2m` | Dim text |
| `\x1b[38;5;Nm` | 256-color foreground (N=242 is gray) |

## Timing

Dumps occur right before `posix_select_()` in `ik_repl_run()` - the moment control returns to wait for user input. This captures the stable screen state the user sees.

## Usage

```bash
# Build with dev dumps enabled
IKIGAI_DEV=1 make clean all

# Create debug directory
mkdir -p .ikigai/debug

# Run ikigai, interact, then inspect
cat .ikigai/debug/repl_viewport.framebuffer

# Or read in Claude for analysis
```

File overwrites on each render cycle - always shows the last frame before input.
