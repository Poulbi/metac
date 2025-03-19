# metac: A table-driven preprocessor for C

## Overview
`metac` is my attempt at a C preprocessor that adds the features I would like C to have. It is
inspired by Ryan Fleury's article, see resources below.

### Features
- `@table` keyword for specifying data, eg.
```
@table(name, str) MyEnumTable
{
    { A "A" }
    { B "B" }
    { C "C" }
}
```
- `@expand` keyword for using data from a table, eg.
```
typedef enum
{
@expand(MyEnumTable t)
`    MyEnum_$a(a.name),`
    MyEnum_Count
} MyEnum;
```

## Build
Run the build script.
```sh
./source/build.sh
```

## Try it out
To chek the usage you can run withuot arguments.
```
./build/metac
Usage: ./build/metac filename [output_filename]
```
Run the program on the example file.
```sh
./build/metac examples/table.c
```
Checkout the newly created `table.meta.c`.  This is your metaprogram!

# Resources
- [Ryan Fleury - table-driven code generation](https://www.rfleury.com/p/table-driven-code-generation)
