

#include "numstore/core/error.h"
#include <nsfilecli.h>
#include <string.h>

int
main (int argc, char **argv)
{
  struct nsfilecli_args args;

  error e = error_create ();

  nsfilecli_args_parse (&args, argc, argv, &e);

  if (e.cause_code)
    {
      return e.cause_code;
    }

  return 0;
}
