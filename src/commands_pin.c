/**
 * @file commands_pin.c
 * @brief Pin command implementations for managing system prompt documents
 */

#include "commands_pin.h"

#include "agent.h"
#include "ansi.h"
#include "db/message.h"
#include "doc_cache.h"
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

void ik_persist_pin_command(void *ctx, ik_repl_ctx_t *repl, const char *command, const char *path)
{
    if (repl->shared->db_ctx == NULL || repl->shared->session_id == 0) {     // LCOV_EXCL_BR_LINE
        return;     // LCOV_EXCL_LINE
    }

    char *data_json = talloc_asprintf(ctx, "{\"command\":\"%s\",\"args\":\"%s\"}", command, path);
    if (!data_json) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }

    res_t db_res = ik_db_message_insert(repl->shared->db_ctx, repl->shared->session_id,
                                        repl->current->uuid, "command", NULL, data_json);
    if (is_err(&db_res)) {     // LCOV_EXCL_BR_LINE
        yyjson_mut_doc *log_doc = ik_log_create();  // LCOV_EXCL_LINE
        yyjson_mut_val *log_root = yyjson_mut_doc_get_root(log_doc);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "event", "db_persist_failed");  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "operation", command);  // LCOV_EXCL_LINE
        yyjson_mut_obj_add_str(log_doc, log_root, "error", error_message(db_res.err));  // LCOV_EXCL_LINE
        ik_log_warn_json(log_doc);  // LCOV_EXCL_LINE
        talloc_free(db_res.err);  // LCOV_EXCL_LINE
    }
    talloc_free(data_json);
}

res_t ik_cmd_pin_list(void *ctx, ik_repl_ctx_t *repl)
{
    if (repl->current->pinned_count == 0) {
        char *msg = talloc_strdup(ctx, "No pinned documents.");
        if (!msg) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        res_t result = ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        talloc_free(msg);
        return result;
    }

    for (size_t i = 0; i < repl->current->pinned_count; i++) {
        char *line = talloc_asprintf(ctx, "  - %s", repl->current->pinned_paths[i]);
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

res_t ik_cmd_pin_add(void *ctx, ik_repl_ctx_t *repl, const char *path)
{
    if (repl->current->doc_cache != NULL) {
        char *content = NULL;
        res_t doc_res = ik_doc_cache_get(repl->current->doc_cache, path, &content);
        if (is_err(&doc_res)) {
            const char *prefix = ik_output_prefix(IK_OUTPUT_WARNING);
            int32_t color_code = ik_output_color(IK_OUTPUT_WARNING);

            char color_seq[16] = {0};
            if (ik_ansi_colors_enabled() && color_code >= 0) {
                ik_ansi_fg_256(color_seq, sizeof(color_seq), (uint8_t)color_code);
            }

            char *msg = NULL;
            if (color_seq[0] != '\0') {
                msg = talloc_asprintf(ctx, "%s%s File not found: %s%s",
                                     color_seq, prefix, path, IK_ANSI_RESET);
            } else {
                msg = talloc_asprintf(ctx, "%s File not found: %s", prefix, path);
            }

            if (!msg) {     // LCOV_EXCL_BR_LINE
                PANIC("OOM");   // LCOV_EXCL_LINE
            }
            res_t result = ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
            talloc_free(msg);
            talloc_free(doc_res.err);
            return result;
        }
    }

    for (size_t i = 0; i < repl->current->pinned_count; i++) {
        if (strcmp(repl->current->pinned_paths[i], path) == 0) {
            char *msg = talloc_asprintf(ctx, "Already pinned: %s", path);
            if (!msg) {     // LCOV_EXCL_BR_LINE
                PANIC("OOM");   // LCOV_EXCL_LINE
            }
            res_t result = ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
            talloc_free(msg);
            if (is_err(&result)) {     // LCOV_EXCL_BR_LINE
                return result;     // LCOV_EXCL_LINE
            }
            ik_persist_pin_command(ctx, repl, "pin", path);
            return OK(NULL);
        }
    }

    char **new_paths = talloc_realloc(repl->current, repl->current->pinned_paths,
                                       char *, (unsigned int)(repl->current->pinned_count + 1));
    if (!new_paths) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }
    repl->current->pinned_paths = new_paths;
    repl->current->pinned_paths[repl->current->pinned_count] = talloc_strdup(repl->current, path);
    if (!repl->current->pinned_paths[repl->current->pinned_count]) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }
    repl->current->pinned_count++;

    char *msg = talloc_asprintf(ctx, "Pinned: %s", path);
    if (!msg) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }
    res_t result = ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
    talloc_free(msg);
    if (is_err(&result)) {     // LCOV_EXCL_BR_LINE
        return result;     // LCOV_EXCL_LINE
    }

    ik_persist_pin_command(ctx, repl, "pin", path);
    return OK(NULL);
}

res_t ik_cmd_pin(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE

    if (args == NULL) {
        return ik_cmd_pin_list(ctx, repl);
    }

    return ik_cmd_pin_add(ctx, repl, args);
}

res_t ik_cmd_unpin(void *ctx, ik_repl_ctx_t *repl, const char *args)
{
    assert(ctx != NULL);      // LCOV_EXCL_BR_LINE
    assert(repl != NULL);     // LCOV_EXCL_BR_LINE

    if (args == NULL) {
        char *msg = talloc_strdup(ctx, "Error: /unpin requires a path argument");
        if (!msg) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        return ERR(ctx, INVALID_ARG, "Missing path argument");
    }

    const char *path = args;
    int64_t found_index = -1;
    for (size_t i = 0; i < repl->current->pinned_count; i++) {
        if (strcmp(repl->current->pinned_paths[i], path) == 0) {
            found_index = (int64_t)i;
            break;
        }
    }

    if (found_index < 0) {
        char *msg = talloc_asprintf(ctx, "Not pinned: %s", path);
        if (!msg) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        res_t result = ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        talloc_free(msg);
        return result;
    }

    talloc_free(repl->current->pinned_paths[found_index]);
    for (size_t i = (size_t)found_index; i < repl->current->pinned_count - 1; i++) {
        repl->current->pinned_paths[i] = repl->current->pinned_paths[i + 1];
    }
    repl->current->pinned_count--;

    if (repl->current->pinned_count == 0) {
        talloc_free(repl->current->pinned_paths);
        repl->current->pinned_paths = NULL;
    } else {
        char **new_paths = talloc_realloc(repl->current, repl->current->pinned_paths,
                                           char *, (unsigned int)repl->current->pinned_count);
        if (!new_paths) {     // LCOV_EXCL_BR_LINE
            PANIC("OOM");   // LCOV_EXCL_LINE
        }
        repl->current->pinned_paths = new_paths;
    }

    char *msg = talloc_asprintf(ctx, "Unpinned: %s", path);
    if (!msg) {     // LCOV_EXCL_BR_LINE
        PANIC("OOM");   // LCOV_EXCL_LINE
    }
    res_t result = ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
    talloc_free(msg);
    if (is_err(&result)) {     // LCOV_EXCL_BR_LINE
        return result;     // LCOV_EXCL_LINE
    }

    ik_persist_pin_command(ctx, repl, "unpin", path);
    return OK(NULL);
}
