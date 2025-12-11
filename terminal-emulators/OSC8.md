# OSC 8 - Terminal Hyperlinks

OSC 8 (Operating System Command 8) is a terminal escape sequence protocol that enables clickable hyperlinks in terminal output.

## Syntax

The basic format consists of three parts:

```
\e]8;;URL\e\\LINK_TEXT\e]8;;\e\\
```

- `\e]8;;URL\e\\` - Start the hyperlink with the target URL
- `LINK_TEXT` - The visible text that will be clickable
- `\e]8;;\e\\` - End the hyperlink (empty URL terminates)

### Alternative notations

The escape sequence `\e]8` can also be written as:
- `\033]8` (octal)
- `\x1b]8` (hex)
- `ESC]8` (symbolic)

The string terminator `\e\\` can also be written as:
- `\033\\` (octal)
- `\x1b\\` (hex)
- `ESC\` (symbolic)
- `\007` or `\a` (BEL character, older style)

## Examples

### Basic hyperlink

```bash
echo -e '\e]8;;https://example.com\e\\Click me!\e]8;;\e\\'
```

Prints "Click me!" as a clickable link to https://example.com.

### File path hyperlink

```bash
file_path="/home/user/document.txt"
printf '\e]8;;file://%s\e\\%s\e]8;;\e\\\n' "$file_path" "$(basename "$file_path")"
```

### With link ID

You can add an `id` parameter to associate multiple text segments with the same link:

```
\e]8;id=UNIQUE_ID;URL\e\\text\e]8;;\e\\
```

Example:
```bash
echo -e '\e]8;id=link1;https://example.com\e\\Part 1\e]8;;\e\\ and \e]8;id=link1;https://example.com\e\\Part 2\e]8;;\e\\'
```

Both "Part 1" and "Part 2" will link to the same URL and hover together.

## How It Works

### Application Responsibility

The **application or script** must:
1. Identify what should be a link (URLs, file paths, etc.)
2. Wrap it with OSC 8 escape codes
3. Output it to stdout/stderr

### Terminal Responsibility

The **terminal emulator** will:
1. Recognize the escape sequences
2. Render the text as clickable/underlined
3. Handle click actions (open URLs in browser, files in editor, etc.)

The terminal does not scan output looking for URLs - it only makes text clickable when explicitly wrapped in OSC 8 codes.

## Supported Terminals

Modern terminals with OSC 8 support include:

- **iTerm2** (macOS)
- **GNOME Terminal**
- **Konsole** (KDE)
- **WezTerm**
- **Alacritty** (recent versions)
- **Windows Terminal**
- **kitty**
- **VSCode integrated terminal**
- **Hyper**
- **Tabby**

## Use Cases

- File paths in `ls` or `find` output (e.g., `ls --hyperlink=auto`)
- URLs in `git log` or commit messages
- Clickable matches in `grep` output
- Documentation links in error messages
- Issue/PR references in development tools
- Log file references that open in editors

## Tools with Built-in Support

Many modern CLI tools are adding OSC 8 support:

- `ls --hyperlink=auto` - Makes filenames clickable
- `grep --hyperlink` - Makes matching files clickable (GNU grep 3.5+)
- Various `git` integrations
- Custom scripts and development tools

## Testing

To test if your terminal supports OSC 8:

```bash
printf '\e]8;;https://example.com\e\\This should be a clickable link\e]8;;\e\\\n'
```

If your terminal supports it, you should see "This should be a clickable link" as clickable text.

## References

- [Gist specification by egmontkob](https://gist.github.com/egmontkob/eb114294efbcd5adb1944c9f3cb5feda)
- Terminal emulator support varies - check your terminal's documentation
