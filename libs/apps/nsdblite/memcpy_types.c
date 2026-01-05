/*
 * Copyright 2025 Theo Lincke
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Description:
 *   Implements memcpy_types.h. Provides resumable memory copy operations with
 *   complex access patterns including stride-based and jump-based copying for
 *   efficient data serialization and deserialization.
 */

#include <memcpy_types.h>

#include <numstore/core/assert.h>

#include <string.h>

// Core
// C Lib
bool
memcpy_strinc_acc (
    u8 *dest,
    const u8 *src,
    const u32 slen,
    const struct mtipl plan,
    struct mstist *state)
{
  u32 sidx = 0;

  while (true)
    {

    next_hop:
      if (state->hnum == plan.nacc)
        {
          return true;
        }

      // Finish the current hop
      {
        /**
         * [________________++--++--++--...]
         *     ^						^
         *    hidx			 target_hop
         *     |						|
         *      ------------
         *       Hop Remain
         *
         */
        u32 target_hop = plan.accs[state->hnum].hop;
        ASSERT (state->hidx <= target_hop);
        if (state->hidx < target_hop)
          {
            u32 hop_remain = target_hop - state->hidx;

            /**
             * Not enough to complete the hop
             * [__________]
             *     ^						^
             *    hidx			 target_hop
             */
            if (sidx + hop_remain > slen)
              {
                // Just move hidx to the end
                u32 consumed = slen - sidx;
                state->hidx += consumed;

                ASSERT (state->hidx < target_hop);

                return false;
              }

            // Otherwise complete the hop
            state->hidx += hop_remain;
            sidx += hop_remain;
          }
      }

      // Do Current Access Pattern
      {
        struct acc_pat acc = plan.accs[state->hnum].acc;
        u32 dofst = plan.accs[state->hnum].dofst;

        while (true)
          {
            ASSERT (state->nread <= acc.nreads);

            // Done reading this access pattern
            if (state->nread == acc.nreads)
              {
                state->hnum += 1;
                state->hidx = 0;
                state->lidx = 0;
                state->nread = 0;
                state->isskipping = false;
                goto next_hop;
              }

            /**
             * Skip bytes
             *
             * [++----------++----------++----------...]
             * 		 ^	      ^
             * 	  lidx		acc.skip
             */
            if (state->isskipping)
              {
                u32 skip_remain = acc.skip - state->lidx;

                /**
                 * Not enough room to complete the skip
                 *
                 * [++------]
                 * 		 ^	      ^
                 * 	  lidx		acc.skip
                 */
                if (sidx + skip_remain > slen)
                  {
                    u32 consumed = slen - sidx;
                    state->lidx += consumed;

                    ASSERT (state->lidx < acc.skip);

                    return false;
                  }
                else
                  {
                    sidx += skip_remain;
                    state->lidx = 0;
                    state->isskipping = false;
                    state->nread += 1;
                  }

                /**
                 * Read bytes
                 *
                 * [--++++++++++--++++++++++--++++++++++...]
                 * 		 ^	      ^
                 * 	  lidx		acc.read
                 */
              }
            else
              {
                u32 read_remain = acc.read - state->lidx;

                /**
                 * Not enough room to complete the read
                 *
                 * [--++++++++++--++++++++++--++++++++++...]
                 * 		 ^	      ^
                 * 	  lidx		acc.read
                 */
                if (sidx + read_remain > slen)
                  {
                    u32 consumed = slen - sidx;
                    memcpy (&dest[dofst + state->nread * acc.read + state->lidx], &src[sidx], consumed);
                    state->lidx += consumed;

                    ASSERT (state->lidx < acc.read);

                    return false;
                  }
                else
                  {
                    memcpy (&dest[dofst + state->nread * acc.read + state->lidx], &src[sidx], read_remain);
                    sidx += read_remain;
                    state->lidx = 0;
                    if (state->nread + 1 < acc.nreads)
                      {
                        state->isskipping = true;
                      }
                    else
                      {
                        state->nread += 1;
                      }
                  }
              }
          }
      }
    }
}

bool
memcpy_jmp_acc (
    u8 *dest,
    const u32 dlen,
    const u8 *src,
    const struct mjmpl plan,
    struct mjmst *state

)
{
  u32 didx = 0; // Destination index (bytes written to dest)

  while (true)
    {

    next_pattern:
      if (state->onum == plan.nacc)
        {
          return true;
        }

      // Do Current Access Pattern
      struct acc_pat acc = plan.accs[state->onum].acc;
      u32 sofst = plan.accs[state->onum].ofst;

      while (true)
        {
          ASSERT (state->nread <= acc.nreads);

          // Done reading this access pattern
          if (state->nread == acc.nreads)
            {
              state->onum += 1;
              state->lidx = 0;
              state->nread = 0;
              state->isskipping = false;
              goto next_pattern;
            }

          /**
           * Skip bytes
           *
           * [++----------++----------++----------...]
           * 		 ^	      ^
           * 	  lidx		acc.skip
           */
          if (state->isskipping)
            {
              state->lidx = 0;
              state->isskipping = false;
              state->nread += 1;

              /**
               * Read bytes
               *
               * [--++++++++++--++++++++++--++++++++++...]
               * 		 ^	      ^
               * 	  lidx		acc.read
               */
            }
          else
            {
              u32 read_remain = acc.read - state->lidx;

              /**
               * Not enough to completely read bytes
               *
               * [--++++]
               * 		 ^	      ^
               * 	  lidx		acc.read
               */
              if (didx + read_remain > dlen)
                {
                  u32 available = dlen - didx;
                  u32 src_offset = sofst + state->nread * (acc.read + acc.skip) + state->lidx;
                  memcpy (&dest[didx], &src[src_offset], available);
                  state->lidx += available;

                  ASSERT (state->lidx < acc.read);

                  return false;
                }
              else
                {
                  u32 src_offset = sofst + state->nread * (acc.read + acc.skip) + state->lidx;
                  memcpy (&dest[didx], &src[src_offset], read_remain);
                  didx += read_remain;
                  state->lidx = 0;
                  if (state->nread + 1 < acc.nreads)
                    {
                      state->isskipping = true;
                    }
                  else
                    {
                      state->nread += 1;
                    }
                }
            }
        }
    }
}
