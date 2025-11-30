#ifndef IK_CONVERSATION_H
#define IK_CONVERSATION_H

#include "msg.h"
#include "error.h"
#include <talloc.h>
#include <stddef.h>

/**
 * Conversation structure
 *
 * Container for a sequence of canonical messages that form a conversation.
 * Used to maintain conversation state and provide context to LLM.
 *
 * All messages are owned by the conversation and freed when conversation is freed.
 */
typedef struct {
    ik_msg_t **messages;     /* Array of message pointers */
    size_t message_count;    /* Number of messages */
    size_t capacity;         /* Allocated capacity */
} ik_conversation_t;

#endif /* IK_CONVERSATION_H */
