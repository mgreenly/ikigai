// Core wait logic - shared between slash command and internal tool
#pragma once

#include "apps/ikigai/db/connection.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <talloc.h>

// Fan-in result entry
typedef struct {
    char *agent_uuid;
    char *agent_name;
    char *status;      // "received", "running", "idle", "dead"
    char *message;     // NULL unless status == "received"
} ik_wait_fanin_entry_t;

// Wait result structure
typedef struct {
    bool is_fanin;
    // Mode 1: next message
    char *from_uuid;
    char *message;
    // Mode 2: fan-in
    ik_wait_fanin_entry_t *entries;
    size_t entry_count;
} ik_wait_result_t;

// Core wait - next message mode
void ik_wait_core_next_message(TALLOC_CTX *ctx, ik_db_ctx_t *db_ctx, int64_t session_id,
                                const char *my_uuid, int32_t timeout_sec, bool *interrupted,
                                ik_wait_result_t *result);

// Core wait - fan-in mode
void ik_wait_core_fanin(TALLOC_CTX *ctx, ik_db_ctx_t *db_ctx, int64_t session_id,
                        const char *my_uuid, int32_t timeout_sec, char **target_uuids,
                        size_t target_count, bool *interrupted, ik_wait_result_t *result);
