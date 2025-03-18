#include <stdio.h>



typedef enum {
    MyEnum_A, // "A"    MyEnum_B, // "Beau Gosse"    MyEnum_C, // "C"    MyEnum_Count
} MyEnum;

char *StringTable[MyEnum_Count] = {
    "A",
    "Beau Gosse",
    "C",
};

int
main(int Argc, char *Args[])
{
    printf("%s\n", StringTable[MyEnum_B]);
    
    return 0;
}
