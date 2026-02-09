#ifndef IK_TEMPLATE_H
#define IK_TEMPLATE_H

#include <inttypes.h>
#include <talloc.h>
#include "apps/ikigai/agent.h"
#include "apps/ikigai/config.h"
#include "shared/error.h"

/**
 * @brief Result of template processing
 */
typedef struct {
    char *processed;           // Processed text with variables resolved
    char **unresolved;         // Array of unresolved variable names
    size_t unresolved_count;   // Number of unresolved variables
} ik_template_result_t;

/**
 * @brief Process template text, resolving ${variable} syntax
 *
 * Supports:
 * - ${agent.*} - agent context fields
 * - ${config.*} - config fields
 * - ${env.*} - environment variables
 * - ${func.*} - computed values (now, cwd, hostname, random)
 * - $$ - escape to literal $
 *
 * Unresolved variables remain as literal text (not replaced).
 *
 * @param ctx      Talloc context for allocations
 * @param text     Input text to process
 * @param agent    Agent context (for ${agent.*})
 * @param config   Config (for ${config.*})
 * @param out      Template result (allocated on ctx)
 * @return         OK on success, ERR on failure
 */
res_t ik_template_process(TALLOC_CTX *ctx,
                          const char *text,
                          ik_agent_ctx_t *agent,
                          ik_config_t *config,
                          ik_template_result_t **out);

#endif // IK_TEMPLATE_H
