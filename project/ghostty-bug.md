# Ghostty Mouse Scroll Bug

## Summary

"Split Down" progressively degrades mouse scroll events. This is a ghostty bug, not an ikigai bug.

## Discovered

2025-12-10, while debugging why ikigai appeared to receive fewer scroll events after repeated runs.

## Version

Ghostty 1.1.3-dev+0000000 (outdated - current stable is 1.2.1)

## Reproduction

1. Fresh ghostty window
2. Right-click → Split Down
3. Mouse scroll now sends 2 events per notch instead of 3
4. Split Down again → 1 event per notch
5. Stays at 1 (doesn't degrade further)

## Key Details

- "Split Right" does NOT cause this
- Affects all panes in the window, not just the one that was split
- `reset` command does not fix it
- Only fix is killing ghostty and starting a new instance
- Happens entirely outside of any application - pure ghostty operation
- Alternate screen mode (`\e[?1049h`) is irrelevant

## Test Script

Use `test1049.sh` to observe scroll events - it prints each arrow key received.

## Status

- [ ] Upgrade ghostty to 1.2.1
- [ ] Retest to see if bug is fixed
- [ ] If not fixed, file issue at https://github.com/ghostty-org/ghostty/issues
