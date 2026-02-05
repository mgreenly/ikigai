/**
 * @file equivalence_fixtures.h
 * @brief Test fixtures for OpenAI equivalence validation
 *
 * Provides standard request fixtures used across equivalence tests.
 */

#ifndef IK_TESTS_OPENAI_EQUIVALENCE_FIXTURES_H
#define IK_TESTS_OPENAI_EQUIVALENCE_FIXTURES_H

#include "apps/ikigai/providers/provider.h"
#include <talloc.h>

/**
 * Create simple text request fixture
 *
 * Creates a request with a simple question that expects a text response.
 *
 * @param ctx Talloc context for allocation
 * @return    Request with "What is 2+2?"
 */
ik_request_t *ik_test_fixture_simple_text(TALLOC_CTX *ctx);

/**
 * Create tool call request fixture
 *
 * Creates a request with tool definitions and a prompt that should
 * trigger tool use.
 *
 * @param ctx Talloc context for allocation
 * @return    Request with tool definitions and triggering prompt
 */
ik_request_t *ik_test_fixture_tool_call(TALLOC_CTX *ctx);

/**
 * Create multi-turn conversation fixture
 *
 * Creates a request with user/assistant/user message sequence.
 *
 * @param ctx Talloc context for allocation
 * @return    Request with multi-turn conversation
 */
ik_request_t *ik_test_fixture_multi_turn(TALLOC_CTX *ctx);

/**
 * Create invalid model request fixture
 *
 * Creates a request with non-existent model name to trigger error.
 *
 * @param ctx Talloc context for allocation
 * @return    Request with invalid model
 */
ik_request_t *ik_test_fixture_invalid_model(TALLOC_CTX *ctx);

#endif /* IK_TESTS_OPENAI_EQUIVALENCE_FIXTURES_H */
