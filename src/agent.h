#pragma once

#include "error.h"
#include "layer.h"
#include "layer_wrappers.h"
#include "scrollback.h"
#include "tool.h"

#include <talloc.h>
#include <stdbool.h>
#include <pthread.h>

// Forward declarations
typedef struct ik_shared_ctx ik_shared_ctx_t;
typedef struct ik_input_buffer_t ik_input_buffer_t;
typedef struct ik_message ik_message_t;
struct ik_openai_multi;
struct ik_repl_ctx_t;

// Agent state machine
typedef enum {
    IK_AGENT_STATE_IDLE,              // Normal input mode
    IK_AGENT_STATE_WAITING_FOR_LLM,   // Waiting for LLM response (spinner visible)
    IK_AGENT_STATE_EXECUTING_TOOL     // Tool running in background thread
} ik_agent_state_t;

// Mark structure for conversation checkpoints
typedef struct {
    size_t message_index;     // Position in conversation at time of mark
    char *label;              // Optional user label (or NULL for unlabeled mark)
    char *timestamp;          // ISO 8601 timestamp
} ik_mark_t;

/**
 * Per-agent context for ikigai.
 *
 * Contains all state specific to one agent:
 * - Identity (agent_id, numeric_id)
 * - Display state (scrollback, layers)
 * - Input state (input_buffer, visibility)
 * - Conversation (messages, marks)
 * - LLM interaction (curl_multi, streaming buffers)
 * - Tool execution (thread, mutex, pending calls)
 * - UI state (spinner, completion)
 *
 * Ownership: Created as child of ik_repl_ctx_t.
 * Currently single agent per REPL. Multi-agent support
 * will add agents[] array (manual-top-level-agents work).
 *
 * Thread safety: Tool execution uses mutex for thread safety.
 * Agent fields should only be accessed from main thread
 * except where explicitly synchronized.
 */
typedef struct ik_agent_ctx {
    // Identity (from agent-process-model.md)
    char *uuid;          // Internal unique identifier
    char *name;          // Optional human-friendly name (NULL if unnamed)
    char *parent_uuid;   // Parent agent's UUID (NULL for root agent)
    int64_t created_at;       // Unix timestamp when agent was created
    int64_t fork_message_id;  // Message ID at which this agent forked (0 for root)

    // Provider configuration (from agent-provider-fields.md)
    char *provider;                       // LLM provider name ("anthropic", "openai", "google")
    char *model;                          // Model identifier (e.g., "gpt-4", "claude-sonnet-4-5")
    int thinking_level;                   // Thinking/reasoning level (ik_thinking_level_t from provider.h)
    struct ik_provider *provider_instance; // Cached provider instance (lazy-loaded)

    // Reference to shared infrastructure
    ik_shared_ctx_t *shared;

    // Backpointer to REPL context (set after agent creation)
    struct ik_repl_ctx_t *repl;

    // Display state (per-agent)
    ik_scrollback_t *scrollback;
    ik_layer_cake_t *layer_cake;
    ik_layer_t *scrollback_layer;
    ik_layer_t *spinner_layer;
    ik_layer_t *separator_layer;
    ik_layer_t *input_layer;
    ik_layer_t *completion_layer;

    // Viewport state
    size_t viewport_offset;

    // Spinner state (per-agent)
    ik_spinner_state_t spinner_state;

    // Input state (per-agent - preserves partial composition)
    ik_input_buffer_t *input_buffer;

    // Tab completion state (per-agent)
    ik_completion_t *completion;

    // Conversation state (per-agent)
    ik_message_t **messages;      // Array of message pointers
    size_t message_count;         // Number of messages
    size_t message_capacity;      // Allocated capacity
    ik_mark_t **marks;
    size_t mark_count;

    // LLM interaction state (per-agent)
    int curl_still_running;
    ik_agent_state_t state;
    char *assistant_response;
    char *streaming_line_buffer;
    char *http_error_message;
    char *response_model;
    char *response_finish_reason;
    int32_t response_input_tokens;
    int32_t response_output_tokens;
    int32_t response_thinking_tokens;

    // Layer reference fields (updated before each render)
    bool separator_visible;
    bool input_buffer_visible;
    const char *input_text;
    size_t input_text_len;

    // Pending thinking blocks from response (for tool call messages)
    char *pending_thinking_text;
    char *pending_thinking_signature;
    char *pending_redacted_data;

    // Tool execution state (per-agent)
    ik_tool_call_t *pending_tool_call;
    pthread_t tool_thread;
    pthread_mutex_t tool_thread_mutex;
    bool tool_thread_running;
    bool tool_thread_complete;
    TALLOC_CTX *tool_thread_ctx;
    char *tool_thread_result;
    int32_t tool_iteration_count;

    // Pinned documents for system prompt (per-agent)
    char **pinned_paths;      // Ordered list of paths (FIFO)
    size_t pinned_count;      // Number of pinned paths
} ik_agent_ctx_t;

// Create agent context
// ctx: talloc parent (repl_ctx)
// shared: shared infrastructure
// parent_uuid: parent agent's UUID (NULL for root agent)
// out: receives allocated agent context
res_t ik_agent_create(TALLOC_CTX *ctx, ik_shared_ctx_t *shared, const char *parent_uuid, ik_agent_ctx_t **out);

/**
 * Restore agent context from database row
 *
 * Creates an agent context populated with data from a DB row.
 * Used during startup to restore agents that were running when
 * the process last exited.
 *
 * Unlike ik_agent_create():
 * - Uses row->uuid instead of generating new UUID
 * - Sets fork_message_id from row
 * - Sets created_at from row
 * - Sets name from row (if present)
 * - Sets parent_uuid from row
 * - Does NOT register agent in database (already exists)
 *
 * @param ctx Talloc parent (repl_ctx)
 * @param shared Shared infrastructure
 * @param row Database row with agent data (must not be NULL)
 * @param out Receives allocated agent context
 * @return OK on success, ERR on failure
 */
res_t ik_agent_restore(TALLOC_CTX *ctx, ik_shared_ctx_t *shared, const void *row, ik_agent_ctx_t **out);

/**
 * Copy conversation from one agent to another
 *
 * Copies the in-memory conversation array from parent to child.
 * Used during fork to give the child the parent's history.
 *
 * @param child Child agent context (destination)
 * @param parent Parent agent context (source)
 * @return OK on success, ERR on failure
 */
res_t ik_agent_copy_conversation(ik_agent_ctx_t *child, const ik_agent_ctx_t *parent);

/**
 * Check if agent has any running tools
 *
 * Used by fork sync barrier to wait for tool completion before forking.
 * A tool is considered running if tool_thread_running is true.
 *
 * @param agent Agent context to check
 * @return true if agent has running tools, false otherwise
 */
bool ik_agent_has_running_tools(const ik_agent_ctx_t *agent);

// State transition functions (moved from repl.h/repl.c)
// These operate on a specific agent, enabling proper multi-agent support.

/**
 * Transition agent from IDLE to WAITING_FOR_LLM state
 *
 * Updates state, shows spinner, hides input buffer.
 * Thread-safe: Uses mutex for state update.
 *
 * @param agent Agent context to transition
 */
void ik_agent_transition_to_waiting_for_llm(ik_agent_ctx_t *agent);

/**
 * Transition agent from WAITING_FOR_LLM to IDLE state
 *
 * Updates state, hides spinner, shows input buffer.
 * Thread-safe: Uses mutex for state update.
 *
 * @param agent Agent context to transition
 */
void ik_agent_transition_to_idle(ik_agent_ctx_t *agent);

/**
 * Transition agent from WAITING_FOR_LLM to EXECUTING_TOOL state
 *
 * Updates state. Spinner stays visible, input stays hidden.
 * Thread-safe: Uses mutex for state update.
 *
 * @param agent Agent context to transition
 */
void ik_agent_transition_to_executing_tool(ik_agent_ctx_t *agent);

/**
 * Transition agent from EXECUTING_TOOL to WAITING_FOR_LLM state
 *
 * Updates state. Spinner stays visible, input stays hidden.
 * Thread-safe: Uses mutex for state update.
 *
 * @param agent Agent context to transition
 */
void ik_agent_transition_from_executing_tool(ik_agent_ctx_t *agent);

/**
 * Start async tool execution on specific agent
 *
 * Spawns background thread to execute pending tool call.
 * Works with agent-specific context, not repl->current.
 *
 * @param agent Agent context with pending_tool_call set
 */
void ik_agent_start_tool_execution(ik_agent_ctx_t *agent);

/**
 * Complete async tool execution on specific agent
 *
 * Harvests result from thread, adds messages to conversation,
 * updates scrollback and database. Works with agent-specific context.
 *
 * @param agent Agent context with completed tool thread
 */
void ik_agent_complete_tool_execution(ik_agent_ctx_t *agent);

/**
 * Apply configuration defaults to agent
 *
 * Sets provider, model, and thinking_level from config defaults.
 * For root agents: uses ik_config_get_default_provider().
 * For forked agents: should inherit from parent (caller's responsibility).
 *
 * @param agent Agent context to configure (must not be NULL)
 * @param config Configuration with defaults (must not be NULL)
 * @return OK on success, ERR_INVALID_ARG if config is NULL
 */
res_t ik_agent_apply_defaults(ik_agent_ctx_t *agent, void *config);

/**
 * Restore agent from database row
 *
 * Populates provider, model, and thinking_level fields from database row.
 * If DB fields are NULL (old agents pre-migration), falls back to config defaults.
 * Does NOT load provider_instance (lazy-loaded on first use).
 *
 * @param agent Agent context to populate (must not be NULL)
 * @param row Database row with agent data (must not be NULL)
 * @return OK on success, ERR_INVALID_ARG if row is NULL
 */
res_t ik_agent_restore_from_row(ik_agent_ctx_t *agent, const void *row);

/**
 * Get or create provider instance
 *
 * Lazy-loads and caches provider instance. If already cached, returns existing.
 * If not cached, calls ik_provider_create() and caches result.
 *
 * @param agent Agent context (must not be NULL)
 * @param out Output parameter for provider instance (must not be NULL)
 * @return OK with provider on success, ERR_MISSING_CREDENTIALS if provider creation fails
 */
res_t ik_agent_get_provider(ik_agent_ctx_t *agent, struct ik_provider **out);

/**
 * Invalidate cached provider instance
 *
 * Frees cached provider_instance and sets to NULL.
 * Called when /model command changes provider or model.
 * Next ik_agent_get_provider() call creates new provider for updated model.
 * Safe to call multiple times (idempotent).
 * Safe to call when provider_instance is NULL.
 *
 * @param agent Agent context (must not be NULL)
 */
void ik_agent_invalidate_provider(ik_agent_ctx_t *agent);

/**
 * Add message to agent's conversation
 *
 * Grows messages array if needed using geometric growth (initial capacity 16).
 * Reparents message to agent context and adds to messages array.
 *
 * @param agent Agent context (must not be NULL)
 * @param msg   Message to add (will be reparented to agent, must not be NULL)
 * @return      OK(msg) on success
 */
res_t ik_agent_add_message(ik_agent_ctx_t *agent, ik_message_t *msg);

/**
 * Clear all messages from agent's conversation
 *
 * Frees messages array and resets count/capacity to zero.
 * Safe to call on empty conversation.
 *
 * @param agent Agent context (must not be NULL)
 */
void ik_agent_clear_messages(ik_agent_ctx_t *agent);

/**
 * Clone messages from source agent to destination agent
 *
 * Deep copies all messages and their content blocks from src to dest.
 * Used during fork to copy parent's conversation to child.
 *
 * @param dest Destination agent (must not be NULL)
 * @param src  Source agent (must not be NULL)
 * @return     OK(dest->messages) on success
 */
res_t ik_agent_clone_messages(ik_agent_ctx_t *dest, const ik_agent_ctx_t *src);
