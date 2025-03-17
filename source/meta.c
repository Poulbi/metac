/* 
 C preprocessor that provides meta functionality.

 To-Do's
 - [ ] Error messages instead of asserts
       - Show byte offset to show where the errors happens in the file and highlight that.
 - [ ] Expanding over multiple tables
 - [ ] Syntactic sugar to specify array of single values ?
 - [ ] Preserve indent when expansion is on a new line
*/


#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/mman.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
#define true 1
#define false 0

typedef struct {
    char *Memory;
    u64 Size;
} s8;
#define S8(str) { str, sizeof(str) - 1 }
#define S8_LIT(str) str, sizeof(str) - 1
#define S8_ARG(str) str.Memory, str.Size

#define Assert(expr) if (!(expr)) { raise(SIGTRAP); }

#define Kilobyte(byte) byte * 1024L
#define Megabyte(byte) Kilobyte(byte) * 1024L
#define Gigabyte(byte) Megabyte(byte) * 1024L

typedef struct {
    char *Memory;
    u64 Pos;
    u64 Size;
} arena;

char *
ArenaPush(arena* Arena, i64 Size)
{
    char *Result;
    Result = (char*)Arena->Memory + Arena->Pos;
    Arena->Pos += Size;
    Assert(Arena->Pos <= Arena->Size);
    return Result;
}

typedef struct {
    s8 Name;
    i32 LabelsCount;
    s8 *Labels;
    i32 ElementsCount;
    s8 *Elements;
} table;

typedef struct {
    i32 Start;
    i32 End;
} range;

s8 ReadEntireFileIntoMemory(char *Filepath)
{
    i32 Ret = 0;
    i32 FD = 0;
    s8 Result = {0};
    struct stat StatBuffer = {0};

    FD = open(Filepath, O_RDONLY);
    Assert(FD != -1);
    fstat(FD, &StatBuffer);
    Assert(Ret != -1);
    Result.Size = StatBuffer.st_size;

    Result.Memory = mmap(0, Result.Size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0); 
    Assert(Result.Memory);
    Ret = read(FD, Result.Memory, Result.Size);
    Assert(Ret != -1);

    return Result;
}

i32
IsWhitespace(char Ch)
{
    return(Ch == ' '  ||
           Ch == '\n' ||
           Ch == '\t');
}

void
PrintTable(table Table)
{
    // TODO: Print the table
    write(STDOUT_FILENO, S8_LIT("table("));
    for (u32 LabelsAt = 0;
         LabelsAt < Table.LabelsCount;
         LabelsAt++)
    {
        s8 Label = Table.Labels[LabelsAt];
        write(STDOUT_FILENO, Label.Memory, Label.Size);
        if (LabelsAt + 1 < Table.LabelsCount)
        {
            write(STDOUT_FILENO, S8_LIT(", "));
        }
    }
    write(STDOUT_FILENO, S8_LIT(") "));
    write(STDOUT_FILENO, Table.Name.Memory, Table.Name.Size);
    write(STDOUT_FILENO, S8_LIT("\n{\n"));
    for (i32 ElementAt = 0;
         ElementAt < Table.ElementsCount;
         ElementAt++)
    {
        write(STDOUT_FILENO, S8_LIT("\t{ "));
        for (i32 LabelAt = 0;
             LabelAt < Table.LabelsCount;
             LabelAt++)
        {
            s8 CurrentElement = Table.Elements[ElementAt * Table.LabelsCount + LabelAt]; 
            write(STDOUT_FILENO, CurrentElement.Memory, CurrentElement.Size);
            if (LabelAt + 1 < Table.LabelsCount)
            {
                write(STDOUT_FILENO, S8_LIT(" "));
            }
        }
        write(STDOUT_FILENO, S8_LIT(" }\n"));
    }
    write(STDOUT_FILENO, S8_LIT("}\n"));
}

int
main(int ArgC, char *Args[])
{
    char *Filename = 0;

    char *Storage = mmap(0, Megabyte(4), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    Assert(Storage);
    arena ScratchArena = {
        .Memory = Storage,
        .Pos = 0,
        .Size = Megabyte(1)
    };
    arena TablesArena = { 
        .Memory = Storage + ScratchArena.Size,
        .Pos = 0,
        .Size = Megabyte(1)
    };
    table *Tables = (table*)TablesArena.Memory;
    i32 TablesCount = 0;
    char *Out = ScratchArena.Memory + TablesArena.Size;

    if (ArgC > 1)
    {
        Filename = Args[1];
    }
#if 1
    else
    {
        Filename = "table.c";
    }
#endif

    // NOTE(luca): The memory is assumed to stay mapped until program exits, because we will use
    // pointers into that memory.
    s8 File = ReadEntireFileIntoMemory(Filename);
    char *In = File.Memory;

    for (i64 At = 0;
         At < File.Size;
         At++)
    {
        if (In[At] == '@')
        {
            At++;
            
            s8 TableKeyword = S8("table");
            s8 TableGenEnumKeyword = S8("table_gen_enum");
            s8 ExpandKeyword = S8("expand");
            s8 Keywords[] = { TableKeyword, TableGenEnumKeyword, ExpandKeyword };

            if (!strncmp(In + At, S8_ARG(ExpandKeyword)))
            {
                i32 ExpressionAt             = 0;
                s8 ExpressionTableName       = {0};
                i32 ExpressionTableNameAt    = 0;
                s8 ExpressionTableArgument    = {0};
                i32 ExpressionTableArgumentAt = 0;

                s8 ExpandArgument = {0};
                i32 ExpandArgumentAt = 0;
                s8 ExpandArgumentLabel = {0};
                i32 ExpandArgumentLabelAt = 0;
                range Expansion = {0};

                At += ExpandKeyword.Size;
                Assert(At < File.Size);
                Assert(In[At] == '(');
                At++;

                ExpressionTableNameAt = At;
                while (!IsWhitespace(In[At]) && At < File.Size) At++;
                Assert(At < File.Size);
                ExpressionTableName.Memory = In + ExpressionTableNameAt;
                ExpressionTableName.Size = At - ExpressionTableNameAt;

                while (IsWhitespace(In[At]) && At < File.Size) At++;
                Assert(At < File.Size);
                ExpressionTableArgumentAt = At;
                while (In[At] != ')' && At < File.Size) At++;
                Assert(At < File.Size);
                ExpressionTableArgument.Memory = In + ExpressionTableArgumentAt;
                ExpressionTableArgument.Size = At - ExpressionTableArgumentAt;
                At++;

                while (IsWhitespace(In[At]) && At < File.Size) At++;
                Assert(At < File.Size);
                Assert(In[At] == '`');
                At++;
                ExpressionAt = At;

                // TODO: multiple expansions in one expression
                while (In[At] != '`')
                {
                    if (In[At] == '$' && In[At + 1] == '(')
                    {
                        Expansion.Start = At;
                        At += 2;

                        ExpandArgumentAt = At;
                        while (In[At] != '.' && At < File.Size) At++;
                        Assert(At < File.Size);
                        ExpandArgument.Memory = In + ExpandArgumentAt;
                        ExpandArgument.Size = At - ExpandArgumentAt;
                        At++;

                        ExpandArgumentLabelAt = At;
                        while (In[At] != ')' && At < File.Size) At++;
                        Assert(At < File.Size);
                        ExpandArgumentLabel.Memory = In + ExpandArgumentLabelAt;
                        ExpandArgumentLabel.Size = At - ExpandArgumentLabelAt;

                        Expansion.End = At;
                        At++;
                        
                        // ExpressionAt|            |   Start    |     End    |            |     At     |
                        //                repeat              Labels              repeat

                        table *CurrentTable = 0;
                        for (i32 TableAt = 0;
                             TableAt < TablesCount;
                             TableAt++)
                        {
                            if (!strncmp(Tables[TableAt].Name.Memory, S8_ARG(ExpressionTableName)))
                            {
                                CurrentTable = Tables + TableAt;
                                break;
                            }
                        }
                        Assert(CurrentTable);

                        // TODO(now): Debug this
                        i32 LabelIndex = -1;
                        for (i32 LabelAt = 0;
                             LabelAt < CurrentTable->LabelsCount;
                             LabelAt++)
                        {
                            if (!strncmp(CurrentTable->Labels[LabelAt].Memory, S8_ARG(ExpandArgumentLabel)))
                            {
                                LabelIndex = LabelAt;
                                break;
                            }
                        }
                        Assert(LabelIndex != -1);

                        // TODO(now): the bug

                        while (In[At] != '`' && At < File.Size) At++;
                        Assert(File.Size);

                        for (i32 ElementAt = 0;
                             ElementAt < CurrentTable->ElementsCount;
                             ElementAt++)
                        {
                            s8 ExpansionText = CurrentTable->Elements[ElementAt * CurrentTable->LabelsCount + LabelIndex];
                            write(STDOUT_FILENO, In + ExpressionAt, Expansion.Start - ExpressionAt);
                            write(STDOUT_FILENO, S8_ARG(ExpansionText));
                            write(STDOUT_FILENO, In + Expansion.End + 1, At - (Expansion.End + 1));
                            write(STDOUT_FILENO, S8_LIT("\n"));
                        }

                        break;
                    }

                    At++;
                    Assert(At < File.Size);
                }
                At++;

            }
            else if (!strncmp(In + At, TableGenEnumKeyword.Memory, TableGenEnumKeyword.Size))
            {
                // TODO: not implemented yet
                while (In[At] != '}' && At < File.Size) At++;
                Assert(At < File.Size);
            }
            else if (!strncmp(In + At, TableKeyword.Memory, TableKeyword.Size))
            {
                s8  TableName     = {0};
                i32 LabelsCount   = 0;
                s8* Labels        = 0;
                i32 ElementsCount = 0;
                s8* Elements      = 0;

                // Parse the labels
                At += TableKeyword.Size;
                Assert(In[At] == '(');
                i32 BeginParenAt = At;
                At++;
                i32 CurrentLabelAt = At;
                s8* CurrentLabel = 0;

                while (In[At] != ')')
                {
                    if (In[At] == ',')
                    {
                        CurrentLabel = (s8*)ArenaPush(&ScratchArena, sizeof(*CurrentLabel));
                        CurrentLabel->Memory = In + CurrentLabelAt;
                        CurrentLabel->Size = At - CurrentLabelAt;
                        LabelsCount++;

                        At++;
                        while (IsWhitespace(In[At]) && At < File.Size) At++;
                        Assert(At < File.Size);
                        CurrentLabelAt = At;
                    }

                    At++;
                    Assert(At < File.Size);
                }

                if (BeginParenAt + 1 == At)
                {
                    Labels = 0;
                    // ERROR: no labels?
                }
                else
                {
                    Labels = (s8*)(ScratchArena.Memory);
                    CurrentLabel = (s8*)ArenaPush(&ScratchArena, sizeof(*CurrentLabel));
                    CurrentLabel->Memory = In + CurrentLabelAt;
                    CurrentLabel->Size = At - CurrentLabelAt;
                    LabelsCount++;
                }

                // Parse table name
                At++;
                while (IsWhitespace(In[At]) && At < File.Size) At++;
                Assert(At < File.Size);
                i32 TableNameAt = At;
                while (!IsWhitespace(In[At]) && At < File.Size) At++;
                Assert(At < File.Size);
                TableName.Memory = In + TableNameAt;
                TableName.Size = At - TableNameAt;

                while (IsWhitespace(In[At]) && At < File.Size) At++;
                Assert(At < File.Size);
                Assert(In[At] == '{');
                At++;
                
                if (LabelsCount == 0)
                {
                    // ERROR: Table without labels?
                } 
                // TODO: syntactic sugar when LabelsCount is 1
                else
                {
                    Elements = (s8*)(ScratchArena.Memory + ScratchArena.Pos);

                    i32 CurrentElementAt = 0;
                    i32 ShouldStop = false;
                    i32 IsPair = false;
                    u8 PairChar = 0;
                    s8* CurrentElement = 0;

                    while (!ShouldStop)
                    {
                        while (IsWhitespace(In[At]) && At < File.Size) At++;
                        Assert(At < File.Size);
                        if (In[At] == '}')
                        {
                            ShouldStop = true;
                        }
                        else
                        {
                            Assert(In[At] == '{');
                            At++;

                            CurrentElement = (s8*)ArenaPush(&ScratchArena, sizeof(*CurrentElement) * LabelsCount);

                            for (i32 LabelAt = 0;
                                 LabelAt < LabelsCount;
                                 LabelAt++)
                            {
                                while (IsWhitespace(In[At]) && At < File.Size) At++;
                                Assert(At < File.Size);
                                CurrentElementAt = At;

                                IsPair = true;
                                switch (In[At])
                                {
                                case '\'': PairChar = '\''; break;
                                case '"':  PairChar = '"';  break;
                                case '(':  PairChar = ')';  break;
                                case '{':  PairChar = '}';  break;
                                case '[':  PairChar = ']';  break;
                                default: IsPair = false; break;
                                }   
                                if (IsPair)
                                {
                                    At++; // NOTE(luca): We need to skip quotes because they are the
                                          // same character to open and to close. We can also assume
                                          // that a label within an element must be a minimum of 1
                                          // character so skipping should be fine.
                                    while (In[At] != PairChar && At < File.Size) At++;
                                    Assert(At < File.Size);
                                    At++;
                                }
                                else
                                {
                                    while (!IsWhitespace(In[At]) && At < File.Size) At++;
                                    Assert(At < File.Size);
                                }
                                                        
                                CurrentElement[LabelAt].Memory = In + CurrentElementAt;
                                CurrentElement[LabelAt].Size = At - CurrentElementAt;
                            }
                            ElementsCount++;

                            // Find end of element '}'
                            while (IsWhitespace(In[At]) && At < File.Size) At++;
                            Assert(At < File.Size);
                            Assert(In[At] == '}');
                            At++;
                        }
                    }
                }

                table *CurrentTable = (table*)ArenaPush(&TablesArena, sizeof(*CurrentTable));
                CurrentTable->Name          = TableName;
                CurrentTable->LabelsCount   = LabelsCount;
                CurrentTable->Labels        = Labels;
                CurrentTable->ElementsCount = ElementsCount;
                CurrentTable->Elements      = Elements;
                TablesCount++;
            }
            else
            {
                // ERROR: What if the code contains a non meta-"@_expand" tag ???
                write(STDOUT_FILENO, In + At, 1);
            }

        }
        else
        {
            write(STDOUT_FILENO, In + At, 1);
        }
    }


    return 0;
}
