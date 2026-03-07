#ifndef IK_SUMMARY_H
#define IK_SUMMARY_H

#include "apps/ikigai/msg.h"
#include "apps/ikigai/providers/provider.h"
#include "shared/error.h"
#include <stddef.h>
#include <stdint.h>
#include <talloc.h>

/**
 * Maximum token budget for the recent summary (current epoch, pruned portion).
 * The LLM is instructed to stay within this limit; truncation is a safety net.
 */
#define IK_SUMMARY_RECENT_MAX_TOKENS 4000

/**
 * Maximum token budget for each previous-session summary (completed clear epochs).
 * At most IK_SUMMARY_PREVIOUS_SESSION_MAX_COUNT summaries are stored per agent.
 */
#define IK_SUMMARY_PREVIOUS_SESSION_MAX_TOKENS 2000

/**
 * Range of message indices [start, end).
 * An empty range has start == end.
 */
typedef struct {
    size_t start; /* First index (inclusive) */
    size_t end;   /* Last index (exclusive) */
} ik_summary_range_t;

/**
 * System prompt instructing the LLM to produce a conversation summary.
 *
 * Instructs the model to capture key decisions, note unresolved questions,
 * preserve technical details, omit filler, and be concise.
 */
extern const char IK_SUMMARY_PROMPT[];

/**
 * Compute the range of messages that should be summarized.
 *
 * The "recent" context window begins at context_start_index. Everything
 * before that index is a candidate for summarization. The epoch always
 * starts at 0 because /clear resets the message array.
 *
 * @param message_count        Total number of messages in the agent's array
 * @param context_start_index  First message index still in active context
 * @return Range [0, context_start_index). Empty when context_start_index == 0.
 */
ik_summary_range_t ik_summary_boundaries(size_t message_count, size_t context_start_index);

/**
 * Build a plain-text transcript from an array of messages.
 *
 * Only conversation-kind messages are included (see ik_msg_is_conversation_kind).
 * Metadata kinds (clear, mark, rewind, agent_killed) are skipped.
 *
 * Each message is rendered as "<kind>: <content>\n".
 *
 * @param ctx   Talloc parent for the returned string
 * @param msgs  Array of message pointers
 * @param count Number of messages in the array
 * @return Allocated transcript string (empty string if no conversation messages)
 */
char *ik_summary_transcript(TALLOC_CTX *ctx, ik_msg_t * const *msgs, size_t count);

/**
 * Generate a summary of a message range via an LLM call.
 *
 * Builds a request with IK_SUMMARY_PROMPT as the system block and the
 * message transcript as the user message, then calls the provider
 * synchronously by driving its event loop until the request completes.
 *
 * If the generated summary exceeds max_tokens (estimated as bytes / 4),
 * it is truncated at the last sentence boundary before the limit.
 * Truncation is a safety net — the prompt instructs the LLM to stay
 * within the limit.
 *
 * @param ctx        Talloc parent for the returned summary string
 * @param msgs       Array of message pointers to summarize
 * @param count      Number of messages in the array
 * @param provider   Provider instance to use for the LLM call
 * @param model      Model identifier (e.g. "claude-sonnet-4-6")
 * @param max_tokens Maximum tokens for the summary output
 * @param summary_out Receives the allocated summary string
 * @return OK on success, ERR on provider failure
 */
res_t ik_summary_generate(TALLOC_CTX *ctx,
                          ik_msg_t * const *msgs,
                          size_t count,
                          ik_provider_t *provider,
                          const char *model,
                          int32_t max_tokens,
                          char **summary_out);

#endif /* IK_SUMMARY_H */
