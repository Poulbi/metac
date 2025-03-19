#include <stdio.h>



typedef enum {
    MyEnum_A,
    MyEnum_B,
    MyEnum_C,
    MyEnum_Count
} MyEnum;

char *StringTable[MyEnum_Count] = {
    "A",
    "Beau gosse",
    "C",
};

int
main(int Argc, char *Args[])
{
    printf("@: %s\n", StringTable[MyEnum_B]);
    
    return 0;
}
