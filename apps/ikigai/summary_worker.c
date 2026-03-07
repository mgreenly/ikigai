#include "apps/ikigai/summary_worker.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/summary.h"
#include "shared/error.h"
#include "shared/logger.h"
#include "apps/ikigai/wrapper_pthread.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

/*
 * Worker thread — executes ik_summary_generate() synchronously and stores
 * the result on the agent.  Exposed non-static so unit tests can call it
 * directly without spawning a real thread.
 */
void *ik_summary_worker_fn(void *arg)
{
    ik_summary_worker_args_t *args = (ik_summary_worker_args_t *)arg;

    /* Use a temporary NULL-parent talloc context so the thread's allocations
     * are isolated from the main thread's talloc tree.  After the call we
     * steal the result to a parentless allocation and free the rest. */
    TALLOC_CTX *tmp = talloc_new(NULL);
    char *summary = NULL;
    res_t res = ik_summary_generate(tmp,
                                    args->msgs,
                                    args->msg_count,
                                    args->provider,
                                    args->model,
                                    args->max_tokens,
                                    &summary);

    /* Record generation before signalling completion */
    args->agent->summary_thread_generation = args->generation;

    if (is_ok(&res) && summary != NULL) {
        /* Detach the result from the temporary context so the main thread
         * can talloc_steal() it onto the agent after joining. */
        args->agent->summary_thread_result = talloc_steal(NULL, summary);
    } else {
        if (is_err(&res)) {
            yyjson_mut_doc *doc = ik_log_create();                            // LCOV_EXCL_LINE
            yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);              // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(doc, root, "event", "summary_error");      // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(doc, root, "error",                        // LCOV_EXCL_LINE
                                   error_message(res.err));                    // LCOV_EXCL_LINE
            ik_log_warn_json(doc);                                             // LCOV_EXCL_LINE
            talloc_free(res.err);                                              // LCOV_EXCL_LINE
        }
        args->agent->summary_thread_result = NULL;
    }

    talloc_free(tmp);

    pthread_mutex_lock_(&args->agent->summary_thread_mutex);
    args->agent->summary_thread_complete = true;
    pthread_mutex_unlock_(&args->agent->summary_thread_mutex);

    /* Return args so poll() can free the model string and struct. */
    return args;
}

/*
 * Dispatch — spawn the summary thread if one is not already running.
 */
void ik_summary_worker_dispatch(ik_agent_ctx_t *agent,
                                ik_provider_t *provider,
                                const char *model,
                                ik_msg_t * const *msgs,
                                size_t msg_count,
                                int32_t max_tokens)
{
    assert(agent != NULL);    // LCOV_EXCL_BR_LINE
    assert(provider != NULL); // LCOV_EXCL_BR_LINE
    assert(model != NULL);    // LCOV_EXCL_BR_LINE

    pthread_mutex_lock_(&agent->summary_thread_mutex);
    bool already_running = agent->summary_thread_running;
    pthread_mutex_unlock_(&agent->summary_thread_mutex);

    if (already_running) {
        return;
    }

    agent->recent_summary_generation++;

    ik_summary_worker_args_t *args = malloc(sizeof(ik_summary_worker_args_t));
    if (args == NULL) return; // LCOV_EXCL_LINE

    args->msgs = msgs;
    args->msg_count = msg_count;
    args->provider = provider;
    args->model = strdup(model);
    args->max_tokens = max_tokens;
    args->agent = agent;
    args->generation = agent->recent_summary_generation;

    if (args->model == NULL) { // LCOV_EXCL_BR_LINE
        free(args);            // LCOV_EXCL_LINE
        return;                // LCOV_EXCL_LINE
    }

    pthread_mutex_lock_(&agent->summary_thread_mutex);
    agent->summary_thread_running = true;
    agent->summary_thread_complete = false;
    agent->summary_thread_result = NULL;
    pthread_mutex_unlock_(&agent->summary_thread_mutex);

    int ret = pthread_create_(&agent->summary_thread, NULL,
                              ik_summary_worker_fn, args);
    if (ret != 0) {                                         // LCOV_EXCL_BR_LINE
        pthread_mutex_lock_(&agent->summary_thread_mutex); // LCOV_EXCL_LINE
        agent->summary_thread_running = false;             // LCOV_EXCL_LINE
        pthread_mutex_unlock_(&agent->summary_thread_mutex); // LCOV_EXCL_LINE
        free(args->model);                                 // LCOV_EXCL_LINE
        free(args);                                        // LCOV_EXCL_LINE
    }
}

/*
 * Poll — harvest summary thread result from main event loop.
 */
void ik_summary_worker_poll(ik_agent_ctx_t *agent)
{
    assert(agent != NULL); // LCOV_EXCL_BR_LINE

    pthread_mutex_lock_(&agent->summary_thread_mutex);
    bool complete = agent->summary_thread_complete;
    pthread_mutex_unlock_(&agent->summary_thread_mutex);

    if (!complete) {
        return;
    }

    /* Recover args pointer returned by the worker to free the model string
     * and the struct itself (both allocated with malloc in dispatch). */
    void *thread_ret = NULL;
    pthread_join_(agent->summary_thread, &thread_ret);
    ik_summary_worker_args_t *args = (ik_summary_worker_args_t *)thread_ret;
    if (args != NULL) {
        free(args->model);
        free(args);
    }

    /* Free owned message snapshot buffers set by prune wiring (NULL-safe). */
    free(agent->summary_msgs_stubs);
    agent->summary_msgs_stubs = NULL;
    free(agent->summary_msgs_ptrs);
    agent->summary_msgs_ptrs = NULL;

    bool accept = (agent->summary_thread_generation ==
                   agent->recent_summary_generation);

    if (accept) {
        if (agent->summary_thread_result != NULL) {
            if (agent->recent_summary != NULL) {
                talloc_free(agent->recent_summary);
            }
            agent->recent_summary = talloc_steal(agent,
                                                 agent->summary_thread_result);
            agent->recent_summary_tokens =
                (int32_t)(strlen(agent->recent_summary) / 4);
        } else {
            /* LLM call failed — keep previous summary */
            yyjson_mut_doc *doc = ik_log_create();                           // LCOV_EXCL_LINE
            yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);             // LCOV_EXCL_LINE
            yyjson_mut_obj_add_str(doc, root, "event",                       // LCOV_EXCL_LINE
                                   "summary_worker_poll_failure");            // LCOV_EXCL_LINE
            ik_log_warn_json(doc);                                            // LCOV_EXCL_LINE
        }
    } else {
        /* Stale result — discard */
        if (agent->summary_thread_result != NULL) {
            talloc_free(agent->summary_thread_result);
        }
    }

    pthread_mutex_lock_(&agent->summary_thread_mutex);
    agent->summary_thread_running = false;
    agent->summary_thread_complete = false;
    agent->summary_thread_result = NULL;
    pthread_mutex_unlock_(&agent->summary_thread_mutex);
}
