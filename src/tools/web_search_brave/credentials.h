#ifndef WEB_SEARCH_BRAVE_CREDENTIALS_H
#define WEB_SEARCH_BRAVE_CREDENTIALS_H

#include <inttypes.h>

int32_t load_api_key(void *ctx, char **out_key);
void write_auth_error_json(void);

#endif
