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

///~ Misc macro's //////////////////////////////////////
#define Assert(expr) if (!(expr)) { raise(SIGTRAP); }
#define Kilobyte(byte) byte * 1024L
#define Megabyte(byte) Kilobyte(byte) * 1024L
#define Gigabyte(byte) Megabyte(byte) * 1024L
#define internal static
#define global_variable static
#define local_persist static
////////////////////////////////////////////////////////

///~ String ////////////////////////////////////////////
typedef struct {
    char *Memory;
    u64 Size;
} s8;
#define S8_LIT(str) str, sizeof(str) - 1
#define S8_ARG(str) str.Memory, str.Size
#define S8(str) { S8_LIT(str) }
#define S8_FMT "Memory: %.5s Size: %lu"
////////////////////////////////////////////////////////

///~ Arena /////////////////////////////////////////////
typedef struct {
    char *Memory;
    u64 Pos;
    u64 Size;
} arena;

///~ Global variables /////////////////////////////////////////////
// TODO: use meta program to generate Keywords table
global_variable s8 TableKeyword = S8("table");
global_variable s8 TableGenEnumKeyword = S8("table_gen_enum");
global_variable s8 ExpandKeyword = S8("expand");
////////////////////////////////////////////////////////

char *
ArenaPush(arena* Arena, i64 Size)
{
    char *Result;
    Result = (char*)Arena->Memory + Arena->Pos;
    Arena->Pos += Size;
    Assert(Arena->Pos <= Arena->Size);
    return Result;
}
////////////////////////////////////////////////////////

///~ MetaC data structures /////////////////////////////
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
////////////////////////////////////////////////////////

s8 ReadEntireFileIntoMemory(char *Filepath)
{
    i32 FD = 0;
    s8 Result = {0};
    struct stat StatBuffer = {0};

    FD = open(Filepath, O_RDONLY);
    fstat(FD, &StatBuffer);

    Result.Size = StatBuffer.st_size;
    Result.Memory = mmap(0, Result.Size, PROT_READ | PROT_WRITE, MAP_PRIVATE, FD, 0); 

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
        .Memory = ScratchArena.Memory + ScratchArena.Size,
        .Pos = 0,
        .Size = Megabyte(1)
    };
    table *Tables = (table*)TablesArena.Memory;
    i32 TablesCount = 0;
    char *Out = TablesArena.Memory + TablesArena.Size;
    char *OutBase = Out;

    if (ArgC > 1)
    {
        Filename = Args[1];
    }
    else
    {
        fprintf(stderr, "Usage: %s [filename]\n", Args[0]);
        return 1;
    }

    // NOTE(luca): The memory is assumed to stay mapped until program exits, because we will use
    // pointers into that memory.
    s8 FileContents = ReadEntireFileIntoMemory(Filename);

    if (!FileContents.Memory || (void*)FileContents.Memory == (void*)-1)
    {
        fprintf(stderr, "File '%s' could not be loaded into memory.\n", Filename);
        return 1;
    }

    char *In = FileContents.Memory;
    i64 InSize = FileContents.Size;

    for (i64 At = 0;
         At < InSize;
         At++)
    {
        if (In[At] == '@')
        {
            At++;
            
            if (!strncmp(In + At, S8_ARG(ExpandKeyword)))
            {
                table *ExpressionTable = 0;
                s8 ExpressionTableName       = {0};
                i32 ExpressionTableNameAt    = 0;
                s8 ExpressionTableArgument    = {0};
                i32 ExpressionTableArgumentAt = 0;

                At += ExpandKeyword.Size;
                Assert(At < InSize);
                Assert(In[At] == '(');
                At++;

                ExpressionTableNameAt = At;
                while (!IsWhitespace(In[At]) && At < InSize) At++;
                Assert(At < InSize);
                ExpressionTableName.Memory = In + ExpressionTableNameAt;
                ExpressionTableName.Size = At - ExpressionTableNameAt;
                
                for (i32 TableAt = 0;
                     TableAt < TablesCount;
                     TableAt++)
                {
                    if (!strncmp(Tables[TableAt].Name.Memory, ExpressionTableName.Memory, ExpressionTableName.Size))
                    {
                        ExpressionTable = Tables + TableAt;
                        break;
                    }
                }
                Assert(ExpressionTable);

                while (IsWhitespace(In[At]) && At < InSize) At++;
                Assert(At < InSize);
                ExpressionTableArgumentAt = At;
                while (In[At] != ')' && At < InSize) At++;
                Assert(At < InSize);
                ExpressionTableArgument.Memory = In + ExpressionTableArgumentAt;
                ExpressionTableArgument.Size = At - ExpressionTableArgumentAt;
                At++;

                while (IsWhitespace(In[At]) && At < InSize) At++;
                Assert(At < InSize);
                Assert(In[At] == '`');
                At++;

                i32 ExpressionAt = At;
                for (i32 ElementAt = 0;
                     ElementAt < ExpressionTable->ElementsCount;
                     ElementAt++)
                {
                    At = ExpressionAt;

                    while (In[At] != '`' && At < InSize)
                    {
                        while ((In[At] != '$' && In[At] != '`') && At < InSize) 
                        {
                            if (In[At] == '\\') At++;
                            *Out++ = In[At++];
                        }

                        // TODO: allow escaping characters with '\'
                        if (In[At] == '$' && In[At + 1] == '(')
                        {
                            At += 2;

                            s8 ExpandArgument = {0};
                            i32 ExpandArgumentAt = At;
                            while (In[At] != '.' && At < InSize) At++;
                            ExpandArgument.Memory = In + ExpandArgumentAt;
                            ExpandArgument.Size = At - ExpandArgumentAt;
                            Assert(!strncmp(ExpandArgument.Memory, ExpressionTableArgument.Memory, ExpandArgument.Size));
                            At++;

                            s8 ExpansionLabel = {0};
                            i32 ExpansionLabelAt = At;
                            while (In[At] != ')' && At < InSize) At++;
                            Assert(At < InSize);
                            ExpansionLabel.Memory = In + ExpansionLabelAt;
                            ExpansionLabel.Size = At - ExpansionLabelAt;
                            At++;

                            i32 LabelIndex = -1;
                            for (i32 LabelAt = 0;
                                 LabelAt < ExpressionTable->LabelsCount;
                                 LabelAt++)
                            {
                                if (!strncmp(ExpansionLabel.Memory,
                                             ExpressionTable->Labels[LabelAt].Memory,
                                             ExpansionLabel.Size))
                                {
                                    LabelIndex = LabelAt;
                                    break;
                                }
                            }
                            Assert(LabelIndex != -1);

                            s8 Expansion = ExpressionTable->Elements[ElementAt * ExpressionTable->LabelsCount + LabelIndex];
                            memcpy(Out, Expansion.Memory, Expansion.Size);
                            Out += Expansion.Size;
                        }
                        else if (In[At] != '`')
                        {
                            *Out++ = In[At++];
                        }

                    }
                    *Out++ = '\n';

                }
                Assert(At < InSize);

                At++;

            }
            else if (!strncmp(In + At, TableGenEnumKeyword.Memory, TableGenEnumKeyword.Size))
            {
                // TODO: not implemented yet
                while (In[At] != '}' && At < InSize) At++;
                Assert(At < InSize);
            }
            else if (!strncmp(In + At, TableKeyword.Memory, TableKeyword.Size))
            {
                s8  TableName     = {0};
                i32 LabelsCount   = 0;
                s8* Labels        = 0;
                i32 ElementsCount = 0;
                s8* Elements      = 0;

                Labels = (s8*)(ScratchArena.Memory + ScratchArena.Pos);

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
                        while (IsWhitespace(In[At]) && At < InSize) At++;
                        Assert(At < InSize);
                        CurrentLabelAt = At;
                    }

                    At++;
                    Assert(At < InSize);
                }

                if (BeginParenAt + 1 == At)
                {
                    Labels = 0;
                    // ERROR: no labels?
                }
                else
                {
                    CurrentLabel = (s8*)ArenaPush(&ScratchArena, sizeof(*CurrentLabel));
                    CurrentLabel->Memory = In + CurrentLabelAt;
                    CurrentLabel->Size = At - CurrentLabelAt;
                    LabelsCount++;
                }

                // Parse table name
                At++;
                while (IsWhitespace(In[At]) && At < InSize) At++;
                Assert(At < InSize);
                i32 TableNameAt = At;
                while (!IsWhitespace(In[At]) && At < InSize) At++;
                Assert(At < InSize);
                TableName.Memory = In + TableNameAt;
                TableName.Size = At - TableNameAt;

                while (IsWhitespace(In[At]) && At < InSize) At++;
                Assert(At < InSize);
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
                        while (IsWhitespace(In[At]) && At < InSize) At++;
                        Assert(At < InSize);
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
                                while (IsWhitespace(In[At]) && At < InSize) At++;
                                Assert(At < InSize);
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
                                    while (In[At] != PairChar && At < InSize) At++;
                                    Assert(At < InSize);
                                    At++;
                                }
                                else
                                {
                                    while (!IsWhitespace(In[At]) && At < InSize) At++;
                                    Assert(At < InSize);
                                }
                                                        
                                CurrentElement[LabelAt].Memory = In + CurrentElementAt;
                                CurrentElement[LabelAt].Size = At - CurrentElementAt;
                            }
                            ElementsCount++;

                            // Find end of element '}'
                            while (IsWhitespace(In[At]) && At < InSize) At++;
                            Assert(At < InSize);
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
                *Out++ = '@';
                *Out++ = In[At];
            }

        }
        else
        {
            *Out++ = In[At];
        }
    }
    
    write(STDOUT_FILENO, OutBase, Out - OutBase);

    return 0;
}
