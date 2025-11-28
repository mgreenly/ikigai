/* Helper functions for client_multi_info_read tests */

#ifndef CLIENT_MULTI_INFO_READ_HELPERS_H
#define CLIENT_MULTI_INFO_READ_HELPERS_H

#include "client_multi_test_common.h"

/* Helper to create a standard test config */
static inline ik_cfg_t *create_test_config(void)
{
    ik_cfg_t *cfg = talloc_zero(ctx, ik_cfg_t);
    cfg->openai_api_key = talloc_strdup(cfg, "sk-test");
    cfg->openai_model = talloc_strdup(cfg, "gpt-4");
    cfg->openai_temperature = 0.7;
    cfg->openai_max_completion_tokens = 1000;
    return cfg;
}

/* Helper to create a conversation with a single message */
static inline ik_openai_conversation_t *create_test_conversation(const char *msg_text)
{
    res_t conv_res = ik_openai_conversation_create(ctx);
    if (conv_res.is_err) return NULL;
    ik_openai_conversation_t *conv = conv_res.ok;

    res_t msg_res = ik_openai_msg_create(ctx, "user", msg_text);
    if (msg_res.is_err) return NULL;
    ik_openai_conversation_add_msg(conv, msg_res.ok);

    return conv;
}

/* Helper to setup a mock curl message */
static inline void setup_mock_curl_msg(CURLMsg *msg, CURL *handle, CURLcode result, long http_code)
{
    msg->msg = CURLMSG_DONE;
    msg->easy_handle = handle;
    msg->data.result = result;
    mock_curl_msg = msg;
    mock_http_response_code = http_code;
}

/* Helper to add a request with standard params */
static inline res_t add_test_request(ik_openai_multi_t *multi, ik_cfg_t *cfg,
                                     ik_openai_conversation_t *conv)
{
    return ik_openai_multi_add_request(multi, cfg, conv, NULL, NULL, NULL, NULL, NULL);
}

/* Completion callback that returns error */
static inline res_t error_completion_callback(const ik_http_completion_t *completion, void *callback_ctx)
{
    (void)completion;
    return ERR(callback_ctx, IO, "Completion callback error");
}

#endif /* CLIENT_MULTI_INFO_READ_HELPERS_H */
