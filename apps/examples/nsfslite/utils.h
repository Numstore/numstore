#include <stdlib.h>

static inline __attribute__ ((unused)) int *
int_range (size_t size)
{
  int *ret = malloc (size * sizeof (int));
  for (size_t i = 0; i < size; i++)
    {
      ret[i] = i;
    }
  return ret;
}

static inline __attribute__ ((unused)) int *
int_random (size_t size)
{
  int *ret = malloc (size * sizeof (int));
  return ret;
}

#define COMPARE(left, right, size)                                                                  \
  do                                                                                                \
    {                                                                                               \
      for (size_t i = 0; i < size; i++)                                                             \
        {                                                                                           \
          if ((left)[i] != (right)[i])                                                              \
            {                                                                                       \
              fprintf (stderr, "Mismatch at %zu: expected %d, got %d\n", i, (left)[i], (right)[i]); \
              ret = -1;                                                                             \
              goto cleanup;                                                                         \
            }                                                                                       \
        }                                                                                           \
    }                                                                                               \
  while (0)

#define CHECK(expr)                                                         \
  do                                                                        \
    {                                                                       \
      if ((expr) < 0)                                                       \
        {                                                                   \
          fprintf (stderr, "Failed: %s - %s\n", #expr, nsfslite_error (n)); \
          ret = -1;                                                         \
          goto cleanup;                                                     \
        }                                                                   \
    }                                                                       \
  while (0)

#define CHECKN(expr)                                                        \
  do                                                                        \
    {                                                                       \
      if ((expr) == NULL)                                                   \
        {                                                                   \
          fprintf (stderr, "Failed: %s - %s\n", #expr, nsfslite_error (n)); \
          ret = -1;                                                         \
          goto cleanup;                                                     \
        }                                                                   \
    }                                                                       \
  while (0)
