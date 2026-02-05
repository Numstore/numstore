#include "numstore/core/cbuffer.h"
#include "numstore/core/lalloc.h"
#include "numstore/intf/os/memory.h"
#include "numstore/types/byte_accessor.h"
#include <numstore/nsdb/transform_predicate.h>

/**
void
trpr_init (struct transform_predicate *dest, struct transform_predicate_params params, error *e)
{
  dest->params = params;
  dest->singleoutlen = 0;
  for (u32 i = 0; i < params.alen; ++i)
    {
      dest->singleoutlen += params.acc[i]->size;
    }
  ASSERT (params.output->cap >= dest->singleoutlen);
}
*/

void
trpr_execute (struct transform_predicate *r)
{
  /**
  while (true)
    {
      for (u32 i = 0; i < r->params.vlen; ++i)
        {
          // Block on downstream
          // all inputs should have at least 1 element
          if (cbuffer_len (r->params.data[i]) < r->params.tsizes[i])
            {
              return;
            }
        }

      // Block on upstream
      // output must have space for one combined write
      if (cbuffer_avail (r->params.output) < r->singleoutlen)
        {
          return;
        }

      // Execute Predicate
      if (r->params.predicate (r->params.data, r->params.pctx))
        {
          // Write 1 element to destination buffer
          for (u32 i = 0; i < r->params.alen; ++i)
            {
              ba_memcpy_to (r->params.output, r->params.data[r->params.vnums[i]], r->params.acc[i]);
            }
        }
      // Clear all buffers and continue
      for (u32 i = 0; i < r->params.vlen; ++i)
        {
          // Consume 1 element from each
          cbuffer_fakeread (r->params.data[i], r->params.tsizes[i]);
        }
    }
  */
}
