#ifndef IK_SUMMARY_H
#define IK_SUMMARY_H

#include "apps/ikigai/msg.h"
#include <stddef.h>
#include <talloc.h>

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
ik_summary_range_t ik_summary_boundaries(size_t message_count,
                                          size_t context_start_index);

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
char *ik_summary_transcript(TALLOC_CTX *ctx,
                             ik_msg_t * const *msgs,
                             size_t count);

#endif /* IK_SUMMARY_H */
