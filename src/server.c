#include <stdio.h>
#include <stdlib.h>
#include "version.h"

int
main (void)
{
  printf ("Hello from ikigai-server %s!\n", IK_VERSION);
  return EXIT_SUCCESS;
}
