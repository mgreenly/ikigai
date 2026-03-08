/**
 * @file bang_commands.c
 * @brief Bang command dispatcher — reads command files, resolves templates, sends to LLM
 *
 * Typing !<name> [args...] reads ik://commands/<name>.md, performs positional
 * argument substitution (${1}, ${2}, ...) and full template processing, then
 * sends the resolved content to the LLM as a user message recorded with
 * kind="bang_command" and data_json={"command":"!name args..."}.
 */

#include "apps/ikigai/bang_commands.h"

#include "apps/ikigai/doc_cache.h"
#include "apps/ikigai/repl.h"
#include "apps/ikigai/repl_actions_internal.h"
#include "apps/ikigai/scrollback.h"
#include "apps/ikigai/scrollback_utils.h"
#include "apps/ikigai/shared.h"
#include "apps/ikigai/template.h"
#include "shared/error.h"
#include "shared/panic.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>

#include "shared/poison.h"

/* Match ${N} numeric positional var; returns replacement or NULL if not positional. */
static const char *match_positional_var_(const char *p, const char *end,
                                         char **pos_args, size_t pos_arg_count)
{
    size_t var_len = (size_t)(end - p - 2);
    if (var_len == 0 || var_len >= 10) return NULL;
    for (size_t k = 0; k < var_len; k++) {
        if (!isdigit((unsigned char)p[2 + k])) return NULL;
    }
    char num_buf[12];
    memcpy(num_buf, p + 2, var_len);
    num_buf[var_len] = '\0';
    size_t idx = (size_t)strtoul(num_buf, NULL, 10);
    if (idx < 1 || idx > pos_arg_count) return NULL;
    return pos_args[idx - 1];
}

/* Replace ${N} positional placeholders; unresolved placeholders remain as-is. */
static char *apply_positional_args_(TALLOC_CTX *ctx, const char *content,
                                    char **pos_args, size_t pos_arg_count)
{
    if (pos_arg_count == 0) {
        char *result = talloc_strdup(ctx, content);
        if (!result) PANIC("OOM");  /* LCOV_EXCL_LINE */
        return result;
    }

    char *result = talloc_strdup(ctx, "");
    if (!result) PANIC("OOM");  /* LCOV_EXCL_LINE */

    const char *start = content;
    const char *p = content;

    while (*p != '\0') {
        if (p[0] == '$' && p[1] == '{') {
            const char *end = strchr(p + 2, '}');
            if (end != NULL) {
                const char *arg_val = match_positional_var_(p, end, pos_args, pos_arg_count);
                if (arg_val != NULL) {
                    char *tmp = talloc_asprintf(ctx, "%s%.*s%s",
                                                result, (int)(p - start), start, arg_val);
                    if (!tmp) PANIC("OOM");  /* LCOV_EXCL_LINE */
                    talloc_free(result);
                    result = tmp;
                    p = end + 1;
                    start = p;
                    continue;
                }
            }
        }
        p++;
    }
    if (p > start) {
        char *tmp = talloc_asprintf(ctx, "%s%s", result, start);
        if (!tmp) PANIC("OOM");  /* LCOV_EXCL_LINE */
        talloc_free(result);
        result = tmp;
    }
    return result;
}

/* Parse whitespace-separated tokens from rest into an array. */
static char **parse_pos_args_(TALLOC_CTX *ctx, const char *rest, size_t *out_count)
{
    *out_count = 0;
    if (rest == NULL || *rest == '\0') return NULL;

    size_t count = 0;
    const char *scan = rest;
    while (*scan != '\0') {
        while (*scan && !isspace((unsigned char)*scan)) scan++;
        count++;
        while (*scan && isspace((unsigned char)*scan)) scan++;
    }
    if (count == 0) return NULL;

    char *args_copy = talloc_strdup(ctx, rest);
    if (!args_copy) PANIC("OOM");  /* LCOV_EXCL_LINE */
    char **args = talloc_zero_array(ctx, char *, (unsigned int)count);
    if (!args) PANIC("OOM");  /* LCOV_EXCL_LINE */

    size_t i = 0;
    char *tok = args_copy;
    while (*tok != '\0' && i < count) {
        while (*tok && isspace((unsigned char)*tok)) tok++;
        if (*tok == '\0') break;
        args[i] = tok;
        while (*tok && !isspace((unsigned char)*tok)) tok++;
        if (*tok != '\0') { *tok = '\0'; tok++; }
        i++;
    }
    *out_count = i;
    return args;
}

res_t ik_bang_dispatch(void *ctx, ik_repl_ctx_t *repl, const char *input)
{
    assert(ctx != NULL);    /* LCOV_EXCL_BR_LINE */
    assert(repl != NULL);   /* LCOV_EXCL_BR_LINE */
    assert(input != NULL);  /* LCOV_EXCL_BR_LINE */
    assert(input[0] == '!'); /* LCOV_EXCL_BR_LINE */

    // Skip leading '!'
    const char *cmd_start = input + 1;

    // Skip leading whitespace after '!'
    while (isspace((unsigned char)*cmd_start)) {  /* LCOV_EXCL_BR_LINE */
        cmd_start++;
    }

    // Echo command to scrollback
    ik_scrollback_append_line(repl->current->scrollback, input, strlen(input));

    // Empty command (just "!")
    if (*cmd_start == '\0') {  /* LCOV_EXCL_BR_LINE */
        char *msg = ik_scrollback_format_warning(ctx, "Empty command");
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        talloc_free(msg);
        return ERR(ctx, INVALID_ARG, "Empty command");
    }

    // Find end of command name
    const char *args_start = cmd_start;
    while (*args_start && !isspace((unsigned char)*args_start)) {  /* LCOV_EXCL_BR_LINE */
        args_start++;
    }

    // Extract command name
    size_t cmd_len = (size_t)(args_start - cmd_start);
    char *cmd_name = talloc_strndup(ctx, cmd_start, cmd_len);
    if (!cmd_name) {  /* LCOV_EXCL_BR_LINE */
        PANIC("OOM");  /* LCOV_EXCL_LINE */
    }

    // Skip whitespace to reach args
    const char *args_text = args_start;
    while (isspace((unsigned char)*args_text)) {
        args_text++;
    }

    // Parse positional arguments
    size_t pos_arg_count = 0;
    char **pos_args = parse_pos_args_(ctx, args_text, &pos_arg_count);

    // Build URI: ik://commands/<name>.md
    char *uri = talloc_asprintf(ctx, "ik://commands/%s.md", cmd_name);
    if (!uri) {  /* LCOV_EXCL_BR_LINE */
        PANIC("OOM");  /* LCOV_EXCL_LINE */
    }

    // Load command file from doc cache
    char *file_content = NULL;
    res_t cache_res = ik_doc_cache_get(repl->current->doc_cache, uri, &file_content);
    talloc_free(uri);

    if (is_err(&cache_res)) {
        char *err_text = talloc_asprintf(ctx, "Command not found: !%s", cmd_name);
        if (!err_text) PANIC("OOM");  /* LCOV_EXCL_LINE */
        char *msg = ik_scrollback_format_warning(ctx, err_text);
        talloc_free(err_text);
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));
        talloc_free(msg);
        res_t result = ERR(ctx, INVALID_ARG, "Command not found: !%s", cmd_name);
        talloc_free(cache_res.err);
        talloc_free(cmd_name);
        return result;
    }

    // Apply positional arg substitution
    char *substituted = apply_positional_args_(ctx, file_content, pos_args, pos_arg_count);

    // Apply full template processing
    ik_template_result_t *tmpl_result = NULL;
    res_t tmpl_res = ik_template_process(ctx, substituted, repl->current,
                                         repl->shared->cfg, &tmpl_result);
    talloc_free(substituted);

    if (is_err(&tmpl_res)) {  /* LCOV_EXCL_BR_LINE */
        char *msg = ik_scrollback_format_warning(ctx, "Template processing failed");  /* LCOV_EXCL_LINE */
        ik_scrollback_append_line(repl->current->scrollback, msg, strlen(msg));  /* LCOV_EXCL_LINE */
        talloc_free(msg);  /* LCOV_EXCL_LINE */
        talloc_free(cmd_name);  /* LCOV_EXCL_LINE */
        return tmpl_res;  /* LCOV_EXCL_LINE */
    }

    // Send resolved content to LLM, preserving original input for display
    send_to_llm_for_agent_bang(repl, repl->current, tmpl_result->processed, input);

    talloc_free(tmpl_result);
    talloc_free(cmd_name);
    return OK(NULL);
}
