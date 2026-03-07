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
 * When owns_provider is true, the provider is a fresh talloc allocation
 * (NULL parent) created exclusively for this thread; poll() will free it
 * after the thread joins.  When false (e.g. injected by unit tests), the
 * provider is borrowed and must not be freed by poll().
 *
 * Session summary fields (is_session_summary == true):
 *   - The result is stored to DB (not assigned to agent->recent_summary).
 *   - session_msgs_stubs is a malloc'd ik_msg_t[] whose kind/content are
 *     strdup copies; poll() frees each string then the array.
 *   - session_msgs_ptrs is a malloc'd ik_msg_t*[] (the msgs pointer array);
 *     poll() frees it after the stubs.
 *   - generation is unused; poll() always accepts the result.
 *
 * Exposed in header for direct unit-test invocation of the worker function.
 */
typedef struct {
    ik_msg_t * const *msgs;  /* Borrowed: agent owns messages; valid for thread lifetime */
    size_t msg_count;
    ik_provider_t *provider;      /* See owns_provider for lifetime semantics */
    bool owns_provider;           /* true → poll() must talloc_free(provider) after join */
    char *model;                  /* Owned: strdup copy, freed after join */
    int32_t max_tokens;
    ik_agent_ctx_t *agent;        /* Borrowed: caller ensures lifetime */
    uint32_t generation;          /* Generation counter at dispatch time */
    /* Session summary routing (set by ik_session_summary_dispatch) */
    bool is_session_summary;      /* true → poll stores result to DB */
    int64_t start_msg_id;         /* First message ID in epoch (DB insert) */
    int64_t end_msg_id;           /* Last message ID in epoch (DB insert) */
    void *session_msgs_stubs;     /* malloc'd ik_msg_t[] with owned strings, freed by poll */
    void *session_msgs_ptrs;      /* malloc'd ik_msg_t*[], freed by poll */
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
 * Increments agent->recent_summary_generation, creates a fresh independent
 * provider instance (via ik_provider_create with NULL parent) from
 * agent->provider, allocates worker args, and spawns agent->summary_thread.
 * If a previous summary thread is still running (summary_thread_running ==
 * true), or if provider creation fails, dispatch is silently skipped.
 *
 * The created provider is owned by the worker args (owns_provider = true) and
 * freed by poll() after the thread joins — the agent's cached provider_instance
 * is never touched by the worker thread.
 *
 * @param agent      Agent whose recent_summary to update
 * @param model      Model identifier (copied internally)
 * @param msgs       Array of messages to summarize (borrowed, caller owns)
 * @param msg_count  Number of messages
 * @param max_tokens Maximum tokens for the generated summary
 */
void ik_summary_worker_dispatch(ik_agent_ctx_t *agent,
                                const char *model,
                                ik_msg_t * const *msgs,
                                size_t msg_count,
                                int32_t max_tokens);

/**
 * Dispatch a background session summary thread for /clear.
 *
 * Non-blocking: returns immediately after spawning the thread.  The thread
 * generates the summary text and stores the result; poll() handles the DB
 * insert and reloads agent->session_summaries on completion.
 *
 * Ownership of session_msgs_stubs and session_msgs_ptrs is transferred to
 * this function.  On dispatch failure (thread already running, provider
 * creation failure, OOM), the stubs are freed internally.  On success, they
 * are freed by poll() after the thread joins.
 *
 * @param agent               Agent context (must not be NULL)
 * @param model               Model identifier (copied internally)
 * @param msgs                Pointer array built from session_msgs_stubs (transferred)
 * @param msg_count           Number of messages
 * @param session_msgs_stubs  malloc'd ik_msg_t[] with strdup'd strings (transferred)
 * @param session_msgs_ptrs   malloc'd ik_msg_t*[] (transferred)
 * @param max_tokens          Maximum tokens for the generated summary
 * @param start_msg_id        First message ID in epoch (for DB insert)
 * @param end_msg_id          Last message ID in epoch (for DB insert)
 */
void ik_session_summary_dispatch(ik_agent_ctx_t *agent,
                                 const char *model,
                                 ik_msg_t * const *msgs,
                                 size_t msg_count,
                                 void *session_msgs_stubs,
                                 void *session_msgs_ptrs,
                                 int32_t max_tokens,
                                 int64_t start_msg_id,
                                 int64_t end_msg_id);

/**
 * Poll for summary thread completion — call from main event loop tick.
 *
 * If summary_thread_complete is set, joins the thread and decides whether
 * to accept or discard the result based on the generation counter:
 *   - Accept: summary_thread_generation == recent_summary_generation
 *             → updates agent->recent_summary and recent_summary_tokens
 *   - Discard: generation mismatch (stale result from an old prune epoch)
 *
 * For session summary threads (is_session_summary == true), the generation
 * check is skipped; the result is always stored to DB and
 * agent->session_summaries is reloaded.
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
