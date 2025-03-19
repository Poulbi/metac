/* 
 C preprocessor that provides meta functionality.

 To-Do's
 - [ ] Error messages instead of asserts
       - Show byte offset to show where the errors happens in the file and highlight that.
       Idea
       - Print error
       - exit
       Problem what if inside function
       - global variable error
       When pushing new error do not overwrite
       Since we have only fatal errors we could say there is only one global variable with the error contents.

       Idea
       - Error stack
       - Push new errors on the stack along with a message
       - Error location (byte offset in file)
        -> Maybe pretty print this
       - Error kinds (optional, first only fatal errors)
       - After parse check if there are any (fatal) errors

 - [ ] Expanding over multiple tables
 - [ ] Syntactic sugar to specify array of single values ?
 - [ ] Preserve indent when expansion is on a new line
 - [ ] Compress the code
*/


#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
#define true     1
#define false    0

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

global_variable char *ErrorMessage = 0;
global_variable i32 ErrorLocation = 0;
global_variable i32 ErrorAt;

void
Error(i32 Expr, char *Message, i32 Offset)
{
    if (!Expr && !ErrorMessage)
    {
        printf("%i: %s\n", Offset, Message);
        ErrorMessage = Message;
        ErrorLocation = Offset;
    }
}

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
                Error(TablesCount > 0, "no tables defined", At);

                table *ExpressionTable = 0;
                s8 ExpressionTableName       = {0};
                i32 ExpressionTableNameAt    = 0;
                s8 ExpressionTableArgument    = {0};
                i32 ExpressionTableArgumentAt = 0;

                At += ExpandKeyword.Size;
                Error(At < InSize, "expected '('", At);
                Error(In[At] == '(', "expected '('", At);
                At++;

                while (IsWhitespace(In[At]) && At < InSize) At++;
                ExpressionTableNameAt = At;
                while (!IsWhitespace(In[At]) && In[At] != ')' && At < InSize) At++;
                Error(At - ExpressionTableNameAt > 0, "table name required", ExpressionTableNameAt);
                Error(At < InSize, "cannot parse table name", ExpressionTableNameAt);
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
                Error(ExpressionTable != 0, "undefined table name", ExpressionTableNameAt);

                while (IsWhitespace(In[At]) && At < InSize) At++;
                Error(At < InSize, "expected argument name", At);
                ExpressionTableArgumentAt = At;
                ErrorAt = At;
                while (In[At] != ')' && At < InSize) At++;
                Error(At > ExpressionTableArgumentAt, "argument name required", ExpressionTableArgumentAt);
                Error(At < InSize, "expected ')'", ErrorAt);
                ExpressionTableArgument.Memory = In + ExpressionTableArgumentAt;
                ExpressionTableArgument.Size = At - ExpressionTableArgumentAt;
                At++;

                while (IsWhitespace(In[At]) && At < InSize) At++;
                Error(At < InSize, "expected opening '`", At);
                Error(In[At] == '`', "expected closing '`'", At);
                At++;

                if (ExpressionTable)
                {
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

                            if (In[At] == '$' && In[At + 1] == '(')
                            {
                                At += 2;

                                s8 ExpandArgument = {0};
                                i32 ExpandArgumentAt = At;
                                while (In[At] != '.' && At < InSize) At++;
                                // NOTE(luca): to make errors even smarter we should stop searching at the
                                // closing characters up one level. ')' in this case.
                                Error(At < InSize, "expected '.'", ExpandArgumentAt);
                                ExpandArgument.Memory = In + ExpandArgumentAt;
                                ExpandArgument.Size = At - ExpandArgumentAt;
                                Error(!strncmp(ExpandArgument.Memory, 
                                               ExpressionTableArgument.Memory,
                                               ExpandArgument.Size),
                                      "argument name does not match defined one",
                                      ExpandArgumentAt);
                                At++;

                                s8 ExpansionLabel = {0};
                                i32 ExpansionLabelAt = At;
                                while (In[At] != ')' && At < InSize) At++;
                                Error(At < InSize, "expected ')'", ExpandArgumentAt - 1);
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
                                Error(LabelIndex != -1, "undefined label", ExpansionLabelAt);

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
                    Error(At < InSize, "expected closing '`'", ExpressionAt - 1);

                    At++;
                }
            }
            else if (!strncmp(In + At, TableGenEnumKeyword.Memory, TableGenEnumKeyword.Size))
            {
                // TODO: not implemented yet
                while (In[At] != '}' && At < InSize) At++;
                Error(At < InSize, "expected '}'", At);
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
                Error(In[At] == '(', "expected '('", At);
                i32 BeginParenAt = At;
                At++;
                i32 CurrentLabelAt = At;
                s8* CurrentLabel = 0;

                while (In[At] != ')' && At < InSize)
                {
                    if (In[At] == ',')
                    {
                        CurrentLabel = (s8*)ArenaPush(&ScratchArena, sizeof(*CurrentLabel));
                        CurrentLabel->Memory = In + CurrentLabelAt;
                        CurrentLabel->Size = At - CurrentLabelAt;
                        LabelsCount++;

                        At++;
                        while (IsWhitespace(In[At]) && At < InSize) At++;
                        Error(At < InSize, "expected next label", At);
                        CurrentLabelAt = At;
                    }

                    At++;
                }
                Error(At < InSize, "expected ')'", At);

                if (BeginParenAt + 1 != At)
                {
                    CurrentLabel = (s8*)ArenaPush(&ScratchArena, sizeof(*CurrentLabel));
                    CurrentLabel->Memory = In + CurrentLabelAt;
                    CurrentLabel->Size = At - CurrentLabelAt;
                    LabelsCount++;
                }
                Error(LabelsCount, "no labels defined", At);

                // Parse table name
                At++;
                while (IsWhitespace(In[At]) && At < InSize) At++;
                Error(At < InSize, "expected table name", At);
                i32 TableNameAt = At;
                while (!IsWhitespace(In[At]) && At < InSize) At++;
                Error(At < InSize, "EOF while parsing table name", At);
                TableName.Memory = In + TableNameAt;
                TableName.Size = At - TableNameAt;

                ErrorAt = At;
                while (In[At] != '{' && At < InSize) At++;
                Error(At < InSize, "expected '{'", ErrorAt);
                At++;
                
                Elements = (s8*)(ScratchArena.Memory + ScratchArena.Pos);

                i32 CurrentElementAt = 0;
                i32 ShouldStop = false;
                i32 IsPair = false;
                u8 PairChar = 0;
                s8* CurrentElement = 0;

                while (!ShouldStop)
                {
                    while (IsWhitespace(In[At]) && At < InSize) At++;
                    Error(At < InSize, "expected '}' or '{'", At);
                    if (In[At] == '}')
                    {
                        ShouldStop = true;
                    }
                    else
                    {
                        Error(In[At] == '{', "expected '{'", At);
                        At++;

                        CurrentElement = (s8*)ArenaPush(&ScratchArena, sizeof(*CurrentElement) * LabelsCount);

                        // Parse elements
                        for (i32 LabelAt = 0;
                             LabelAt < LabelsCount;
                             LabelAt++)
                        {
                            while (IsWhitespace(In[At]) && At < InSize) At++;
                            Error(At < InSize, "expected element label", At);
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

                            ErrorAt = At;
                            // TODO: escape characters with '\'
                            if (IsPair)
                            {
                                At++; // NOTE(luca): We need to skip quotes because they are the
                                      // same character to open and to close. We can also assume
                                      // that a label within an element must be a minimum of 1
                                      // character so skipping should be fine.
                                while (In[At] != PairChar && At < InSize) At++;
                                At++;
                            }
                            else
                            {
                                ErrorAt = At;
                                while (!IsWhitespace(In[At]) && At < InSize) At++;
                            }
                            Error(At < InSize, "EOF while parsing element label", ErrorAt);
                                                    
                            CurrentElement[LabelAt].Memory = In + CurrentElementAt;
                            CurrentElement[LabelAt].Size = At - CurrentElementAt;
                        }
                        ElementsCount++;

                        // Find end of element '}'
                        ErrorAt = At;
                        while (In[At] != '}' && At < InSize) At++;
                        Error(At < InSize, "expected '}'", ErrorAt);
                        At++;
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
    
    if (ErrorMessage)
    {
        // printf("%i: %s\n", ErrorLocation, ErrorMessage);
    }
    else
    {
        write(STDOUT_FILENO, OutBase, Out - OutBase);
    }

    return 0;
}
