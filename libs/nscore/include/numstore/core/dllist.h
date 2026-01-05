#pragma once

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
 *   Doubly linked list data structure and operations. Provides push, append, pop,
 *   remove, reverse, and iteration utilities for intrusive list nodes.
 */

// core
#include <numstore/core/signatures.h>
#include <numstore/intf/types.h>

typedef struct dllnode_s
{
  struct dllnode_s *prev;
  struct dllnode_s *next;
} dllnode;

HEADER_FUNC void
dllnode_init (dllnode *n)
{
  n->prev = NULL;
  n->next = NULL;
}

HEADER_FUNC u32
dlist_length (const dllnode *head)
{
  u32 len = 0;
  for (const dllnode *cur = head; cur; cur = cur->next)
    {
      len++;
    }
  return len;
}

HEADER_FUNC void
dlist_push (dllnode **head, dllnode *n)
{
  n->next = *head;
  *head = n;
}

HEADER_FUNC void
dlist_append (dllnode **head, dllnode *n)
{
  n->next = NULL;
  if (!*head)
    {
      *head = n;
    }
  else
    {
      dllnode *cur = *head;
      while (cur->next)
        {
          cur = cur->next;
        }
      cur->next = n;
    }
}

HEADER_FUNC dllnode *
dlist_pop (dllnode **head)
{
  if (!*head)
    return NULL;
  dllnode *n = *head;
  *head = n->next;
  n->next = NULL;
  return n;
}

HEADER_FUNC dllnode *
dlist_tail (dllnode *head)
{
  if (!head)
    return NULL;
  while (head->next)
    {
      head = head->next;
    }
  return head;
}

HEADER_FUNC void
dlist_reverse (dllnode **head)
{
  dllnode *prev = NULL;
  dllnode *cur = *head;
  while (cur)
    {
      dllnode *next = cur->next;
      cur->next = prev;
      prev = cur;
      cur = next;
    }
  *head = prev;
}

HEADER_FUNC void
dlist_remove (dllnode **head, dllnode *n)
{
  dllnode **cur = head;
  while (*cur && *cur != n)
    {
      cur = &(*cur)->next;
    }
  if (*cur)
    {
      *cur = n->next;
      n->next = NULL;
    }
}

HEADER_FUNC dllnode *
dllnode_get_n (dllnode *head, u32 index)
{
  dllnode *cur = head;
  for (u32 i = 0; cur && i < index; ++i)
    {
      cur = cur->next;
    }
  return cur;
}

/* Iterate over list */
#define DLLIST_FOR_EACH(head, iter) \
  for (dllnode *iter = (head); iter; iter = iter->next)
