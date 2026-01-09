#include <stdio.h>
#include <talloc.h>
#include "src/commands.h"
#include "src/repl.h"
#include "src/scrollback.h"
#include "src/agent.h"
#include "src/shared.h"
#include "src/config.h"

static ik_repl_ctx_t *create_test_repl(void *parent) {
    ik_scrollback_t *scrollback = ik_scrollback_create(parent, 80);
    ik_config_t *cfg = talloc_zero(parent, ik_config_t);
    ik_shared_ctx_t *shared = talloc_zero(parent, ik_shared_ctx_t);
    shared->cfg = cfg;

    ik_repl_ctx_t *r = talloc_zero(parent, ik_repl_ctx_t);
    ik_agent_ctx_t *agent = talloc_zero(r, ik_agent_ctx_t);
    agent->scrollback = scrollback;

    r->current = agent;
    r->current->marks = NULL;
    r->current->mark_count = 0;
    r->shared = shared;

    return r;
}

int main(void) {
    void *ctx = talloc_new(NULL);
    ik_repl_ctx_t *repl = create_test_repl(ctx);

    res_t res = ik_cmd_dispatch(ctx, repl, "/help");
    if (is_err(&res)) {
        printf("Error running help command\n");
        return 1;
    }

    size_t line_count = ik_scrollback_get_line_count(repl->current->scrollback);
    printf("Total lines: %zu\n\n", line_count);

    for (size_t i = 0; i < line_count; i++) {
        const char *line = NULL;
        size_t length = 0;
        res = ik_scrollback_get_line_text(repl->current->scrollback, i, &line, &length);
        if (is_ok(&res)) {
            printf("Line %2zu: '%s'\n", i, line);
        }
    }

    talloc_free(ctx);
    return 0;
}