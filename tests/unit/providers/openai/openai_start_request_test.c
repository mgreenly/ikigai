/**
 * @file openai_start_request_test.c
 * @brief Unit tests for OpenAI provider start_request (non-streaming) method
 *
 * Tests the non-streaming request interface through the provider vtable.
 */

#include <check.h>
#include <talloc.h>
#include "providers/openai/openai.h"
#include "providers/provider.h"

static TALLOC_CTX *test_ctx;

/* Completion callback for tests */
static res_t dummy_completion_cb(const ik_provider_completion_t *completion, void *ctx)
{
    (void)completion;
    (void)ctx;
    return OK(NULL);
}

static void setup(void)
{
    test_ctx = talloc_new(NULL);
}

static void teardown(void)
{
    talloc_free(test_ctx);
}

/* ================================================================
 * Start Request Tests
 * ================================================================ */

START_TEST(test_start_request_with_chat_api) {
    /* Create provider instance */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(provider);

    /* Build minimal request with text content */
    ik_content_block_t content = {
        .type = IK_CONTENT_TEXT,
        .data = {
            .text = {
                .text = talloc_strdup(test_ctx, "Hello!")
            }
        }
    };

    ik_message_t msg = {
        .role = IK_ROLE_USER,
        .content_blocks = &content,
        .content_count = 1,
        .provider_metadata = NULL
    };

    ik_request_t req = {
        .system_prompt = NULL,
        .messages = &msg,
        .message_count = 1,
        .model = talloc_strdup(test_ctx, "gpt-4"),
        .thinking = { .level = IK_THINKING_NONE, .include_summary = false },
        .tools = NULL,
        .tool_count = 0,
        .max_output_tokens = 100,
        .tool_choice_mode = 0,
        .tool_choice_name = NULL
    };

    /* Call start_request - should return immediately */
    r = provider->vt->start_request(provider->ctx, &req, dummy_completion_cb, NULL);

    /* Should return OK (request queued successfully) */
    ck_assert(!is_err(&r));

    /* Cleanup */
    talloc_free(provider);
}
END_TEST

START_TEST(test_start_request_with_responses_api) {
    /* Create provider with responses API enabled */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create_with_options(test_ctx, "sk-test-key", true, &provider);
    ck_assert(!is_err(&r));
    ck_assert_ptr_nonnull(provider);

    /* Build minimal request */
    ik_content_block_t content = {
        .type = IK_CONTENT_TEXT,
        .data = {
            .text = {
                .text = talloc_strdup(test_ctx, "Test message")
            }
        }
    };

    ik_message_t msg = {
        .role = IK_ROLE_USER,
        .content_blocks = &content,
        .content_count = 1,
        .provider_metadata = NULL
    };

    ik_request_t req = {
        .system_prompt = NULL,
        .messages = &msg,
        .message_count = 1,
        .model = talloc_strdup(test_ctx, "gpt-4"),
        .thinking = { .level = IK_THINKING_NONE, .include_summary = false },
        .tools = NULL,
        .tool_count = 0,
        .max_output_tokens = 100,
        .tool_choice_mode = 0,
        .tool_choice_name = NULL
    };

    /* Call start_request with responses API */
    r = provider->vt->start_request(provider->ctx, &req, dummy_completion_cb, NULL);

    /* Should return OK */
    ck_assert(!is_err(&r));

    /* Cleanup */
    talloc_free(provider);
}
END_TEST

START_TEST(test_start_request_with_o1_model) {
    /* Create provider instance */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(!is_err(&r));

    /* Build request with o1 model (should use responses API) */
    ik_content_block_t content = {
        .type = IK_CONTENT_TEXT,
        .data = {
            .text = {
                .text = talloc_strdup(test_ctx, "Test")
            }
        }
    };

    ik_message_t msg = {
        .role = IK_ROLE_USER,
        .content_blocks = &content,
        .content_count = 1,
        .provider_metadata = NULL
    };

    ik_request_t req = {
        .system_prompt = NULL,
        .messages = &msg,
        .message_count = 1,
        .model = talloc_strdup(test_ctx, "o1-preview"),
        .thinking = { .level = IK_THINKING_NONE, .include_summary = false },
        .tools = NULL,
        .tool_count = 0,
        .max_output_tokens = 100,
        .tool_choice_mode = 0,
        .tool_choice_name = NULL
    };

    /* Call start_request - o1 model prefers responses API */
    r = provider->vt->start_request(provider->ctx, &req, dummy_completion_cb, NULL);

    /* Should return OK */
    ck_assert(!is_err(&r));

    /* Cleanup */
    talloc_free(provider);
}
END_TEST

START_TEST(test_start_stream_with_chat_api) {
    /* Create provider instance */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(!is_err(&r));

    /* Dummy stream callback */
    res_t (*stream_cb)(const ik_stream_event_t *, void *) =
        (res_t (*)(const ik_stream_event_t *, void *))dummy_completion_cb;

    /* Build minimal request */
    ik_content_block_t content = {
        .type = IK_CONTENT_TEXT,
        .data = {
            .text = {
                .text = talloc_strdup(test_ctx, "Stream test")
            }
        }
    };

    ik_message_t msg = {
        .role = IK_ROLE_USER,
        .content_blocks = &content,
        .content_count = 1,
        .provider_metadata = NULL
    };

    ik_request_t req = {
        .system_prompt = NULL,
        .messages = &msg,
        .message_count = 1,
        .model = talloc_strdup(test_ctx, "gpt-4"),
        .thinking = { .level = IK_THINKING_NONE, .include_summary = false },
        .tools = NULL,
        .tool_count = 0,
        .max_output_tokens = 100,
        .tool_choice_mode = 0,
        .tool_choice_name = NULL
    };

    /* Call start_stream */
    r = provider->vt->start_stream(provider->ctx, &req, stream_cb, NULL,
                                   dummy_completion_cb, NULL);

    /* Should return OK */
    ck_assert(!is_err(&r));

    /* Cleanup */
    talloc_free(provider);
}
END_TEST

START_TEST(test_start_stream_with_responses_api) {
    /* Create provider with responses API */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create_with_options(test_ctx, "sk-test-key", true, &provider);
    ck_assert(!is_err(&r));

    /* Dummy stream callback */
    res_t (*stream_cb)(const ik_stream_event_t *, void *) =
        (res_t (*)(const ik_stream_event_t *, void *))dummy_completion_cb;

    /* Build minimal request */
    ik_content_block_t content = {
        .type = IK_CONTENT_TEXT,
        .data = {
            .text = {
                .text = talloc_strdup(test_ctx, "Stream test")
            }
        }
    };

    ik_message_t msg = {
        .role = IK_ROLE_USER,
        .content_blocks = &content,
        .content_count = 1,
        .provider_metadata = NULL
    };

    ik_request_t req = {
        .system_prompt = NULL,
        .messages = &msg,
        .message_count = 1,
        .model = talloc_strdup(test_ctx, "gpt-4"),
        .thinking = { .level = IK_THINKING_NONE, .include_summary = false },
        .tools = NULL,
        .tool_count = 0,
        .max_output_tokens = 100,
        .tool_choice_mode = 0,
        .tool_choice_name = NULL
    };

    /* Call start_stream with responses API */
    r = provider->vt->start_stream(provider->ctx, &req, stream_cb, NULL,
                                   dummy_completion_cb, NULL);

    /* Should return OK */
    ck_assert(!is_err(&r));

    /* Cleanup */
    talloc_free(provider);
}
END_TEST

START_TEST(test_start_stream_with_o1_model) {
    /* Create provider */
    ik_provider_t *provider = NULL;
    res_t r = ik_openai_create(test_ctx, "sk-test-key", &provider);
    ck_assert(!is_err(&r));

    /* Dummy stream callback */
    res_t (*stream_cb)(const ik_stream_event_t *, void *) =
        (res_t (*)(const ik_stream_event_t *, void *))dummy_completion_cb;

    /* Build request with o1 model */
    ik_content_block_t content = {
        .type = IK_CONTENT_TEXT,
        .data = {
            .text = {
                .text = talloc_strdup(test_ctx, "Test")
            }
        }
    };

    ik_message_t msg = {
        .role = IK_ROLE_USER,
        .content_blocks = &content,
        .content_count = 1,
        .provider_metadata = NULL
    };

    ik_request_t req = {
        .system_prompt = NULL,
        .messages = &msg,
        .message_count = 1,
        .model = talloc_strdup(test_ctx, "o1-preview"),
        .thinking = { .level = IK_THINKING_NONE, .include_summary = false },
        .tools = NULL,
        .tool_count = 0,
        .max_output_tokens = 100,
        .tool_choice_mode = 0,
        .tool_choice_name = NULL
    };

    /* Call start_stream - o1 prefers responses API */
    r = provider->vt->start_stream(provider->ctx, &req, stream_cb, NULL,
                                   dummy_completion_cb, NULL);

    /* Should return OK */
    ck_assert(!is_err(&r));

    /* Cleanup */
    talloc_free(provider);
}
END_TEST

/* ================================================================
 * Test Suite
 * ================================================================ */

static Suite *openai_start_request_suite(void)
{
    Suite *s = suite_create("OpenAI Start Request");

    TCase *tc_request = tcase_create("StartRequest");
    tcase_add_checked_fixture(tc_request, setup, teardown);
    tcase_add_test(tc_request, test_start_request_with_chat_api);
    tcase_add_test(tc_request, test_start_request_with_responses_api);
    tcase_add_test(tc_request, test_start_request_with_o1_model);
    suite_add_tcase(s, tc_request);

    TCase *tc_stream = tcase_create("StartStream");
    tcase_add_checked_fixture(tc_stream, setup, teardown);
    tcase_add_test(tc_stream, test_start_stream_with_chat_api);
    tcase_add_test(tc_stream, test_start_stream_with_responses_api);
    tcase_add_test(tc_stream, test_start_stream_with_o1_model);
    suite_add_tcase(s, tc_stream);

    return s;
}

int main(void)
{
    Suite *s = openai_start_request_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? 0 : 1;
}
