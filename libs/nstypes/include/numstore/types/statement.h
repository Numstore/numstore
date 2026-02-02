#pragma once

#include <numstore/core/stride.h>
#include <numstore/types/sarray.h>
#include <numstore/types/type_accessor.h>
#include <numstore/types/type_accessor_list.h>
#include <numstore/types/types.h>
#include <numstore/types/vref.h>

struct statement
{
  enum stmnt_type
  {
    ST_CREATE,
    ST_DELETE,
    ST_INSERT,
    ST_APPEND,
    ST_READ,
    ST_WRITE,
    ST_REMOVE,
    ST_TAKE,
  } type;

  union
  {
    struct create_stmt
    {
      struct string vname;
      struct type vtype;
    } create;

    struct delete_stmt
    {
      struct string vname;
    } delete;

    struct insert_stmt
    {
      struct string vname;
      b_size ofst;
      b_size nelems;
    } insert;

    struct read_stmt
    {
      struct vref_list vrefs;
      struct type_accessor_list acc;
      struct user_stride gstride;
    } read;

    struct take_stmt
    {
      struct vref_list vrefs;
      struct type_accessor_list acc;
      struct user_stride gstride;
    } take;

    struct remove_stmt
    {
      struct vref ref;
      struct user_stride gstride;
    } remove;

    struct write_stmt
    {
      struct vref vref;
      struct type_accessor_list acc;
      struct user_stride gstride;
    } write;
  };
};

///////////////////////////////////////////////////
/////////// CREATE Statement Builder

struct create_builder
{
  struct string vname;
  struct type vtype;
  struct chunk_alloc *persistent;
};

void crb_create (struct create_builder *dest, struct chunk_alloc *persistent);

err_t crb_accept_vname (struct create_builder *dest, struct string vname, error *e);
err_t crb_accept_type (struct create_builder *dest, struct type t, error *e);

err_t crb_build (struct create_stmt *dest, struct create_builder *builder, error *e);

///////////////////////////////////////////////////
/////////// DELETE Statement Builder

struct delete_builder
{
  struct string vname;
  struct chunk_alloc *persistent;
};

void dlb_create (struct delete_builder *dest, struct chunk_alloc *persistent);

err_t dlb_accept_vname (struct delete_builder *dest, struct string vname, error *e);

err_t dlb_build (struct delete_stmt *dest, struct delete_builder *builder, error *e);

///////////////////////////////////////////////////
/////////// INSERT Statement Builder

struct insert_builder
{
  struct string vname;
  b_size ofst;
  b_size nelems;
  struct chunk_alloc *persistent;
};

void inb_create (struct insert_builder *dest, struct chunk_alloc *persistent);

err_t inb_accept_vname (struct insert_builder *dest, struct string vname, error *e);
err_t inb_accept_ofst (struct insert_builder *dest, b_size ofst, error *e);
err_t inb_accept_nelems (struct insert_builder *dest, b_size nelems, error *e);

err_t inb_build (struct insert_stmt *dest, struct insert_builder *builder, error *e);

///////////////////////////////////////////////////
/////////// APPEND Statement Builder

struct append_builder
{
  struct string vname;
  b_size nelems;
  struct chunk_alloc *persistent;
};

void apb_create (struct append_builder *dest, struct chunk_alloc *persistent);

err_t apb_accept_vname (struct append_builder *dest, struct string vname, error *e);
err_t apb_accept_nelems (struct append_builder *dest, b_size nelems, error *e);

err_t apb_build (struct insert_stmt *dest, struct append_builder *builder, error *e);

///////////////////////////////////////////////////
/////////// READ Statement Builder

struct read_builder
{
  struct vref_list vrefs;
  struct type_accessor_list acc;
  struct user_stride gstride;
  bool has_gstride;
};

void rdb_create (
    struct read_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent);

err_t rdb_accept_vref_list (
    struct read_builder *builder,
    struct vref_list list,
    error *e);

err_t rdb_accept_accessor_list (
    struct read_builder *builder,
    struct type_accessor_list *acc,
    error *e);

err_t rdb_accept_stride (
    struct read_builder *builder,
    struct user_stride stride,
    error *e);

err_t rdb_build (struct read_stmt *dest, struct read_builder *builder, error *e);

///////////////////////////////////////////////////
/////////// TAKE Statement Builder

struct take_builder
{
  struct vref_list vrefs;
  struct type_accessor_list accs;
  struct user_stride gstride;
  bool has_gstride;
};

void tkb_create (
    struct take_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent);

err_t tkb_accept_vref_list (
    struct take_builder *builder,
    struct vref_list vrefs,
    error *e);

err_t tkb_accept_accessor_list (
    struct take_builder *builder,
    struct type_accessor_list *acc,
    error *e);

err_t tkb_accept_stride (
    struct take_builder *builder,
    struct user_stride stride,
    error *e);

err_t tkb_build (struct take_stmt *dest, struct take_builder *builder, error *e);

///////////////////////////////////////////////////
/////////// WRITE Statement Builder

struct write_builder
{
  struct vref vref;
  struct type_accessor_list acc;
  struct user_stride gstride;
  bool has_gstride;
  struct chunk_alloc *persistent;
};

void wrb_create (
    struct write_builder *dest,
    struct chunk_alloc *temp,
    struct chunk_alloc *persistent);

err_t wrb_accept_vref (
    struct write_builder *builder,
    struct string name,
    struct string ref,
    error *e);

err_t wrb_accept_accessor_list (
    struct write_builder *builder,
    struct type_accessor_list *acc,
    error *e);

err_t wrb_accept_stride (
    struct write_builder *builder,
    struct user_stride stride,
    error *e);

err_t wrb_build (struct write_stmt *dest, struct write_builder *builder, error *e);

///////////////////////////////////////////////////
/////////// REMOVE Statement Builder

struct remove_builder
{
  struct vref vref;
  struct user_stride gstride;
  bool has_gstride;
  struct chunk_alloc *persistent;
};

void rmb_create (struct remove_builder *dest, struct chunk_alloc *persistent);

err_t rmb_accept_vref (struct remove_builder *builder, struct vref ref, error *e);

err_t rmb_accept_stride (struct remove_builder *builder, struct user_stride stride, error *e);

err_t rmb_build (struct remove_stmt *dest, struct remove_builder *builder, error *e);
