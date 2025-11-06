#include <stdlib.h>
#include "version.h"
#include "logger.h"

int
main (void)
{
  ik_log_info ("ikigai client %s", IK_VERSION);

  return EXIT_SUCCESS;
}
