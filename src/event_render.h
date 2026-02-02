/**
 * @file event_render.h
 * @brief Universal event renderer for scrollback
 *
 * Provides a unified rendering interface for all event types (user, assistant,
 * system, mark, rewind, clear). This ensures identical visual output for both
 * live commands and replay from database.
 *
 * Design decisions:
 * - All events use kind + data_json pattern
 * - Mark events: content=NULL, label from data_json
 * - Rendering is kind-specific but unified interface
 * - Clear/rewind events render nothing (handled differently)
 */

#ifndef IK_EVENT_RENDER_H
#define IK_EVENT_RENDER_H

#include "error.h"
#include "scrollback.h"
#include <stdbool.h>

/**
 * Render an event to scrollback buffer
 *
 * Universal renderer that handles all event types:
 * - "user": Render content as-is
 * - "assistant": Render content as-is
 * - "system": Render content as-is
 * - "mark": Render as "/mark LABEL" where LABEL from data_json, or "/mark" if no label
 * - "rewind": Render nothing (result shown elsewhere)
 * - "clear": Render nothing (clears scrollback)
 *
 * @param scrollback  Scrollback buffer to render to
 * @param kind        Event kind string
 * @param content     Event content (may be NULL for mark/rewind/clear)
 * @param data_json   JSON data string (may be NULL)
 * @param interrupted True if message was interrupted/cancelled
 * @return            OK on success, ERR on failure (invalid parameters or unknown kind)
 *
 * Error conditions:
 * - Returns ERR if scrollback is NULL
 * - Returns ERR if kind is NULL
 * - Returns ERR if kind is not a recognized event type
 */
res_t ik_event_render(ik_scrollback_t *scrollback, const char *kind, const char *content, const char *data_json, bool interrupted);

/**
 * Check if an event kind should render visible output
 *
 * Returns true for kinds that produce visible scrollback content.
 * Used to determine if scrollback append is needed.
 *
 * @param kind  Event kind string
 * @return      true if kind renders visible output, false otherwise
 */
bool ik_event_renders_visible(const char *kind);

#endif // IK_EVENT_RENDER_H
