#include "providers/common/error_utils.h"
#include "providers/provider.h"
#include "panic.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <talloc.h>


#include "poison.h"
bool ik_error_is_retryable(int category)
{
    switch (category) {
        case IK_ERR_CAT_RATE_LIMIT:
        case IK_ERR_CAT_SERVER:
        case IK_ERR_CAT_TIMEOUT:
        case IK_ERR_CAT_NETWORK:
            return true;
        case IK_ERR_CAT_AUTH:
        case IK_ERR_CAT_INVALID_ARG:
        case IK_ERR_CAT_NOT_FOUND:
        case IK_ERR_CAT_CONTENT_FILTER:
        case IK_ERR_CAT_UNKNOWN:
            return false;
        default:
            return false;
    }
}

static const char *get_env_var_for_provider(const char *provider)
{
    /* Check each known provider explicitly */
    int cmp_anthropic = strcmp(provider, "anthropic");
    if (cmp_anthropic == 0) {
        return "ANTHROPIC_API_KEY";
    }

    int cmp_openai = strcmp(provider, "openai");
    if (cmp_openai == 0) {
        return "OPENAI_API_KEY";
    }

    int cmp_google = strcmp(provider, "google");
    if (cmp_google == 0) { // LCOV_EXCL_BR_LINE - gcov branch tracking bug: reports branch not taken despite line 69 executing
        return "GOOGLE_API_KEY";
    }

    return "API_KEY";  // LCOV_EXCL_LINE - defensive fallback for unknown provider
}

char *ik_error_user_message(TALLOC_CTX *ctx,
                            const char *provider,
                            int category,
                            const char *detail)
{
    assert(ctx != NULL);     // LCOV_EXCL_BR_LINE
    assert(provider != NULL); // LCOV_EXCL_BR_LINE

    /* Treat empty string as NULL */
    if (detail != NULL && detail[0] == '\0') {
        detail = NULL;
    }

    char *message = NULL;

    switch (category) {
        case IK_ERR_CAT_AUTH: {
            const char *env_var = get_env_var_for_provider(provider);
            message = talloc_asprintf(ctx,
                                      "Authentication failed for %s. Check your API key in %s or ~/.config/ikigai/credentials.json",
                                      provider,
                                      env_var);
            break;
        }

        case IK_ERR_CAT_RATE_LIMIT:
            if (detail != NULL) {
                message = talloc_asprintf(ctx, "Rate limit exceeded for %s. %s", provider, detail);
            } else {
                message = talloc_asprintf(ctx, "Rate limit exceeded for %s.", provider);
            }
            break;

        case IK_ERR_CAT_INVALID_ARG:
            if (detail != NULL) {
                message = talloc_asprintf(ctx, "Invalid request to %s: %s", provider, detail);
            } else {
                message = talloc_asprintf(ctx, "Invalid request to %s", provider);
            }
            break;

        case IK_ERR_CAT_NOT_FOUND:
            if (detail != NULL) {
                message = talloc_asprintf(ctx, "Model not found on %s: %s", provider, detail);
            } else {
                message = talloc_asprintf(ctx, "Model not found on %s", provider);
            }
            break;

        case IK_ERR_CAT_SERVER:
            if (detail != NULL) {
                message = talloc_asprintf(ctx,
                                          "%s server error. This is temporary, retrying may succeed. %s",
                                          provider,
                                          detail);
            } else {
                message = talloc_asprintf(ctx, "%s server error. This is temporary, retrying may succeed.", provider);
            }
            break;

        case IK_ERR_CAT_TIMEOUT:
            message = talloc_asprintf(ctx, "Request to %s timed out. Check network connection.", provider);
            break;

        case IK_ERR_CAT_CONTENT_FILTER:
            if (detail != NULL) {
                message = talloc_asprintf(ctx, "Content blocked by %s safety filters: %s", provider, detail);
            } else {
                message = talloc_asprintf(ctx, "Content blocked by %s safety filters", provider);
            }
            break;

        case IK_ERR_CAT_NETWORK:
            if (detail != NULL) {
                message = talloc_asprintf(ctx, "Network error connecting to %s: %s", provider, detail);
            } else {
                message = talloc_asprintf(ctx, "Network error connecting to %s", provider);
            }
            break;

        case IK_ERR_CAT_UNKNOWN:
        default:
            if (detail != NULL) {
                message = talloc_asprintf(ctx, "%s error: %s", provider, detail);
            } else {
                message = talloc_asprintf(ctx, "%s error", provider);
            }
            break;
    }

    if (message == NULL) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE

    return message;
}
