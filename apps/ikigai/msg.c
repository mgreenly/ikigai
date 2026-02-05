/**
 * @file msg.c
 * @brief Canonical message format implementation
 */

#include "apps/ikigai/msg.h"
#include "shared/panic.h"
#include <assert.h>
#include <string.h>
#include <talloc.h>


#include "shared/poison.h"
bool ik_msg_is_conversation_kind(const char *kind)
{
    if (kind == NULL) {
        return false;
    }

    /* Conversation kinds that should be sent to LLM */
    if (strcmp(kind, "system") == 0 ||
        strcmp(kind, "user") == 0 ||
        strcmp(kind, "assistant") == 0 ||
        strcmp(kind, "tool_call") == 0 ||
        strcmp(kind, "tool_result") == 0 ||
        strcmp(kind, "tool") == 0) {  /* Legacy/wire-format tool messages */
        return true;
    }

    /* All other kinds (including metadata events) should not be sent to LLM */
    return false;
}
