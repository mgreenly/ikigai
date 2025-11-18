# Key Bindings

Fast keyboard navigation for power users. All key bindings are designed to minimize hand movement and maximize efficiency.

## Design Philosophy

**Vim-Inspired:**
- Efficient, minimal hand movement
- Modal where appropriate
- Composable operations

**Tmux-Inspired:**
- Prefix key for multiplexing (Ctrl-\ for agents)
- Quick switching between contexts
- Status line always visible

**Terminal-Native:**
- Keyboard-driven for maximum efficiency
- Works over SSH
- Fast and lightweight

## Quick Reference

### Agent Navigation

| Key | Action |
|-----|--------|
| `Ctrl-\` | Agent switcher (fuzzy find) |
| `Ctrl-\ Ctrl-\` | Toggle to last agent |
| `Ctrl-\ m` | Jump to main (root) agent |
| `Ctrl-\ l` | List all agents |
| `Ctrl-\ n` | New agent (prompts for name) |
| `Ctrl-\ c` | Close current agent |
| `Ctrl-\ r` | Rename current agent |

### Mark/Rewind

| Key | Action |
|-----|--------|
| `Ctrl-m` | Quick mark (anonymous) |
| `Ctrl-Shift-M` | Named mark (prompts for label) |
| `Ctrl-r` | Quick rewind (to last mark) |
| `Ctrl-Shift-R` | Rewind to specific mark (fuzzy find) |

### Scrollback Navigation

| Key | Action |
|-----|--------|
| `Page Up` | Scroll up one page |
| `Page Down` | Scroll down one page |
| `Ctrl-Home` | Jump to top of scrollback |
| `Ctrl-End` | Jump to bottom of scrollback |
| `Ctrl-u` | Scroll up half page (vim-style) |
| `Ctrl-d` | Scroll down half page (vim-style) |

### Input Editing

| Key | Action |
|-----|--------|
| `Ctrl-a` | Move to start of line |
| `Ctrl-e` | Move to end of line |
| `Ctrl-k` | Kill to end of line |
| `Ctrl-u` | Kill to start of line |
| `Alt-Backspace` | Delete word backward |
| `Alt-d` | Delete word forward |
| `Ctrl-w` | Delete word backward |
| `Enter` | Submit input (send to LLM) |
| `Shift-Enter` | New line in input (multi-line) |

### Command Mode

| Key | Action |
|-----|--------|
| `:` | Enter command mode (vim-style) |
| `/` | Start slash command |
| `Esc` | Cancel command mode |

### System

| Key | Action |
|-----|--------|
| `Ctrl-l` | Redraw screen |
| `Ctrl-c` | Cancel current operation |
| `Ctrl-z` | Suspend ikigai |
| `Ctrl-d` | Quit (if input empty) |

## Detailed Key Binding Descriptions

### Agent Switcher (Ctrl-\)

**Primary Navigation:**

Press `Ctrl-\` to open the agent switcher overlay:

```
┌─────────────────────────────────────┐
│ Switch to Agent:                    │
│                                     │
│ > _                                 │
│                                     │
│ main - Branch: main                 │
│ oauth-impl - Branch: feature-oauth  │
│ research - No worktree              │
└─────────────────────────────────────┘
```

**Features:**
- Fuzzy matching as you type
- Shows branch info
- Shows marks count
- Up/Down arrows to select
- Enter to switch
- Esc to cancel

**Quick Actions (while switcher open):**

| Key | Action |
|-----|--------|
| `Ctrl-\` | Switch to last agent |
| `m` | Jump to main |
| `n` | New agent |
| `l` | List (stay in current) |

**Example Workflow:**
```
Ctrl-\          → Open switcher
oauth <Enter>   → Switch to "oauth-impl"
Ctrl-\          → Open switcher
m               → Jump to main
Ctrl-\ Ctrl-\   → Toggle back to "oauth-impl"
```

### Mark/Rewind Keys

**Quick Mark (Ctrl-m):**

Creates anonymous checkpoint instantly:

```
[You]
Let's try a different approach

Ctrl-m → [Mark created] ⟡ Checkpoint #1

[You]
Actually, let's refactor this first...
```

**Named Mark (Ctrl-Shift-M):**

Prompts for descriptive label:

```
Ctrl-Shift-M

┌─────────────────────────────────────┐
│ Mark Label:                         │
│ before-refactor_                    │
└─────────────────────────────────────┘
```

**Quick Rewind (Ctrl-r):**

Rewinds to most recent mark:

```
Ctrl-r → [Rewound to Checkpoint #1] ⟡
```

**Named Rewind (Ctrl-Shift-R):**

Opens mark selector:

```
Ctrl-Shift-R

┌─────────────────────────────────────┐
│ Rewind to:                          │
│                                     │
│ > before-refactor                   │
│   approach-a                        │
│   checkpoint-1 (anonymous)          │
└─────────────────────────────────────┘
```

### Input Editing Keys

**Readline Compatibility:**

ikigai supports standard readline shortcuts:

| Key | Action | Origin |
|-----|--------|--------|
| `Ctrl-a` | Start of line | Emacs |
| `Ctrl-e` | End of line | Emacs |
| `Ctrl-k` | Kill to end | Emacs |
| `Ctrl-u` | Kill to start | Emacs |
| `Ctrl-w` | Delete word back | Emacs |
| `Alt-Backspace` | Delete word back | Readline |
| `Alt-d` | Delete word forward | Emacs |

**Multi-Line Input:**

```
[You]
Let's implement OAuth:
1. Token validation<Shift-Enter>
2. Refresh logic<Shift-Enter>
3. Expiry handling<Enter>

→ Sends all 3 lines to LLM
```

**vs:**

```
[You]
Let's implement OAuth<Enter>

→ Sends immediately
```

### Scrollback Navigation

**Page-Based:**
- `Page Up/Down` - Full screen pages
- `Ctrl-u/d` - Half pages (vim-style)

**Jump:**
- `Ctrl-Home` - Top of scrollback
- `Ctrl-End` - Bottom (most recent)

**Auto-Scroll Behavior:**
- Scrollback auto-follows new output
- Manual scroll disables auto-follow
- Jump to bottom re-enables auto-follow

**Indicator:**
```
[Agent: main] [Scrolled: -156 lines] ↑
```

### Command Mode (Vim-Style)

Press `:` to enter command mode:

```
[You]
Working on the auth module...

: _

→ Command mode active
→ Type command without leading /
→ Tab completion available
```

**Examples:**
```
:mark before-refactor
:agent new oauth-impl --worktree
:memory list --filter=openai/*
```

**Comparison:**
- `/mark` - Literal slash command
- `:mark` - Vim-style command mode (same result)

**Design Choice:** Both styles supported. Use whichever feels natural.

## Conflict Resolution

Some key bindings may conflict with terminal settings or shells.

### Ctrl-\ Conflicts

**Problem:** Some terminals use `Ctrl-\` for `SIGQUIT`.

**Solutions:**
1. Reconfigure terminal to disable `Ctrl-\` signal
2. Use alternative prefix key (configure in `~/.ikigai/config.json`)

```json
{
  "keybindings": {
    "agent_prefix": "Ctrl-Space"
  }
}
```

### Shift-Enter Conflicts

**Problem:** Some terminals don't distinguish `Enter` from `Shift-Enter`.

**Solutions:**
1. Use `Alt-Enter` as alternative (automatically detected)
2. Use explicit newline command: `Ctrl-j`

### Ctrl-m vs Enter

**Note:** Many terminals send identical codes for `Ctrl-m` and `Enter`.

**Solutions:**
1. Use `Ctrl-Shift-M` for named marks (always distinct)
2. Quick marks available via command: `/mark`

## Custom Key Bindings

Configure custom bindings in `~/.ikigai/config.json`:

```json
{
  "keybindings": {
    "agent_prefix": "Ctrl-Space",
    "quick_mark": "F2",
    "quick_rewind": "F3",
    "agent_switcher": "F4",
    "command_mode": ":"
  }
}
```

**Supported Key Names:**
- `Ctrl-<key>` - Control combinations
- `Alt-<key>` - Alt/Meta combinations
- `Shift-<key>` - Shift combinations (limited terminal support)
- `F1-F12` - Function keys
- Named keys: `Home`, `End`, `PageUp`, `PageDown`, `Insert`, `Delete`

## Vim Mode (Future)

Optional vim-style modal editing:

**Normal Mode:**
- `j/k` - Scroll scrollback
- `g g` - Top of scrollback
- `Shift-G` - Bottom of scrollback
- `/` - Search scrollback
- `n/N` - Next/previous search result

**Insert Mode:**
- `i` - Insert mode (from normal)
- `Esc` - Normal mode (from insert)
- `Enter` - Submit (from insert)

**Agent Navigation (Normal Mode):**
- `g a` - Agent switcher
- `g m` - Jump to main
- `g n` - New agent
- `g c` - Close agent

**Mark/Rewind (Normal Mode):**
- `m a-z` - Set mark a-z
- `' a-z` - Rewind to mark a-z
- `m m` - Quick mark
- `' '` - Quick rewind

**Configuration:**
```json
{
  "keybindings": {
    "vim_mode": true
  }
}
```

## Terminal Compatibility

**Tested Terminals:**
- Alacritty - Full support ✓
- kitty - Full support ✓
- iTerm2 - Full support ✓
- GNOME Terminal - Most features ✓
- xterm - Basic support ✓
- tmux - Full support ✓
- screen - Basic support ⚠

**Known Issues:**
- **xterm:** Limited Shift key support
- **screen:** Some Ctrl combinations conflict
- **Windows Terminal:** Most features work, some key codes differ

**Recommendations:**
- Modern GPU-accelerated terminal (Alacritty, kitty)
- Configure terminal to pass through all key codes
- Disable terminal-level keyboard shortcuts that conflict

## Key Binding Philosophy

### 1. Minimal Modifier Keys

Prefer single modifiers:
- `Ctrl-\` over `Ctrl-Alt-\`
- `Ctrl-m` over `Ctrl-Shift-Alt-m`

**Why:** Faster, less hand strain, works better over SSH

### 2. Consistent Prefixes

Related operations share prefixes:
- `Ctrl-\` prefix for all agent operations
- `Ctrl-` prefix for editing (readline)
- `:` prefix for vim-style commands

**Why:** Muscle memory, discoverability

### 3. Common Patterns

Borrow from established tools:
- Readline/Emacs for editing
- Vim for navigation
- Tmux for multiplexing

**Why:** Existing muscle memory, reduced learning curve

### 4. No Mouse Required

All operations keyboard-accessible:
- Agent switching
- Scrollback navigation
- Text selection (future)

**Why:** Speed, SSH compatibility, accessibility

### 5. Configurable Defaults

All bindings configurable:
- User preferences vary
- Terminal capabilities vary
- Avoid hard conflicts

**Why:** Flexibility, user control

## Implementation Notes

### Key Code Detection

```c
// Raw terminal input handling
char seq[8];
read(STDIN_FILENO, seq, sizeof(seq));

// Map to logical keys
ik_key_t key = ik_term_parse_key(seq);

// Dispatch to handler
switch (key) {
    case IK_KEY_CTRL_BACKSLASH:
        handle_agent_switcher();
        break;
    // ... more handlers
}
```

### Terminal Database

Use terminfo for terminal capability detection:

```c
// Check if terminal supports Shift-Enter
bool has_shift_enter = tigetstr("kLFT") != NULL;

if (!has_shift_enter) {
    // Fall back to Alt-Enter or Ctrl-j
}
```

### Key Binding Registry

All bindings registered in `src/keybindings.c`:

```c
static ik_keybinding_t keybindings[] = {
    {
        .key = IK_KEY_CTRL_BACKSLASH,
        .description = "Agent switcher",
        .handler = handle_agent_switcher,
    },
    {
        .key = IK_KEY_CTRL_M,
        .description = "Quick mark",
        .handler = handle_quick_mark,
    },
    // ... more bindings
};
```

**Benefits:**
- Auto-generated help (`:keys`)
- Easy customization
- Consistent handling

## Related Documentation

- [commands.md](commands.md) - Slash command reference
- [multi-agent.md](multi-agent.md) - Multi-agent workflows
- [mark-rewind.md](mark-rewind.md) - Checkpoint system
