// SPDX-License-Identifier: MIT

#ifndef IK_FZY_WRAPPER_H
#define IK_FZY_WRAPPER_H

#include <talloc.h>
#include <stddef.h>
#include <stdbool.h>

// Scored candidate result
typedef struct {
    const char *candidate;  // Points to original (not copied)
    double score;           // fzy score (higher = better match)
} ik_fzy_result_t;

// Filter and score candidates against a search string
// Returns array of results sorted by score (descending), limited to max_results
// Returns NULL if no matches found
// candidates: NULL-terminated array of strings to search
// search: the search string to match against
// max_results: maximum number of results to return (e.g., 10)
// count_out: set to number of results returned
ik_fzy_result_t *ik_fzy_filter(TALLOC_CTX *ctx,
                                const char **candidates,
                                size_t candidate_count,
                                const char *search,
                                size_t max_results,
                                size_t *count_out);

#endif
