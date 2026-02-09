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

// Resolve a single variable to its value
static char *resolve_variable(TALLOC_CTX *ctx,
                             const char *var,
                             ik_agent_ctx_t *agent,
                             ik_config_t *config)
{
    // Agent namespace
    if (strncmp(var, "agent.", 6) == 0) {
        const char *field = var + 6;
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

    // Config namespace
    if (strncmp(var, "config.", 7) == 0) {
        const char *field = var + 7;
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

    // Environment namespace
    if (strncmp(var, "env.", 4) == 0) {
        const char *env_name = var + 4;
        const char *env_value = getenv(env_name);
        if (env_value != NULL) {
            return talloc_strdup(ctx, env_value);
        }
        return NULL;
    }

    // Function namespace
    if (strncmp(var, "func.", 5) == 0) {
        const char *func_name = var + 5;

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

    return NULL;
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
        // Handle $$ escape
        if (p[0] == '$' && p[1] == '$') {
            char *new_processed = talloc_asprintf(result, "%s$", processed);
            if (new_processed == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
            talloc_free(processed);
            processed = new_processed;
            p += 2;
            continue;
        }

        // Handle ${variable}
        if (p[0] == '$' && p[1] == '{') {
            const char *end = strchr(p + 2, '}');
            if (end != NULL) {
                size_t var_len = (size_t)(end - p - 2);
                char *var = talloc_strndup(result, p + 2, var_len);
                if (var == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

                char *value = resolve_variable(result, var, agent, config);
                if (value != NULL) {
                    char *new_processed = talloc_asprintf(result, "%s%s", processed, value);
                    if (new_processed == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                    talloc_free(processed);
                    processed = new_processed;
                } else {
                    // Unresolved - keep literal and track
                    size_t literal_len = (size_t)(end - p + 1);
                    char *literal = talloc_strndup(result, p, literal_len);
                    if (literal == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE

                    char *new_processed = talloc_asprintf(result, "%s%s", processed, literal);
                    if (new_processed == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                    talloc_free(processed);
                    processed = new_processed;

                    // Add to unresolved list if not already present
                    bool already_tracked = false;
                    for (size_t i = 0; i < unresolved_count; i++) {
                        if (strcmp(unresolved[i], literal) == 0) {
                            already_tracked = true;
                            break;
                        }
                    }
                    if (!already_tracked) {
                        char **new_unresolved = talloc_realloc(result, unresolved, char *, (unsigned int)(unresolved_count + 1));
                        if (new_unresolved == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
                        unresolved = new_unresolved;
                        unresolved[unresolved_count] = literal;
                        unresolved_count++;
                    }
                }

                p = end + 1;
                continue;
            }
        }

        // Regular character
        char ch[2] = {*p, '\0'};
        char *new_processed = talloc_asprintf(result, "%s%s", processed, ch);
        if (new_processed == NULL) PANIC("Out of memory");  // LCOV_EXCL_BR_LINE
        talloc_free(processed);
        processed = new_processed;
        p++;
    }

    result->processed = processed;
    result->unresolved = unresolved;
    result->unresolved_count = unresolved_count;

    *out = result;
    return OK(*out);
}
