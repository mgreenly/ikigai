#ifndef IK_CONFIG_ENV_H
#define IK_CONFIG_ENV_H

#include "config.h"

// Apply environment variable overrides to database configuration
void ik_config_apply_env_overrides(ik_config_t *cfg);

#endif // IK_CONFIG_ENV_H
