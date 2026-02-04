#include <numstore/types/types.h>

#include <numstore/core/assert.h>
#include <numstore/test/testing.h>

DEFINE_DBG_ASSERT (
    struct enum_builder, enum_builder, s,
    {
      ASSERT (s);
    })

void
enb_create (struct enum_builder *dest, struct chunk_alloc *temp, struct chunk_alloc *persistent)
{
  *dest = (struct enum_builder){
    .head = NULL,
    .temp = temp,
    .persistent = persistent,
  };
}

static bool
enb_has_key_been_used (const struct enum_builder *eb, struct string key)
{
  for (struct llnode *it = eb->head; it; it = it->next)
    {
      struct k_llnode *kn = container_of (it, struct k_llnode, link);
      if (string_equal (kn->key, key))
        {
          return true;
        }
    }
  return false;
}

err_t
enb_accept_key (struct enum_builder *eb, struct string key, error *e)
{
  DBG_ASSERT (enum_builder, eb);

  if (key.len == 0)
    {
      return error_causef (
          e, ERR_INTERP,
          "Key length must be > 0");
    }

  if (enb_has_key_been_used (eb, key))
    {
      return error_causef (
          e, ERR_INTERP,
          "Key '%.*s' already used",
          key.len, key.data);
    }

  // Move key data into persistent memory
  key.data = chunk_alloc_move_mem (eb->persistent, key.data, key.len, e);
  if (key.data == NULL)
    {
      return e->cause_code;
    }

  u16 idx = (u16)list_length (eb->head);
  struct llnode *slot = llnode_get_n (eb->head, idx);
  struct k_llnode *node;
  if (slot)
    {
      node = container_of (slot, struct k_llnode, link);
    }
  else
    {
      node = chunk_malloc (eb->temp, 1, sizeof *node, e);
      if (!node)
        {
          return e->cause_code;
        }
      llnode_init (&node->link);
      node->key = (struct string){ 0 };
      if (!eb->head)
        {
          eb->head = &node->link;
        }
      else
        {
          list_append (&eb->head, &node->link);
        }
    }

  node->key = key;
  return SUCCESS;
}

err_t
enb_build (struct enum_t *persistent, struct enum_builder *eb, error *e)
{
  DBG_ASSERT (enum_builder, eb);
  ASSERT (persistent);

  u16 len = (u16)list_length (eb->head);
  if (len == 0)
    {
      return error_causef (e, ERR_INTERP, "no keys to build");
    }

  struct string *keys = chunk_malloc (eb->persistent, len, sizeof *keys, e);
  if (!keys)
    {
      return e->cause_code;
    }

  u16 i = 0;
  for (struct llnode *it = eb->head; it; it = it->next)
    {
      struct k_llnode *kn = container_of (it, struct k_llnode, link);
      keys[i++] = kn->key;
    }

  persistent->len = len;
  persistent->keys = keys;

  return SUCCESS;
}

#ifndef NTEST
TEST (TT_UNIT, enum_builder)
{
  error err = error_create ();

  /* provide two simple heap allocators for builder + strings */
  struct chunk_alloc persistent;
  chunk_alloc_create_default (&persistent);

  /* 0. freshly-created builder must be clean */
  struct enum_builder eb;
  enb_create (&eb, &persistent, &persistent);
  test_fail_if (eb.head != NULL);

  /* 1. rejecting empty key */
  test_assert_int_equal (enb_accept_key (&eb, (struct string){ 0 }, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 2. accept first key "A" */
  struct string A = strfcstr ("A");
  test_assert_int_equal (enb_accept_key (&eb, A, &err), SUCCESS);

  /* 3. duplicate key "A" must fail */
  test_assert_int_equal (enb_accept_key (&eb, A, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  /* 4. accept a second key "B" */
  struct string B = strfcstr ("B");
  test_assert_int_equal (enb_accept_key (&eb, B, &err), SUCCESS);

  /* 5. build now that we have two keys */
  struct enum_t en = { 0 };
  test_assert_int_equal (enb_build (&en, &eb, &err), SUCCESS);
  test_assert_int_equal (en.len, 2);
  test_fail_if_null (en.keys);
  test_assert_int_equal (string_equal (en.keys[0], A) || string_equal (en.keys[1], A), true);
  test_assert_int_equal (string_equal (en.keys[0], B) || string_equal (en.keys[1], B), true);

  chunk_alloc_reset_all (&persistent);

  /* 6. build with empty builder must fail */
  struct enum_builder empty;
  enb_create (&empty, &persistent, &persistent);
  struct enum_t en2 = { 0 };
  test_assert_int_equal (enb_build (&en2, &empty, &err), ERR_INTERP);
  err.cause_code = SUCCESS;

  chunk_alloc_free_all (&persistent);
}
#endif
