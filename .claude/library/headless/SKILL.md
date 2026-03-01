---
name: headless
description: Driving ikigai interactively from an agent session
---

# Headless Mode

How to interact with a running ikigai instance from an agent session. For `ikigai-ctl` command reference, see `/load ikigai-ctl`.

## Launching

```bash
bin/ikigai --headless &          # Start in background (no TTY required)
```

Without `--headless`, ikigai renders to the alternate terminal buffer — no visible output, no way to interact from an agent session.

## The Interact-Observe Loop

1. Send keystrokes: `ikigai-ctl send_keys "hello\r"`
2. Wait for UI to update (0.5s for UI commands, longer for LLM responses)
3. Read the screen: `ikigai-ctl read_framebuffer`
4. Inspect the output, repeat

## Reading the Framebuffer

The `read_framebuffer` response contains a `lines` array. Each line has `spans` with `text` fields. Concatenate span texts per row to reconstruct what's on screen.

Visual markers:
- **`❯`** prefix — user messages sent to the LLM
- **`●`** prefix — LLM responses

## Timing

- After UI commands (`/model`, `/clear`): wait 0.5 seconds
- After sending a prompt to the LLM: use `ikigai-ctl wait_idle <timeout_ms>` to wait for the agent to finish

## Key Rules

- **Never start ikigai** — the user manages the instance
- **If no instance is running**, report the failure and stop
- **If multiple sockets exist**, try each or use `--socket PATH`
- **Kill the process when done** with headless sessions you started
