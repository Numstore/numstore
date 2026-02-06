# Read Query C Library

## Core Types

```c
ns_result_set_t *rs;           // an executable/materialized query result
ns_result_set_builder_t *rsb;  // builder for constructing a query
```

## API

```c
ns_result_set_t *db_result_set(const char *var);          // base variable from db
ns_result_set_builder_t *new_result_set_builder(void);    // new builder

void add_from(ns_result_set_builder_t *rsb, ns_result_set_t *src, const char *alias);
void add_select(ns_result_set_builder_t *rsb, ns_select_expr_t *expr);
void add_where(ns_result_set_builder_t *rsb, ns_where_clause_t *wc);
void add_order(ns_result_set_builder_t *rsb, ns_order_expr_t *ord);
void add_slice(ns_result_set_builder_t *rsb, ns_slice_t slice);

ns_result_set_t *result_set_build(ns_result_set_builder_t *rsb);
```

## Build Order

A query is built bottom-up: leaf sources first, then compose into the outer query.

```
1. Create base variables with db_result_set()
2. Build inner/sub queries (from → where → select → slice)
3. Build outer query using inner results as sources
4. Add from → where → select → order → slice on outer
5. result_set_build()
```

## Struct Select

When selecting a struct, field names default to the leaf of the path.

```c
// struct { v1.a.b, v1.a as biz }
//   → field "b" (from v1.a.b), field "biz" (from v1.a)
add_select(rsb, STRUCT("b", PATH("v1.a.b"), "biz", PATH("v1.a")));
```

---

## Full Example

Building the equivalent of:

```
read struct { a.b + a.c as foo, (b.c * sqrt(1/b.d)) as bar, d.e, biz[0:-1:2], z }
  from
    v1.a.b                                       as a
    struct { v1.a.b, v1.a as biz }[0:10:100]     as z
    v1[0:10:100]                                 as b
    v3                                           as d
    (read [10](c.d, f.f, 0...) slice [0:1:1000]
      from v3 as c  v5 as f
      where sum(v3.a) > 10 && f.z == 10
    )                                            as biz
  order by foo asc  bar desc
  slice [0:100:1000]
```

```c
// ── Base variables ──────────────────────────────────────
ns_result_set_t *v1 = db_result_set("v1");
ns_result_set_t *v3 = db_result_set("v3");
ns_result_set_t *v5 = db_result_set("v5");

// ── Subquery: biz ───────────────────────────────────────
//   read [10](c.d, f.f, 0...) slice [0:1:1000]
//     from v3 as c, v5 as f
//     where sum(v3.a) > 10 && f.z == 10
ns_result_set_builder_t *rsb = new_result_set_builder();
add_from(rsb, v3, "c");
add_from(rsb, v5, "f");
add_where(rsb, wc);  // sum(v3.a) > 10 && f.z == 10
add_select(rsb, FIXED_TUPLE(10, PATH("c.d"), PATH("f.f"), FILL(0)));
add_slice(rsb, SLICE(0, 1, 1000));
ns_result_set_t *rs_biz = result_set_build(rsb);

// ── Source: a ───────────────────────────────────────────
//   read v1.a.b
rsb = new_result_set_builder();
add_from(rsb, v1, "v1");
add_select(rsb, PATH("v1.a.b"));
ns_result_set_t *rs_a = result_set_build(rsb);

// ── Source: z ───────────────────────────────────────────
//   read struct { v1.a.b, v1.a as biz }
rsb = new_result_set_builder();
add_from(rsb, v1, "v1");
add_select(rsb, STRUCT("b", PATH("v1.a.b"), "biz", PATH("v1.a")));
ns_result_set_t *rs_z = result_set_build(rsb);

// ── Source: b ───────────────────────────────────────────
//   read v1[0:10:100]
rsb = new_result_set_builder();
add_from(rsb, v1, "v1");
add_select(rsb, PATH("v1"));
add_slice(rsb, SLICE(0, 10, 100));
ns_result_set_t *rs_b = result_set_build(rsb);

// ── Source: d ───────────────────────────────────────────
//   read v3
rsb = new_result_set_builder();
add_from(rsb, v3, "v3");
add_select(rsb, PATH("v3"));
ns_result_set_t *rs_d = result_set_build(rsb);

// ── Top-level query ─────────────────────────────────────
rsb = new_result_set_builder();
add_from(rsb, rs_a,   "a");
add_from(rsb, rs_z,   "z");
add_from(rsb, rs_b,   "b");
add_from(rsb, rs_d,   "d");
add_from(rsb, rs_biz, "biz");
add_select(rsb, STRUCT(
    "foo", EXPR_ADD(PATH("a.b"), PATH("a.c")),
    "bar", EXPR_MUL(PATH("b.c"), EXPR_SQRT(EXPR_DIV(LITERAL(1), PATH("b.d")))),
    "e",   PATH("d.e"),
    "biz", SLICED(PATH("biz"), SLICE(0, -1, 2)),
    "z",   PATH("z")
));
add_order(rsb, ORDER_ASC("foo"));
add_order(rsb, ORDER_DESC("bar"));
add_slice(rsb, SLICE(0, 100, 1000));
ns_result_set_t *rs = result_set_build(rsb);
```
