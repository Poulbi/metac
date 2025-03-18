#include <stdio.h>

@table(name, str) MyEnumTable
{
    { A "A" }
    { B "Beau Gosse" }
    { C "C" }
}

typedef enum {
@expand(MyEnumTable a)
`    MyEnum_$(a.name),`
    MyEnum_Count
} MyEnum;

char *StringTable[MyEnum_Count] = {
@expand(MyEnumTable a)
`    $(a.str),`
};

int
main(int Argc, char *Args[])
{
    printf("@: %s\n", StringTable[MyEnum_B]);
    
    return 0;
}
