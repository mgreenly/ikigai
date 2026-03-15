#pragma once

#include "apps/ikigai/tool_scheduler.h"
#include "shared/error.h"

// Forward declarations
typedef struct ik_repl_ctx_t ik_repl_ctx_t;
typedef struct ik_agent_ctx ik_agent_ctx_t;
typedef struct ik_message ik_message_t;

// Handle tool thread completion for specific agent
void ik_repl_handle_agent_tool_completion(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent);

// Handle interrupted tool completion for specific agent
void ik_repl_handle_interrupted_tool_completion(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent);

// Poll for tool thread completion across all agents
res_t ik_repl_poll_tool_completions(ik_repl_ctx_t *repl);

// Submit continuation of tool loop (starts new LLM request after tool completes)
void ik_repl_submit_tool_loop_continuation(ik_repl_ctx_t *repl, ik_agent_ctx_t *agent);

// Build an assistant message containing all N tool_call content blocks.
// Exposed for unit testing.
ik_message_t *ik_repl_build_multi_tool_call_msg(ik_agent_ctx_t *agent,
                                                ik_tool_scheduler_t *sched);

// Build a user/tool message containing all N tool_result content blocks.
// Exposed for unit testing.
ik_message_t *ik_repl_build_multi_tool_result_msg(ik_agent_ctx_t *agent,
                                                   ik_tool_scheduler_t *sched);
