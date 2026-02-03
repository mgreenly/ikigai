#ifndef IK_EVENT_RENDER_FORMAT_H
#define IK_EVENT_RENDER_FORMAT_H

#include <talloc.h>

/**
 * Apply formatting/truncation to tool_call content if needed.
 * Returns formatted content, or original content if already formatted or if data unavailable.
 *
 * @param ctx Talloc context for allocations
 * @param content Current content (may be raw or already formatted)
 * @param data_json JSON data with tool_name, tool_args, tool_call_id
 * @return Formatted content (owned by ctx)
 */
const char *ik_event_render_format_tool_call(TALLOC_CTX *ctx, const char *content, const char *data_json);

/**
 * Apply formatting/truncation to tool_result content if needed.
 * Returns formatted content, or original content if already formatted or if data unavailable.
 *
 * @param ctx Talloc context for allocations
 * @param content Current content (may be raw or already formatted)
 * @param data_json JSON data with name and output fields
 * @return Formatted content (owned by ctx)
 */
const char *ik_event_render_format_tool_result(TALLOC_CTX *ctx, const char *content, const char *data_json);

/**
 * Format raw tool_result content with truncation when tool name is unavailable.
 * Adds the tool response prefix and truncates content to 3 lines or 400 chars.
 *
 * @param ctx Talloc context for allocations
 * @param content Raw content to format (may be NULL)
 * @return Formatted content (owned by ctx)
 */
const char *ik_event_render_format_tool_result_raw(TALLOC_CTX *ctx, const char *content);

#endif // IK_EVENT_RENDER_FORMAT_H
