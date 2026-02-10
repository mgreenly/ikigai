/**
 * @file request.h
 * @brief Google request serialization (internal)
 *
 * Transforms internal ik_request_t to Google JSON wire format.
 */

#ifndef IK_PROVIDERS_GOOGLE_REQUEST_H
#define IK_PROVIDERS_GOOGLE_REQUEST_H

#include <talloc.h>
#include <stdbool.h>
#include "shared/error.h"
#include "apps/ikigai/providers/provider.h"

/**
 * Serialize internal request to Google JSON format
 *
 * @param ctx      Talloc context for JSON allocation
 * @param req      Internal request structure
 * @param out_json Output: JSON string (allocated on ctx)
 * @return         OK with JSON string, ERR on failure
 *
 * Transformation:
 * - System prompt: add as "systemInstruction.parts[]" with text parts
 * - Messages: serialize as "contents[]" with "role" and "parts[]"
 * - Role mapping: USER->"user", ASSISTANT->"model", TOOL->"function"
 * - Content blocks:
 *   - TEXT: {"text": "..."}
 *   - THINKING: {"text": "...", "thought": true}
 *   - TOOL_CALL: {"functionCall": {"name": "...", "args": {...}}}
 *   - TOOL_RESULT: {"functionResponse": {"name": "...", "response": {"content": "..."}}}
 * - Tools: wrap in {"tools": [{"functionDeclarations": [...]}]}
 * - Tool choice: NONE->"NONE", AUTO->"AUTO", REQUIRED->"ANY"
 * - Thinking: add generationConfig.thinkingConfig
 *   - Gemini 2.5: thinkingBudget (0-32768)
 *   - Gemini 3: thinkingLevel ("LOW"/"HIGH")
 * - Provider data: extract thought_signature from message provider_metadata
 *   and add as first part with {"thoughtSignature": "value"}
 *
 * Errors:
 * - ERR(INVALID_ARG) if model is NULL
 * - ERR(PARSE) if JSON serialization fails
 */
res_t ik_google_serialize_request(TALLOC_CTX *ctx, const ik_request_t *req, char **out_json);

/**
 * Build API URL with model method and key parameter
 *
 * @param ctx       Talloc context for URL allocation
 * @param base_url  Base API URL (e.g., "https://generativelanguage.googleapis.com/v1beta")
 * @param model     Model identifier (e.g., "gemini-2.5-flash")
 * @param api_key   API key for query parameter
 * @param streaming true for streaming, false for non-streaming
 * @param out_url   Output: URL string (allocated on ctx)
 * @return          OK with URL, ERR on failure
 *
 * Format:
 * - Non-streaming: {base_url}/models/{model}:generateContent?key={api_key}
 * - Streaming: {base_url}/models/{model}:streamGenerateContent?key={api_key}&alt=sse
 *
 * Errors:
 * - ERR(INVALID_ARG) if any parameter is NULL
 * - ERR(PARSE) if URL construction fails
 */
res_t ik_google_build_url(TALLOC_CTX *ctx,
                          const char *base_url,
                          const char *model,
                          const char *api_key,
                          bool streaming,
                          char **out_url);

/**
 * Build HTTP headers for Google API
 *
 * @param ctx         Talloc context for header allocation
 * @param streaming   true for streaming, false for non-streaming
 * @param out_headers Output: NULL-terminated array of header strings
 * @return            OK with headers, ERR on failure
 *
 * Returns NULL-terminated array:
 * - Non-streaming: ["Content-Type: application/json", NULL]
 * - Streaming: ["Content-Type: application/json", "Accept: text/event-stream", NULL]
 */
#endif /* IK_PROVIDERS_GOOGLE_REQUEST_H */
