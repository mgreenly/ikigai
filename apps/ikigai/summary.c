/**
 * @file summary.c
 * @brief Context summary: boundaries, prompt, and transcript generation
 */

#include "apps/ikigai/summary.h"
#include "apps/ikigai/msg.h"
#include <stddef.h>
#include <talloc.h>

#include "shared/poison.h"

const char IK_SUMMARY_PROMPT[] =
    "You are summarizing a conversation for long-term memory. "
    "Capture key decisions and conclusions. "
    "Note unresolved questions and open issues. "
    "Preserve technical details: file names, function names, error messages, "
    "command output, and identifiers. "
    "Omit filler, pleasantries, and repetition. "
    "Be concise. Use bullet points for lists. "
    "Write in past tense from the perspective of the conversation participants.";

ik_summary_range_t ik_summary_boundaries(size_t message_count,
                                          size_t context_start_index)
{
    (void)message_count; /* epoch starts at 0 after /clear resets the array */

    if (context_start_index == 0) {
        return (ik_summary_range_t){ .start = 0, .end = 0 };
    }

    return (ik_summary_range_t){ .start = 0, .end = context_start_index };
}

char *ik_summary_transcript(TALLOC_CTX *ctx,
                             ik_msg_t * const *msgs,
                             size_t count)
{
    char *result = talloc_strdup(ctx, "");

    for (size_t i = 0; i < count; i++) {
        const ik_msg_t *msg = msgs[i];

        if (!ik_msg_is_conversation_kind(msg->kind)) {
            continue;
        }

        const char *content = msg->content ? msg->content : "";
        result = talloc_asprintf_append(result, "%s: %s\n", msg->kind, content);
    }

    return result;
}
