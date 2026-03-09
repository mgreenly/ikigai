/**
 * @file summary.c
 * @brief Context summary: boundaries, prompt, transcript, and generation
 */

#include "apps/ikigai/summary.h"
#include "apps/ikigai/msg.h"
#include "apps/ikigai/providers/request.h"
#include "shared/error.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include <talloc.h>

#include "shared/poison.h"

const char IK_SUMMARY_PROMPT[] =
    "You are summarizing a conversation for long-term memory. "
    "Capture key decisions and conclusions. "
    "Note unresolved questions and open issues. "
    "Preserve technical details: file names, function names, error messages, "
    "command output, and identifiers. "
    "Omit filler, pleasantries, and repetition. "
    "Be concise. Use bullet points for lists. "
    "Write in past tense from the perspective of the conversation participants.";

ik_summary_range_t ik_summary_boundaries(size_t message_count,
                                         size_t context_start_index)
{
    (void)message_count; /* epoch starts at 0 after /clear resets the array */

    if (context_start_index == 0) {
        return (ik_summary_range_t){ .start = 0, .end = 0 };
    }

    return (ik_summary_range_t){ .start = 0, .end = context_start_index };
}

char *ik_summary_transcript(TALLOC_CTX *ctx,
                            ik_msg_t * const *msgs,
                            size_t count)
{
    char *result = talloc_strdup(ctx, "");

    for (size_t i = 0; i < count; i++) {
        const ik_msg_t *msg = msgs[i];

        if (!ik_msg_is_conversation_kind(msg->kind)) {
            continue;
        }

        const char *content = msg->content ? msg->content : ""; // LCOV_EXCL_BR_LINE
        result = talloc_asprintf_append(result, "%s: %s\n", msg->kind, content);
    }

    return result;
}

/* Internal context for the completion callback */
typedef struct {
    TALLOC_CTX *ctx;
    char *summary;
    res_t result;
    bool done;
} generate_cb_ctx_t;

static res_t generate_completion_cb(const ik_provider_completion_t *completion,
                                    void *ctx)
{
    generate_cb_ctx_t *cb = ctx;
    cb->done = true;

    if (!completion->success) {
        const char *msg = completion->error_message ? completion->error_message : "unknown error"; // LCOV_EXCL_BR_LINE
        cb->result = ERR(cb->ctx, PROVIDER, "%s", msg);
        return OK(NULL);
    }

    /* Extract first text block from response */
    if (completion->response) {
        for (size_t i = 0; i < completion->response->content_count; i++) {
            if (completion->response->content_blocks[i].type == IK_CONTENT_TEXT) {
                const char *text = completion->response->content_blocks[i].data.text.text;
                cb->summary = talloc_strdup(cb->ctx, text ? text : ""); // LCOV_EXCL_BR_LINE
                break;
            }
        }
    }

    if (!cb->summary) {
        cb->result = ERR(cb->ctx, PROVIDER, "summary response contained no text content");
        return OK(NULL);
    }

    cb->result = OK(NULL);
    return OK(NULL);
}

/*
 * Truncate text at the last sentence boundary before max_tokens.
 * Estimation: max_bytes = max_tokens * 4.
 * Searches backwards from max_bytes for a period ('.') followed by whitespace
 * or end of string. Falls back to a hard cut at max_bytes if no boundary found.
 * Returns a new talloc string parented to ctx; the caller should free the old one
 * if it was separately allocated.
 */
static char *truncate_at_sentence(TALLOC_CTX *ctx, const char *text,
                                  int32_t max_tokens)
{
    size_t max_bytes = (size_t)max_tokens * 4;
    size_t len = strlen(text);

    if (len <= max_bytes) {
        return talloc_strdup(ctx, text);
    }

    /* Search backwards from max_bytes for a sentence boundary */
    size_t cut = max_bytes;
    while (cut > 0) {
        if (text[cut - 1] == '.') {
            /* Accept: period at end of string or followed by whitespace */
            if (cut == len || text[cut] == ' ' || text[cut] == '\n' || // LCOV_EXCL_BR_LINE
                text[cut] == '\t' || text[cut] == '\r') { // LCOV_EXCL_BR_LINE
                break;
            }
        }
        cut--;
    }

    /* No sentence boundary found — hard cut */
    if (cut == 0) {
        cut = max_bytes;
    }

    return talloc_strndup(ctx, text, cut);
}

res_t ik_summary_generate(TALLOC_CTX *ctx,
                          ik_msg_t * const *msgs,
                          size_t count,
                          ik_provider_t *provider,
                          const char *model,
                          int32_t max_tokens,
                          char **summary_out)
{
    /* Build transcript of conversation messages */
    char *transcript = ik_summary_transcript(ctx, msgs, count);

    /* Build the summarization request */
    ik_request_t *req = NULL;
    CHECK(ik_request_create(ctx, model, &req)); // LCOV_EXCL_BR_LINE
    CHECK(ik_request_add_system_block(req, IK_SUMMARY_PROMPT, false, IK_SYSTEM_BLOCK_BASE_PROMPT)); // LCOV_EXCL_BR_LINE
    CHECK(ik_request_add_message(req, IK_ROLE_USER, transcript)); // LCOV_EXCL_BR_LINE
    req->max_output_tokens = max_tokens;

    /* Callback context */
    generate_cb_ctx_t cb = {
        .ctx = ctx,
        .summary = NULL,
        .result = OK(NULL),
        .done = false,
    };

    /* Initiate the request */
    CHECK(provider->vt->start_request(provider->ctx, req, // LCOV_EXCL_BR_LINE
                                      generate_completion_cb, &cb));

    /* Drive the provider event loop synchronously until done */
    while (!cb.done) {
        fd_set read_fds, write_fds, exc_fds;
        FD_ZERO(&read_fds);
        FD_ZERO(&write_fds);
        FD_ZERO(&exc_fds);
        int max_fd = -1;

        provider->vt->fdset(provider->ctx, &read_fds, &write_fds, &exc_fds, &max_fd);

        long timeout_ms = 1000;
        provider->vt->timeout(provider->ctx, &timeout_ms);

        struct timeval tv = {
            .tv_sec = timeout_ms / 1000,
            .tv_usec = (timeout_ms % 1000) * 1000,
        };

        if (max_fd >= 0) {
            select(max_fd + 1, &read_fds, &write_fds, &exc_fds, &tv);
        } else {
            select(0, NULL, NULL, NULL, &tv);
        }

        int running_handles = 0;
        provider->vt->perform(provider->ctx, &running_handles);
        provider->vt->info_read(provider->ctx, NULL);

        if (running_handles == 0) { // LCOV_EXCL_BR_LINE
            break; /* No more active handles */
        }
    }

    if (is_err(&cb.result)) {
        return cb.result;
    }

    if (!cb.summary) {
        return ERR(ctx, PROVIDER, "summary generation did not produce text");
    }

    /* Truncation safety net */
    *summary_out = truncate_at_sentence(ctx, cb.summary, max_tokens);
    return OK(NULL);
}
