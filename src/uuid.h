#ifndef UUID_H
#define UUID_H

#include <talloc.h>

// Generate a base64url-encoded UUID (22 characters)
char *ik_generate_uuid(TALLOC_CTX *ctx);

#endif
