#ifndef IK_OPENAI_HTTP_HANDLER_INTERNAL_H
#define IK_OPENAI_HTTP_HANDLER_INTERNAL_H

/**
 * Internal header for http_handler implementation
 *
 * Exposes internal functions for unit testing.
 */

/**
 * Extract finish_reason from SSE event (exposed for testing)
 *
 * Parses the raw SSE event JSON to extract choices[0].finish_reason if present.
 *
 * @param parent  Talloc context parent
 * @param event   Raw SSE event string (with "data: " prefix)
 * @return        Finish reason string or NULL if not present
 */
char *ik_openai_http_extract_finish_reason(void *parent, const char *event);

#endif /* IK_OPENAI_HTTP_HANDLER_INTERNAL_H */
