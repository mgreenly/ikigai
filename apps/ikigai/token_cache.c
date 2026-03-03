/**
 * @file token_cache.c
 * @brief Token count cache for sliding context window
 */

#include "apps/ikigai/token_cache.h"
#include "apps/ikigai/agent.h"
#include "apps/ikigai/providers/provider.h"
#include "apps/ikigai/providers/provider_vtable.h"
#include "apps/ikigai/providers/request.h"
#include "apps/ikigai/token_count.h"
#include "shared/error.h"
#include "shared/panic.h"

#include <assert.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

/* --- Internal structure --- */

#define TOKEN_UNCACHED   (-1)
#define TURN_CAP_INITIAL 8

struct ik_token_cache {
    ik_agent_ctx_t *agent;       /* non-owning */
    size_t context_start_index;  /* first msg in context */
    int32_t system_tokens;       /* -1=uncached */
    int32_t tool_tokens;         /* -1=uncached */
    int32_t *turn_tokens;        /* per-turn, -1=uncached */
    size_t turn_count;           /* active turns */
    size_t turn_capacity;        /* capacity */
    int32_t total_tokens;        /* -1=uncached */
    int32_t budget;              /* token budget */
    size_t  pruned_turn_count;   /* turns pruned */
};

/* --- Internal helpers --- */

static ik_token_cache_t *alloc_cache(TALLOC_CTX *ctx,
                                      ik_agent_ctx_t *agent,
                                      size_t turn_capacity)
{
    ik_token_cache_t *c = talloc_zero(ctx, ik_token_cache_t);
    if (c == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    c->agent = agent;
    c->system_tokens = TOKEN_UNCACHED;
    c->tool_tokens   = TOKEN_UNCACHED;
    c->total_tokens  = TOKEN_UNCACHED;
    c->context_start_index = 0;
    c->budget = 100000;
    c->pruned_turn_count = 0;

    if (turn_capacity > 0) {
        c->turn_tokens = talloc_array(c, int32_t, (unsigned int)turn_capacity);
        if (c->turn_tokens == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        c->turn_capacity = turn_capacity;
    }
    return c;
}

/* Estimate bytes in agent messages[start..end) for fallback */
static size_t estimate_turn_bytes(ik_agent_ctx_t *agent, size_t start, size_t end)
{
    size_t total = 0;
    for (size_t i = start; i < end; i++) {
        const ik_message_t *msg = agent->messages[i];
        for (size_t j = 0; j < msg->content_count; j++) {
            const ik_content_block_t *blk = &msg->content_blocks[j];
            if (blk->type == IK_CONTENT_TEXT && blk->data.text.text != NULL) {
                total += strlen(blk->data.text.text);
            } else if (blk->type == IK_CONTENT_TOOL_CALL) {
                if (blk->data.tool_call.arguments != NULL)
                    total += strlen(blk->data.tool_call.arguments);
            } else if (blk->type == IK_CONTENT_TOOL_RESULT) {
                if (blk->data.tool_result.content != NULL)
                    total += strlen(blk->data.tool_result.content);
            } else if (blk->type == IK_CONTENT_THINKING) {
                if (blk->data.thinking.text != NULL)
                    total += strlen(blk->data.thinking.text);
            }
        }
    }
    return total;
}

/* Find message [start,end) for turn_index (relative to context_start_index). */
static bool find_turn_bounds(const ik_token_cache_t *cache, size_t turn_index,
                              size_t *start_out, size_t *end_out)
{
    size_t user_count = 0;
    size_t turn_start = 0;
    bool found = false;

    for (size_t i = cache->context_start_index; i < cache->agent->message_count; i++) {
        if (cache->agent->messages[i]->role == IK_ROLE_USER) {
            if (user_count == turn_index) {
                turn_start = i;
                found = true;
            } else if (user_count == turn_index + 1 && found) {
                *start_out = turn_start;
                *end_out   = i;
                return true;
            }
            user_count++;
        }
    }

    if (found) {
        *start_out = turn_start;
        *end_out   = cache->agent->message_count;
        return true;
    }
    return false;
}

/* Call provider count_tokens. Returns -1 on failure. */
static int32_t provider_count(ik_token_cache_t *cache, ik_request_t *req)
{
    struct ik_provider *p = cache->agent->provider_instance;
    if (p == NULL || p->vt == NULL || p->vt->count_tokens == NULL) {
        return TOKEN_UNCACHED;
    }
    int32_t count = 0;
    res_t r = p->vt->count_tokens(p->ctx, req, &count);
    if (is_err(&r)) {
        return TOKEN_UNCACHED;
    }
    return count;
}

/* Build minimal request for a turn's messages and count via provider */
static int32_t count_turn_via_provider(ik_token_cache_t *cache,
                                        size_t msg_start, size_t msg_end)
{
    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    const char *model = (cache->agent->model != NULL) ? cache->agent->model : "default";
    ik_request_t *req = NULL;
    res_t r = ik_request_create(tmp, model, &req);
    if (is_err(&r)) {
        talloc_free(tmp);
        return TOKEN_UNCACHED;
    }

    for (size_t i = msg_start; i < msg_end; i++) {
        const ik_message_t *msg = cache->agent->messages[i];
        r = ik_request_add_message_blocks(req, msg->role,
                                           msg->content_blocks,
                                           msg->content_count);
        if (is_err(&r)) {
            talloc_free(tmp);
            return TOKEN_UNCACHED;
        }
    }

    int32_t result = provider_count(cache, req);
    talloc_free(tmp);
    return result;
}

/* --- Lifecycle --- */

ik_token_cache_t *ik_token_cache_create(TALLOC_CTX *ctx, ik_agent_ctx_t *agent)
{
    assert(agent != NULL); // LCOV_EXCL_BR_LINE
    return alloc_cache(ctx, agent, TURN_CAP_INITIAL);
}

ik_token_cache_t *ik_token_cache_clone(TALLOC_CTX *ctx,
                                        const ik_token_cache_t *src,
                                        ik_agent_ctx_t *new_agent)
{
    assert(src != NULL);       // LCOV_EXCL_BR_LINE
    assert(new_agent != NULL); // LCOV_EXCL_BR_LINE

    size_t cap = src->turn_capacity > 0 ? src->turn_capacity : TURN_CAP_INITIAL;
    ik_token_cache_t *c = alloc_cache(ctx, new_agent, cap);

    c->context_start_index = src->context_start_index;
    c->system_tokens = src->system_tokens;
    c->tool_tokens   = src->tool_tokens;
    c->total_tokens  = src->total_tokens;
    c->turn_count    = src->turn_count;
    c->budget        = src->budget;
    c->pruned_turn_count = src->pruned_turn_count;

    if (src->turn_count > 0 && src->turn_tokens != NULL) {
        for (size_t i = 0; i < src->turn_count; i++) {
            c->turn_tokens[i] = src->turn_tokens[i];
        }
    }
    return c;
}

/* --- Getters --- */

int32_t ik_token_cache_get_system_tokens(ik_token_cache_t *cache)
{
    assert(cache != NULL); // LCOV_EXCL_BR_LINE

    if (cache->system_tokens != TOKEN_UNCACHED) {
        return cache->system_tokens;
    }

    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    char *prompt = NULL;
    res_t r = ik_agent_get_effective_system_prompt(cache->agent, &prompt);

    ik_request_t *req = NULL;
    if (is_ok(&r)) {
        const char *model = (cache->agent->model != NULL) ? cache->agent->model : "default";
        r = ik_request_create(tmp, model, &req);
        if (is_ok(&r) && prompt != NULL) {
            r = ik_request_set_system(req, prompt);
        }
    }

    if (is_ok(&r) && req != NULL) {
        int32_t count = provider_count(cache, req);
        if (count != TOKEN_UNCACHED) {
            cache->system_tokens = count;
            cache->total_tokens = TOKEN_UNCACHED;
            talloc_free(tmp);
            return count;
        }
    }

    size_t bytes = (prompt != NULL) ? strlen(prompt) : 0;
    talloc_free(tmp);
    return ik_token_count_from_bytes(bytes);
}

int32_t ik_token_cache_get_tool_tokens(ik_token_cache_t *cache)
{
    assert(cache != NULL); // LCOV_EXCL_BR_LINE

    if (cache->tool_tokens != TOKEN_UNCACHED) {
        return cache->tool_tokens;
    }

    TALLOC_CTX *tmp = talloc_new(NULL);
    if (tmp == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    const char *model = (cache->agent->model != NULL) ? cache->agent->model : "default";
    ik_request_t *req = NULL;
    res_t r = ik_request_create(tmp, model, &req);

    if (is_ok(&r)) {
        int32_t count = provider_count(cache, req);
        if (count != TOKEN_UNCACHED) {
            cache->tool_tokens = count;
            cache->total_tokens = TOKEN_UNCACHED;
            talloc_free(tmp);
            return count;
        }
    }

    talloc_free(tmp);
    return 0;
}

int32_t ik_token_cache_get_turn_tokens(ik_token_cache_t *cache, size_t turn_index)
{
    assert(cache != NULL); // LCOV_EXCL_BR_LINE
    if (turn_index >= cache->turn_count) PANIC("turn_index out of range");

    if (cache->turn_tokens[turn_index] != TOKEN_UNCACHED) {
        return cache->turn_tokens[turn_index];
    }

    /* No cached value — try to find turn messages and count */
    size_t msg_start = 0;
    size_t msg_end   = 0;
    if (cache->agent != NULL &&
        find_turn_bounds(cache, turn_index, &msg_start, &msg_end)) {
        int32_t count = count_turn_via_provider(cache, msg_start, msg_end);
        if (count != TOKEN_UNCACHED) {
            cache->turn_tokens[turn_index] = count;
            cache->total_tokens = TOKEN_UNCACHED;
            return count;
        }
        /* Bytes fallback — not cached */
        size_t bytes = estimate_turn_bytes(cache->agent, msg_start, msg_end);
        return ik_token_count_from_bytes(bytes);
    }

    /* No messages found for this turn */
    return 0;
}

int32_t ik_token_cache_get_total(ik_token_cache_t *cache)
{
    assert(cache != NULL); // LCOV_EXCL_BR_LINE

    if (cache->total_tokens != TOKEN_UNCACHED) {
        return cache->total_tokens;
    }

    int32_t total = 0;
    total += ik_token_cache_get_system_tokens(cache);
    total += ik_token_cache_get_tool_tokens(cache);
    for (size_t i = 0; i < cache->turn_count; i++) {
        total += ik_token_cache_get_turn_tokens(cache, i);
    }

    cache->total_tokens = total;
    return total;
}

/* --- Record --- */

void ik_token_cache_record_turn(ik_token_cache_t *cache,
                                size_t turn_index,
                                int32_t tokens)
{
    assert(cache != NULL); // LCOV_EXCL_BR_LINE
    if (turn_index >= cache->turn_count) PANIC("turn_index out of range");

    cache->turn_tokens[turn_index] = tokens;
    cache->total_tokens = TOKEN_UNCACHED;
}

/* --- Invalidation --- */

void ik_token_cache_invalidate_all(ik_token_cache_t *cache)
{
    assert(cache != NULL); // LCOV_EXCL_BR_LINE
    cache->system_tokens = TOKEN_UNCACHED;
    cache->tool_tokens   = TOKEN_UNCACHED;
    cache->total_tokens  = TOKEN_UNCACHED;
    for (size_t i = 0; i < cache->turn_count; i++) {
        cache->turn_tokens[i] = TOKEN_UNCACHED;
    }
}

void ik_token_cache_reset(ik_token_cache_t *cache)
{
    assert(cache != NULL); // LCOV_EXCL_BR_LINE
    cache->system_tokens       = TOKEN_UNCACHED;
    cache->tool_tokens         = TOKEN_UNCACHED;
    cache->total_tokens        = TOKEN_UNCACHED;
    cache->turn_count          = 0;
    cache->context_start_index = 0;
    cache->pruned_turn_count   = 0;
}

void ik_token_cache_invalidate_system(ik_token_cache_t *cache)
{
    assert(cache != NULL); // LCOV_EXCL_BR_LINE
    cache->system_tokens = TOKEN_UNCACHED;
    cache->total_tokens  = TOKEN_UNCACHED;
}

void ik_token_cache_invalidate_tools(ik_token_cache_t *cache)
{
    assert(cache != NULL); // LCOV_EXCL_BR_LINE
    cache->tool_tokens  = TOKEN_UNCACHED;
    cache->total_tokens = TOKEN_UNCACHED;
}

/* --- Structural --- */

void ik_token_cache_prune_oldest_turn(ik_token_cache_t *cache)
{
    assert(cache != NULL); // LCOV_EXCL_BR_LINE

    if (cache->turn_count == 0) {
        return;
    }

    /* Subtract oldest turn from cached total if both are known */
    if (cache->total_tokens != TOKEN_UNCACHED &&
        cache->turn_tokens[0] != TOKEN_UNCACHED) {
        cache->total_tokens -= cache->turn_tokens[0];
        if (cache->total_tokens < 0) {
            cache->total_tokens = TOKEN_UNCACHED;
        }
    } else {
        cache->total_tokens = TOKEN_UNCACHED;
    }

    /* Advance context_start_index past oldest turn's messages */
    if (cache->agent != NULL) {
        size_t user_count = 0;
        bool found_second = false;
        for (size_t i = cache->context_start_index;
             i < cache->agent->message_count; i++) {
            if (cache->agent->messages[i]->role == IK_ROLE_USER) {
                user_count++;
                if (user_count == 2) {
                    cache->context_start_index = i;
                    found_second = true;
                    break;
                }
            }
        }
        if (!found_second) {
            cache->context_start_index = cache->agent->message_count;
        }
    }

    cache->pruned_turn_count++;

    /* Remove first turn entry (shift left) */
    cache->turn_count--;
    if (cache->turn_count > 0) {
        memmove(&cache->turn_tokens[0], &cache->turn_tokens[1],
                cache->turn_count * sizeof(int32_t));
    }
}

void ik_token_cache_add_turn(ik_token_cache_t *cache)
{
    assert(cache != NULL); // LCOV_EXCL_BR_LINE

    /* Grow array if needed */
    if (cache->turn_count >= cache->turn_capacity) {
        size_t new_cap = cache->turn_capacity > 0 ?
                         cache->turn_capacity * 2 : TURN_CAP_INITIAL;
        int32_t *new_arr = talloc_realloc(cache, cache->turn_tokens,
                                           int32_t, (unsigned int)new_cap);
        if (new_arr == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
        cache->turn_tokens  = new_arr;
        cache->turn_capacity = new_cap;
    }

    cache->turn_tokens[cache->turn_count] = TOKEN_UNCACHED;
    cache->turn_count++;
    cache->total_tokens = TOKEN_UNCACHED;
}

void ik_token_cache_remove_turns_from(ik_token_cache_t *cache, size_t turn_index)
{
    assert(cache != NULL); // LCOV_EXCL_BR_LINE
    if (turn_index > cache->turn_count) PANIC("turn_index out of range");

    if (turn_index == cache->turn_count) {
        return; /* no-op */
    }

    /* Update cached total — subtract known costs, invalidate on sentinel */
    for (size_t i = turn_index; i < cache->turn_count; i++) {
        if (cache->turn_tokens[i] == TOKEN_UNCACHED) {
            cache->total_tokens = TOKEN_UNCACHED;
            break;
        }
        if (cache->total_tokens != TOKEN_UNCACHED) {
            cache->total_tokens -= cache->turn_tokens[i];
            if (cache->total_tokens < 0) {
                cache->total_tokens = TOKEN_UNCACHED;
            }
        }
    }

    cache->turn_count = turn_index;

    if (cache->turn_count == 0 && cache->context_start_index > 0) {
        cache->context_start_index = 0;
        cache->pruned_turn_count   = 0;
    }
}

/* Introspection */

int32_t ik_token_cache_get_budget(const ik_token_cache_t *cache)
{
    assert(cache != NULL); // LCOV_EXCL_BR_LINE
    return cache->budget;
}

void ik_token_cache_set_budget(ik_token_cache_t *cache, int32_t budget)
{
    assert(cache != NULL); // LCOV_EXCL_BR_LINE
    cache->budget = budget;
}

size_t ik_token_cache_get_turn_count(const ik_token_cache_t *cache)
{
    assert(cache != NULL); // LCOV_EXCL_BR_LINE
    return cache->turn_count;
}

size_t ik_token_cache_get_context_start_turn(const ik_token_cache_t *cache)
{
    assert(cache != NULL); // LCOV_EXCL_BR_LINE
    return cache->pruned_turn_count;
}

size_t ik_token_cache_get_context_start_index(const ik_token_cache_t *cache)
{
    assert(cache != NULL); // LCOV_EXCL_BR_LINE
    return cache->context_start_index;
}

int32_t ik_token_cache_peek_total(const ik_token_cache_t *cache)
{
    assert(cache != NULL); // LCOV_EXCL_BR_LINE
    return (cache->total_tokens == TOKEN_UNCACHED) ? 0 : cache->total_tokens;
}

int32_t ik_token_cache_peek_turn_tokens(const ik_token_cache_t *cache,
                                         size_t turn_index)
{
    assert(cache != NULL); // LCOV_EXCL_BR_LINE
    if (turn_index >= cache->turn_count) PANIC("turn_index out of range"); // LCOV_EXCL_BR_LINE
    int32_t v = cache->turn_tokens[turn_index];
    return (v == TOKEN_UNCACHED) ? 0 : v;
}
