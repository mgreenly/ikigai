/**
 * @file agent_restore_test_helpers.h
 * @brief Helper functions for agent restore tests
 */

#pragma once

#include "../../../src/db/connection.h"
#include "../../../src/repl.h"
#include <talloc.h>
#include <stdint.h>

// External state for helpers
extern TALLOC_CTX *test_ctx;
extern ik_db_ctx_t *db;
extern int64_t session_id;

/**
 * Insert an agent into the database registry
 * @param uuid Agent UUID
 * @param parent_uuid Parent agent UUID (NULL for root)
 * @param created_at Creation timestamp
 * @param fork_message_id Fork message ID
 */
void insert_agent(const char *uuid, const char *parent_uuid, int64_t created_at, int64_t fork_message_id);

/**
 * Insert a message into the database
 * @param agent_uuid Agent UUID
 * @param kind Message kind
 * @param content Message content
 */
void insert_message(const char *agent_uuid, const char *kind, const char *content);

/**
 * Create minimal repl context for testing
 * @return Initialized repl context
 */
ik_repl_ctx_t *create_test_repl(void);
