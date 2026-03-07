#ifndef MEM_H
#define MEM_H

#include <inttypes.h>
#include <stdbool.h>

typedef enum {
    MEM_ACTION_CREATE,
    MEM_ACTION_GET,
    MEM_ACTION_LIST,
    MEM_ACTION_DELETE,
} mem_action_t;

typedef struct {
    mem_action_t action;
    const char *id;    // For get and delete
    const char *body;  // For create (required)
    const char *title; // For create (optional)
} mem_params_t;

// Execute mem operation and output result to stdout
// Returns 0 on success (including errors reported as JSON)
int32_t mem_execute(void *ctx, const mem_params_t *params);

#endif
