# Read Query Language

## Basic Shape

```
read <select>
  from <sources>
  [where <conditions>]
  [order by <ordering>]
  [slice <slice>]
```

---

## Select

What to return. Can be a single element, an sarray, or a struct.

```
read v1.a                                       -- single path
read [10](v1.a, v1.b, 0...)                     -- sarray (10 cols, zero-filled)
read struct { v1.a, v1.b as foo }               -- named struct
read struct { a.b + a.c as foo, d.e }           -- expressions 
```

Alias defaults to the leaf name: `v1.a.b` → `b`, `v1.a` → `a`.

Use `as` to override: `v1.a.b as foo`.

---

## From

Each source is a variable path, optionally aliased. Sources in `from` are implicitly wrapped in their own `read`.

```
-- These are equivalent:
from v1.a.b as a                             -- implicit read
from read v1.a.b as a                        -- explicit read
```

A source can be a struct constructor:

```
from struct { v1.a.b, v1.a as biz } as z
```

A source can be a subquery:

```
from (read v3.x from v3 as c where c.y > 5) as sub
```

A source can include a slice:

```
from v1[0:10:100] as b
from struct { v1.a.b, v1.a as biz }[0:10:100] as z
```

---

## Where

Filter rows. Boolean operators, comparisons, aggregates.

```
where v3.a > 10 && f.z == 10
where sum(v3.a) > 10
where a.x != 0 || b.y < 100
```

---

## Order By

Sort results. Each field is `asc` or `desc`.

```
order by foo asc
order by foo asc bar desc                    -- multi-field
```

---

## Slice

`[start:step:end]` — controls which rows to return.

```
slice [0:1:1000]                             -- rows 0..1000, step 1
slice [0:100:1000]                           -- every 100th row
```

Slices can also appear inline on sources or select expressions:

```
biz[0:-1:2]                                  -- reverse slice on a field
v1[0:10:100]                                 -- slice on a source
```

---

## Full Example

```
read struct { a.b + a.c as foo, (b.c * sqrt(1/b.d)) as bar, d.e, biz[0:-1:2], z }
  from
    v1.a.b                                       as a
    struct { v1.a.b, v1.a as biz }[0:10:100]     as z
    v1[0:10:100]                                 as b
    v3                                           as d
    (read [10](c.d, f.f, 0...) slice [0:1:1000]
      from
        v3 as c
        v5 as f
      where
        sum(v3.a) > 10 && f.z == 10
    )                                            as biz
  order by
    foo asc
    bar desc
  slice
    [0:100:1000]
```

### Equivalent Expanded Form

Each `from` source is its own `read`:

```
read struct { a.b + a.c as foo, (b.c * sqrt(1/b.d)) as bar, d.e, biz[0:-1:2], z }
  from
    read v1.a.b                                  as a
    read struct { v1.a.b, v1.a as biz }          as z
    read v1[0:10:100]                            as b
    read v3                                      as d
    (read [10](c.d, f.f, 0...) slice [0:1:1000]
      from
        read v3 as c
        read v5 as f
      where
        sum(v3.a) > 10 && f.z == 10
    )                                            as biz
  order by
    foo asc
    bar desc
  slice
    [0:100:1000]
```

### Equivalent With CTE (preview)

Subqueries can be lifted out with `with`:

```
with biz as (
  read [10](c.d, f.f, 0...) slice [0:1:1000]
    from
      read v3 as c
      read v5 as f
    where
      sum(v3.a) > 10 && f.z == 10
)
read struct { a.b + a.c as foo, (b.c * sqrt(1/b.d)) as bar, d.e, biz[0:-1:2], z }
  from
    read v1.a.b                                  as a
    read struct { v1.a.b, v1.a as biz }          as z
    read v1[0:10:100]                            as b
    read v3                                      as d
  order by
    foo asc
    bar desc
  slice
    [0:100:1000]
```
