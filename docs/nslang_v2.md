# numstore Query Language Reference

## Type Definition

```
create a struct { a [10]i32, b [20][20]u32 };
create b union { foo i32, b struct { c u32, d i32} };
create c [10] struct { a f32, c [20]i32, d [20][20]u32 };
```

---

## READ Operations

### Basic Read - All Data
```
read a;
read b;
read c;
```
SQL: `SELECT * FROM a`

### Range Read with Stride
```
read a[10:100:2];
read a[0:100:2] from a;
read (a)[10:100:2];
```
All equivalent. SQL: `SELECT * FROM a LIMIT 50 OFFSET 10 WHERE row_id % 2 == 0`

### Nested Field Access
```
read a.b;
read a.b[0:10];
read c.a[5:15:1];
read b.b.c;
read b.b.d[0:20];
```

### Nested Field Access with Parentheses (Type Filtering)
```
read (a.b);
read (a.b)[0:10];
read (a.b[0:10])[0:10];
read (c.a)[5:15:1];
read (b.b.c);
read (b.b.d)[0:20];
```
Parentheses filter internal type; outer `[]` applies global stride to result.
```
read (a.b)[0:100:2];     # Filter a.b's type, then stride result
read a.b[0:100:2];       # Stride applied to a.b's array directly
```

### Projections
```
read struct { a a.b }[0:100:2];
read struct { a a.b }[0:100:2] from a;
```
SQL: `SELECT a.b AS a FROM a WHERE ...`

### Projection - Direct Field Extract
```
read a.b[0:100:2];
read a.b[0:100:2] from a;
read c.d[10:20];
```

### Aliasing - Single Variable
```
read a[10:100:2] from variable1 as a;
read b[0:50] from input_table as b;
```

### Aliasing - Subquery Result
```
read struct { a.foo } [10:100:2] 
  from (read struct { foo b.c }[0:10]) as a;
```

### Aliasing - Multiple Sources
```
read struct { x a, y b }[0:100] 
  from variable1 as a, variable2 as b;
```

### Combining - Multiple Fields (Broadcast Modes)

#### Broadcast Zip (Default)
```
read struct { a a, b b }[0:100];
read struct { a a, b b }[0:100] BROADCAST zip;
read struct { x a.x, y b.y[0:10] }[0:50] BROADCAST zip;
```
Takes `min(len(a), len(b))`. Stops at shortest.

#### Broadcast Pad
```
read struct { a a, b b }[0:100] BROADCAST pad;
read struct { x a, y b.subset }[0:200] BROADCAST pad;
```
Takes `max(len(a), len(b))`. Pads shorter with nulls/zeros.

#### Broadcast Strict
```
read struct { a a, b b }[0:100] BROADCAST strict;
read struct { x a.field, y b.field }[0:50] BROADCAST strict;
```
Errors if `len(a) != len(b)`. Requires exact match.

### Complex Combining with Ranges
```
read struct { _c q[0:10], _b b.b.d, a a.a[0:10] }[:-1] from c as q;
read struct { first a[0:5], second b[10:20:2], third c.d } from var1 as a, var2 as b, var3 as c;
```

### Nested Subqueries - Simple
```
read a[0:100:2] 
  from (read (b.c, b.e) from variable1 as b) as a;
```

### Nested Subqueries - Multiple Sources
```
read a[0:100:2] 
  from (read (b.c, b.e) from variable1 as b, variable2 as c) as a;
```

### Nested Subqueries - Complex Projection
```
read struct { _c a._c[0:2], _b a._b.d }[0:10] 
  from (
    read struct { _c q[0:10], _b b.b, a a.a[0:10] }[:-1] 
      from c as q
  ) as a;
```

### Deeply Nested with Multiple Levels
```
read struct { result a.data }[0:50] 
  from (
    read struct { data x.subset[0:100] }[0:200] 
      from (
        read y[10:200:5] 
          from original_table as y
      ) as x
  ) as a;
```

### WHERE Conditions - Single Predicate
```
read a[0:1000] where a.a[0] < 10;
read b[0:500] where b.c > 100;
```

### WHERE Conditions - Multiple Predicates
```
read a[0:1000] where a.a[0] < 10 and a.b > 5;
read c[0:100] where c.x == 42 and c.y[0] < 200 and c.z >= 0;
```

### WHERE Conditions - Nested Field Access
```
read a[0:1000] where a.nested.field < 50;
read b[0:500] where b.array[0] > 10 and b.struct.value == 1;
```

### SELECT Multiple Output Columns
```
read [a.x, b.y[0:10]][0::10];
read [a.x, b.y, c.z[5:15]] from variable1 as a, variable2 as b, variable3 as c;
```

### SELECT with WHERE and Multiple Outputs
```
read [a.x, b.y[0:10]][0::10]
  from
    variable1 as a,
    variable2 as b
  where
    a.c < 10 and a.d < 1000;
```

### SELECT to File Output
```
read [a.x, b.y[0:10]][0::10]
  from
    variable1 as a,
    variable2 as b
  where
    a.c < 10 and a.d < 1000
  to
    file("out.bin"),
    file("out2.bin");
```

### SELECT to Multiple Outputs
```
read [a.x, b.y, c.z]
  to
    file("output1.bin"),
    stdout(),
    file("output2.bin");
```

---

## INSERT Operations

### Insert Static Values
```
insert into b values ({ .foo = 10 }, { .b = { .c = 10, .d = 11 } });
insert into a values ({ .a = 5, .b = 20 });
```

### Insert from Read
```
insert into a read { a c[0].c[0:-1:2], b c[10].d };
insert into target read struct { x source.field[0:100], y source.other };
```

### Insert from Subquery
```
insert into a read (
  read struct { col1 b.x, col2 b.y[0:50] }[0:100] 
    from source as b
);
```

### Insert with Multiple Selections
```
insert into table1 read [
  source1.field[0:100],
  source2.field[10:50]
];
```

---

## DELETE Operations

### Delete Range with Stride
```
remove a[0:10:200];
remove b[10:100:5];
```

### Delete Range Simple
```
remove a[0:100];
remove c[50:150];
```

### Delete All
```
remove a;
```

### Delete with Condition
```
remove a[0:1000] where a.field < 10;
remove b where b.status == 0;
```

---

## UPDATE Operations

### Update Range from Range
```
update a[0:100] read a[10:110];
update a[0:100] = a[10:110];
```

### Update Single Field
```
update a.b = 1;
update c.field = 42;
```

### Update Range with Scalar
```
update a[0:100] = a[10:110];
```

### Update with Function
```
update a.a[0] = sqrt(a.a[0]);
update b.field = abs(b.field) * 2;
```

### Update from Subquery Read
```
update a.a = (read c[0].c[0:-1:2]);
update table1.col = (read struct { val source.x }[0:100] from src as source);
```

### Update from Read with Broadcast Zip
```
update a.a = (read c[0].c[0:-1:2]) BROADCAST zip;
```

### Update from Read with Broadcast Pad
```
update a.a = (read c[0].c[0:-1:2]) BROADCAST pad;
```

### Update from Read with Broadcast Strict
```
update a.a = (read c[0].c[0:-1:2]) BROADCAST strict;
```

### Update Multiple Fields
```
update a set a.x = 10, a.y = 20, a.z = 30;
update b[0:50] set b.field1 = (read c[0]), b.field2 = 100;
```

### Update Range with WHERE Condition
```
update a[0:100] where a.status == 1 = a[10:110];
update b where b.value > 1000 = 0;
update c[0:50] where c.flag == 1 = (read d[0:50]);
```
WHERE filters rows before assignment. Only matching rows updated.

### Update Field with WHERE Condition
```
update a.field where a.status == 1 = 100;
update b.x where b.y > 50 = b.z * 2;
update c.nested.value where c.active == 1 = (read source.value);
```

### Update with Complex WHERE
```
update a[0:100] where a.status == 1 and a.priority > 5 = a[10:110];
update b where b.x > 10 and b.y < 100 = 0;
update c.field where c.a[0] > 0 and c.b == 1 = sqrt(c.field);
```

### Update with Broadcast and WHERE
```
update a.values where a.flag == 1 = (read source[0:100]) BROADCAST zip;
update b where b.active == 1 = (read c[0:50]) BROADCAST pad;
update d.data where d.status != 0 = (read e.original) BROADCAST strict;
```

---

## Complex Query Examples

### Multi-table Join with Projection
```
read struct { 
  user_id a.id, 
  user_name a.name, 
  score b.value[0:10] 
}[0:100]
  from variable1 as a, variable2 as b
  BROADCAST zip
  where a.active == 1 and b.valid == 1;
```

### Nested Read with Aliasing
```
read struct { result combined.output }[0:50]
  from (
    read struct {
      output a.data[0:100:2]
    }
    from input_table as a
  ) as combined;
```

### Read Multiple Columns to Files
```
read [
  a.column1[0:1000],
  a.column2[100:200:2],
  a.column3
][0:500]
  from data_table as a
  where a.quality > 0.5
  to file("col1.bin"), file("col2.bin"), file("col3.bin");
```

### Insert from Complex Read
```
insert into results read (
  read struct {
    computed_x a.x[0:100],
    computed_y a.y[0:100],
    computed_z a.z
  }[0:50]
  from source as a
  where a.valid == 1
);
```

### Update from Nested Read
```
update target.values = (
  read struct { 
    val (
      read a.original[0:100:2] 
      from source as a
    ) 
  }[0:50]
) BROADCAST pad;
```

### Conditional Delete and Update
```
remove a[0:1000] where a.status == 0;
update a[0:500] = a[500:1000] where a.flag == 1;
read a[0:100] where a.value > 0 to file("remaining.bin");
```
