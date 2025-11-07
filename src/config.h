#ifndef IK_CONFIG_H
#define IK_CONFIG_H

#include <talloc.h>
#include "error.h"

typedef struct
{
  char *openai_api_key;
  char *listen_address;
  int listen_port;
} ik_cfg_t;

ik_result_t ik_cfg_load (TALLOC_CTX * ctx, const char *path);

// Internal helper function (exposed for testing)
char *expand_tilde (TALLOC_CTX * ctx, const char *path);

#endif // IK_CONFIG_H
