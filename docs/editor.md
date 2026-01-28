# Editor

The ikigai REPL includes a multi-line text editor with readline-style keybindings.

## Text Entry

| Key | Action |
|-----|--------|
| Any printable character | Insert at cursor |
| Enter | Submit input |
| Ctrl+J or Shift+Enter | Insert newline (multi-line input) |
| Tab | Trigger completion |
| Escape | Dismiss completion menu |
| Backspace | Delete character before cursor |
| Delete | Delete character at cursor |

## Cursor Movement

| Key | Action |
|-----|--------|
| Left Arrow | Move cursor left |
| Right Arrow | Move cursor right |
| Up Arrow | Move cursor up (in multi-line) |
| Down Arrow | Move cursor down (in multi-line) |
| Ctrl+A | Move to beginning of line |
| Ctrl+E | Move to end of line |

## Text Editing (Readline-style)

| Key | Action |
|-----|--------|
| Ctrl+K | Kill (delete) from cursor to end of line |
| Ctrl+U | Kill entire line |
| Ctrl+W | Delete word backward |

### Ctrl+W Behavior

Ctrl+W uses smart word boundary detection:
1. First skips any trailing whitespace
2. Then deletes backward through characters of the same class (alphanumeric, punctuation, etc.)

This matches standard readline/bash behavior.

## History Navigation

| Key | Action |
|-----|--------|
| Ctrl+P or Up Arrow | Previous history entry |
| Ctrl+N or Down Arrow | Next history entry |

When in multi-line input, Up/Down arrows move within the text. Use Ctrl+P/Ctrl+N for history.

## Scrolling

| Key | Action |
|-----|--------|
| Page Up | Scroll conversation up |
| Page Down | Scroll conversation down |
| Mouse Scroll | Scroll conversation |

## Agent Navigation

| Key | Action |
|-----|--------|
| Ctrl+Left | Navigate to previous sibling agent |
| Ctrl+Right | Navigate to next sibling agent |
| Ctrl+Up | Navigate to parent agent |
| Ctrl+Down | Navigate to child agent |

## Control

| Key | Action |
|-----|--------|
| Ctrl+C | Interrupt current operation / Exit |

## Unicode Support

The editor supports full Unicode input including multi-byte UTF-8 characters. Characters are inserted at the cursor position and the cursor advances appropriately.

## Terminal Compatibility

ikigai supports two input modes:

1. **CSI u mode** (modern terminals like Alacritty, Kitty, WezTerm) - Provides unambiguous key reporting with modifier support
2. **Legacy mode** (traditional terminals) - Uses standard escape sequences

The editor automatically handles both modes. If Ctrl+key combinations aren't working, your terminal may not be sending the expected byte sequences. Most modern terminals work out of the box.
