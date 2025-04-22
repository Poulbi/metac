/*
 C preprocessor that provides meta functionality.

 To-Do's
   - [x] Error messages instead of asserts
- [ ] Compress the code
 - [ ] Use the real metadesk
- [ ] Expanding over multiple tables
 - [ ] Syntactic sugar to specify array of single values ?
 - [ ] Preserve indent when expansion is on a new line
 - [ ] Get rid of standard library, create an OS layer instead
*/

#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;
typedef u32      b32;
#define true     1
#define false    0

///~ Misc macro's //////////////////////////////////////
#define Assert(Expression) if (!(Expression)) { raise(SIGTRAP); }
#define Kilobyte(Bytes) Bytes * 1024LL
#define Megabyte(Bytes) Kilobyte(Bytes) * 1024
#define Gigabyte(Bytes) Megabyte(Bytes) * 1024
#define Terabyte(Bytes) Gigabyte(Bytes) * 1024
#define internal static
#define global_variable static
#define local_persist static
////////////////////////////////////////////////////////

///~ String ////////////////////////////////////////////
typedef struct {
    char *Data;
    u64 Size;
} s8;
#define S8_LIT(String) { (String), sizeof((String)) - 1 }
#define S8_ARG(String) (String.Data), (String.Size)
#define S8_FMT "Data: %.5s Size: %lu"
#define S8_SIZE_DATA(String) (sizeof((String)) - 1), (String)
////////////////////////////////////////////////////////

///~ Arena /////////////////////////////////////////////
typedef struct {
    void *Memory;
    u64 Pos;
    u64 Size;
} arena;

void *
ArenaPush(arena* Arena, u64 Size)
{
    void *Result = Arena->Memory + Arena->Pos;
    Arena->Pos += Size;
    Assert(Arena->Pos <= Arena->Size);
    return Result;
}

void *
ArenaPop(arena *Arena, u64 Size)
{
    Assert(Size <= Arena->Pos);
    Arena->Pos -= Size;
    return Arena->Memory + Arena->Pos;
}
////////////////////////////////////////////////////////

///~ MetaC data structures /////////////////////////////
struct table {
    s8 Name;
    i32 LabelsCount;
    s8 *Labels;
    i32 ElementsCount;
    s8 *Elements;
};
typedef struct table table;

struct parse_result {
    u64 End;
    u64 Size;
    char *Data;
};
typedef struct parse_result parse_result;

struct error {
    u64 At;
    u64 Size;
    char Message[];
};
typedef struct error error;
////////////////////////////////////////////////////////

///~ Global variables /////////////////////////////////////////////
// TODO: use meta program to generate Keywords table
global_variable s8 TableKeyword = S8_LIT("table");
global_variable s8 TableGenEnumKeyword = S8_LIT("table_gen_enum");
global_variable s8 ExpandKeyword = S8_LIT("expand");
////////////////////////////////////////////////////////

///~ MetaC functions ///////////////////////////////////
void
ErrorPush(arena *ErrorsArena, u64 MessageAt, u64 MessageSize, char *MessageData)
{
    error *Error = (error *)(ErrorsArena->Memory + ErrorsArena->Pos);
    Error->At = MessageAt;
    Error->Size = MessageSize;
    memcpy(Error->Message, MessageData, MessageSize);
    ErrorsArena->Pos += sizeof(MessageAt) + sizeof(MessageSize) + MessageSize;
}

void
ErrorPushAssert(b32 Condition, arena *ErrorsArena, u64 MessageAt, u64 MessageSize, char *MessageData)
{
    if (!Condition)
    {
        ErrorPush(ErrorsArena, MessageAt, MessageSize, MessageData);
    }
}

s8 ReadEntireFileIntoMemory(char *Filepath)
{
    i32 FD = 0;
    s8 Result = {0};
    struct stat StatBuffer = {0};
    
    FD = open(Filepath, O_RDONLY);
    fstat(FD, &StatBuffer);
    
    Result.Size = StatBuffer.st_size;
    Result.Data = mmap(0, Result.Size, PROT_READ | PROT_WRITE, MAP_PRIVATE, FD, 0); 
    
    return Result;
}

i32
IsWhitespace(char Ch)
{
    return(Ch == ' '  ||
           Ch == '\n' ||
           Ch == '\t');
}

parse_result
ParseUntilChar(u8 *In, u64 InSize, u64 At, char Ch, arena *ErrorsArena)
{
    parse_result Result = {0};
    u64 ExpressionAt = At;
    
    while (In[At] != Ch && At < InSize) At++;
    if (At < InSize)
    {
        Result.Data = In + ExpressionAt;
        Result.Size = At - ExpressionAt;
        Result.End = At + 1;
    }
    else
    {
        s8 ErrorMessage = S8_LIT("Expected C");
        ErrorMessage.Data[ErrorMessage.Size - 1] = Ch; 
        ErrorPush(ErrorsArena, ExpressionAt, ErrorMessage.Size, ErrorMessage.Data);
    }
    
    return Result;
}

u64
DecimalArenaPush(arena *Arena, u64 Value)
{
    u64 TempValue = Value;
    
    u32 DigitsCount = 1;
    while (TempValue /= 10) DigitsCount++;
    char *Number = (char *)ArenaPush(Arena, DigitsCount);
    
    u32 TempDigitsCount = DigitsCount;
    for (TempValue = Value;
         TempValue;
         TempValue /= 10)
    {
        u32 Digit = TempValue % 10;
        Number[--TempDigitsCount] = (char)(Digit + '0');
    }
    
    return DigitsCount;
}

void
Memcpy(char *Destination, char *Source, u64 Size)
{
    while (Size--) *Destination++ = *Source++;
}

char *
StringArenaPush(arena *Arena, char *Source, u64 Size)
{
    char *Pos = (char *)ArenaPush(Arena, Size);
    Memcpy((char *)Pos, Source, Size);
    return Pos;
}
////////////////////////////////////////////////////////

int
main(int ArgC, char *Args[])
{
    char *Filename = 0;
    char *OutputFilename = 0;
    
    char *Storage = mmap(0, Megabyte(4), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    Assert(Storage);
    
    arena ScratchArena = {0};
    ScratchArena.Size = Megabyte(1);
    ScratchArena.Memory = Storage;
    
    arena TablesArena = {0}; 
    TablesArena.Memory = ScratchArena.Memory + ScratchArena.Size;
    TablesArena.Size = Megabyte(1);
    table *Tables = (table*)TablesArena.Memory;
    u32 TablesCount = 0;
    
    arena ErrorsArena = {0};
    ErrorsArena.Memory = TablesArena.Memory + TablesArena.Size;
    ErrorsArena.Size = Megabyte(1);
    
    char *Out = ErrorsArena.Memory + ErrorsArena.Size;
    char *OutBase = Out;
    
    if (ArgC > 1)
    {
        Filename = Args[1];
        if (ArgC > 2)
        {
            if (Args[2][0] == '-' && Args[2][1] == '\0')
            {
                OutputFilename = 0;
            }
            else
            {
                OutputFilename = Args[2];
            }
        }
        else
        {
            u32 Len = strlen(Filename);
            if (!strncmp(Filename + Len - 2, ".c", 2))
            {
                u32 SufLen = sizeof(".meta") - 1;
                OutputFilename = malloc(Len + SufLen + 1);
                memcpy(OutputFilename, Filename, Len - 2);
                memcpy(OutputFilename + Len - 2, ".meta.c", SufLen + 2);
                OutputFilename[Len + SufLen] = 0; 
            }
            else
            {
                u32 SuffixLength = sizeof(".meta.c") - 1;
                OutputFilename = malloc(Len + SuffixLength + 1);
                memcpy(OutputFilename, Filename, Len);
                memcpy(OutputFilename + Len, ".meta.c", SuffixLength);
                OutputFilename[Len + SuffixLength] = 0;
            }
            
        }
    }
    else
    {
        fprintf(stderr, "Usage: %s filename [output_filename]\n", Args[0]);
        return 1;
    }
    
    // NOTE(luca): The memory is assumed to stay mapped until program exits, because we will use
    // pointers into that memory.
    s8 FileContents = ReadEntireFileIntoMemory(Filename);
    
    if (!FileContents.Data || (void*)FileContents.Data == (void*)-1)
    {
        fprintf(stderr, "File '%s' could not be loaded into memory.\n", Filename);
        return 1;
    }
    
    char *In = FileContents.Data;
    u64 InSize = FileContents.Size;
    
    for (u64 At = 0;
         At < InSize;
         At++)
    {
        if (In[At] == '@')
        {
            At++;
            
            if (!strncmp(In + At, S8_ARG(ExpandKeyword)))
            {
                if (TablesCount == 0)
                {
                    ErrorPush(&ErrorsArena, At, S8_SIZE_DATA("no tables defined"));
                }
                
                table *ExpressionTable = 0;
                s8 ExpressionTableName       = {0};
                i32 ExpressionTableNameAt    = 0;
                s8 ExpressionTableArgument    = {0};
                i32 ExpressionTableArgumentAt = 0;
                
                At += ExpandKeyword.Size;
                ErrorPushAssert(At < InSize, &ErrorsArena, At, S8_SIZE_DATA("expected '('"));
                ErrorPushAssert(In[At] == '(', &ErrorsArena, At, S8_SIZE_DATA("expected '('"));
                At++;
                
                while (IsWhitespace(In[At]) && At < InSize) At++;
                
                // @compress_parse
                ExpressionTableNameAt = At;
                while (!IsWhitespace(In[At]) && In[At] != ')' && At < InSize) At++;
                ErrorPushAssert(At - ExpressionTableNameAt > 0, &ErrorsArena, At, S8_SIZE_DATA("table name required"));
                ErrorPushAssert(At < InSize, &ErrorsArena, ExpressionTableNameAt, S8_SIZE_DATA("cannot parse table name"));
                ExpressionTableName.Data = In + ExpressionTableNameAt;
                ExpressionTableName.Size = At - ExpressionTableNameAt;
                
                for (i32 TableAt = 0;
                     TableAt < TablesCount;
                     TableAt++)
                {
                    if (ExpressionTableName.Size == Tables[TableAt].Name.Size && !strncmp(Tables[TableAt].Name.Data, ExpressionTableName.Data, ExpressionTableName.Size))
                    {
                        ExpressionTable = Tables + TableAt;
                        break;
                    }
                }
                ErrorPushAssert(ExpressionTable != 0, &ErrorsArena, ExpressionTableNameAt, S8_SIZE_DATA("undefined table name"));
                
                while (IsWhitespace(In[At]) && At < InSize) At++;
                ErrorPushAssert(At < InSize, &ErrorsArena, At, S8_SIZE_DATA("expected argument name"));
                int ErrorAt = At;
                
                parse_result ParseResult = ParseUntilChar(In, InSize, At, ')', &ErrorsArena);
                if (ParseResult.Size)
                {
                    ExpressionTableArgument.Size = ParseResult.Size;
                    ExpressionTableArgument.Data = ParseResult.Data;
                    At = ParseResult.End;
                }
                else
                {
                    // TODO: Logging
                }
                
                // Error(At > ExpressionTableArgumentAt, "argument name required", ExpressionTableArgumentAt);
                
                // @compress_parse
                while (IsWhitespace(In[At]) && At < InSize) At++;
                ErrorPushAssert(At < InSize, &ErrorsArena, At, S8_SIZE_DATA("expected opening '`"));
                ErrorPushAssert(In[At] == '`', &ErrorsArena, At, S8_SIZE_DATA("expected closing '`'"));
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
                                // NOTE(luca): to make errors even smarter we should stop searching at the
                                // closing characters up one level. ')' in this case.
                                ParseResult = ParseUntilChar(In, InSize, At, '.', &ErrorsArena); 
                                if (ParseResult.Size)
                                {
                                    if (ParseResult.Size != ExpressionTableArgument.Size ||
                                        strncmp(ExpandArgument.Data, 
                                                ExpressionTableArgument.Data,
                                                ExpandArgument.Size))
                                    {
                                        ErrorPush(&ErrorsArena, At, S8_SIZE_DATA("argument name does not match defined one"));
                                    }
                                    else
                                    {
                                        ExpandArgument.Data = ParseResult.Data;
                                        ExpandArgument.Size = ParseResult.Size;
                                        At = ParseResult.End;
                                    }
                                }
                                
                                s8 ExpansionLabel = {0};
                                parse_result ParseResult = ParseUntilChar(In, InSize, At, ')', &ErrorsArena);
                                if (ParseResult.Size)
                                {
                                    ExpansionLabel.Data = ParseResult.Data;
                                    ExpansionLabel.Size = ParseResult.Size;
                                    At = ParseResult.End;
                                }
                                
                                i32 LabelIndex = -1;
                                for (i32 LabelAt = 0;
                                     LabelAt < ExpressionTable->LabelsCount;
                                     LabelAt++)
                                {
                                    if (!strncmp(ExpansionLabel.Data,
                                                 ExpressionTable->Labels[LabelAt].Data,
                                                 ExpansionLabel.Size))
                                    {
                                        LabelIndex = LabelAt;
                                        break;
                                    }
                                }
                                ErrorPushAssert(LabelIndex != -1, &ErrorsArena, At, S8_SIZE_DATA("undefined label"));
                                
                                s8 Expansion = ExpressionTable->Elements[ElementAt * ExpressionTable->LabelsCount + LabelIndex];
                                memcpy(Out, Expansion.Data, Expansion.Size);
                                Out += Expansion.Size;
                            }
                            else if (In[At] != '`')
                            {
                                *Out++ = In[At++];
                            }
                            
                        }
                        *Out++ = '\n';
                        
                    }
                    ErrorPushAssert(At < InSize, &ErrorsArena, ExpressionAt - 1, S8_SIZE_DATA("expected closing '`'") - 1);
                    
                    At++;
                }
            }
            else if (!strncmp(In + At, TableGenEnumKeyword.Data, TableGenEnumKeyword.Size))
            {
                // TODO: not implemented yet
                while (In[At] != '}' && At < InSize) At++;
                ErrorPushAssert(At < InSize, &ErrorsArena, At, S8_SIZE_DATA("expected '}'"));
            }
            else if (!strncmp(In + At, TableKeyword.Data, TableKeyword.Size))
            {
                s8  TableName     = {0};
                i32 LabelsCount   = 0;
                s8* Labels        = 0;
                i32 ElementsCount = 0;
                s8* Elements      = 0;
                
                Labels = (s8*)(ScratchArena.Memory + ScratchArena.Pos);
                
                // Parse the labels
                At += TableKeyword.Size;
                ErrorPushAssert(In[At] == '(', &ErrorsArena, At, S8_SIZE_DATA("expected '('"));
                i32 BeginParenAt = At;
                At++;
                i32 CurrentLabelAt = At;
                s8* CurrentLabel = 0;
                
                while (In[At] != ')' && At < InSize)
                {
                    if (In[At] == ',')
                    {
                        CurrentLabel = (s8*)ArenaPush(&ScratchArena, sizeof(*CurrentLabel));
                        CurrentLabel->Data = In + CurrentLabelAt;
                        CurrentLabel->Size = At - CurrentLabelAt;
                        LabelsCount++;
                        
                        At++;
                        while (IsWhitespace(In[At]) && At < InSize) At++;
                        ErrorPushAssert(At < InSize, &ErrorsArena, At, S8_SIZE_DATA("expected next label"));
                        CurrentLabelAt = At;
                    }
                    
                    At++;
                }
                ErrorPushAssert(At < InSize, &ErrorsArena, At, S8_SIZE_DATA("expected ')'"));
                
                if (BeginParenAt + 1 != At)
                {
                    CurrentLabel = (s8*)ArenaPush(&ScratchArena, sizeof(*CurrentLabel));
                    CurrentLabel->Data = In + CurrentLabelAt;
                    CurrentLabel->Size = At - CurrentLabelAt;
                    LabelsCount++;
                }
                ErrorPushAssert(LabelsCount, &ErrorsArena, At, S8_SIZE_DATA("no labels defined"));
                
                // Parse table name
                At++;
                while (IsWhitespace(In[At]) && At < InSize) At++;
                ErrorPushAssert(At < InSize, &ErrorsArena, At, S8_SIZE_DATA("expected table name"));
                
                // @compress_parse
                i32 TableNameAt = At;
                while (!IsWhitespace(In[At]) && At < InSize) At++;
                ErrorPushAssert(At < InSize, &ErrorsArena, At, S8_SIZE_DATA("EOF while parsing table name"));
                TableName.Data = In + TableNameAt;
                TableName.Size = At - TableNameAt;
                
                parse_result ParseResult = ParseUntilChar(In, InSize, At, '{', &ErrorsArena);
                if (ParseResult.Size)
                {
                    At = ParseResult.End;
                }
                
                Elements = (s8*)(ScratchArena.Memory + ScratchArena.Pos);
                
                i32 CurrentElementAt = 0;
                i32 ShouldStop = false;
                i32 IsPair = false;
                u8 PairChar = 0;
                s8* CurrentElement = 0;
                
                while (!ShouldStop)
                {
                    while (IsWhitespace(In[At]) && At < InSize) At++;
                    ErrorPushAssert(At < InSize, &ErrorsArena, At, S8_SIZE_DATA("expected '}' or '{'"));
                    if (In[At] == '}')
                    {
                        ShouldStop = true;
                    }
                    else
                    {
                        ErrorPushAssert(In[At] == '{', &ErrorsArena, At, S8_SIZE_DATA("expected '{'"));
                        At++;
                        
                        CurrentElement = (s8*)ArenaPush(&ScratchArena, sizeof(*CurrentElement) * LabelsCount);
                        
                        // Parse elements
                        for (i32 LabelAt = 0;
                             LabelAt < LabelsCount;
                             LabelAt++)
                        {
                            while (IsWhitespace(In[At]) && At < InSize) At++;
                            ErrorPushAssert(At < InSize, &ErrorsArena, At, S8_SIZE_DATA("expected element label"));
                            
                            // @compress_parse
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
                                while (!IsWhitespace(In[At]) && At < InSize) At++;
                            }
                            ErrorPushAssert(At < InSize, &ErrorsArena, At, S8_SIZE_DATA("EOF while parsing element label"));
                            
                            CurrentElement[LabelAt].Data = In + CurrentElementAt;
                            CurrentElement[LabelAt].Size = At - CurrentElementAt;
                        }
                        ElementsCount++;
                        
                        // NOTE(luca): Find end of element '}'
                        parse_result ParseResult = ParseUntilChar(In, InSize, At, '}', &ErrorsArena);
                        if (ParseResult.Size)
                        {
                            At = ParseResult.End;
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
    
    if (ErrorsArena.Pos)
    {
        for (error *ErrorAt = (error *)ErrorsArena.Memory;
             ErrorAt->Size;
             ErrorAt = (error *)((void *)ErrorAt + sizeof(ErrorAt->At) + sizeof(ErrorAt->Size) + ErrorAt->Size))
        {
            void *PosBase = ScratchArena.Memory + ScratchArena.Pos;
            StringArenaPush(&ScratchArena, "Error(", 6);
            DecimalArenaPush(&ScratchArena, ErrorAt->At);
            StringArenaPush(&ScratchArena, "): ", 3);
            StringArenaPush(&ScratchArena, ErrorAt->Message, ErrorAt->Size);
            StringArenaPush(&ScratchArena, "\n", 1);
            void *Pos = ScratchArena.Memory + ScratchArena.Pos;
            
            write(STDERR_FILENO, PosBase, Pos - PosBase);
        }
        
    }
    else
    {
        u32 FD = STDOUT_FILENO;
        if (OutputFilename)
        {
            FD = open(OutputFilename, O_WRONLY | O_CREAT, 0644);
            if (FD == -1)
            {
                fprintf(stderr, "Could not open %s\n", OutputFilename);
                return 1;
            }
            else
            {
                fprintf(stderr, "Output: %s\n", OutputFilename);
            }
        }
        
        write(FD, OutBase, Out - OutBase);
    }
    
    return 0;
}
