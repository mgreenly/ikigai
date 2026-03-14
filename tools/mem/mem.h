#ifndef MEM_H
#define MEM_H

#include <inttypes.h>
#include <stdbool.h>

typedef enum {
    MEM_ACTION_CREATE,
    MEM_ACTION_GET,
    MEM_ACTION_LIST,
    MEM_ACTION_DELETE,
    MEM_ACTION_UPDATE,
    MEM_ACTION_REVISIONS,
    MEM_ACTION_REVISION_GET,
} mem_action_t;

typedef enum {
    MEM_SCOPE_DEFAULT,
    MEM_SCOPE_GLOBAL,
} mem_scope_t;

typedef struct {
    mem_action_t action;
    const char *path;        // Document identifier (required for create, get, delete, update, revisions, revision_get)
    const char *body;        // For create (required), update (required)
    mem_scope_t scope;       // default or global
    const char *title;       // Optional rename target for update
    const char *search;      // For list: full-text search query (mapped to ?q=)
    const char *revision_id; // For revision_get: revision identifier
    int32_t limit;           // For list: max results (default 0 = use server default)
    int32_t offset;          // For list: pagination offset
} mem_params_t;

// Execute mem operation and output result to stdout
// Returns 0 on success (including errors reported as JSON)
int32_t mem_execute(void *ctx, const mem_params_t *params);

#endif
