#include "providers/provider.h"
#include <string.h>

const char *ik_infer_provider(const char *model_name)
{
    if (model_name == NULL) {
        return NULL;
    }

    // OpenAI models: gpt-*, o1-*, o3-*
    if (strncmp(model_name, "gpt-", 4) == 0) {
        return "openai";
    }
    if (strncmp(model_name, "o1-", 3) == 0) {
        return "openai";
    }
    if (strncmp(model_name, "o3-", 3) == 0) {
        return "openai";
    }

    // Anthropic models: claude-*
    if (strncmp(model_name, "claude-", 7) == 0) {
        return "anthropic";
    }

    // Google models: gemini-*
    if (strncmp(model_name, "gemini-", 7) == 0) {
        return "google";
    }

    // Unknown model
    return NULL;
}
