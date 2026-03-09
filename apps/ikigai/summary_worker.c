#include "apps/ikigai/summary_worker.h"

#include "apps/ikigai/agent.h"
#include "apps/ikigai/db/session_summary.h"
#include "apps/ikigai/providers/factory.h"
#include "apps/ikigai/shared.h"
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

    if (is_ok(&res) && summary != NULL) { // LCOV_EXCL_BR_LINE
        /* Detach the result from the temporary context so the main thread
         * can talloc_steal() it onto the agent after joining. */
        args->agent->summary_thread_result = talloc_steal(NULL, summary);
    } else {
        if (is_err(&res)) { // LCOV_EXCL_BR_LINE
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

/* Dispatch — spawn the summary thread if one is not already running. */
void ik_summary_worker_dispatch(ik_agent_ctx_t *agent,
                                const char *model,
                                ik_msg_t * const *msgs,
                                size_t msg_count,
                                int32_t max_tokens)
{
    assert(agent != NULL); // LCOV_EXCL_BR_LINE
    assert(model != NULL); // LCOV_EXCL_BR_LINE

    pthread_mutex_lock_(&agent->summary_thread_mutex);
    bool already_running = agent->summary_thread_running;
    pthread_mutex_unlock_(&agent->summary_thread_mutex);

    if (already_running) {
        return;
    }

    if (agent->provider == NULL || agent->provider[0] == '\0') { // LCOV_EXCL_BR_LINE
        return;
    }

    agent->recent_summary_generation++;

    /* Create an independent provider (parentless) for the worker thread. */
    TALLOC_CTX *prov_ctx = talloc_new(NULL);
    if (prov_ctx == NULL) return;    // LCOV_EXCL_LINE
    ik_provider_t *worker_provider = NULL;
    res_t pres = ik_provider_create(prov_ctx, agent->provider, &worker_provider);
    if (is_err(&pres)) {             // LCOV_EXCL_LINE
        talloc_free(prov_ctx);       // LCOV_EXCL_LINE
        return;                      // LCOV_EXCL_LINE
    }
    // LCOV_EXCL_START
    talloc_steal(NULL, worker_provider);
    talloc_free(prov_ctx);

    ik_summary_worker_args_t *args = malloc(sizeof(ik_summary_worker_args_t));
    if (args == NULL) {              // LCOV_EXCL_BR_LINE
        talloc_free(worker_provider);
        return;
    }

    args->msgs = msgs;
    args->msg_count = msg_count;
    args->provider = worker_provider;
    args->owns_provider = true;
    args->model = strdup(model);
    args->max_tokens = max_tokens;
    args->agent = agent;
    args->generation = agent->recent_summary_generation;
    args->is_session_summary = false;
    args->start_msg_id = 0;
    args->end_msg_id = 0;
    args->session_msgs_stubs = NULL;
    args->session_msgs_ptrs = NULL;

    if (args->model == NULL) {        // LCOV_EXCL_BR_LINE
        talloc_free(worker_provider);
        free(args);
        return;
    }

    pthread_mutex_lock_(&agent->summary_thread_mutex);
    agent->summary_thread_running = true;
    agent->summary_thread_complete = false;
    agent->summary_thread_result = NULL;
    pthread_mutex_unlock_(&agent->summary_thread_mutex);

    int ret = pthread_create_(&agent->summary_thread, NULL,
                              ik_summary_worker_fn, args);
    if (ret != 0) {                                          // LCOV_EXCL_BR_LINE
        pthread_mutex_lock_(&agent->summary_thread_mutex);
        agent->summary_thread_running = false;
        pthread_mutex_unlock_(&agent->summary_thread_mutex);
        talloc_free(worker_provider);
        free(args->model);
        free(args);
    }
    // LCOV_EXCL_STOP
}

/* Free session summary stubs (malloc'd ik_msg_t[] with owned strings). */
static void free_session_stubs(void *stubs, void *ptrs, size_t count)
{
    if (stubs != NULL) {
        ik_msg_t *s = (ik_msg_t *)stubs;
        for (size_t i = 0; i < count; i++) {
            free(s[i].kind);
            free(s[i].content);
        }
        free(stubs);
    }
    free(ptrs);
}

/* Session summary dispatch — spawn summary thread for /clear (non-blocking). */
void ik_session_summary_dispatch(ik_agent_ctx_t *agent,
                                 const char *model,
                                 ik_msg_t * const *msgs,
                                 size_t msg_count,
                                 void *session_msgs_stubs,
                                 void *session_msgs_ptrs,
                                 int32_t max_tokens,
                                 int64_t start_msg_id,
                                 int64_t end_msg_id)
{
    assert(agent != NULL); // LCOV_EXCL_BR_LINE
    assert(model != NULL); // LCOV_EXCL_BR_LINE

    pthread_mutex_lock_(&agent->summary_thread_mutex);
    bool already_running = agent->summary_thread_running;
    pthread_mutex_unlock_(&agent->summary_thread_mutex);

    if (already_running) { // LCOV_EXCL_BR_LINE
        free_session_stubs(session_msgs_stubs, session_msgs_ptrs, msg_count);
        return;
    }

    if (agent->provider == NULL || agent->provider[0] == '\0') { // LCOV_EXCL_BR_LINE
        free_session_stubs(session_msgs_stubs, session_msgs_ptrs, msg_count);
        return;
    }

    TALLOC_CTX *prov_ctx2 = talloc_new(NULL);
    if (prov_ctx2 == NULL) {                                                      // LCOV_EXCL_BR_LINE
        free_session_stubs(session_msgs_stubs, session_msgs_ptrs, msg_count);     // LCOV_EXCL_LINE
        return;                                                                   // LCOV_EXCL_LINE
    }
    ik_provider_t *worker_provider = NULL;
    res_t pres = ik_provider_create(prov_ctx2, agent->provider, &worker_provider);
    if (is_err(&pres)) {                                                          // LCOV_EXCL_LINE
        talloc_free(prov_ctx2);                                                   // LCOV_EXCL_LINE
        free_session_stubs(session_msgs_stubs, session_msgs_ptrs, msg_count);     // LCOV_EXCL_LINE
        return;                                                                   // LCOV_EXCL_LINE
    }
    // LCOV_EXCL_START
    talloc_steal(NULL, worker_provider);
    talloc_free(prov_ctx2);

    ik_summary_worker_args_t *args = malloc(sizeof(ik_summary_worker_args_t));
    if (args == NULL) {                                                           // LCOV_EXCL_BR_LINE
        talloc_free(worker_provider);
        free_session_stubs(session_msgs_stubs, session_msgs_ptrs, msg_count);
        return;
    }

    args->msgs = msgs;
    args->msg_count = msg_count;
    args->provider = worker_provider;
    args->owns_provider = true;
    args->model = strdup(model);
    args->max_tokens = max_tokens;
    args->agent = agent;
    args->generation = 0;
    args->is_session_summary = true;
    args->start_msg_id = start_msg_id;
    args->end_msg_id = end_msg_id;
    args->session_msgs_stubs = session_msgs_stubs;
    args->session_msgs_ptrs = session_msgs_ptrs;

    if (args->model == NULL) {                                                    // LCOV_EXCL_BR_LINE
        talloc_free(worker_provider);
        free_session_stubs(session_msgs_stubs, session_msgs_ptrs, msg_count);
        free(args);
        return;
    }

    pthread_mutex_lock_(&agent->summary_thread_mutex);
    agent->summary_thread_running = true;
    agent->summary_thread_complete = false;
    agent->summary_thread_result = NULL;
    pthread_mutex_unlock_(&agent->summary_thread_mutex);

    int ret = pthread_create_(&agent->summary_thread, NULL,
                              ik_summary_worker_fn, args);
    if (ret != 0) {                                                               // LCOV_EXCL_BR_LINE
        pthread_mutex_lock_(&agent->summary_thread_mutex);
        agent->summary_thread_running = false;
        pthread_mutex_unlock_(&agent->summary_thread_mutex);
        talloc_free(worker_provider);
        free(args->model);
        free_session_stubs(session_msgs_stubs, session_msgs_ptrs, msg_count);
        free(args);
    }
    // LCOV_EXCL_STOP
}

/* Handle session summary result: store to DB and reload agent state. */
static void poll_apply_session(ik_agent_ctx_t *agent,
                               int64_t start_id, int64_t end_id)
{
    if (agent->summary_thread_result == NULL) return; // LCOV_EXCL_BR_LINE
    if (agent->shared == NULL || agent->shared->db_ctx == NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(agent->summary_thread_result);
        agent->summary_thread_result = NULL;
        return;
    }
    char *text = agent->summary_thread_result;
    int32_t token_count = (int32_t)(strlen(text) / 4);
    res_t ins = ik_db_session_summary_insert(agent->shared->db_ctx,
                                             agent->uuid, text,
                                             start_id, end_id, token_count);
    if (is_err(&ins)) talloc_free(ins.err); // LCOV_EXCL_BR_LINE
    talloc_free(agent->summary_thread_result);
    agent->summary_thread_result = NULL;
    if (agent->session_summaries != NULL) { // LCOV_EXCL_BR_LINE
        talloc_free(agent->session_summaries);
        agent->session_summaries = NULL;
        agent->session_summary_count = 0;
    }
    res_t load = ik_db_session_summary_load(agent->shared->db_ctx,
                                            agent, agent->uuid,
                                            &agent->session_summaries,
                                            &agent->session_summary_count);
    if (is_err(&load)) talloc_free(load.err); // LCOV_EXCL_BR_LINE
}

/* Handle recent summary result: accept if generation matches, else discard. */
static void poll_apply_recent(ik_agent_ctx_t *agent)
{
    bool accept = (agent->summary_thread_generation ==
                   agent->recent_summary_generation);
    if (!accept) {
        if (agent->summary_thread_result != NULL) {
            talloc_free(agent->summary_thread_result);
        }
        return;
    }
    if (agent->summary_thread_result != NULL) { // LCOV_EXCL_BR_LINE
        if (agent->recent_summary != NULL) talloc_free(agent->recent_summary); // LCOV_EXCL_BR_LINE
        agent->recent_summary = talloc_steal(agent, agent->summary_thread_result);
        agent->recent_summary_tokens =
            (int32_t)(strlen(agent->recent_summary) / 4);
    } else {
        yyjson_mut_doc *doc = ik_log_create();                                   // LCOV_EXCL_LINE
        yyjson_mut_val *root = yyjson_mut_doc_get_root(doc);                     // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(doc, root, "event",                               // LCOV_EXCL_LINE
                               "summary_worker_poll_failure");                    // LCOV_EXCL_LINE
        ik_log_warn_json(doc);                                                    // LCOV_EXCL_LINE
    }
}

/* Poll — harvest summary thread result from main event loop. */
void ik_summary_worker_poll(ik_agent_ctx_t *agent)
{
    assert(agent != NULL); // LCOV_EXCL_BR_LINE

    pthread_mutex_lock_(&agent->summary_thread_mutex);
    bool complete = agent->summary_thread_complete;
    pthread_mutex_unlock_(&agent->summary_thread_mutex);

    if (!complete) return;

    void *thread_ret = NULL;
    pthread_join_(agent->summary_thread, &thread_ret);
    ik_summary_worker_args_t *args = (ik_summary_worker_args_t *)thread_ret;

    bool is_session = false;
    int64_t start_id = 0, end_id = 0;
    size_t session_msg_count = 0;
    void *session_stubs = NULL, *session_ptrs = NULL;

    if (args != NULL) { // LCOV_EXCL_BR_LINE
        is_session        = args->is_session_summary;
        start_id          = args->start_msg_id;
        end_id            = args->end_msg_id;
        session_msg_count = args->msg_count;
        session_stubs     = args->session_msgs_stubs;
        session_ptrs      = args->session_msgs_ptrs;
        if (args->owns_provider && args->provider != NULL) // LCOV_EXCL_BR_LINE
            talloc_free(args->provider);
        free(args->model);
        free(args);
    }

    free_session_stubs(session_stubs, session_ptrs, session_msg_count);
    free(agent->summary_msgs_stubs);
    agent->summary_msgs_stubs = NULL;
    free(agent->summary_msgs_ptrs);
    agent->summary_msgs_ptrs = NULL;

    if (is_session) poll_apply_session(agent, start_id, end_id);
    else            poll_apply_recent(agent);

    pthread_mutex_lock_(&agent->summary_thread_mutex);
    agent->summary_thread_running = false;
    agent->summary_thread_complete = false;
    agent->summary_thread_result = NULL;
    pthread_mutex_unlock_(&agent->summary_thread_mutex);
}
