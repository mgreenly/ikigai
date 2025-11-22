// REPL action processing module
#pragma once

#include "error.h"
#include "repl.h"
#include "input.h"
#include "scrollback.h"

// Process single input action and update REPL state
res_t ik_repl_process_action(ik_repl_ctx_t *repl, const ik_input_action_t *action);

// Append multi-line output to scrollback (exposed for testing)
void ik_repl_append_multiline_to_scrollback(ik_scrollback_t *scrollback, const char *output, size_t output_len);
