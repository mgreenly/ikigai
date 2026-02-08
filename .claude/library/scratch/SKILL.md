---
name: scratch
description: Temporary storage for carrying information between sessions
---

# Scratch

`.claude/data/scratch.md` is a temporary storage file for carrying information between sessions.

## Usage

When user says "write X to scratch", **overwrite** the entire file with the new content. Previous contents are discarded.

Common use cases:
- **Resume prompts**: "Write a resume prompt to scratch" - save instructions to help the next session continue where this one left off. User will start the next session with "read your scratch file and let's resume where you left off"
- **Context handoff**: Save key decisions, current state, or next steps
- **Temporary notes**: Any information that needs to survive session boundaries

## Behavior

- Always **overwrite** (never append)
- Use the **Write** tool directly — do NOT read the file first (wastes tokens)
- The file may not exist - create it when needed
- Content format is freeform markdown
- The file is gitignored - contents won't be committed
