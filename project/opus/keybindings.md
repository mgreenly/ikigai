# Keybindings

**Status:** Early design discussion, not finalized.

## Agent Navigation

| Binding | Action |
|---------|--------|
| `Ctrl-Space` | Agent switcher (fuzzy search) |
| `Ctrl-Space Ctrl-Space` | Toggle to last agent |
| `Ctrl-Space m` | Jump to main agent |
| `Ctrl-1` to `Ctrl-0` | Jump to agent 1-10 |

## Mark/Rewind

| Binding | Action |
|---------|--------|
| `Ctrl-m` | Quick mark (anonymous) |
| `Ctrl-r` | Quick rewind (to last mark) |

## Queue/Inbox

| Binding | Action |
|---------|--------|
| `Ctrl-q` | Show queue status |
| `Ctrl-i` | Show inbox status |

## Scrollback

| Binding | Action |
|---------|--------|
| `Page Up/Down` | Scroll |
| `Ctrl-Home/End` | Jump to top/bottom |
| `Ctrl-l` | Redraw screen |

## Editing (Standard)

| Binding | Action |
|---------|--------|
| `Ctrl-a` | Beginning of line |
| `Ctrl-e` | End of line |
| `Ctrl-k` | Kill to end of line |
| `Ctrl-u` | Kill to beginning of line |
| `Ctrl-w` | Kill word backward |
| `Ctrl-c` | Cancel/interrupt |
| `Ctrl-d` | EOF / exit (on empty line) |

## Design Notes

- Most actions available via slash commands; keybindings for frequent operations
- Follows standard Linux/terminal conventions where possible
- `Ctrl-r` shadows bash reverse-search but context is different here

## Related

- [agents.md](agents.md) - Agent types and switching
- [agent-queues.md](agent-queues.md) - Queue and inbox operations
