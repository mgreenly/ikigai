#ifndef IK_PROVIDERS_OPENAI_SHIM_H
#define IK_PROVIDERS_OPENAI_SHIM_H

#include "error.h"
#include "providers/provider.h"
#include <talloc.h>

/* Forward declaration to avoid including openai/client_multi.h */
typedef struct ik_openai_multi ik_openai_multi_t;

/**
 * OpenAI Provider Shim
 *
 * This module adapts the existing OpenAI client (src/openai/) to the
 * new unified provider interface (src/providers/provider.h).
 *
 * The shim context holds OpenAI-specific state needed to bridge between
 * the provider vtable and the existing OpenAI implementation.
 */

/**
 * OpenAI shim context
 *
 * Holds OpenAI-specific state for the provider vtable callbacks.
 * This context is the bridge between the generic provider interface
 * and the existing OpenAI client code.
 *
 * Memory ownership:
 * - Allocated as child of the provider struct
 * - api_key is child of this context
 * - multi is child of this context
 * - Single talloc_free on provider releases everything
 */
typedef struct {
    char *api_key;              /* OpenAI API key (owned) */
    ik_openai_multi_t *multi;   /* Multi-handle for async HTTP */
} ik_openai_shim_ctx_t;

/**
 * Create an OpenAI provider instance
 *
 * Factory function that creates a new provider with the OpenAI vtable.
 * The provider uses the existing OpenAI client code via the shim layer.
 *
 * Memory ownership:
 * - Provider allocated on ctx
 * - Shim context allocated on provider
 * - API key duplicated and owned by shim context
 *
 * @param ctx     Talloc context for allocation (parent of provider)
 * @param api_key OpenAI API key (must not be NULL)
 * @param out     Output: created provider (NULL on error)
 * @return        OK(provider) on success, ERR(...) on failure
 *
 * Error cases:
 * - ERR_MISSING_CREDENTIALS if api_key is NULL
 * - ERR_OUT_OF_MEMORY on allocation failure (via PANIC)
 * - ERR_NOT_IMPLEMENTED for stub vtable methods
 */
res_t ik_openai_create(TALLOC_CTX *ctx, const char *api_key, ik_provider_t **out);

/**
 * Destroy OpenAI shim context
 *
 * Cleanup function for the shim context. This is called from the
 * provider vtable cleanup method.
 *
 * NULL-safe: calling with NULL context is a no-op.
 *
 * Memory: All cleanup is handled by talloc hierarchy. This function
 * exists to provide a symmetrical API and for future cleanup needs.
 *
 * @param impl_ctx Shim context to destroy (may be NULL)
 */
void ik_openai_shim_destroy(void *impl_ctx);

#endif /* IK_PROVIDERS_OPENAI_SHIM_H */
