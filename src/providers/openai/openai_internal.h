/**
 * @file openai_internal.h
 * @brief Internal wrappers for openai.c testing
 *
 * This header provides MOCKABLE wrappers for internal functions used by openai.c.
 * These wrappers enable error injection for coverage testing.
 * Do not include this in production code - only in openai.c and its tests.
 */

#ifndef IK_PROVIDERS_OPENAI_INTERNAL_H
#define IK_PROVIDERS_OPENAI_INTERNAL_H

#include <talloc.h>
#include "error.h"
#include "providers/request.h"
#include "wrapper.h"

/**
 * Wrapper for ik_openai_serialize_chat_request
 * Enables mocking for error injection in tests
 */
MOCKABLE res_t ik_openai_serialize_chat_request_(TALLOC_CTX *ctx, const ik_request_t *req, bool stream,
                                                 char **out_json);

/**
 * Wrapper for ik_openai_serialize_responses_request
 * Enables mocking for error injection in tests
 */
MOCKABLE res_t ik_openai_serialize_responses_request_(TALLOC_CTX *ctx,
                                                      const ik_request_t *req,
                                                      bool stream,
                                                      char **out_json);

/**
 * Wrapper for ik_openai_build_chat_url
 * Enables mocking for error injection in tests
 */
MOCKABLE res_t ik_openai_build_chat_url_(TALLOC_CTX *ctx, const char *base_url, char **out_url);

/**
 * Wrapper for ik_openai_build_responses_url
 * Enables mocking for error injection in tests
 */
MOCKABLE res_t ik_openai_build_responses_url_(TALLOC_CTX *ctx, const char *base_url, char **out_url);

/**
 * Wrapper for ik_openai_build_headers
 * Enables mocking for error injection in tests
 */
MOCKABLE res_t ik_openai_build_headers_(TALLOC_CTX *ctx, const char *api_key, char ***out_headers);

#endif /* IK_PROVIDERS_OPENAI_INTERNAL_H */
