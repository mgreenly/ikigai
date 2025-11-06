#include <stdio.h>
#include <stdlib.h>
#include "foo.h"
#include "version.h"

int
main (void)
{
  printf ("Hello from ikigai %s!\n", IK_VERSION);

  int result = add (5, 3);
  printf ("5 + 3 = %d\n", result);

  return EXIT_SUCCESS;
}
