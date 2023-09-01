// { - basic programming language. This program is basic translator, 
// which can do interpretation and compilation. 

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#define MEMORY_ALLOCATION_FAILURE   2
#define MEMORY_REALLOCATION_FAILURE 3

// Enums for different token kinds
typedef enum TokenType
{
  DIGIT = 0,
  OPERATOR = 1
} TokenType;

// Token data type
typedef struct Token
{
  TokenType token_type;
  char* token_value;
} Token;

// Data type that will contain tokens in one place
typedef struct Node
{
  Token token;
  struct Node* node;
} Node;

// Vector implementation with dynamic array
typedef struct Vector
{
  Token* token_buff; // Array that contain all tokens
  size_t capacity; // Actual size of array
  size_t used;     // How many slots in array is already used
} Vector;

// Global buffer, that contain text from source file
char* gBuffer;
// Global character
char character;
// Global token
Token token;

Vector* VectorInit()
{
  Vector* vec = (Vector*)malloc(sizeof(Vector));
  if(vec == NULL)
  {
    fprintf(stderr, "error: bad allocation in [vec]: %p\n", (void*)vec);
    exit(MEMORY_ALLOCATION_FAILURE);
  }

  vec->capacity = 1;
  vec->used = 0;
  vec->token_buff = (Token*)malloc(sizeof(Token) * vec->capacity);
  if(vec->token_buff == NULL)
  {
    fprintf(stderr, "error: bad allocation in [token_buff]: %p\n", (void*)vec->token_buff);
    exit(MEMORY_ALLOCATION_FAILURE);
  }

  return vec;
}

void VectorAdd(Token token, Vector* vector)
{
  if(vector->capacity == vector->used)
  {
    vector->capacity *= 2;

    if(vector->token_buff == NULL)
    {
      vector->token_buff = (Token*)malloc(sizeof(Token) * vector->capacity);
      if(vector->token_buff == NULL)
      {
        fprintf(stderr, "error: bad allocation in [token_buff]: %p\n", (void*)vector->token_buff);
        exit(MEMORY_ALLOCATION_FAILURE);
      }
    }
    else
    {
      vector->token_buff = (Token*)realloc(vector->token_buff, sizeof(Token) * vector->capacity);
      if(vector->token_buff == NULL)
      {
        fprintf(stderr, "error: bad reallocation in [token_buff]: %p\n", (void*)vector->token_buff);
        exit(MEMORY_REALLOCATION_FAILURE);
      }
    }
  }
  
  size_t token_value_size = strlen(token.token_value) + sizeof(char);

  Token* tmp_token = (Token*)malloc(sizeof(Token));
  if(tmp_token == NULL)
  {
    fprintf(stderr, "error: bad allocation in [tmp_token]: %p\n", (void*)tmp_token);
    exit(MEMORY_ALLOCATION_FAILURE);
  }

  tmp_token->token_type = token.token_type;
  tmp_token->token_value = (char*)malloc(token_value_size);
  if(tmp_token->token_value == NULL)
  {
    fprintf(stderr, "error: bad allocation in [token_value]: %p\n", (void*)tmp_token->token_value);
    exit(MEMORY_ALLOCATION_FAILURE);  
  }

  // copy string from global token to temporary token 
  memcpy(tmp_token->token_value, token.token_value, token_value_size);
  // add temporary token to inner array of tokens
  vector->token_buff[vector->used++] = *tmp_token;
}

uint64_t VectorSize(Vector* vector)
{
  return vector->used;
}

void VectorPrint(Vector* vector)
{
  if(vector->used == 0)
    printf("[VEC_DEBUG]. vector is empty\n");
  else
  {
    printf("[VEC_DEBUG]. %d elements in vector\n", vector->used);
    for(size_t i = 0; i <  vector->used; i++)
    {
      printf("[VEC_DEBUG]. value of %d token: type - %d, value: %s\n", (i+1), 
      vector->token_buff[i].token_type, 
      vector->token_buff[i].token_value
      );
    }
  }
}

void VectorFree(Vector* vector)
{
  free(vector->token_buff);
  free(vector);
}

// Function that get source code from file to global buffer
static void ScanSourceFile(const char* filename)
{
  FILE *src = fopen(filename, "r");

  if(src != NULL)
  {
    uint64_t character_count = 0;
    char ch = fgetc(src); // character that we get from file

    while(!feof(src))
    {
      ++character_count;
      fgetc(src);
    }

    // Allocate enough memory for characters from file
    gBuffer = (char*)malloc(character_count * sizeof(char) + 1);

    // Check for bad allocation
    if(gBuffer == NULL)
    {
      fprintf(stderr, "error: bad malloc()\n");
      exit(0);
    }

    // Change position indicator for the file stream to start
    fseek(src, 0, 0);
  
    ch = fgetc(src);
    uint64_t i = 0;
    while(!feof(src))
    {
      gBuffer[i++] = ch;
      ch = fgetc(src);
    }
    gBuffer[++i] = '\0';

    fclose(src);
  }
  else
  {
    printf("File was't opened.\n");
  }
}

void AddCharToToken(Token* token, char ch)
{
  if(token->token_value == NULL)
  {
    token->token_value = (char*)malloc(sizeof(char) * 2);
    if(token->token_value == NULL)
    {
      fprintf(stderr, "error: bad allocation in [token_value]: %p\n", (void*)token->token_value);
      exit(MEMORY_ALLOCATION_FAILURE);
    }

    token->token_value[0] = ch;
    token->token_value[1] = '\0';
  }
  else
  {
    size_t token_value_count = strlen(token->token_value) + 2;
    token->token_value = (char*)realloc(token->token_value, sizeof(char) * token_value_count);
    if(token->token_value == NULL)
    {
      fprintf(stderr, "error: bad reallocation in [token_value]: %p\n", (void*)token->token_value);
      exit(MEMORY_REALLOCATION_FAILURE);
    }

    size_t new_right_index = token_value_count - 2;
    token->token_value[new_right_index] = ch;
    token->token_value[++new_right_index] = '\0';
  }
}

void NextCharacter()
{
  character = *gBuffer;
  gBuffer++;
}

// TODO: Debug NextToken()
void NextToken(Vector* vector)
{
again:
  switch (character)
  {
    case 0:
      break;
    case ' ':
    case '\n':
    case '\t':
      NextCharacter();
      goto again;
      break;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      token.token_type = DIGIT;
      AddCharToToken(&token, character);
      NextCharacter();
      goto again;
      break;
    case '+':
    case '-':
    case '*':
    case '/':
      if(token.token_value != NULL)
      {
        VectorAdd(token, vector);
        free(token.token_value);
        token.token_value = NULL;
        token.token_type = OPERATOR;
        AddCharToToken(&token, character);
        NextCharacter();
      }
      else
      {
        token.token_type = OPERATOR;
        AddCharToToken(&token, character);
        NextCharacter();
      }
      break;
    default:
      NextCharacter();
      goto again;
      break;
  }
}

void Parse(char* source_text_buffer, Vector* vector)
{
  
}

int main(int argc, char **argv)
{
  char* buff = NULL;

  if(argc == 1)
  {
    printf("Type 1 filename to interpret\n");
  } 
  else if(argc == 2) 
  { 
    printf("Need to chose one of two flags [i, c] to continue\n");
  } 
  else if(argc == 3)
  {
    // ...
    const char _f1[] = "i";
    const char _f2[] = "c";
    uint8_t _size_of_console_arg_ = strlen(argv[2]);
    char* flag = (char*)malloc((sizeof(char)*_size_of_console_arg_)+sizeof(char));
    for(uint8_t i = 0; i < _size_of_console_arg_; ++i)
    {
      flag[i] = argv[2][i];
    }
    flag[_size_of_console_arg_] = '\0';
    // ...

    // Interpretation 
    if(strcmp(_f1, flag) == 0) { }
    // Compilation
    else if(strcmp(_f2, flag) == 0)
    {
      printf("filename: %s\n", argv[1]);

      printf("Compilation...\n");
      
      // Test Vector
      printf("...\n");

      Token tok;
      tok.token_type = 0;
      tok.token_value = NULL;
      AddCharToToken(&tok, 'a');

      Vector* vec = VectorInit();

      VectorAdd(tok, vec);
      AddCharToToken(&tok, 'a');
      VectorAdd(tok, vec);
      AddCharToToken(&tok, 'a');
      VectorAdd(tok, vec);
      AddCharToToken(&tok, 'a');
      VectorAdd(tok, vec);

      VectorPrint(vec);
      printf("...\n");
      // ...

      Vector* vector = VectorInit();
      printf("Vector size: %d\n", VectorSize(vector));

      ScanSourceFile(argv[1]);

      token.token_value = NULL;

      NextCharacter();
      NextToken(vector);
      VectorPrint(vector);

      free(token.token_value);
      VectorFree(vec);
      VectorFree(vector);
    } 
    else 
    { 
      printf("Incorrectly selected flag\n");
    }

    free(flag);
  } else
  {
    printf("Too many filenames, need one\n");
  }

  //getchar(); to pause the program.
  return 0;
}