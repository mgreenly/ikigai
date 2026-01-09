#include "src/agent.h"
#include "src/commands.h"
#include "src/config.h"
#include "src/shared.h"
#include "src/error.h"
#include "src/repl.h"
#include "src/scrollback.h"

#include <talloc.h>
#include <stdio.h>

static ik_repl_ctx_t *create_test_repl_for_commands(void *parent)
{
    // Create scrollback buffer (80 columns is standard)
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    if (!scrollback) return NULL;

    // Create minimal config
    ik_config_t *cfg = talloc_zero(parent, ik_config_t);
    if (!cfg) return NULL;

    // Create shared context
    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    if (!shared) return NULL;
    shared->cfg = cfg;

    // Create minimal REPL context
    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    if (!r) return NULL;

    // Create agent context
    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    if (!agent) return NULL;
    agent->scrollback = scrollback;

    r->current = agent;
    r->current->marks = NULL;
    r->current->mark_count = 0;
    r->shared = shared;

    return r;
}

int main(void)
{
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl_for_commands(ctx);

    if (!repl) {
        printf("Failed to create test repl\n");
        return 1;
    }

    // Execute help command
    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    if (is_err(&res)) {
        printf("Help command failed\n");
        return 1;
    }

    // Print all lines
    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    printf("Total lines: %zu\n", line_count);

    for (size_t i = 0; i < line_count; i++) {
        const char *line = NULL;
        size_t length = 0;
        res_t line_res = ik_scrollback_get_line_text(repl->current->scrollback, i, &line, &length);
        if (is_ok(&line_res)) {
            printf("Line %zu: '%s'\n", i, line);
        }
    }

    talloc_free(ctx);
    return 0;
}