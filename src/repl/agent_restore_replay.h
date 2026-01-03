#ifndef IK_REPL_AGENT_RESTORE_REPLAY_H
#define IK_REPL_AGENT_RESTORE_REPLAY_H

#include "../agent.h"
#include "../db/agent_replay.h"
#include "../logger.h"

/**
 * Populate agent conversation from replay context
 *
 * Iterates through replay messages and adds conversation-kind messages
 * to the agent's OpenAI conversation.
 *
 * @param agent Agent context (must not be NULL)
 * @param replay_ctx Replay context with messages (must not be NULL)
 * @param logger Logger for warnings (may be NULL)
 */
void ik_agent_restore_populate_conversation(
    ik_agent_ctx_t *agent,
    ik_replay_context_t *replay_ctx,
    ik_logger_t *logger);

/**
 * Populate agent scrollback from replay context
 *
 * Renders each message from replay context into the agent's scrollback
 * buffer for terminal display.
 *
 * @param agent Agent context (must not be NULL)
 * @param replay_ctx Replay context with messages (must not be NULL)
 * @param logger Logger for warnings (may be NULL)
 */
void ik_agent_restore_populate_scrollback(
    ik_agent_ctx_t *agent,
    ik_replay_context_t *replay_ctx,
    ik_logger_t *logger);

/**
 * Restore mark stack from replay context
 *
 * Recreates the agent's mark stack from the replay context's mark stack.
 * Currently minimal implementation as mark replay is not yet fully implemented.
 *
 * @param agent Agent context (must not be NULL)
 * @param replay_ctx Replay context with marks (must not be NULL)
 */
void ik_agent_restore_marks(
    ik_agent_ctx_t *agent,
    ik_replay_context_t *replay_ctx);

#endif // IK_REPL_AGENT_RESTORE_REPLAY_H
