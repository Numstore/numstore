# numstore Query Language v0.0.1

## CREATE

```
create a struct { x [100]f32, y [100]f32 };
create b [50] struct { id i32, value f32 };
create data [1000] struct { timestamp i64, value f32, quality u8 };
```

## READ

### Read All
```
read a;
read data;
```

### Read with Range
```
read a[10:100];
read data[0:500];
```

### Read with Stride
```
read a[0:50:2];
read data[0:1000:10];
read b[10:100:5];
```

### Read to binary file
```
read a[0:50:2] to file("data.bin";
```

## INSERT

### Insert Binary File
```
insert into a from file("data.bin");
insert into data from file("raw.bin");
```

## DELETE

### Delete Range
```
delete a[10:100];
delete data[500:600];
```

### Delete Range with Stride
```
delete a[0:50:2];
delete data[0:1000:10];
```

## WRITE

### Write Range to File
```
write a[10:100] to file("out.bin");
write data[0:500] to file("subset.bin");
```

### Write Range with Stride to File
```
write a[0:50:2] to file("out.bin");
write data[0:1000:10] to file("sampled.bin");
write b[10:100:5] to file("filtered.bin");
```

## Complete Example

```
create data [1000] struct { timestamp i64, value f32, quality u8 };

insert into data from file("raw.bin");

read data[0:100];

write data[0:1000:10] to file("sampled.bin");

delete data[500:600];

write data[0:500] to file("cleaned.bin");
```
