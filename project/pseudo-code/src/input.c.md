## Overview

The input parser module converts raw byte streams into semantic input actions. It handles three main categories of input: escape sequences (like arrow keys and special function keys), UTF-8 multi-byte character sequences, and ASCII control characters (like Ctrl+A, Tab, Enter). The parser maintains state machines for UTF-8 sequence accumulation and escape sequence parsing, then delegates to specialized handlers for each category.

## Code

```
function create_input_parser(parent):
    allocate a new parser structure

    initialize parser state:
        - clear escape sequence buffer and flags
        - clear UTF-8 sequence buffer, byte count, and expected length
        - prepare xkbcommon state for shifted key translation

    set up destructor to clean up xkbcommon resources

    return the parser


function parse_byte(parser, byte):

    # If already accumulating a UTF-8 sequence, this must be a continuation byte
    if parser is in UTF-8 mode:
        validate that byte matches continuation byte pattern (10xxxxxx format)

        if invalid continuation byte:
            reset UTF-8 state
            return unknown action

        buffer the continuation byte

        if we now have all expected bytes:
            decode the complete UTF-8 sequence into a Unicode codepoint
            validate the codepoint (reject overlong encodings, surrogates, out-of-range)
            reset UTF-8 state
            return character action with the codepoint

        otherwise, return unknown (still incomplete)
        return

    # If already in an escape sequence, this byte is part of that sequence
    if parser is in escape sequence mode:
        delegate to escape sequence parser
        return

    # Handle single byte as possible escape starter or character

    if byte is ESC (0x1B):
        enter escape sequence mode
        reset escape buffer
        return unknown action
        return

    # Handle control characters (special keys and commands)

    if byte is Tab (0x09):
        return tab action (completion trigger)

    if byte is CR (0x0D):
        return newline action (submit command)

    if byte is LF (0x0A):
        return insert-newline action (add line without submitting)

    if byte is Ctrl+A (0x01):
        return ctrl-A action

    if byte is Ctrl+C (0x03):
        return ctrl-C action

    if byte is Ctrl+E (0x05):
        return ctrl-E action

    if byte is Ctrl+K (0x0B):
        return ctrl-K action

    if byte is Ctrl+N (0x0E):
        return ctrl-N action

    if byte is Ctrl+P (0x10):
        return ctrl-P action

    if byte is Ctrl+U (0x15):
        return ctrl-U action

    if byte is Ctrl+W (0x17):
        return ctrl-W action

    # Handle printable ASCII range (0x20-0x7F)

    if byte is in printable ASCII range:
        if byte is DEL (0x7F):
            return backspace action

        otherwise:
            return character action with the ASCII codepoint
        return

    # Handle UTF-8 multi-byte lead bytes

    if byte matches 2-byte UTF-8 lead pattern (110xxxxx):
        enter UTF-8 mode
        buffer this first byte
        set expected length to 2
        return unknown action (incomplete sequence)
        return

    if byte matches 3-byte UTF-8 lead pattern (1110xxxx):
        enter UTF-8 mode
        buffer this first byte
        set expected length to 3
        return unknown action (incomplete sequence)
        return

    if byte matches 4-byte UTF-8 lead pattern (11110xxx):
        enter UTF-8 mode
        buffer this first byte
        set expected length to 4
        return unknown action (incomplete sequence)
        return

    # Unrecognized byte
    return unknown action


function decode_utf8_sequence(buffer, length):

    switch on length:
        case 2:
            extract 5 bits from first byte and 6 bits from second byte
        case 3:
            extract 4 bits from first byte and 6 bits each from 2nd and 3rd bytes
        case 4:
            extract 3 bits from first byte and 6 bits each from 2nd, 3rd, and 4th bytes

    # Validate the decoded codepoint against UTF-8 standards

    if 2-byte sequence but codepoint < 0x80:
        return replacement character (overlong encoding)

    if 3-byte sequence but codepoint < 0x800:
        return replacement character (overlong encoding)

    if 4-byte sequence but codepoint < 0x10000:
        return replacement character (overlong encoding)

    if codepoint is in UTF-16 surrogate range (0xD800-0xDFFF):
        return replacement character (invalid surrogate)

    if codepoint > maximum valid Unicode (0x10FFFF):
        return replacement character (out of range)

    return the validated codepoint
```
