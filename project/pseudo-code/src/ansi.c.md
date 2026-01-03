## Overview

Provides utilities for handling ANSI escape sequences and color management. Includes functions to parse and skip CSI (Control Sequence Introducer) sequences, generate 256-color foreground codes, and manage global color settings based on environment variables (NO_COLOR and TERM).

## Code

```
function skip_ansi_csi_sequence(text, text_length, current_position):
    validate that we have enough space for ESC and '['
    if position + 1 >= length:
        return 0 (no valid sequence)

    check if current position starts with ESC '['
    if text[position] is not ESC or text[position+1] is not '[':
        return 0 (not a CSI sequence)

    start parsing from position + 2 (after ESC '[')

    while scanning remaining text:
        get current byte

        if byte is parameter or intermediate byte (0x20-0x3F):
            advance to next byte
            continue

        if byte is terminal byte (0x40-0x7E):
            return total bytes consumed from start position

        if byte is invalid:
            return 0 (invalid sequence)

    if we reach end of buffer without terminal byte:
        return 0 (incomplete sequence)


function generate_256_color_foreground(buffer, buffer_size, color_number):
    format ANSI escape sequence: ESC [ 3 8 ; 5 ; <color> m

    write formatted sequence to buffer using snprintf

    if write failed:
        return 0

    if formatted text exceeds buffer size:
        return 0

    return number of bytes written


function initialize_color_settings():
    start with colors enabled

    check NO_COLOR environment variable
    if set (regardless of value):
        disable colors
        return

    check TERM environment variable
    if set to "dumb":
        disable colors
        return


function query_color_status():
    return whether colors are currently enabled
```
