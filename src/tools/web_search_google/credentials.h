#ifndef WEB_SEARCH_GOOGLE_CREDENTIALS_H
#define WEB_SEARCH_GOOGLE_CREDENTIALS_H

#include <inttypes.h>

int32_t load_credentials(void *ctx, char **out_api_key, char **out_engine_id);

#endif
