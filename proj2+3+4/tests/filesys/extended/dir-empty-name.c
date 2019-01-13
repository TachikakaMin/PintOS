/* Tries to create a directory named as the empty string,
   which must return failure. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
#include "../../../lib/user/syscall.h"
#include "../../lib.h"

void
test_main (void) 
{
  CHECK (!mkdir (""), "mkdir \"\" (must return false)");
}
