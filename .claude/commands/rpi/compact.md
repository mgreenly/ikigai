---
description: Condense a scratch/ file by ~50% while preserving all facts.
---

Compact the file at `$ARGUMENTS` to reduce token usage while preserving information.

**Requirements:**
1. The file MUST be within `$PWD/scratch/` - reject if not
2. Read the entire file content
3. Rewrite to reduce size by approximately 50%
4. Preserve ALL facts, specifications, code examples, and technical details
5. Techniques to use:
   - Remove redundant explanations
   - Consolidate repeated concepts
   - Use terse phrasing (no filler words)
   - Convert prose to bullet points or tables where appropriate
   - Remove unnecessary examples if one example suffices
   - Eliminate hedging language ("might", "could potentially", "it's worth noting")
6. Do NOT remove: function signatures, type definitions, code blocks, dependency lists, file paths
7. Overwrite the original file with the compacted version
8. Commit the change with message: `chore(scratch): compact <filename>`

**Validation:**
- If `$ARGUMENTS` is empty, respond with: "Usage: /rpi:compact <filepath>"
- If file is not in scratch/, respond with: "Error: File must be within scratch/ directory"
- If file doesn't exist, respond with: "Error: File not found: <filepath>"

**Output:** Report the before/after line count and approximate reduction percentage.
