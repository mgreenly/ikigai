#include <stdlib.h>

#include <talloc.h>

#include "version.h"
#include "logger.h"
#include "config.h"

#define IK_CONFIG_PATH "~/.ikigai/config.json"

int
main (void)
{
  ik_log_info ("ikigai-server %s starting...", IK_VERSION);

  TALLOC_CTX *ctx = talloc_new (NULL);
  if (!ctx)
    {
      ik_log_fatal ("Failed to create talloc context");
    }

  ik_log_info ("Loading configuration from %s", IK_CONFIG_PATH);
  ik_result_t result = ik_cfg_load (ctx, IK_CONFIG_PATH);
  if (result.is_err)
    {
      ik_log_error ("Failed to load config: %s", result.err->msg);
      talloc_free (ctx);
      return EXIT_FAILURE;
    }
  ik_cfg_t *cfg = result.ok;
  ik_log_info ("openai_api_key: %s", cfg->openai_api_key);
  ik_log_info ("listen_address: %s", cfg->listen_address);
  ik_log_info ("listen_port: %u", cfg->listen_port);
  ik_log_info ("Server initialization complete");

  // ...

  talloc_free (ctx);
  return EXIT_SUCCESS;
}
