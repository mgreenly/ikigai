#ifndef IK_CONFIG_H
#define IK_CONFIG_H

#include <inttypes.h>
#include <talloc.h>
#include "error.h"

typedef struct {
    char *openai_api_key;
    char *listen_address;
    uint16_t listen_port;
} ik_cfg_t;

res_t ik_cfg_load(TALLOC_CTX *ctx, const char *path);

// Internal helper function (exposed for testing)
res_t expand_tilde(TALLOC_CTX *ctx, const char *path);

#endif // IK_CONFIG_H
