## Overview

Renders conversation events (user messages, assistant responses, system messages, marks, commands, forks, etc.) into the scrollback buffer for terminal display. Handles event-specific formatting, color styling based on event kind, and JSON parsing for mark labels.

## Code

```
function ik_event_renders_visible(kind):
    if kind is null:
        return false

    if kind is one of: "user", "assistant", "system", "mark", "command", "fork":
        return true

    return false


function apply_style(content, color):
    if colors are disabled or color is 0:
        return content unchanged

    generate ANSI color sequence for the color code
    wrap content with color sequence and reset code
    return styled content


function extract_label_from_json(data_json):
    if data_json is null:
        return null

    parse data_json as JSON document
    if parsing fails:
        return null

    extract "label" field from root object
    if label exists and is non-empty string:
        return label value

    return null


function render_mark_event(scrollback, data_json):
    allocate temporary context

    label = extract label from data_json

    if label exists:
        format text as "/mark LABEL"
    else:
        format text as "/mark"

    append text line to scrollback
    if error:
        clean up and return error

    append blank line for spacing
    clean up temporary context
    return result


function render_content_event(scrollback, content, color):
    if content is null or empty:
        return success (nothing to render)

    allocate temporary context

    trim trailing whitespace from content
    if content is empty after trimming:
        clean up and return success

    apply color styling to trimmed content

    append styled content line to scrollback
    if error:
        clean up and return error

    append blank line for spacing
    clean up temporary context
    return result


function ik_event_render(scrollback, kind, content, data_json):
    validate kind parameter is not null
    if invalid:
        return error

    determine color code based on kind:
        if kind is "assistant":
            color = light gray
        else if kind is "tool_call", "tool_result", "system", "command", "fork":
            color = subdued gray
        else:
            color = 0 (no color)

    if kind is content-type: "user", "assistant", "system", "tool_call", "tool_result", "command", "fork":
        render content event with determined color

    else if kind is "mark":
        render mark event

    else if kind is "rewind", "clear", "agent_killed":
        return success (these don't render visual content)

    else:
        return error (unknown event kind)
```
