# Mouse Scroll Should Scroll Scrollback Not History

## Description

Mouse wheel scrolling should scroll the scrollback viewport (conversation history visible on screen), not cycle through input history (previous commands). Currently, mouse wheel incorrectly triggers input history navigation.

## Transcript

**Current behavior (incorrect):**

User has scrollback with conversation:
```text
User: What is the capital of France?
Assistant: The capital of France is Paris.

User: What about Germany?
Assistant: The capital of Germany is Berlin.

────────────────────────────────────────────────────────────────────────────────
What about Spain?_
────────────────────────────────────────────────────────────────────────────────
```

User scrolls mouse wheel up.

```text
User: What is the capital of France?
Assistant: The capital of France is Paris.

User: What about Germany?
Assistant: The capital of Germany is Berlin.

────────────────────────────────────────────────────────────────────────────────
What about Germany?_
────────────────────────────────────────────────────────────────────────────────
```

Input buffer incorrectly replaced with previous command from history.

**Expected behavior (correct):**

User has scrollback with conversation:
```text
User: What is the capital of France?
Assistant: The capital of France is Paris.

User: What about Germany?
Assistant: The capital of Germany is Berlin.

────────────────────────────────────────────────────────────────────────────────
What about Spain?_
────────────────────────────────────────────────────────────────────────────────
```

User scrolls mouse wheel up.

```text
User: What is the capital of France?
────────────────────────────────────────────────────────────────────────────────
What about Spain?_
────────────────────────────────────────────────────────────────────────────────
```

Scrollback viewport scrolls up (like Page Up). Input buffer unchanged.

## Walkthrough

1. User scrolls mouse wheel up or down

2. Terminal emits mouse event escape sequence

3. Input parser detects mouse scroll event

4. **CURRENT (WRONG):** Event handler calls input history navigation
   - Mouse up → previous history entry
   - Mouse down → next history entry
   - Input buffer content replaced

5. **EXPECTED (CORRECT):** Event handler calls scrollback viewport scroll
   - Mouse up → scroll scrollback up (earlier conversation)
   - Mouse down → scroll scrollback down (later conversation)
   - Input buffer content untouched

6. Render system updates to show new viewport position

7. User sees scrollback scrolled, input unchanged

## Reference

Mouse events should map to scrollback actions:

```c
// In input event handler:
case MOUSE_SCROLL_UP:
    ik_scrollback_scroll_up(repl->scrollback, lines_per_scroll);
    break;

case MOUSE_SCROLL_DOWN:
    ik_scrollback_scroll_down(repl->scrollback, lines_per_scroll);
    break;
```

Input history navigation should ONLY be triggered by keyboard:
- Up Arrow / Ctrl+P → previous history
- Down Arrow / Ctrl+N → next history

Never triggered by mouse events.
