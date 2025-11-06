#include <stdlib.h>
#include "version.h"
#include "logger.h"

int
main (void)
{
  ik_log_info ("Hello from ikigai-server %s!", IK_VERSION);
  return EXIT_SUCCESS;
}
