#include "apps/ikigai/config_env.h"

#include "shared/panic.h"

#include <inttypes.h>
#include <stdlib.h>
#include <talloc.h>


#include "shared/poison.h"
void ik_config_apply_env_overrides(ik_config_t *cfg)
{
    const char *env_db_host = getenv("IKIGAI_DB_HOST");
    if (env_db_host && env_db_host[0] != '\0') {
        talloc_free(cfg->db_host);
        cfg->db_host = talloc_strdup(cfg, env_db_host);
        if (!cfg->db_host) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    const char *env_db_port = getenv("IKIGAI_DB_PORT");
    if (env_db_port && env_db_port[0] != '\0') {
        char *endptr = NULL;
        long port_val = strtol(env_db_port, &endptr, 10);
        if (endptr != env_db_port && *endptr == '\0' && port_val >= 1 && port_val <= 65535) {
            cfg->db_port = (int32_t)port_val;
        }
    }

    const char *env_db_name = getenv("IKIGAI_DB_NAME");
    if (env_db_name && env_db_name[0] != '\0') {
        talloc_free(cfg->db_name);
        cfg->db_name = talloc_strdup(cfg, env_db_name);
        if (!cfg->db_name) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }

    const char *env_db_user = getenv("IKIGAI_DB_USER");
    if (env_db_user && env_db_user[0] != '\0') {
        talloc_free(cfg->db_user);
        cfg->db_user = talloc_strdup(cfg, env_db_user);
        if (!cfg->db_user) PANIC("Out of memory"); // LCOV_EXCL_BR_LINE
    }
}
