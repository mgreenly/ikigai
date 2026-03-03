/**
 * @file token_cache.h
 * @brief Token count cache for sliding context window
 *
 * Tracks per-component token costs (system prompt, tools, turns) with
 * two-layer caching: per-component values and a cached total.
 *
 * Sentinel value: -1 indicates uncached.
 * API results are cached; bytes-estimate fallback values are not.
 * Single-threaded, main-thread-only. No thread safety.
 */

#ifndef IK_TOKEN_CACHE_H
#define IK_TOKEN_CACHE_H

#include <stddef.h>
#include <stdint.h>
#include <talloc.h>

typedef struct ik_agent_ctx ik_agent_ctx_t;
typedef struct ik_token_cache ik_token_cache_t;

/* ================================================================
 * Lifecycle
 * ================================================================ */

/**
 * Create token cache for an agent.
 *
 * The cache holds a non-owning reference to agent. The agent must
 * outlive the cache.
 *
 * @param ctx   Talloc parent
 * @param agent Agent context (must not be NULL)
 * @return      Allocated cache (never NULL, panics on OOM)
 */
ik_token_cache_t *ik_token_cache_create(TALLOC_CTX *ctx, ik_agent_ctx_t *agent);

/**
 * Clone a token cache for a new agent (fork semantics).
 *
 * All cached values are copied. The clone references new_agent,
 * not the source agent.
 *
 * @param ctx       Talloc parent
 * @param src       Source cache (must not be NULL)
 * @param new_agent New agent for the clone (must not be NULL)
 * @return          Allocated clone (never NULL, panics on OOM)
 */
ik_token_cache_t *ik_token_cache_clone(TALLOC_CTX *ctx,
                                        const ik_token_cache_t *src,
                                        ik_agent_ctx_t *new_agent);

/* ================================================================
 * Getters — return cached value or compute (bytes fallback on API failure)
 * ================================================================ */

/**
 * Get total context token count.
 *
 * Returns cached total or recomputes as sum of all per-component values.
 * The total is cached regardless of whether components used API or bytes
 * estimates.
 */
int32_t ik_token_cache_get_total(ik_token_cache_t *cache);

/**
 * Get system prompt token count.
 *
 * Returns cached value or calls provider count_tokens. On API failure,
 * returns bytes estimate but does NOT cache it (retries API next call).
 */
int32_t ik_token_cache_get_system_tokens(ik_token_cache_t *cache);

/**
 * Get tool definitions token count.
 *
 * Returns cached value or calls provider count_tokens. On API failure,
 * returns bytes estimate but does NOT cache it.
 */
int32_t ik_token_cache_get_tool_tokens(ik_token_cache_t *cache);

/**
 * Get per-turn token count.
 *
 * Returns cached value or calls provider count_tokens for the turn's
 * messages. On API failure, returns bytes estimate but does NOT cache it.
 *
 * PANIC if turn_index >= turn_count (caller bug).
 *
 * @param cache      Token cache
 * @param turn_index Zero-based turn index
 */
int32_t ik_token_cache_get_turn_tokens(ik_token_cache_t *cache, size_t turn_index);

/* ================================================================
 * Record — store a known token count after turn completion
 * ================================================================ */

/**
 * Record a known token count for a turn.
 *
 * Called after turn completion when response_input_tokens delta is known.
 * Invalidates cached total.
 *
 * PANIC if turn_index >= turn_count.
 *
 * @param cache      Token cache
 * @param turn_index Zero-based turn index
 * @param tokens     Token count to store
 */
void ik_token_cache_record_turn(ik_token_cache_t *cache,
                                size_t turn_index,
                                int32_t tokens);

/* ================================================================
 * Invalidation — clear cached values
 * ================================================================ */

/** Clear all cached values (system, tools, all turns, total). */
void ik_token_cache_invalidate_all(ik_token_cache_t *cache);

/**
 * Full reset: clear all cached values, empty turn array, reset context window.
 *
 * After reset: turn_count=0, context_start_index=0, pruned_turn_count=0,
 * all cached values invalidated. Called by /clear to start fresh.
 */
void ik_token_cache_reset(ik_token_cache_t *cache);

/** Clear system prompt cache and total. */
void ik_token_cache_invalidate_system(ik_token_cache_t *cache);

/** Clear tool definitions cache and total. */
void ik_token_cache_invalidate_tools(ik_token_cache_t *cache);

/* ================================================================
 * Structural — update cache when turns are added or removed
 * ================================================================ */

/**
 * Remove oldest turn from the cache.
 *
 * Advances the context start index past the oldest turn's messages.
 * Subtracts the pruned turn's cost from the cached total (if both
 * are known), otherwise invalidates total.
 * Shifts the turn_tokens array left by one.
 *
 * No-op if turn_count == 0.
 */
void ik_token_cache_prune_oldest_turn(ik_token_cache_t *cache);

/**
 * Grow turn array by one uncached entry.
 *
 * Called when a new user message (turn boundary) is added to the agent.
 * The new entry is initialized to -1 (uncached).
 */
void ik_token_cache_add_turn(ik_token_cache_t *cache);

/**
 * Remove all turns from turn_index onward (inclusive).
 * Subtracts their cached costs from the total.
 * Asserts turn_index <= turn_count. If turn_index == turn_count, this is a no-op.
 * If a removed turn had sentinel value (-1), the cached total is invalidated.
 */
void ik_token_cache_remove_turns_from(ik_token_cache_t *cache, size_t turn_index);

/* ================================================================
 * Introspection — read-only access to cache internals
 * Used by control socket read_token_cache command.
 * ================================================================ */

/** Get configured token budget. Default 100000. */
int32_t ik_token_cache_get_budget(const ik_token_cache_t *cache);

/** Set configured token budget. */
void ik_token_cache_set_budget(ik_token_cache_t *cache, int32_t budget);

/** Get number of active turns currently in cache. */
size_t ik_token_cache_get_turn_count(const ik_token_cache_t *cache);

/**
 * Get absolute turn index of the oldest active turn.
 *
 * Equals the number of turns that have been pruned. Used by the
 * read_token_cache command to report per-turn absolute indices.
 */
size_t ik_token_cache_get_context_start_turn(const ik_token_cache_t *cache);

/**
 * Get message index of the oldest message still in active context.
 *
 * Returns the index into agent->messages[] of the first message that
 * is still included when building requests to the LLM. Returns 0 when
 * no pruning has occurred. Used by the scrollback renderer to position
 * the horizontal rule.
 */
size_t ik_token_cache_get_context_start_index(const ik_token_cache_t *cache);

/**
 * Peek at cached total without triggering computation.
 *
 * Returns the cached total, or 0 if uncached.
 */
int32_t ik_token_cache_peek_total(const ik_token_cache_t *cache);

/**
 * Peek at cached turn token count without triggering computation.
 *
 * Returns the cached value for turn_index, or 0 if uncached.
 * PANIC if turn_index >= turn_count.
 */
int32_t ik_token_cache_peek_turn_tokens(const ik_token_cache_t *cache,
                                         size_t turn_index);

#endif /* IK_TOKEN_CACHE_H */
