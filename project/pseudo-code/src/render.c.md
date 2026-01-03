## Overview

Direct ANSI terminal rendering engine. Manages screen output for the interactive editor, including the input buffer (with cursor positioning), scrollback history, and combined display with visual separators. All output is built into a framebuffer and sent to the terminal in a single atomic write to ensure flicker-free updates.

## Code

```
function create_render_context(rows, cols, tty_fd):
    validate rows and cols are positive

    allocate a new render context
    initialize with terminal dimensions and file descriptor

    return success with new context


function render_input_buffer(context, text, cursor_byte_offset):
    validate inputs

    if text is empty:
        position cursor at home (0, 0)
    else:
        calculate where the cursor appears on screen given text and byte offset

    count newlines in text (each becomes \r\n for terminal line endings)

    allocate framebuffer for: clear screen + home + text + newlines + cursor position escapes

    build framebuffer:
        write clear screen escape (\x1b[2J)
        write home cursor escape (\x1b[H)

        copy text, expanding \n to \r\n for each newline

        write cursor position escape (1-based terminal coordinates)

    write entire framebuffer to terminal in single operation

    return success


function render_scrollback(context, scrollback, start_line, line_count):
    validate inputs and line range

    ensure scrollback layout is current for terminal width

    clamp line_count to available lines

    calculate total buffer size needed for:
        clear screen + home escapes
        all line text (with \r\n expansions)
        final \r\n after each line

    allocate framebuffer

    build framebuffer:
        write clear screen escape (\x1b[2J)
        write home cursor escape (\x1b[H)

        for each line in range:
            copy line text, expanding \n to \r\n
            add \r\n at end of line
            accumulate physical row count (accounts for line wrapping)

    write entire framebuffer to terminal

    return success and rows_used count


function render_combined(context, scrollback, scrollback_start_line, scrollback_line_count,
                         input_text, input_cursor_offset, render_separator, render_input_buffer):
    validate inputs

    ensure scrollback layout is current
    validate scrollback range
    clamp scrollback_line_count to available lines

    calculate physical rows used by scrollback lines (accounts for wrapping)

    if input_text exists:
        calculate cursor screen position within input buffer

    offset input cursor row by scrollback rows used

    calculate total buffer size for:
        clear screen + home escapes
        all scrollback line content
        separator line (if visible): dashes + width of terminal
        input buffer text (if visible)
        cursor visibility escape
        cursor position escape

    allocate framebuffer

    build framebuffer:
        write clear screen escape (\x1b[2J)
        write home cursor escape (\x1b[H)

        for each scrollback line:
            copy line text, expanding \n to \r\n
            add \r\n at end UNLESS this is last line and nothing follows
            (prevents terminal scroll when content fills screen)

        if separator visible:
            write dashes for full terminal width
            add \r\n if input buffer follows
            increment final cursor row

        if input buffer visible and has text:
            copy input text, expanding \n to \r\n

        write cursor visibility escape:
            show cursor (\x1b[?25h) if input buffer visible
            hide cursor (\x1b[?25l) otherwise

        if input buffer visible:
            write cursor position escape (1-based terminal coordinates)

    write entire framebuffer to terminal in single atomic operation

    return success
```
