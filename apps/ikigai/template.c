/**
 * @file template.c
 * @brief Template variable processing for pinned documents
 */

#include "template.h"

#include "shared/panic.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>
#include <time.h>
#include <unistd.h>
#include <uuid/uuid.h>

static char *resolve_agent_field(TALLOC_CTX *ctx, const char *field, ik_agent_ctx_t *agent)
{
    if (agent == NULL) return NULL;

    if (strcmp(field, "uuid") == 0 && agent->uuid != NULL) {
        return talloc_strdup(ctx, agent->uuid);
    }
    if (strcmp(field, "name") == 0 && agent->name != NULL) {
        return talloc_strdup(ctx, agent->name);
    }
    if (strcmp(field, "parent_uuid") == 0 && agent->parent_uuid != NULL) {
        return talloc_strdup(ctx, agent->parent_uuid);
    }
    if (strcmp(field, "provider") == 0 && agent->provider != NULL) {
        return talloc_strdup(ctx, agent->provider);
    }
    if (strcmp(field, "model") == 0 && agent->model != NULL) {
        return talloc_strdup(ctx, agent->model);
    }
    if (strcmp(field, "created_at") == 0) {
        char *val = talloc_asprintf(ctx, "%" PRId64, agent->created_at);
        if (val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return val;
    }
    return NULL;
}

static char *resolve_config_field(TALLOC_CTX *ctx, const char *field, ik_config_t *config)
{
    if (config == NULL) return NULL;

    if (strcmp(field, "openai_model") == 0 && config->openai_model != NULL) {
        return talloc_strdup(ctx, config->openai_model);
    }
    if (strcmp(field, "db_host") == 0 && config->db_host != NULL) {
        return talloc_strdup(ctx, config->db_host);
    }
    if (strcmp(field, "db_port") == 0) {
        char *val = talloc_asprintf(ctx, "%" PRId32, config->db_port);
        if (val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return val;
    }
    if (strcmp(field, "db_name") == 0 && config->db_name != NULL) {
        return talloc_strdup(ctx, config->db_name);
    }
    if (strcmp(field, "db_user") == 0 && config->db_user != NULL) {
        return talloc_strdup(ctx, config->db_user);
    }
    if (strcmp(field, "default_provider") == 0 && config->default_provider != NULL) {
        return talloc_strdup(ctx, config->default_provider);
    }
    if (strcmp(field, "max_tool_turns") == 0) {
        char *val = talloc_asprintf(ctx, "%" PRId32, config->max_tool_turns);
        if (val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return val;
    }
    if (strcmp(field, "max_output_size") == 0) {
        char *val = talloc_asprintf(ctx, "%" PRId64, config->max_output_size);
        if (val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return val;
    }
    if (strcmp(field, "history_size") == 0) {
        char *val = talloc_asprintf(ctx, "%" PRId32, config->history_size);
        if (val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return val;
    }
    if (strcmp(field, "listen_address") == 0 && config->listen_address != NULL) {
        return talloc_strdup(ctx, config->listen_address);
    }
    if (strcmp(field, "listen_port") == 0) {
        char *val = talloc_asprintf(ctx, "%" PRIu16, config->listen_port);
        if (val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return val;
    }
    if (strcmp(field, "openai_temperature") == 0) {
        char *val = talloc_asprintf(ctx, "%.2f", config->openai_temperature);
        if (val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return val;
    }
    if (strcmp(field, "openai_max_completion_tokens") == 0) {
        char *val = talloc_asprintf(ctx, "%" PRId32, config->openai_max_completion_tokens);
        if (val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return val;
    }
    if (strcmp(field, "openai_system_message") == 0 && config->openai_system_message != NULL) {
        return talloc_strdup(ctx, config->openai_system_message);
    }
    return NULL;
}

static char *resolve_func_value(TALLOC_CTX *ctx, const char *func_name)
{
    if (strcmp(func_name, "now") == 0) {
        time_t now = time(NULL);
        struct tm *tm = gmtime(&now);
        if (tm == NULL) return NULL;  // LCOV_EXCL_LINE
        char *val = talloc_asprintf(ctx, "%04d-%02d-%02dT%02d:%02d:%02dZ",
                                   tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
                                   tm->tm_hour, tm->tm_min, tm->tm_sec);
        if (val == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        return val;
    }

    if (strcmp(func_name, "cwd") == 0) {
        char buf[PATH_MAX];
        if (getcwd(buf, sizeof(buf)) != NULL) {
            return talloc_strdup(ctx, buf);
        }
        return NULL;  // LCOV_EXCL_LINE
    }

    if (strcmp(func_name, "hostname") == 0) {
        char buf[256];
        if (gethostname(buf, sizeof(buf)) == 0) {
            return talloc_strdup(ctx, buf);
        }
        return NULL;  // LCOV_EXCL_LINE
    }

    if (strcmp(func_name, "random") == 0) {
        uuid_t uuid;
        uuid_generate(uuid);
        char uuid_str[37];
        uuid_unparse_lower(uuid, uuid_str);
        return talloc_strdup(ctx, uuid_str);
    }

    return NULL;
}

static char *resolve_variable(TALLOC_CTX *ctx,
                             const char *var,
                             ik_agent_ctx_t *agent,
                             ik_config_t *config)
{
    if (strncmp(var, "agent.", 6) == 0) {
        return resolve_agent_field(ctx, var + 6, agent);
    }
    if (strncmp(var, "config.", 7) == 0) {
        return resolve_config_field(ctx, var + 7, config);
    }
    if (strncmp(var, "env.", 4) == 0) {
        const char *env_value = getenv(var + 4);
        return (env_value != NULL) ? talloc_strdup(ctx, env_value) : NULL;
    }
    if (strncmp(var, "func.", 5) == 0) {
        return resolve_func_value(ctx, var + 5);
    }
    return NULL;
}

static void append_to_processed(TALLOC_CTX *ctx, char **processed, const char *append_text)
{
    char *new_processed = talloc_asprintf(ctx, "%s%s", *processed, append_text);
    if (new_processed == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    talloc_free(*processed);
    *processed = new_processed;
}

static void track_unresolved(TALLOC_CTX *ctx,
                            char ***unresolved,
                            size_t *unresolved_count,
                            const char *literal)
{
    for (size_t i = 0; i < *unresolved_count; i++) {
        if (strcmp((*unresolved)[i], literal) == 0) {
            return;
        }
    }

    char **new_unresolved = talloc_realloc(ctx, *unresolved, char *, (unsigned int)(*unresolved_count + 1));
    if (new_unresolved == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
    *unresolved = new_unresolved;
    (*unresolved)[*unresolved_count] = talloc_strdup(ctx, literal);
    (*unresolved_count)++;
}

res_t ik_template_process(TALLOC_CTX *ctx,
                          const char *text,
                          ik_agent_ctx_t *agent,
                          ik_config_t *config,
                          ik_template_result_t **out)
{
    assert(text != NULL);       // LCOV_EXCL_BR_LINE
    assert(out != NULL);        // LCOV_EXCL_BR_LINE

    *out = NULL;

    ik_template_result_t *result = talloc_zero(ctx, ik_template_result_t);
    if (result == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char *processed = talloc_strdup(result, "");
    if (processed == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

    char **unresolved = NULL;
    size_t unresolved_count = 0;

    const char *p = text;
    while (*p != '\0') {
        if (p[0] == '$' && p[1] == '$') {
            append_to_processed(result, &processed, "$");
            p += 2;
            continue;
        }

        if (p[0] == '$' && p[1] == '{') {
            const char *end = strchr(p + 2, '}');
            if (end == NULL) {
                char ch[2] = {*p, '\0'};
                append_to_processed(result, &processed, ch);
                p++;
                continue;
            }

            size_t var_len = (size_t)(end - p - 2);
            char *var = talloc_strndup(result, p + 2, var_len);
            if (var == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

            char *value = resolve_variable(result, var, agent, config);
            if (value != NULL) {
                append_to_processed(result, &processed, value);
            } else {
                size_t literal_len = (size_t)(end - p + 1);
                char *literal = talloc_strndup(result, p, literal_len);
                if (literal == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

                append_to_processed(result, &processed, literal);
                track_unresolved(result, &unresolved, &unresolved_count, literal);
            }

            p = end + 1;
            continue;
        }

        char ch[2] = {*p, '\0'};
        append_to_processed(result, &processed, ch);
        p++;
    }

    result->processed = processed;
    result->unresolved = unresolved;
    result->unresolved_count = unresolved_count;

    *out = result;
    return OK(*out);
}
