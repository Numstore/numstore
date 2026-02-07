#pragma once
#include <numstore/compiler/expression.h>
#include <numstore/core/chunk_alloc.h>
#include <numstore/core/stride.h>
#include <numstore/intf/types.h>
#include <numstore/rptree/rptree_cursor.h>
#include <numstore/types/type_accessor.h>
#include <numstore/var/var_cursor.h>

/**
 * read (a.b as foo, b.c as bar, d.e as biz)[0:100:1000]
 *   from
 *     variable1.a.b        as a,          // scope (1,0)
 *     variable1[0:10:100]  as b,          // scope (1,1)
 *     variable3            as d,          // scope (1,2)
 *     (read (c.d, f.f_col)[0:1:1000]     // scope (1,3)
 *        from variable3 as c,            // scope (2,0)
 *              variable5 as f            // scope (2,1)
 *        where
 *          c.e > 0 && c.f == 1 && f.d == 5
 *     ) as biz
 *   where
 *     a.b > 10 && sum(d.c) > 100 && a.c + d.f == 5
 *
 * vrds_insert(ds, "variable1", ta1, &e)
 * vrds_insert(ds, "variable3", ta3, &e)
 * vrds_insert(ds, "variable5", ta5, &e)
 *
 * // ── Scope (1,0): biz's read ──────────────────────
 * // FROM — leaf sources into this scope
 * vrds_insert_alias(ds, "c", "variable3", ta3, 1, 0, &e)
 * vrds_insert_alias(ds, "f", "variable5", ta5, 1, 0, &e)
 *
 * // WHERE — sees {c, f}
 * vrds_add_predicate(ds, pred_ce_gt_0, ctx, 1, 0, &e)
 * vrds_add_predicate(ds, pred_cf_eq_1, ctx, 1, 0, &e)
 *
 * // SELECT
 * vrds_append_output(ds, "biz", "d",     c_out, &acc_d,     1, 0, &e)
 * vrds_append_output(ds, "biz", "f_col", f_out, &acc_f_col, 1, 0, &e)
 * vrds_apply_stride(ds, stride_0_1_1000, 1, 0, VRDS_RESULT_SET, &e)
 *
 * // ── Scope (0,0): outer query ─────────────────────
 * // FROM — leaf sources + biz's output aliased in
 * vrds_insert_alias(ds, "a", "variable1", ta1_ab, 0, 0, &e)
 * vrds_insert_alias(ds, "b", "variable1", ta1,    0, 0, &e)
 * vrds_apply_stride(ds, stride_0_10_100, 0, 0, VRDS_SOURCE, "b", &e)
 * vrds_insert_alias(ds, "d", "variable3", ta3,    0, 0, &e)
 * // biz is aliased automatically from (1,0) output
 *
 * // WHERE — sees {a, b, d, biz}
 * vrds_add_predicate(ds, pred_ab_gt_10, ctx, 0, 0, &e)
 * vrds_add_predicate(ds, pred_sum_dc,   ctx, 0, 0, &e)
 * vrds_add_predicate(ds, pred_ac_df,    ctx, 0, 0, &e)
 *
 * // SELECT
 * vrds_append_output(ds, NULL, "foo",     a_out, &acc_b, 0, 0, &e)
 * vrds_append_output(ds, NULL, "bar",     b_out, &acc_c, 0, 0, &e)
 * vrds_append_output(ds, NULL, "biz_val", d_out, &acc_e, 0, 0, &e)
 * vrds_apply_stride(ds, stride_0_100_1000, 0, 0, VRDS_RESULT_SET, &e)
 */

struct var_r_ds;

/* ═══════════════════════════════════════════════════════
 * Scope coordinates
 *
 * vertical:   nesting depth (0 = outermost query)
 * horizontal: sibling index within that depth
 *
 *   (0,0)  outer query
 *   ├── (1,0)  FROM source a
 *   ├── (1,1)  FROM source b
 *   ├── (1,2)  FROM source d
 *   └── (1,3)  derived table (read ... as biz)
 *       ├── (2,0)  FROM source c
 *       └── (2,1)  FROM source f
 *
 * Visibility rules:
 *   - Siblings at the same depth CANNOT see each other
 *   - Children evaluate in isolation, results aliased into parent
 *   - Correlated subqueries walk up via explicit parent link
 *   - Column aliases visible only in ORDER phase
 * ═══════════════════════════════════════════════════════ */

enum vrds_phase
{
  VRDS_FROM,
  VRDS_WHERE,
  VRDS_GROUP,
  VRDS_HAVING,
  VRDS_SELECT,
  VRDS_ORDER,
};

struct vsrc
{
  struct cbuffer *input;
  struct byte_accessor *ba;
};

struct var_output
{
  struct cbuffer *output;
  struct type *type;
};

typedef bool (*pred_func) (struct cbuffer **buffers, void *ctx);

/* ═══════════════════════════════════════════════════════
 * Lifecycle
 * ═══════════════════════════════════════════════════════ */

struct var_r_ds *vrds_open (struct chunk_alloc *alloc, error *e);
err_t vrds_close (struct var_r_ds *ds, error *e);
err_t vrds_execute (struct var_r_ds *ds, error *e);

/* ═══════════════════════════════════════════════════════
 * Base variable registration (global, not scoped)
 * ═══════════════════════════════════════════════════════ */

err_t vrds_insert (
    struct var_r_ds *ds,
    struct string vname,
    struct type_accessor ta,
    error *e);

/* ═══════════════════════════════════════════════════════
 * Scoped alias insertion
 *
 * Places an alias into scope (v, h).
 * After all children at depth v evaluate, their aliases
 * become visible in the parent scope at depth v-1.
 *
 * Siblings at the same depth cannot see each other.
 * ═══════════════════════════════════════════════════════ */

err_t vrds_insert_alias (
    struct var_r_ds *ds,
    struct string alias,     /* local name (e.g. "c", "a") */
    struct string vname,     /* base variable name         */
    struct type_accessor ta, /* accessor/projection        */
    uint16_t vertical,
    uint16_t horizontal,
    error *e);

/* ═══════════════════════════════════════════════════════
 * Correlated subquery parent link
 *
 * Allows scope (child_v, child_h) to walk up and resolve
 * names from (parent_v, parent_h). Without this link,
 * child scopes are fully isolated.
 * ═══════════════════════════════════════════════════════ */

err_t vrds_set_parent (
    struct var_r_ds *ds,
    uint16_t child_v,
    uint16_t child_h,
    uint16_t parent_v,
    uint16_t parent_h,
    error *e);

/* ═══════════════════════════════════════════════════════
 * Predicates (WHERE / HAVING)
 *
 * Phase determines what names are resolvable:
 *   VRDS_WHERE:  table aliases only, type reductions ok,
 *                row aggregates ILLEGAL
 *   VRDS_HAVING: table aliases + row aggregates ok,
 *                column aliases still ILLEGAL
 *
 * Predicate is evaluated at scope (v, h) which determines
 * which aliases are visible. If parent is set (correlated),
 * unresolved names walk up the parent chain.
 * ═══════════════════════════════════════════════════════ */

err_t vrds_add_predicate (
    struct var_r_ds *ds,
    pred_func pred,
    void *ctx,
    uint16_t vertical,
    uint16_t horizontal,
    enum vrds_phase phase,
    error *e);

/* ═══════════════════════════════════════════════════════
 * Grouping (GROUP BY)
 *
 * Sets the grouping expressions at scope (v, h).
 * After this, only grouped columns and aggregates are
 * valid in HAVING and SELECT at this scope.
 * ═══════════════════════════════════════════════════════ */

err_t vrds_set_grouping (
    struct var_r_ds *ds,
    struct expression **group_exprs,
    uint16_t n_exprs,
    uint16_t vertical,
    uint16_t horizontal,
    error *e);

/* ═══════════════════════════════════════════════════════
 * Output construction (SELECT)
 *
 * Builds up the output type for scope (v, h) incrementally.
 * Each call appends one field to the output.
 *
 *   vname: output variable name (NULL for final output)
 *   alias: field label in the output struct
 *   input: the data source for this field
 *   acc:   accessor/projection to apply (stride within entry, etc.)
 *
 * Column aliases defined here become visible ONLY in
 * VRDS_ORDER phase at the same scope.
 * ═══════════════════════════════════════════════════════ */

err_t vrds_append_output (
    struct var_r_ds *ds,
    struct string vname,
    struct string alias,
    struct var_output input,
    struct type_accessor *acc,
    uint16_t vertical,
    uint16_t horizontal,
    error *e);

/* ═══════════════════════════════════════════════════════
 * Stride application
 *
 * VRDS_RESULT_SET: outer stride — filters/steps across
 *   the row set produced by this scope
 * VRDS_ENTRY_DATA: inner stride — applied within each
 *   entry's compound data before output
 *
 * (variable1)[0:10:100]        → VRDS_RESULT_SET
 * (variable1[0:10:100])        → VRDS_ENTRY_DATA
 * (variable1[0:5:100])[0:10]   → both, inner first
 * ═══════════════════════════════════════════════════════ */

err_t vrds_apply_stride (
    struct var_r_ds *ds,
    struct stride stride,
    uint16_t vertical,
    uint16_t horizontal,
    enum vrds_stride_target target,
    error *e);

/* ═══════════════════════════════════════════════════════
 * Ordering (ORDER BY)
 *
 * Added at scope (v, h) in VRDS_ORDER phase.
 * This is the ONLY phase that can resolve both
 * table aliases and column aliases.
 * ═══════════════════════════════════════════════════════ */

err_t vrds_add_ordering (
    struct var_r_ds *ds,
    struct expression *order_expr,
    bool descending,
    uint16_t vertical,
    uint16_t horizontal,
    error *e);

/* ═══════════════════════════════════════════════════════
 * Resolution / lookup
 *
 * Resolves an identifier at scope (v, h) during the given
 * phase. Enforces all visibility rules:
 *
 *   1. Check (v, h) symbol table
 *   2. If correlated: walk parent chain
 *   3. If phase == VRDS_ORDER: also check column aliases
 *   4. Otherwise: column aliases invisible
 *   5. Not found anywhere: returns NULL
 *
 * Siblings at the same vertical are never searched.
 * ═══════════════════════════════════════════════════════ */

struct vsrc *vrds_resolve (
    struct var_r_ds *ds,
    struct string ident,
    uint16_t vertical,
    uint16_t horizontal,
    enum vrds_phase phase);

struct var_output vrds_get_output (
    struct var_r_ds *ds,
    uint16_t vertical,
    uint16_t horizontal);

struct type *vrds_get_type (
    struct var_r_ds *ds,
    struct string alias,
    uint16_t vertical,
    uint16_t horizontal);

struct string vrds_get_vname (
    struct var_r_ds *ds,
    struct string alias,
    uint16_t vertical,
    uint16_t horizontal);
