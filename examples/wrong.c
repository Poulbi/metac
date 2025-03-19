#include <stdio.h>

@table(name, str) MyEnumTable
{
    { A "A" }
    { B "Beau gosse" }
    { C "C" }
}

char *StringTable[MyEnum_Count] = {
    @expand(MyEnumTabl a
            `    $(b.lolol),`
};