#ifndef IK_REPL_SESSION_RESTORE_H
#define IK_REPL_SESSION_RESTORE_H

#include "../error.h"
#include "../repl.h"
#include "../db/connection.h"
#include "../config.h"

/**
 * Restore session on REPL initialization (Model B)
 *
 * Detects active session and replays event stream to restore conversation state.
 * If no active session exists, creates new session and writes initial events.
 *
 * **New session path (no active session):**
 * 1. Create new session via ik_db_session_create()
 * 2. Write initial clear event
 * 3. Write system message event if config->openai_system_message is configured
 * 4. Leave scrollback empty (ready for user input)
 *
 * **Existing session path (active session found):**
 * 1. Detect via ik_db_session_get_active()
 * 2. Load and replay messages via ik_db_messages_load()
 * 3. Populate scrollback with replayed messages (after most recent clear)
 * 4. Ready to continue conversation
 *
 * @param repl REPL context (must not be NULL)
 * @param db_ctx Database context (must not be NULL)
 * @param cfg Configuration (must not be NULL)
 * @return OK on success, ERR on failure
 */
res_t ik_repl_restore_session(ik_repl_ctx_t *repl, ik_db_ctx_t *db_ctx, ik_cfg_t *cfg);

#endif // IK_REPL_SESSION_RESTORE_H
