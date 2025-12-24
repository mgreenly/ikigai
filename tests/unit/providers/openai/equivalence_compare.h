/**
 * @file equivalence_compare.h
 * @brief Comparison functions for OpenAI equivalence validation
 *
 * Provides deep comparison functions for responses and stream events.
 */

#ifndef IK_TESTS_OPENAI_EQUIVALENCE_COMPARE_H
#define IK_TESTS_OPENAI_EQUIVALENCE_COMPARE_H

#include "providers/provider.h"
#include <stdbool.h>
#include <talloc.h>

/**
 * Comparison result with detailed diff information
 */
typedef struct {
    bool matches;                /* true if comparison passed */
    char *diff_message;          /* Human-readable diff (NULL if matches) */
} ik_compare_result_t;

/**
 * Compare two responses for equivalence
 *
 * Compares responses using tolerant matching rules:
 * - Content blocks: same count, same types, same text (exact match)
 * - Tool calls: same name, JSON-equivalent arguments (ID pattern may differ)
 * - Finish reason: must match exactly
 * - Token usage: within 5% tolerance
 * - Model: both return same model string
 *
 * @param ctx     Talloc context for diff message allocation
 * @param resp_a  First response
 * @param resp_b  Second response
 * @return        Comparison result (allocated on ctx)
 */
ik_compare_result_t *ik_compare_responses(TALLOC_CTX *ctx,
                                          const ik_response_t *resp_a,
                                          const ik_response_t *resp_b);

/**
 * Stream event array for comparison
 */
typedef struct {
    ik_stream_event_t *events;   /* Array of stream events */
    size_t count;                 /* Number of events */
} ik_stream_event_array_t;

/**
 * Compare two stream event sequences for equivalence
 *
 * Compares event sequences using tolerant matching rules:
 * - Event sequence: same event types in same order
 * - Text deltas: concatenated text matches
 * - Tool call events: same tool name, same final arguments
 * - Done event: same finish reason
 *
 * @param ctx       Talloc context for diff message allocation
 * @param events_a  First event sequence
 * @param events_b  Second event sequence
 * @return          Comparison result (allocated on ctx)
 */
ik_compare_result_t *ik_compare_stream_events(TALLOC_CTX *ctx,
                                               const ik_stream_event_array_t *events_a,
                                               const ik_stream_event_array_t *events_b);

/**
 * Check if token usage values are within tolerance
 *
 * Compares two token counts with 5% tolerance.
 *
 * @param a First value
 * @param b Second value
 * @return  true if within 5% tolerance
 */
bool ik_compare_token_usage_tolerant(int32_t a, int32_t b);

/**
 * Compare JSON strings for semantic equivalence
 *
 * Parses both JSON strings and compares their structure,
 * ignoring key order and whitespace differences.
 *
 * @param ctx     Talloc context for error message allocation
 * @param json_a  First JSON string
 * @param json_b  Second JSON string
 * @return        Comparison result (allocated on ctx)
 */
ik_compare_result_t *ik_compare_json_equivalent(TALLOC_CTX *ctx,
                                                 const char *json_a,
                                                 const char *json_b);

#endif /* IK_TESTS_OPENAI_EQUIVALENCE_COMPARE_H */
