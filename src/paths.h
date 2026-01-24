#ifndef IK_PATHS_H
#define IK_PATHS_H

#include "error.h"
#include <talloc.h>

// Opaque type - struct defined privately in paths.c
typedef struct ik_paths_t ik_paths_t;

// Initialize path resolution from environment variables
// Reads IKIGAI_*_DIR environment variables set by wrapper script
// Returns ERR_INVALID_ARG if any required environment variable is missing
res_t ik_paths_init(TALLOC_CTX *ctx, ik_paths_t **out);

// Get specific directory paths
// All getters assert paths != NULL
// All getters return const char * (caller must NOT free)
// All getters never return NULL (paths_init guarantees all fields populated)
const char *ik_paths_get_bin_dir(ik_paths_t *paths);
const char *ik_paths_get_config_dir(ik_paths_t *paths);
const char *ik_paths_get_data_dir(ik_paths_t *paths);
const char *ik_paths_get_libexec_dir(ik_paths_t *paths);
const char *ik_paths_get_cache_dir(ik_paths_t *paths);
const char *ik_paths_get_state_dir(ik_paths_t *paths);
const char *ik_paths_get_tools_system_dir(ik_paths_t *paths);
const char *ik_paths_get_tools_user_dir(ik_paths_t *paths);
const char *ik_paths_get_tools_project_dir(ik_paths_t *paths);

// Expand ~ to $HOME in paths (PUBLIC API)
// ~/foo -> $HOME/foo
// /absolute -> /absolute (unchanged)
// relative -> relative (unchanged)
// Returns ERR_IO if HOME not set and tilde expansion needed
// Returns ERR_INVALID_ARG if path is NULL
res_t ik_paths_expand_tilde(TALLOC_CTX *ctx, const char *path, char **out);

#endif // IK_PATHS_H
