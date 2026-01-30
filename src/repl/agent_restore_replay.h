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
void ik_agent_restore_populate_conversation(ik_agent_ctx_t *agent, ik_replay_context_t *replay_ctx,
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
void ik_agent_restore_populate_scrollback(ik_agent_ctx_t *agent, ik_replay_context_t *replay_ctx, ik_logger_t *logger);

/**
 * Restore mark stack from replay context
 *
 * Recreates the agent's mark stack from the replay context's mark stack.
 * Currently minimal implementation as mark replay is not yet fully implemented.
 *
 * @param agent Agent context (must not be NULL)
 * @param replay_ctx Replay context with marks (must not be NULL)
 */
void ik_agent_restore_marks(ik_agent_ctx_t *agent, ik_replay_context_t *replay_ctx);

/**
 * Replay command side effects for agent restoration
 *
 * Some commands (like /model, /pin) have side effects that need to be re-applied
 * when replaying history to restore agent state.
 *
 * @param agent Agent context (must not be NULL)
 * @param msg Message containing command (must not be NULL)
 * @param logger Logger for info messages (may be NULL)
 */
void ik_agent_restore_replay_command_effects(ik_agent_ctx_t *agent, ik_msg_t *msg, ik_logger_t *logger);

/**
 * Replay all pin/unpin commands for an agent (independent of clear boundaries)
 *
 * Queries the agent's fork event to extract initial pinned_paths snapshot,
 * then replays ALL pin/unpin commands chronologically to rebuild pin state.
 * This operates independently of clear boundaries, ensuring pins persist
 * across /clear and restart.
 *
 * @param db Database context (must not be NULL)
 * @param agent Agent context (must not be NULL)
 * @return OK(NULL) on success, ERR on database failure
 */
res_t ik_agent_replay_pins(ik_db_ctx_t *db, ik_agent_ctx_t *agent);

/**
 * Replay all toolset commands for an agent (independent of clear boundaries)
 *
 * Queries the agent's fork event to extract initial toolset_filter snapshot,
 * then replays ALL toolset commands chronologically to rebuild toolset state.
 * This operates independently of clear boundaries, ensuring toolset persists
 * across /clear and restart.
 *
 * @param db Database context (must not be NULL)
 * @param agent Agent context (must not be NULL)
 * @return OK(NULL) on success, ERR on database failure
 */
res_t ik_agent_replay_toolset(ik_db_ctx_t *db, ik_agent_ctx_t *agent);

#endif // IK_REPL_AGENT_RESTORE_REPLAY_H
