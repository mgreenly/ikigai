#include <stdlib.h>
#include "foo.h"
#include "version.h"
#include "logger.h"

int
main (void)
{
  ik_log_info ("Hello from ikigai %s!", IK_VERSION);

  int result = add (5, 3);
  ik_log_info ("5 + 3 = %d", result);

  return EXIT_SUCCESS;
}
