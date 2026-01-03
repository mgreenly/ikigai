## Overview

Parses ANSI escape sequences from terminal input and converts them into semantic input actions. Handles arrow keys, navigation keys, mouse events, special keys (Enter, Escape, Backspace, Delete), and CSI u sequences for modern terminals. Buffers incomplete sequences and processes them incrementally as bytes arrive.

## Code

```
ESCAPE SEQUENCE PARSER

Initialize parser state:
    in_escape = false (not currently processing an escape sequence)
    esc_len = 0 (number of bytes buffered in current sequence)
    esc_buf[] = empty buffer to accumulate bytes

Main entry point: parse_escape_sequence(parser, byte, action_out):
    buffer the incoming byte into esc_buf

    if buffer is full:
        reset state and return UNKNOWN

    Handle first byte after ESC:
        if byte is '[':
            continue parsing (CSI sequence)
        if byte is ESC (double escape):
            output ESCAPE action and start new sequence
            return
        otherwise (invalid first byte):
            output UNKNOWN and reset
            return

    Try parsing as plain arrow key:
        if buffer has 2 bytes and byte is A/B/C/D:
            reset and output ARROW_UP/DOWN/RIGHT/LEFT
            return

    Try parsing as Ctrl+Arrow key:
        if buffer has 5 bytes matching pattern "ESC [ 1 ; 5" and byte is A/B/C/D:
            reset and output NAV_PARENT/CHILD/NEXT_SIBLING/PREV_SIBLING
            return

    Try parsing as mouse SGR sequence:
        if buffer starts with "ESC [ <" and byte is M or m:
            parse button number from buffer
            if button is 64:
                reset and output SCROLL_UP
                return
            if button is 65:
                reset and output SCROLL_DOWN
                return
            otherwise (non-scroll mouse event):
                reset and output UNKNOWN
                return

    Try parsing as tilde-terminated sequence:
        if buffer has 3 bytes and byte is '~':
            if middle byte is '3':
                reset and output DELETE
                return
            if middle byte is '5':
                reset and output PAGE_UP
                return
            if middle byte is '6':
                reset and output PAGE_DOWN
                return

    Try parsing as CSI u sequence:
        if byte is 'u' and buffer has at least 3 bytes:
            parse keycode from buffer (digits after '[')
            parse modifiers if semicolon present (modifier bits after semicolon)

            filter out Alacritty modifier-only events (keycode > 50000)

            if keycode is 13 (Enter):
                if no modifiers:
                    output NEWLINE
                otherwise:
                    output INSERT_NEWLINE
                return

            if keycode is 99 (c) and modifiers is 5 (Ctrl):
                output CTRL_C
                return

            if keycode is 9 (Tab) and no modifiers:
                output TAB
                return

            if keycode is 127 (Backspace) and no modifiers:
                output BACKSPACE
                return

            if keycode is 27 (Escape) and no modifiers:
                output ESCAPE
                return

            if keycode is printable ASCII (32-126) and no modifiers:
                output CHAR with keycode as codepoint
                return

            if keycode is printable ASCII (32-126) and Shift modifier present:
                translate shifted key using keyboard layout
                output CHAR with translated codepoint
                return

            if keycode is Unicode (> 126, <= 0x10FFFF) and no modifiers:
                output CHAR with keycode as codepoint
                return

            otherwise (other CSI u keys):
                output UNKNOWN
                return

    Check for unrecognized CSI sequences to discard:
        if byte is 'm' and buffer has at least 1 byte (SGR color sequence):
            reset and output UNKNOWN
            return

        if buffer has 3 bytes and byte is '~' (unrecognized function key):
            reset and output UNKNOWN
            return

    Check for other unrecognized 2-character sequences:
        if buffer has 2 bytes and byte is uppercase letter A-Z:
            reset and output UNKNOWN
            return

    Sequence is incomplete - wait for more bytes:
        output UNKNOWN (sequence continues)
```
