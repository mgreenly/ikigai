/**
 * @file token_count.h
 * @brief Token count estimation from byte count
 */

#ifndef IK_TOKEN_COUNT_H
#define IK_TOKEN_COUNT_H

#include <stddef.h>
#include <stdint.h>

/**
 * Estimate token count from byte count.
 *
 * Uses the ~4 bytes/token heuristic as a conservative estimate.
 * Overcounts slightly relative to actual tokenizer output, which is
 * the safe direction for budget enforcement.
 *
 * @param byte_count  Number of bytes in the content
 * @return            Estimated token count (byte_count / 4, minimum 0)
 */
int32_t ik_token_count_from_bytes(size_t byte_count);

#endif /* IK_TOKEN_COUNT_H */
