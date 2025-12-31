#include "agent.h"

#include "config.h"
#include "db/agent_row.h"
#include "panic.h"
#include "providers/provider.h"
#include "wrapper.h"

#include <assert.h>
#include <string.h>

// Forward declarations to avoid type conflicts during migration
typedef struct ik_provider ik_provider_t;
extern res_t ik_provider_create(TALLOC_CTX *ctx, const char *name, ik_provider_t **out);

static int parse_thinking_level(const char *level_str)
{
    // Note: level_str guaranteed non-NULL by caller (checked in ik_agent_restore_from_row)
    if (strcmp(level_str, "none") == 0) {
        return IK_THINKING_NONE;
    } else if (strcmp(level_str, "low") == 0) {
        return IK_THINKING_LOW;
    } else if (strcmp(level_str, "med") == 0 || strcmp(level_str, "medium") == 0) {
        return IK_THINKING_MED;
    } else if (strcmp(level_str, "high") == 0) {
        return IK_THINKING_HIGH;
    }
    // Default to none for unknown values
    return IK_THINKING_NONE;
}

res_t ik_agent_restore_from_row(ik_agent_ctx_t *agent, const void *row_ptr)
{
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE

    if (row_ptr == NULL) {
        return ERR(agent, INVALID_ARG, "Row is NULL");
    }

    const ik_db_agent_row_t *row = (const ik_db_agent_row_t *)row_ptr;

    // Load provider, model, thinking_level from database row
    // If DB fields are NULL (old agents pre-migration), leave as NULL
    // and they will be set by caller via ik_agent_apply_defaults

    if (row->provider != NULL) {
        agent->provider = talloc_strdup(agent, row->provider);
        if (agent->provider == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }

    if (row->model != NULL) {
        agent->model = talloc_strdup(agent, row->model);
        if (agent->model == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    }

    if (row->thinking_level != NULL) {
        agent->thinking_level = parse_thinking_level(row->thinking_level);
    }

    // Provider instance is lazy-loaded, leave it NULL
    agent->provider_instance = NULL;

    return OK(NULL);
}

res_t ik_agent_get_provider(ik_agent_ctx_t *agent, struct ik_provider **out)
{
    assert(agent != NULL);   // LCOV_EXCL_BR_LINE
    assert(out != NULL);     // LCOV_EXCL_BR_LINE

    // If already cached, return existing instance
    if (agent->provider_instance != NULL) {
        *out = agent->provider_instance;
        return OK(agent->provider_instance);
    }

    // Check if provider is configured
    if (agent->provider == NULL || strlen(agent->provider) == 0) {
        return ERR(agent, INVALID_ARG, "No provider configured");
    }

    // Create new provider instance
    ik_provider_t *provider = NULL;
    res_t res = ik_provider_create(agent, agent->provider, &provider);
    if (is_err(&res)) {
        // Provider creation failed (likely missing credentials)
        // Note: agent->provider guaranteed non-NULL here (checked at line 108)
        return ERR(agent, MISSING_CREDENTIALS,
                   "Failed to create provider '%s': %s",
                   agent->provider,
                   res.err->msg);
    }

    // Cache the provider instance
    agent->provider_instance = provider;
    *out = provider;

    return OK(provider);
}

void ik_agent_invalidate_provider(ik_agent_ctx_t *agent)
{
    assert(agent != NULL);  // LCOV_EXCL_BR_LINE

    // Free cached provider instance if it exists
    if (agent->provider_instance != NULL) {
        talloc_free(agent->provider_instance);
        agent->provider_instance = NULL;
    }

    // Safe to call multiple times - idempotent
}
