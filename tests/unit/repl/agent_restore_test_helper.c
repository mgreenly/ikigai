/**
 * @file agent_restore_test_helpers.c
 * @brief Helper functions for agent restore tests
 */

#include "agent_restore_test_helper.h"

#include "../../../src/db/agent.h"
#include "../../../src/db/message.h"
#include "../../../src/agent.h"
#include "../../../src/error.h"
#include "../../../src/shared.h"
#include "../../../src/logger.h"
#include "../../../src/layer_wrappers.h"
#include "../../test_utils_helper.h"

#include <check.h>
#include <talloc.h>

// Helper: Insert an agent into the registry
void insert_agent(const char *uuid, const char *parent_uuid,
                  int64_t created_at, int64_t fork_message_id)
{
    ik_agent_ctx_t agent = {0};
    agent.uuid = talloc_strdup(test_ctx, uuid);
    agent.name = NULL;
    agent.parent_uuid = parent_uuid ? talloc_strdup(test_ctx, parent_uuid) : NULL;
    agent.created_at = created_at;
    agent.fork_message_id = fork_message_id;

    res_t res = ik_db_agent_insert(db, &agent);
    ck_assert(is_ok(&res));
}

// Helper: Insert a message
void insert_message(const char *agent_uuid, const char *kind,
                    const char *content)
{
    res_t res = ik_db_message_insert(db, session_id, agent_uuid, kind, content, "{}");
    ck_assert(is_ok(&res));
}

// Helper: Create minimal repl context for testing
ik_repl_ctx_t *create_test_repl(void)
{
    ik_repl_ctx_t *repl = talloc_zero(test_ctx, ik_repl_ctx_t);
    ck_assert_ptr_nonnull(repl);

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(repl, ik_shared_ctx_t);
    ck_assert_ptr_nonnull(shared);
    shared->db_ctx = db;
    shared->session_id = session_id;

    // Initialize paths (environment should be set up in suite_setup or test_setup)
    ik_paths_t *paths = NULL;
    res_t paths_res = ik_paths_init(shared, &paths);
    ck_assert(is_ok(&paths_res));
    shared->paths = paths;

    // Create logger (after paths setup so directories exist)
    shared->logger = ik_logger_create(shared, "/tmp");
    ck_assert_ptr_nonnull(shared->logger);

    // Create config
    shared->cfg = ik_test_create_config(shared);

    repl->shared = shared;

    // Initialize agents array
    repl->agents = talloc_array(repl, ik_agent_ctx_t *, 16);
    ck_assert_ptr_nonnull(repl->agents);
    repl->agent_count = 0;
    repl->agent_capacity = 16;

    // Create Agent 0 (root agent)
    ik_agent_ctx_t *agent0 = NULL;
    res_t res = ik_agent_create(repl, shared, NULL, &agent0);
    ck_assert(is_ok(&res));

    repl->agents[0] = agent0;
    repl->agent_count = 1;
    repl->current = agent0;

    return repl;
}

// Helper: Create repl with lower_separator_layer
ik_repl_ctx_t *create_test_repl_with_lower_separator(void)
{
    ik_repl_ctx_t *repl = create_test_repl();

    // Create lower separator layer (as done in repl_init.c)
    repl->lower_separator_layer = ik_separator_layer_create(repl, "lower_separator", &repl->lower_separator_visible);
    ck_assert_ptr_nonnull(repl->lower_separator_layer);

    return repl;
}
