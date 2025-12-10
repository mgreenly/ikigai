# Decision Log

## Mouse Mode: 1007h (Alternate Scroll Mode)

**Decision:** Use terminal mode 1007h which converts mouse wheel scrolls into arrow key sequences (ESC [ A for scroll up, ESC [ B for scroll down).

**Rationale:** Mode 1007h does NOT capture mouse button clicks, allowing the parent terminal application (Ghostty) to retain full mouse control for text selection, right-click menus, etc.

**Alternative Rejected:** Mode 1006h + 1000h (SGR mouse tracking) would send proper mouse events for both scrolls and clicks, but captures ALL mouse events, preventing the terminal from handling clicks.

**Implementation:** See `src/terminal.c:16` - `ESC_MOUSE_ENABLE "\x1b[?1007h"`

## Current Behavior (2025-12-09)

**Problem:** One mouse wheel scroll notch sends only 1 arrow sequence, which times out after 16ms and is classified as keyboard arrow (not mouse wheel).

**Expected:** Mode 1007h should send multiple arrows per scroll (docs say 3-4), allowing timer-based detection to distinguish mouse wheel from keyboard.

**Log Evidence:**
```json
{"event": "input_parsed", "type": "ARROW_UP"}
{"event": "arrow_arrival", "dir": "UP", "pending": false, "t_ms": 210460116}
{"event": "scroll_detect", "type": "ARROW", "dir": "UP", "t_ms": 210460132, "elapsed_ms": 16, "reason": "timeout"}
```

**Analysis:** Only 1 arrow arrives, waits 15ms threshold, then times out. No second arrow detected. Same terminal session (Ghostty) was sending 2 arrows per scroll yesterday with identical code.

## Investigation Focus

**Known Fact:** The mouse wheel DOES emit multiple arrow events - this was confirmed working yesterday in the same terminal session.

**Current Mystery:** We're only seeing 1 arrow event in the logs, not the multiple events we know are being sent.

**Primary Problem to Troubleshoot:** Why are we not receiving/seeing the multiple arrow events that the terminal is sending? The protocol information about "3-4 arrows" may be incorrect, but we know for certain that Ghostty was sending at least 2 arrows per scroll yesterday. Something in our code is either:
- Not reading all available bytes from the terminal fd
- Consuming/discarding arrow events somewhere
- Processing them in a way that prevents them from being logged

**Next Steps:** Investigate the input reading and parsing pipeline to find where the additional arrow events are being lost.

## Finding: ESC_TERMINAL_RESET Corrupts Ghostty State (2025-12-09)

**Root Cause:** Sending `\x1b[?25h\x1b[0m` (show cursor + reset attributes) during cleanup was corrupting Ghostty's 1007h handling, reducing arrows-per-scroll from 3 to 1.

**Fix:** Remove ESC_TERMINAL_RESET from cleanup. Only send:
```
\x1b[?1007l   (disable 1007h)
\x1b[?1049l   (exit alternate screen)
```

**Verification:** Created `test1007.sh` to test terminal behavior independently. After fix, running ikigai no longer corrupts Ghostty - test1007.sh shows consistent 3 arrows before and after.

## Arrows Per Scroll by Terminal (2025-12-09)

| Terminal       | Arrows per notch |
|----------------|------------------|
| Ghostty        | 3                |
| gnome-terminal | 3                |
| foot           | 3                |
| xterm          | 5                |
| Kitty          | 10               |

**Conclusion:** All terminals send N > 1 arrows per scroll. Detection strategy should rely on "multiple arrows in rapid succession" rather than a specific count.
