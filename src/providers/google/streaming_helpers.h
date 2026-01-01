/**
 * @file streaming_helpers.h
 * @brief Google streaming helper functions (internal)
 */

#ifndef IK_PROVIDERS_GOOGLE_STREAMING_HELPERS_H
#define IK_PROVIDERS_GOOGLE_STREAMING_HELPERS_H

#include "streaming.h"
#include "vendor/yyjson/yyjson.h"

/**
 * End any open tool call
 *
 * NOTE: This function marks the tool call as complete but preserves the
 * accumulated tool data (id, name, args) for the response builder.
 */
void ik_google_stream_end_tool_call_if_needed(ik_google_stream_ctx_t *sctx);

/**
 * Process error object from chunk
 */
void ik_google_stream_process_error(ik_google_stream_ctx_t *sctx, yyjson_val *error_obj);

/**
 * Process content parts array
 */
void ik_google_stream_process_parts(ik_google_stream_ctx_t *sctx, yyjson_val *parts_arr);

/**
 * Process usage metadata
 */
void ik_google_stream_process_usage(ik_google_stream_ctx_t *sctx, yyjson_val *usage_obj);

#endif /* IK_PROVIDERS_GOOGLE_STREAMING_HELPERS_H */
