/**
 * @file stubs.c
 * @brief Stub implementations of provider-specific factories
 *
 * These stub implementations allow the codebase to compile and link
 * before the actual provider implementations are completed.
 * They return ERR_NOT_IMPLEMENTED for all provider creation attempts.
 *
 * This file will be removed once the actual provider implementations
 * (openai-core.md, anthropic-core.md, google-core.md) are complete.
 */

#include "providers/stubs.h"


#include "poison.h"
// OpenAI factory implementation moved to providers/openai/shim.c
// Anthropic factory implementation moved to providers/anthropic/anthropic.c
// Google factory implementation moved to providers/google/google.c
