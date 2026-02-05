#include "apps/ikigai/fzy_wrapper.h"

#include "shared/panic.h"
#include "vendor/fzy/match.h"

#include <assert.h>
#include <stdlib.h>
#include <strings.h>


#include "shared/poison.h"
// Internal struct for sorting
typedef struct {
    size_t index;
    double score;
} scored_index_t;

// Comparison for qsort (descending by score)
static int compare_scores(const void *a, const void *b)
{
    const scored_index_t *sa = (const scored_index_t *)a;
    const scored_index_t *sb = (const scored_index_t *)b;
    if (sb->score > sa->score) return 1;
    if (sb->score < sa->score) return -1;
    return 0;
}

ik_fzy_result_t *ik_fzy_filter(TALLOC_CTX *ctx,
                               const char **candidates,
                               size_t candidate_count,
                               const char *search,
                               size_t max_results,
                               size_t *count_out)
{
    assert(ctx != NULL);  // LCOV_EXCL_BR_LINE
    assert(candidates != NULL);  // LCOV_EXCL_BR_LINE
    assert(search != NULL);  // LCOV_EXCL_BR_LINE
    assert(count_out != NULL);  // LCOV_EXCL_BR_LINE

    *count_out = 0;

    if (candidate_count == 0) return NULL;

    // Score all candidates
    scored_index_t *scored = talloc_zero_array(ctx, scored_index_t, (unsigned int)candidate_count);
    if (scored == NULL) PANIC("OOM");  // LCOV_EXCL_BR_LINE

    size_t search_len = strlen(search);
    size_t match_count = 0;
    for (size_t i = 0; i < candidate_count; i++) {
        // Check prefix match first (case-insensitive)
        if (strncasecmp(candidates[i], search, search_len) != 0) {
            continue;  // Skip candidates that don't start with search
        }

        if (has_match(search, candidates[i])) {
            scored[match_count].index = i;
            scored[match_count].score = match(search, candidates[i]);
            match_count++;
        }
    }

    if (match_count == 0) {
        talloc_free(scored);
        return NULL;
    }

    // Sort by score (descending)
    qsort(scored, match_count, sizeof(scored_index_t), compare_scores);

    // Limit results
    size_t result_count = (match_count < max_results) ? match_count : max_results;

    // Build result array
    ik_fzy_result_t *results = talloc_zero_array(ctx, ik_fzy_result_t, (unsigned int)result_count);
    if (results == NULL) PANIC("OOM");  // LCOV_EXCL_BR_LINE

    for (size_t i = 0; i < result_count; i++) {
        results[i].candidate = candidates[scored[i].index];
        results[i].score = scored[i].score;
    }

    talloc_free(scored);
    *count_out = result_count;
    return results;
}
