#ifndef IK_LIST_H
#define IK_LIST_H

#include <inttypes.h>

// Execute list operation and output JSON result to stdout
// Returns 0 on success, 1 on error
int32_t list_execute(void *ctx, const char *operation, const char *item);

#endif // IK_LIST_H
