/**
 * @file commands_toolset.c
 * @brief Toolset command implementations for filtering tools visible to LLM
 */

#include "commands_toolset.h"

#include "agent.h"
#include "ansi.h"
#include "db/message.h"
#include "logger.h"
#include "output_style.h"
#include "panic.h"
#include "repl.h"
#include "scrollback.h"
#include "shared.h"

#include "vendor/yyjson/yyjson.h"

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <talloc.h>

void ik_persist_toolset_command(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    if (repl->shared->db_ctx == NULL || repl->shared->session_id == 0) {     // LCOV_EXCL_BR_LINE
        return;     // LCOV_EXCL_LINE
    }

    char *data_json = talloc_asprintf(ctx, "{\"command\":\"toolset\",\"args\":\"%s\"}", args);
    if (!data_json) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }

    res_t db_res = ik_db_message_insert(repl->shared->db_ctx, repl->shared->session_id,
                                        repl->current->uuid, "command", NULL, data_json);
    if (is_err(&db_res)) {     // LCOV_EXCL_BR_LINE
        yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_LINE
        yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "event", "db_persist_failed");  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "operation", "toolset");  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "error", error_message(db_res.err));  // LCOV_EXCL_LINE
        ik_log_warn_json(log_doc);  // LCOV_EXCL_LINE
        talloc_free(db_res.err);  // LCOV_EXCL_LINE
    }
    talloc_free(data_json);
}

res_t ik_cmd_toolset_list(void *ctx, ik_repl_ctx_t *repl)
{
    if (repl->current->toolset_count == 0) {
        char *msg = talloc_strdup(ctx, "No toolset filter active");
        if (!msg) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        res_t result = ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        talloc_free(msg);
        return result;
    }

    int32_t color_code = ik_output_color(IK_OUTPUT_SLASH_OUTPUT);
    char color_seq[16] = {0};
    if (ik_ansi_colors_enabled() && color_code >= 0) {
        ik_ansi_fg_256(color_seq, sizeof(color_seq), (uint8_t)color_code);
    }

    for (size_t i = 0; i < repl->current->toolset_count; i++) {
        char *line = NULL;
        if (color_seq[0] != '\0') {
            line = talloc_asprintf(ctx, "%s  - %s%s",
                                   color_seq, repl->current->toolset_filter[i], IK_ANSI_RESET);
        } else {
            line = talloc_asprintf(ctx, "  - %s", repl->current->toolset_filter[i]);
        }
        if (!line) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        res_t result = ik_scrollback_append_line(repl->current->scrollback, line, strlen(line));
        talloc_free(line);
        if (is_err(&result)) {     // LCOV_EXCL_BR_LINE
            return result;     // LCOV_EXCL_LINE
        }
    }

    return OK(NULL);
}

res_t ik_cmd_toolset_set(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(args != NULL);      // LCOV_EXCL_BR_LINE

    char *work = talloc_strdup(ctx, args);
    if (!work) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }

    size_t capacity = 16;
    char **tools = talloc_array(ctx, char *, capacity);
    if (!tools) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }
    size_t count = 0;

    char *saveptr = NULL;
    char *token = strtok_r(work, " ,", &saveptr);
    while (token != NULL) {
        while (*token == ' ' || *token == ',') {
            token++;
        }
        if (*token == '\0') {
            token = strtok_r(NULL, " ,", &saveptr);
            continue;
        }

        char *end = token + strlen(token) - 1;
        while (end > token && (*end == ' ' || *end == ',')) {
            *end = '\0';
            end--;
        }

        if (count >= capacity) {
            capacity *= 2;
            tools = talloc_realloc(ctx, tools, char *, capacity);
            if (!tools) {     // LCOV_EXCL_BR_LINE
                PANIC("OOM");   // LCOV_EXCL_LINE
            }
        }

        tools[count] = talloc_strdup(repl->current, token);
        if (!tools[count]) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        count++;

        token = strtok_r(NULL, " ,", &saveptr);
    }

    if (repl->current->toolset_filter != NULL) {
        talloc_free(repl->current->toolset_filter);
    }

    repl->current->toolset_filter = talloc_realloc(repl->current, tools, char *, count);
    if (count > 0 && !repl->current->toolset_filter) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }
    repl->current->toolset_count = count;

    talloc_free(work);

    ik_persist_toolset_command(ctx, repl, args);

    return OK(NULL);
}

res_t ik_cmd_toolset(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE

    if (args == NULL) {
        return ik_cmd_toolset_list(ctx, repl);
    }

    return ik_cmd_toolset_set(ctx, repl, args);
}
