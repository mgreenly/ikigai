#pragma once

#include "apps/ikigai/tool.h"
#include "apps/ikigai/tool_registry.h"
#include "shared/error.h"

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>
#include <talloc.h>

// Forward declarations
typedef struct ik_agent_ctx ik_agent_ctx_t;

// ---------------------------------------------------------------------------
// Access classification
// ---------------------------------------------------------------------------

// Granularity of filesystem / resource access for a single tool call.
typedef enum {
    IK_ACCESS_NONE,      // No access to this class of resource
    IK_ACCESS_PATHS,     // Specific named paths only
    IK_ACCESS_WILDCARD,  // Accesses any/all paths (e.g. bash, glob, grep)
} ik_access_mode_t;

// Resource access descriptor derived from tool name and arguments.
// Path arrays are talloc'd on the same context as the scheduler; the
// embedded char* pointers borrow that storage.
typedef struct {
    ik_access_mode_t  read_mode;
    ik_access_mode_t  write_mode;
    char            **read_paths;       // NULL when mode is NONE or WILDCARD
    int32_t           read_path_count;
    char            **write_paths;      // NULL when mode is NONE or WILDCARD
    int32_t           write_path_count;
} ik_access_t;

// ---------------------------------------------------------------------------
// Scheduler entry lifecycle
// ---------------------------------------------------------------------------

typedef enum {
    IK_SCHEDULE_QUEUED,     // Waiting for blockers to resolve
    IK_SCHEDULE_RUNNING,    // Executing in a worker thread
    IK_SCHEDULE_COMPLETED,  // Finished successfully; result is set
    IK_SCHEDULE_ERRORED,    // Failed; error is set
    IK_SCHEDULE_SKIPPED,    // Cascading failure from a blocked-by entry
} ik_schedule_status_t;

// One entry in the scheduler queue — corresponds to one tool call.
typedef struct {
    // Tool call (owned by this entry via scheduler talloc context)
    ik_tool_call_t       *tool_call;

    // Lifecycle state
    ik_schedule_status_t  status;

    // Resource access descriptor
    ik_access_t           access;

    // Dependency indices: indices into ik_tool_scheduler_t.entries[]
    // that this entry must wait for before it can run.
    int32_t              *blocked_by;        // talloc'd int32_t array
    int32_t               blocked_by_count;

    // Worker thread state (mutex guards thread_complete, result, error, child_pid)
    pthread_t             thread;
    pthread_mutex_t       mutex;
    bool                  thread_started;    // true after pthread_create succeeds
    bool                  thread_complete;   // set by worker thread on finish

    // Per-entry talloc context for thread-owned memory (result/error strings)
    TALLOC_CTX           *thread_ctx;

    // Results written by the worker thread
    char                 *result;            // JSON result (success path)
    char                 *error;             // Error message (error path)

    // Thought signature for Gemini 3 tool calls (NULL for other providers)
    // talloc'd on the scheduler context; populated after stream completes.
    char                 *thought_signature;

    // Child process PID, for interrupt support
    pid_t                 child_pid;

    // Completion hook and deferred data for internal tools (NULL for external)
    ik_tool_complete_fn   on_complete;    // Copied from registry at dispatch time
    void                 *deferred_data;  // Captured from agent->tool_deferred_data after handler
} ik_schedule_entry_t;

// ---------------------------------------------------------------------------
// Scheduler
// ---------------------------------------------------------------------------

// Ordered queue of tool calls for one LLM response turn.
// Entries are processed in arrival order; dependency indices only point
// backward, so there are no cycles.
typedef struct {
    // Flat entry array (value-typed, grown with talloc_realloc)
    ik_schedule_entry_t *entries;
    int32_t              count;
    int32_t              capacity;

    // Set true when the LLM streaming is complete (message_stop received)
    bool                 stream_complete;

    // Set true for dry-run replay: start_entry shows status but skips thread creation
    bool                 replay_mode;

    // Guards structural changes to the entries array
    pthread_mutex_t      mutex;

    // Borrowed reference to the owning agent (not freed by scheduler)
    ik_agent_ctx_t      *agent;
} ik_tool_scheduler_t;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

// Create a new empty scheduler.
// ctx:   talloc parent context (scheduler is a talloc child).
// agent: borrowed reference for tool execution context.
// Returns the new scheduler, or NULL on allocation failure.
ik_tool_scheduler_t *ik_tool_scheduler_create(TALLOC_CTX *ctx, ik_agent_ctx_t *agent);

// Destroy scheduler and all owned resources.
// Equivalent to talloc_free(sched).
void ik_tool_scheduler_destroy(ik_tool_scheduler_t *sched);

// Derive an access descriptor from a tool name and its JSON arguments.
// Returned path strings are talloc'd on ctx.
// Internal tools (fork/kill/send/wait/skill) are classified WILDCARD/WILDCARD
// so they are treated as full barriers — the caller can skip them separately.
ik_access_t ik_tool_scheduler_classify(TALLOC_CTX *ctx, const char *tool_name,
                                        const char *args_json);

// Return true if access descriptors a and b conflict with each other.
// Conflict means the two tool calls cannot safely run concurrently.
bool ik_tool_scheduler_conflicts(const ik_access_t *a, const ik_access_t *b);

// Add a tool call to the scheduler.
// Classifies its access, scans backward for conflicts, records blockers,
// and starts execution immediately if unblocked.
// tool_call is reparented to the scheduler context.
// Returns OK(NULL) on success, ERR on allocation failure.
res_t ik_tool_scheduler_add(ik_tool_scheduler_t *sched, ik_tool_call_t *tool_call);

// Poll all running entries for worker-thread completion.
// For each finished thread: joins it, marks completed or errored,
// cascades skips, then runs a promotion pass.
// Called from the main event loop on each tick.
void ik_tool_scheduler_poll(ik_tool_scheduler_t *sched);

// Promote all queued entries whose blockers are all in terminal state
// to running by spawning worker threads.
// Called after ik_tool_scheduler_add() and after any entry transitions.
void ik_tool_scheduler_promote(ik_tool_scheduler_t *sched);

// Mark entry[index] as completed with the given result string.
// result is talloc'd on sched; entry takes ownership.
// Cascades and promotes after marking.
void ik_tool_scheduler_on_complete(ik_tool_scheduler_t *sched, int32_t index,
                                    char *result);

// Mark entry[index] as errored with the given message.
// error is copied into the entry (owned by entry->thread_ctx).
// Cascades skips to all direct and transitive dependents, then promotes.
void ik_tool_scheduler_on_error(ik_tool_scheduler_t *sched, int32_t index,
                                 const char *error_msg);

// Return true when every entry is in a terminal state (completed/errored/skipped)
// AND stream_complete has been set.
bool ik_tool_scheduler_all_terminal(const ik_tool_scheduler_t *sched);

// Emit display lines and start execution for all queued entries.
// Call this after rendering usage in the completion callback so that
// tool lifecycle lines (→ input, ▶ Running, ◇ Blocked) appear after the
// usage line rather than interleaved with it during streaming.
void ik_tool_scheduler_begin(ik_tool_scheduler_t *sched);

// Invoke on_complete hooks for all entries that have one.
// deferred_data_ptr must point to agent->tool_deferred_data so each hook
// sees the data its handler deposited. Called from the main thread.
void ik_tool_scheduler_call_on_complete_hooks(ik_tool_scheduler_t *sched,
                                               ik_repl_ctx_t *repl,
                                               ik_agent_ctx_t *agent,
                                               void **deferred_data_ptr);

// Return true for any terminal state (completed / errored / skipped).
// Inlined here for use by both tool_scheduler.c and tool_scheduler_exec.c.
static inline bool ik_schedule_is_terminal(ik_schedule_status_t status)
{
    return status == IK_SCHEDULE_COMPLETED ||
           status == IK_SCHEDULE_ERRORED   ||
           status == IK_SCHEDULE_SKIPPED;
}
