#ifndef IK_PROVIDERS_COMMON_ERROR_UTILS_H
#define IK_PROVIDERS_COMMON_ERROR_UTILS_H

/**
 * Common error utilities for provider adapters
 *
 * These utilities categorize errors, check retryability, generate
 * user-facing messages, and calculate retry delays for async retry
 * via the event loop.
 *
 * IMPORTANT: This header depends on types defined in providers/provider.h
 * Include providers/provider.h before including this header.
 */

#include <stdbool.h>
#include <stdint.h>

/* Forward declaration - TALLOC_CTX is defined in talloc.h */
#ifndef _TALLOC_H
typedef void TALLOC_CTX;
#endif

/* Note: ik_error_category_t is defined in providers/provider.h */
/* Users must include that header before including this one */



#endif /* IK_PROVIDERS_COMMON_ERROR_UTILS_H */
