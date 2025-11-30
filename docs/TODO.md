The OpenAPI over the wire JSON is read into 
  - `ik_openai_http_response_t`

The internal canonical database/memory message format is named
  - `ik_openai_msg_t`

The function ik_openai_chat_create converts the first into the second
  - `ik_openai_http_response_t` TO `ik_openai_msg_t`

The `ik_openai_msg_t` needs to be renamed to `ik_internal_msg_t`

