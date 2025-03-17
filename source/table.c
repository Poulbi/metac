@table(name, str) MyEnumTable
{
    { A "A" }
    { B "B" }
    { C "C" }
}

typedef enum {
@expand(MyEnumTable a)
    `MyEnum_$(a.name), // lololol`
MyEnum_Count
} MyEnum;

char *StringTable[MyEnumCount] = {
@expand(MyEnumTable a) `$(a.str),`
}
