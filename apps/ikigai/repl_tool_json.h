#pragma once
#include "apps/ikigai/tool.h"

#include <talloc.h>

// Build tool_call data_json for database with thinking/redacted blocks.
char *ik_build_tool_call_data_json(TALLOC_CTX *ctx,
                                   const ik_tool_call_t *tc,
                                   const char *thinking_text,
                                   const char *thinking_signature,
                                   const char *redacted_data);

// Build tool_result data_json for database.
char *ik_build_tool_result_data_json(TALLOC_CTX *ctx,
                                     const char *tool_call_id,
                                     const char *tool_name,
                                     const char *result_json);
