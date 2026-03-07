#ifndef IK_SUMMARY_WORKER_H
#define IK_SUMMARY_WORKER_H

#include "apps/ikigai/agent.h"
#include "apps/ikigai/msg.h"
#include "apps/ikigai/providers/provider.h"
#include <stddef.h>
#include <stdint.h>

/**
 * Arguments passed to the summary worker thread.
 *
 * All strings are owned by this struct (talloc children are NOT used here
 * since talloc is not thread-safe across thread boundaries). The model field
 * is a strdup copy freed after the thread joins.
 *
 * Exposed in header for direct unit-test invocation of the worker function.
 */
typedef struct {
    ik_msg_t * const *msgs;  /* Borrowed: agent owns messages; valid for thread lifetime */
    size_t            msg_count;
    ik_provider_t    *provider;   /* Borrowed: caller ensures lifetime */
    char             *model;      /* Owned: strdup copy, freed after join */
    int32_t           max_tokens;
    ik_agent_ctx_t   *agent;      /* Borrowed: caller ensures lifetime */
    uint32_t          generation; /* Generation counter at dispatch time */
} ik_summary_worker_args_t;

/**
 * Worker thread function: generates a summary and stores the result.
 *
 * Calls ik_summary_generate(), writes the result string (or NULL on failure)
 * to agent->summary_thread_result, records the generation in
 * agent->summary_thread_generation, then sets agent->summary_thread_complete
 * under the summary_thread_mutex.
 *
 * Result string is a NULL-parent talloc allocation (parentless), so the main
 * thread can talloc_steal() it onto the agent after joining.
 *
 * Exposed (non-static) so unit tests can call it directly without spawning a
 * real thread.
 *
 * @param arg Pointer to ik_summary_worker_args_t
 * @return NULL always
 */
void *ik_summary_worker_fn(void *arg);

/**
 * Dispatch the background summary thread.
 *
 * Increments agent->recent_summary_generation, allocates worker args,
 * and spawns agent->summary_thread. If a previous summary thread is still
 * running (summary_thread_running == true), dispatch is silently skipped.
 *
 * @param agent      Agent whose recent_summary to update
 * @param provider   Provider instance for the LLM call
 * @param model      Model identifier (copied internally)
 * @param msgs       Array of messages to summarize (borrowed, caller owns)
 * @param msg_count  Number of messages
 * @param max_tokens Maximum tokens for the generated summary
 */
void ik_summary_worker_dispatch(ik_agent_ctx_t *agent,
                                ik_provider_t  *provider,
                                const char     *model,
                                ik_msg_t * const *msgs,
                                size_t          msg_count,
                                int32_t         max_tokens);

/**
 * Poll for summary thread completion — call from main event loop tick.
 *
 * If summary_thread_complete is set, joins the thread and decides whether
 * to accept or discard the result based on the generation counter:
 *   - Accept: summary_thread_generation == recent_summary_generation
 *             → updates agent->recent_summary and recent_summary_tokens
 *   - Discard: generation mismatch (stale result from an old prune epoch)
 *
 * On LLM failure (summary_thread_result == NULL), logs an error and keeps
 * the previous agent->recent_summary intact.
 *
 * Always resets summary_thread_running, summary_thread_complete, and
 * summary_thread_result after processing.
 *
 * @param agent Agent to poll (must not be NULL)
 */
void ik_summary_worker_poll(ik_agent_ctx_t *agent);

#endif /* IK_SUMMARY_WORKER_H */
