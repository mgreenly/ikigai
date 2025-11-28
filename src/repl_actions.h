// REPL action processing module
#pragma once

#include "error.h"
#include "repl.h"
#include "input.h"

// Process single input action and update REPL state
res_t ik_repl_process_action(ik_repl_ctx_t *repl, const ik_input_action_t *action);
