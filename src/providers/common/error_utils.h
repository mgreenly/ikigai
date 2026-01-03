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

/**
 * Convert error category enum to string
 *
 * @param category Error category
 * @return         Static string representation (no allocation)
 *
 * Returns static string literals for logging and debugging.
 */
const char *ik_error_category_name(int category);

/**
 * Check if error category should be retried
 *
 * @param category Error category
 * @return         true if retryable, false otherwise
 *
 * Retryable categories:
 * - IK_ERR_CAT_RATE_LIMIT - retry with provider's suggested delay
 * - IK_ERR_CAT_SERVER     - retry with exponential backoff
 * - IK_ERR_CAT_TIMEOUT    - retry immediately
 * - IK_ERR_CAT_NETWORK    - retry with exponential backoff
 *
 * Non-retryable categories (return false):
 * - IK_ERR_CAT_AUTH
 * - IK_ERR_CAT_INVALID_ARG
 * - IK_ERR_CAT_NOT_FOUND
 * - IK_ERR_CAT_CONTENT_FILTER
 * - IK_ERR_CAT_UNKNOWN
 */
bool ik_error_is_retryable(int category);

/**
 * Generate user-facing error message
 *
 * @param ctx      Talloc context for allocation
 * @param provider Provider name ("anthropic", "openai", "google")
 * @param category Error category
 * @param detail   Additional detail (may be NULL or empty)
 * @return         Allocated error message (never NULL)
 *
 * Generates helpful messages based on category. If detail is NULL
 * or empty, omits the detail portion.
 *
 * Memory: Allocated on provided talloc context.
 */
char *ik_error_user_message(TALLOC_CTX *ctx,
                             const char *provider,
                             int category,
                             const char *detail);

/**
 * Calculate retry delay for async retry via event loop
 *
 * @param attempt             Current retry attempt (1, 2, or 3)
 * @param provider_suggested_ms Provider's suggested delay from error response
 *                             (-1 if not provided)
 * @return                    Delay in milliseconds (always positive)
 *
 * Algorithm:
 * 1. If provider_suggested_ms > 0: Use provider's suggested delay
 * 2. Otherwise: Calculate exponential backoff with jitter:
 *    - Base delay: 1000ms * (2 ^ (attempt - 1))
 *    - Add random jitter: 0-1000ms (prevents thundering herd)
 *    - Attempt 1: 1000ms + jitter(0-1000ms)
 *    - Attempt 2: 2000ms + jitter(0-1000ms)
 *    - Attempt 3: 4000ms + jitter(0-1000ms)
 *
 * This delay is returned via the provider's timeout() method to the
 * REPL's select() call. The REPL does NOT call sleep(); instead,
 * select() naturally wakes after the timeout.
 */
int64_t ik_error_calc_retry_delay_ms(int32_t attempt, int64_t provider_suggested_ms);

#endif /* IK_PROVIDERS_COMMON_ERROR_UTILS_H */
