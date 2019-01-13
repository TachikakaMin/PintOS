/* Open a file. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
#include "../lib.h"
#include "../../lib/user/syscall.h"

void
test_main (void) 
{
  int handle = open ("sample.txt");
  if (handle < 2)
    fail ("open() returned %d", handle);
}
