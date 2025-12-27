/**
 * @file factory_test_stubs.h
 * @brief Stub declarations for provider-specific factories
 */

#ifndef IK_FACTORY_TEST_STUBS_H
#define IK_FACTORY_TEST_STUBS_H

#include <talloc.h>
#include "error.h"
#include "providers/provider.h"

// Stub implementations for testing
res_t ik_openai_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out);
res_t ik_anthropic_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out);
res_t ik_google_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out);

#endif // IK_FACTORY_TEST_STUBS_H
