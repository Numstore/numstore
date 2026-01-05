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
 *   Sketch code demonstrating various data access patterns for design exploration.
 */

#include "memcpy_types.h"

bool
memcpy_strinc_acc (
    u8 *dest,       // Destination (length == enough to fit the parameters)
    const u8 *src,  // Source array
    const u32 slen, // Length of src in bytes
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

/**
 * Jump around accessor
 *
 * Source is fixed length and known - dest is on demand - use an offset buffer
 * for source and write monotonically increasing to destination
 *
 * [_________++--++--++--++--++--_________+-+-+-+-+-+-+-+-+-_____+++++]
 * 					 ^													^
 * 			ofsts[1].ofst 								ofsts[0].ofst
 * 					 [  ofsts[1].acc    ]				  [  ofsts[0].acc  ]
 */

// Memcpy Jump State
struct mjmst
{
  u32 onum;        // Offset number we're currently on
  u32 lidx;        // Location within the current access pattern state
  u32 nread;       // Number of elements read in access pattern
  bool isskipping; // Is skipping within the access pattern
};

typedef struct
{
  u32 ofst;           // Source offsets for each element
  struct acc_pat acc; // Access Patterns for each offset
} mjmpl_entry;

// Memcpy Jump Plan
struct mjmpl
{
  mjmpl_entry *accs;
  u32 nacc; // Number of access patterns
};

bool
memcpy_jmp_acc (
    u8 *dest,       // Destination
    const u32 dlen, // Length of the destination buffer
    const u8 *src,  // Source array (length == enough to fit the parameters)
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

void
print_bytes (const char *label, u8 *data, u32 len)
{
  printf ("%s: ", label);
  for (u32 i = 0; i < len; i++)
    {
      printf ("%02x ", data[i]);
    }
  printf ("\n");
}

void
test_strinc_1 (void)
{
  printf ("\n=== Test 1: Simple read ===\n");

  u8 dest[100] = { 0 };
  u8 src[] = { 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF };

  struct mtipl plan = {
    .accs = (mtipl_entry[]){
        { 0, { 0, 1, 4, 0 }, 0 } },
    .nacc = 1
  };
  struct mstist state = { 0 };

  memcpy_strinc_acc (dest, src, sizeof (src), plan, &state);
  print_bytes ("Result", dest, 4);
}

void
test_strinc_2 (void)
{
  printf ("\n=== Test 2: Read with skip ===\n");

  u8 dest[100] = { 0 };
  u8 src[] = { 1, 2, 3, 99, 99, 4, 5, 6, 99, 99, 7, 8, 9 };

  struct mtipl plan = {
    .accs = (mtipl_entry[]){
        { 0, { 0, 3, 3, 2 }, 0 } },
    .nacc = 1
  };
  struct mstist state = { 0 };

  memcpy_strinc_acc (dest, src, sizeof (src), plan, &state);
  print_bytes ("Result", dest, 9);
}

void
test_strinc_3 (void)
{
  printf ("\n=== Test 3: With hop ===\n");

  u8 dest[100] = { 0 };
  u8 src[] = { 99, 99, 99, 0xDE, 0xAD, 0xBE, 0xEF };

  struct mtipl plan = {
    .accs = (mtipl_entry[]){
        { 3, { 0, 1, 4, 0 }, 0 } },
    .nacc = 1
  };
  struct mstist state = { 0 };

  memcpy_strinc_acc (dest, src, sizeof (src), plan, &state);
  print_bytes ("Result", dest, 4);
}

void
test_strinc_4 (void)
{
  printf ("\n=== Test 4: Multiple patterns ===\n");

  u8 dest[100] = { 0 };
  u8 src[] = { 99, 1, 2, 3, 4, 5, 6, 7, 8 };

  struct mtipl plan = {
    .accs = (mtipl_entry[]){
        { 1, { 0, 2, 2, 0 }, 0 },
        { 0, { 0, 1, 4, 0 }, 10 } },
    .nacc = 2
  };
  struct mstist state = { 0 };

  memcpy_strinc_acc (dest, src, sizeof (src), plan, &state);
  print_bytes ("Pattern 1", dest, 4);
  print_bytes ("Pattern 2", dest + 10, 4);
}

void
test_strinc_5 (void)
{
  printf ("\n=== Test 5: Single struct with nested arrays ===\n");

  struct sensor_data
  {
    u32 id;
    struct
    {
      int temps[10];
      int pressures[10];
    } sensors;
    u32 status;
  };

  struct sensor_data data = {
    .id = 1000,
    .sensors = {
        .temps = { 20, 21, 22, 23, 24, 25, 26, 27, 28, 29 },
        .pressures = { 100, 101, 102, 103, 104, 105, 106, 107, 108, 109 } },
    .status = 200
  };

  u8 dest[100] = { 0 };
  u32 temps_5_offset = offsetof (struct sensor_data, sensors.temps) + 5 * sizeof (int);
  u32 temps_read = 3 * sizeof (int);
  u32 pressures_7_offset = offsetof (struct sensor_data, sensors.pressures) + 7 * sizeof (int);

  struct mtipl plan = {
    .accs = (mtipl_entry[]){
        { temps_5_offset, { 0, 3, sizeof (int), 0 }, 0 },
        { pressures_7_offset - (temps_5_offset + temps_read), { 0, 2, sizeof (int), 0 }, 12 } },
    .nacc = 2
  };
  struct mstist state = { 0 };

  memcpy_strinc_acc (dest, (u8 *)&data, sizeof (data), plan, &state);

  int *temps = (int *)dest;
  int *pressures = (int *)(dest + 12);
  printf ("temps[5-7]: %d %d %d\n", temps[0], temps[1], temps[2]);
  printf ("pressures[7-8]: %d %d\n", pressures[0], pressures[1]);
}

void
test_jmp_1 (void)
{
  printf ("\n=== Test 6: Jump to offset ===\n");

  u8 dest[100] = { 0 };
  u8 src[50] = { 0 };
  src[10] = 0xCA;
  src[11] = 0xFE;
  src[12] = 0xBA;
  src[13] = 0xBE;

  struct mjmpl plan = {
    .accs = (mjmpl_entry[]){
        { 10, { 0, 1, 4, 0 } } },
    .nacc = 1
  };
  struct mjmst state = { 0 };

  memcpy_jmp_acc (dest, sizeof (dest), src, plan, &state);
  print_bytes ("Result", dest, 4);
}

void
test_jmp_2 (void)
{
  printf ("\n=== Test 7: Skip in source ===\n");

  u8 dest[100] = { 0 };
  u8 src[] = { 1, 2, 99, 99, 3, 4, 99, 99, 5, 6 };

  struct mjmpl plan = {
    .accs = (mjmpl_entry[]){
        { 0, { 0, 3, 2, 2 } } },
    .nacc = 1
  };
  struct mjmst state = { 0 };

  memcpy_jmp_acc (dest, sizeof (dest), src, plan, &state);
  print_bytes ("Result", dest, 6);
}

void
test_jmp_3 (void)
{
  printf ("\n=== Test 8: Multiple jumps ===\n");

  u8 dest[100] = { 0 };
  u8 src[100] = { 0 };
  src[20] = 0xAA;
  src[21] = 0xBB;
  src[50] = 0xCC;
  src[51] = 0xDD;
  src[52] = 0xEE;

  struct mjmpl plan = {
    .accs = (mjmpl_entry[]){
        { 20, { 0, 1, 2, 0 } },
        { 50, { 0, 1, 3, 0 } } },
    .nacc = 2
  };
  struct mjmst state = { 0 };

  memcpy_jmp_acc (dest, sizeof (dest), src, plan, &state);
  print_bytes ("Result", dest, 5);
}

void
test_jmp_4 (void)
{
  printf ("\n=== Test 9: Resumable ===\n");

  u8 dest_full[20] = { 0 };
  u8 src[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };

  struct mjmpl plan = {
    .accs = (mjmpl_entry[]){
        { 0, { 0, 3, 4, 0 } } },
    .nacc = 1
  };
  struct mjmst state = { 0 };

  u8 dest1[5];
  bool done = memcpy_jmp_acc (dest1, sizeof (dest1), src, plan, &state);
  printf ("First call done: %d\n", done);
  memcpy (dest_full, dest1, 5);

  done = memcpy_jmp_acc (dest_full + 5, 15, src, plan, &state);
  printf ("Second call done: %d\n", done);
  print_bytes ("Result", dest_full, 12);
}

void
test_jmp_5 (void)
{
  printf ("\n=== Test 10: Single struct with nested array ===\n");

  struct packet_data
  {
    u32 header;
    struct
    {
      u8 payload[32];
      u32 crc;
    } packets[3];
    u32 footer;
  };

  struct packet_data data = {
    .header = 0xDEADBEEF,
    .packets = {
        { { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41 }, 0x1111 },
        { { 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81 }, 0x2222 },
        { { 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121 }, 0x3333 } },
    .footer = 0xCAFEBABE
  };

  u8 dest[100] = { 0 };
  struct mjmpl plan = {
    .accs = (mjmpl_entry[]){
        { offsetof (struct packet_data, packets[0].payload) + 10, { 0, 1, 10, 0 } },
        { offsetof (struct packet_data, packets[1].payload) + 15, { 0, 1, 8, 0 } },
        { offsetof (struct packet_data, packets[2].payload) + 5, { 0, 1, 12, 0 } } },
    .nacc = 3
  };
  struct mjmst state = { 0 };

  memcpy_jmp_acc (dest, sizeof (dest), (u8 *)&data, plan, &state);

  printf ("Packet 0[10-19]: %d %d\n", dest[0], dest[9]);
  printf ("Packet 1[15-22]: %d %d\n", dest[10], dest[17]);
  printf ("Packet 2[5-16]: %d %d\n", dest[18], dest[29]);
}

int
main (void)
{
  printf ("╔══════════════════════════════════════╗\n");
  printf ("║  Memory Copy Accessor Tests          ║\n");
  printf ("╚══════════════════════════════════════╝\n");

  test_strinc_1 ();
  test_strinc_2 ();
  test_strinc_3 ();
  test_strinc_4 ();
  test_strinc_5 ();

  test_jmp_1 ();
  test_jmp_2 ();
  test_jmp_3 ();
  test_jmp_4 ();
  test_jmp_5 ();

  printf ("\n");
  return 0;
}
