#ifndef API_CALL_SETUP_H
#define API_CALL_SETUP_H

#include "response_processor.h"

#include "vendor/yyjson/yyjson.h"

#include <inttypes.h>

int32_t setup_and_execute_api_calls(void *ctx,
                                    const char *encoded_api_key,
                                    const char *encoded_engine_id,
                                    const char *encoded_query,
                                    int64_t num,
                                    int64_t start,
                                    size_t allowed_count,
                                    size_t blocked_count,
                                    yyjson_val *allowed_domains_val,
                                    yyjson_val *blocked_domains_val,
                                    struct api_call **out_calls,
                                    size_t *out_num_calls);

#endif
