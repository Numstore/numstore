# nsfile

nsfile is the interactive REPL and CLI for working with numstore databases. It supports both an interactive prompt for exploratory work and direct CLI invocation for scripting and piping.

## Launch

```
$ ./nsfile <db> <wal>
> .quit
```

Opens the database and WAL file and drops into the interactive REPL. Type `.quit` to exit.

---

## General Command Format

Most commands follow this general structure:

```
COMMAND VARIABLE [as ALIAS][FIELD_SELECTION][SLICE] [SOURCE | DESTINATION...]
```

Commands that consume data take a single source. Commands that produce data can have one or more destinations. See Sources and Destinations below for details.

### Aliases

Variables can be bound to short alias names with `as`. When multiple variables are involved, aliases are used to reference them in field selections and slices:

```
> read variable1 as a [a.field][0:1:-1] .file(out.bin)
> read variable1 as a, variable2 as b [a.field, b][0:1:-1] .file(out.bin)
```

### Field Selection

Square brackets select specific fields from a variable. Multiple fields are laid out contiguously in the output:

```
[a]                          # entire variable
[a.field]                    # single field
[a.field1, a.field2, b]      # multiple fields from one or more variables
```

### Slicing

Slicing uses `[start:stride:end]` syntax. `-1` refers to the end of the variable. A bare index selects a single element.

```
[0:1:-1]       # all elements (start at 0, stride 1, to end)
[0:3:40]       # every 3rd element from 0 to 40
[10]           # single element at index 10
[5:-1]         # from index 5 to end
```

Multi-dimensional arrays can be sliced per dimension:

```
a.c[0:10][5]         # first 10 of dim 0, index 5 of dim 1
a.c[0:2:20][0:5]     # stride-2 slice of dim 0, first 5 of dim 1
```

### Sources and Destinations

Commands that consume data (insert, append, write) take a single **source**. Commands that produce data (read, take) can fan out to one or more **destinations**. Sources and destinations are specified with the same syntax — their role is determined by context.

#### `.file(path)`

Reads from or writes to a file on disk.

```
> insert variable1 0 100 .file(data.bin)                    # source: read from file
> read variable1[0:100] .file(out.bin)                      # destination: write to file
```

#### `.numstore(...)`

Executes an inner nsfile command, using its output as a source or piping data into it as a destination. The inner command has its own source/destination stripped — the `.numstore()` itself becomes that missing I/O.

As a **source** (inner command produces data):
```
> insert variable2 0 100 .numstore(read variable1[0:100])
# reads 100 elements from variable1, inserts into variable2

> append variable2 50 .numstore(take variable1[0:50])
# takes 50 elements from variable1 (destructive), appends to variable2
```

As a **destination** (inner command consumes data):
```
> read variable1[0:100] .numstore(insert variable2 0 100)
# reads from variable1, inserts into variable2

> take variable1[0:50] .numstore(append variable2 50)
# takes from variable1, appends to variable2
```

#### Multiple Destinations

Any command that produces output can fan out to multiple destinations simultaneously. The output is written to all of them:

```
> read variable1[0:100] .file(out.bin) .file(backup.bin)
# writes to both files

> read variable1[0:100] .file(out.bin) .numstore(insert variable2 0 100)
# writes to file and inserts into variable2

> read variable1[0:100] .file(out.bin) .numstore(insert variable2 0 100) .numstore(append variable3 100)
# writes to file, inserts into variable2, appends to variable3

> take variable1[0:100] .numstore(insert variable2 0 100) .numstore(append variable3 100)
# takes from variable1 (destructive), fans out to both variable2 and variable3
```

---

## Commands

### create

Creates a new variable with a given type. Types can be primitives, structs, unions, enums, or arrays of any of these, composed arbitrarily.

```
create NAME TYPE
```

```
> create variable1 struct { a i32, b f32, c [20][40] union { i f32, j i32 }, f enum { FOO, BAR } }
> create variable2 union { k cf128, l u8 }
> create variable3 u8
```

---

### delete

Deletes a variable and all of its data.

```
delete NAME
```

```
> delete variable2
```

---

### insert

Inserts data into a variable at a given offset. Reads binary input from a source. The data must match the variable's type and the number of elements specified.

```
insert NAME OFFSET COUNT SOURCE
```

`OFFSET` is the index to start writing at. `COUNT` is the number of elements to insert. `SOURCE` is `.file(path)` or `.numstore(...)`.

```
> insert variable1 10 100 .file(foo.bin)                          # from file
> insert variable1 10 100 .numstore(read variable2[0:100])        # from another variable
```

---

### append

Shorthand for `insert` with an offset of `-1` (end of variable). Appends data to the end.

```
append NAME COUNT SOURCE
```

```
> append variable1 100 .file(foo.bin)                          # from file
> append variable1 100 .numstore(read variable2[0:100])        # from another variable
```

---

### read

Reads data out of a variable and writes it to one or more destinations. Non-destructive — the data remains in the database.

```
read VARIABLE [as ALIAS][FIELD_SELECTION][SLICE] DESTINATION [DESTINATION...]
```

Multiple variables can be read simultaneously. Fields from any aliased variable can be selected and laid out contiguously in the output. Output fans out to all specified destinations.

```
> read variable1[0:3:40] .file(out.bin)
> read variable1 as a [a][0:3:40] .file(out.bin)
> read variable1 as a [a.a][0:3:40] .file(out.bin)
> read variable1 as a [a.a, a.c[0:10][5], a.f][0:3:40] .file(out.bin)
> read variable1 as a, variable2 as b [a.a, b, a.c[0:10][5], a.f][0:3:40] .file(out.bin)
> read variable1[0:100] .file(out.bin) .numstore(insert variable2 0 100)            # fan out to file and variable
> read variable1[0:100] .file(a.bin) .file(b.bin) .numstore(append variable2 100)   # fan out to two files and variable
```

---

### write

Overwrites data in place at the specified fields and slice. The shape of the incoming data must exactly match the target selection — mismatched shapes produce the same errors as `remove` on an invalid range.

```
write VARIABLE [as ALIAS][FIELD_SELECTION][SLICE] SOURCE
```

```
> write variable1 as a [a.b, a.c[0:10][0:3:40], a.f][0:10:100] .file(in.bin)
> write variable1 as a [a.b][0:10:100] .numstore(read variable2[0:100])        # overwrite from another variable
```

---

### take

Reads data out of a variable and writes it to one or more destinations, then removes any elements that were touched. Destructive — any variable or element referenced in the selection is removed after the data is written out.

```
take VARIABLE [as ALIAS][FIELD_SELECTION][SLICE] DESTINATION [DESTINATION...]
```

Syntax is identical to `read`. The difference is that all touched elements are deleted after the data is written out. Output fans out to all specified destinations before the removal happens.

```
> take variable1[0:3:40] .file(out.bin)
> take variable1 as a [a][0:3:40] .file(out.bin)
> take variable1 as a [a.a][0:3:40] .file(out.bin)
> take variable1 as a [a.a, a.c[0:10][5], a.f][0:3:40] .file(out.bin)
> take variable1 as a, variable2 as b [a.a, b, a.c[0:10][5], a.f][0:3:40] .file(out.bin)
> take variable1[0:100] .file(out.bin) .numstore(insert variable2 0 100)            # fan out then remove
> take variable1[0:100] .file(a.bin) .numstore(insert variable2 0 100) .numstore(append variable3 100)
```

---

### remove

Removes elements from a variable at the specified slice. Produces an error if the slice is out of range or invalid.

```
remove VARIABLE[SLICE]
```

```
> remove variable1[0:10:100]
> remove variable1[5:-1]          # remove from index 5 to end
```

---

## CLI Usage

The same operations are available as one-shot CLI commands for use in scripts and pipes:

```
./nsfile <db> <wal> "command"
```

```
./nsfile test.db test.wal "create variable1 u32"
cat file.bin | ./nsfile test.db test.wal "append variable1 100"
cat file.bin | ./nsfile test.db test.wal "insert variable1 10 100"
./nsfile test.db test.wal "remove variable1[0:10:100]"
./nsfile test.db test.wal "read variable1[0:3:40]" > out.bin
./nsfile test.db test.wal "take variable1[0:3:40]" > out.bin
./nsfile test.db test.wal "delete variable1"
```

When stdin is not a TTY, nsfile skips the REPL and executes the command directly.
