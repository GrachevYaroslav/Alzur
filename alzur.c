// I just want to write my first compiler.
// Alzur - basic programming language. This program is basic translator, 
// which can do compilation(maybe interpretation too?).

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define MEMORY_ALLOCATION_FAILURE   2
#define MEMORY_REALLOCATION_FAILURE 3
#define SYNTAX_ERROR                4

#define ALZUR_TRUE                  1
#define ALZUR_FALSE                 0

#define global_variable static
#define internal        static

typedef enum TokenType
{
  DIGIT = 0,
  OPERATOR = 1,
  ADD = 2,
  SUBTRACT = 3,
  MULTIPLY = 4,
  DIVIDE = 5,
  EMPTY = 255
} TokenType;

typedef struct Token
{
  TokenType token_type;
  char* token_value;
} Token;

typedef struct Vector
{
  Token* token_buff; // Array that contain all tokens
  size_t capacity;
  size_t used;
} Vector;

typedef struct AST
{
  Token item;
  struct AST* left;
  struct AST* right;
} AST;

global_variable char* gBuffer;  // Global buffer, that contain text from source file
global_variable char  character; // Global character

global_variable Token token;    // Global character

global_variable Vector* gTokenVector;
global_variable size_t  gTokenVectorIndex;

global_variable AST* gAST;
// ...

AST* alzur_ast_init()
{
  AST* root = (AST*)malloc(sizeof(AST));
  if(root == NULL)
  {
    fprintf(stderr, "error: bad allocation in root: %p\n", (void*)root);
    exit(MEMORY_ALLOCATION_FAILURE);
  }

  root->item.token_type  = EMPTY;
  root->item.token_value = NULL;

  root->left  = NULL;
  root->right = NULL;

  return root;
}

AST* alzur_ast_add_item(AST* root, Token token)
{
  size_t token_string_size = strlen(token.token_value);

  root->item.token_value = (char*)malloc(sizeof(char) * (token_string_size + 1));
  if(root->item.token_value == NULL)
  {
    fprintf(stderr, "error: bad allocation in root: %p\n", (void*)root);
    exit(MEMORY_ALLOCATION_FAILURE);
  }

  memcpy(root->item.token_type, token.token_value, token_string_size);

  root->item.token_value[token_string_size] = '\0';
  root->item.token_type = OPERATOR;

  return root;
}

AST* alzur_ast_create_node(Token token)
{
  AST* new_node = (AST*)malloc(sizeof(AST));
  if(new_node == NULL)
  {
    fprintf(stderr, "error: bad allocation in new_node: %p\n", (void*)new_node);
    exit(MEMORY_ALLOCATION_FAILURE);
  }

  new_node->item.token_type = token.token_type;

  size_t token_string_size = strlen(token.token_value);
  new_node->item.token_value = (char*)malloc(sizeof(char) * (token_string_size + 1));
  if(new_node->item.token_value == NULL)
  {
    fprintf(stderr, "error: bad allocation in new_node: %p\n", (void*)new_node);
    exit(MEMORY_ALLOCATION_FAILURE);
  }

  memcpy(new_node->item.token_value, token.token_value, token_string_size); // copy string from func argument to new node

  new_node->item.token_value[token_string_size] = '\0';

  new_node->left = NULL;
  new_node->right = NULL;

  return new_node;
}

void alzur_ast_add_on_left(AST** node, Token token)
{
  if(*node == NULL)
  {
    *node = alzur_ast_create_node(token);
  }
  else if((*node)->left != NULL) // if u forget, node->left is a key to traverse next node's.
  {
    alzur_ast_add_on_left(&(*node)->left, token);
  }
  else
  {
    (*node)->left = alzur_ast_create_node(token);
  }
}

void alzur_ast_add_on_right(AST** node, Token token)
{
  if(*node == NULL)
  {
    *node = alzur_ast_create_node(token);
  }
  else if((*node)->right != NULL)
  {
    alzur_ast_add_on_right(&(*node)->right, token);
  }
  else
  {
    (*node)->right = alzur_ast_create_node(token);
  }
}

void alzur_ast_add_operator(AST** node, Token token)
{
  if((*node)->item.token_value == NULL)
  {
    alzur_ast_add_item(*node, token);
  }
}

#define PRINT_TABS(x) for(size_t i = 0; i < x; i++) { printf("\t"); } // macro for ast traverse

void alzur_ast_traverse_recursive(AST* root, size_t level)
{
  if(root != NULL)
  {
    PRINT_TABS(level)
    printf("value = ( %s )\n", root->item.token_value);
    
    PRINT_TABS(level)
    printf("left\n");
    alzur_ast_traverse_recursive(root->left, level + 1);
    
    PRINT_TABS(level)
    printf("right\n");
    alzur_ast_traverse_recursive(root->right, level + 1);
  }
  return;
}

void alzur_ast_traverse(AST* root)
{
  alzur_ast_traverse_recursive(root, 0);
}

// Initialize vector object
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

void VectorAdd(Token token, Vector* vector) // add token to vector
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

// Print value of vector
void VectorPrint(Vector* vector)
{
  if(vector->used == 0)
    printf("[VEC_DEBUG]: vector is empty\n");
  else
  {
    printf("[VEC_DEBUG]: %d elements in vector\n", vector->used);
    for(size_t i = 0; i <  vector->used; i++)
    {
      printf("[VEC_DEBUG]: value of %d token: type - ( %d ), value - ( %s )\n", (i+1), 
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

uint64_t ChrToInt(const char* str)
{
  size_t index = 0;
  size_t strSize = strlen(str);

  uint64_t result = 0;

  if(strSize == 1)
  {
    result = str[index] - '0';
    return result;
  }

  while(index < strSize)
  {
    result += str[index] - '0';

    if((index+1) == strSize)
      break;

    result *= 10;
    index++;
  }

  return result;
}

// Function that get source code from file to global buffer
internal void ScanSourceFile(const char* filename)
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

// Add character in token object
internal void AddCharToToken(Token* token, char ch)
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

internal void NextCharacter()
{
  character = *gBuffer;
  gBuffer++;
}

internal void FecthAndPushToVec(Vector* vector, TokenType tok_type)
{
  VectorAdd(token, vector);
  free(token.token_value);
  token.token_value = NULL;
  token.token_type = tok_type;
}

internal void IfTokenNotEmpty(Vector* vector, TokenType tok_type)
{
  if(token.token_value != NULL)
  {
    VectorAdd(token, vector);
    free(token.token_value);  
    token.token_value = NULL;
    token.token_type = tok_type;
    AddCharToToken(&token, character);
    NextCharacter();
  }
  else
  {
    token.token_type = tok_type;
    AddCharToToken(&token, character);
    VectorAdd(token, vector);
    free(token.token_value);
    token.token_value = NULL;
    token.token_type = EMPTY;
    NextCharacter();
  }
}

// Tokenizer, function that break sentence into tokens
// and put them in vector of tokens
internal void Tokenizer(Vector* vector)
{
again:
  switch (character)
  {
    case 0:
      if(token.token_value != NULL)
        VectorAdd(token, vector);

      break;
    case ' ':
      if(token.token_value != NULL)
      {
        FecthAndPushToVec(vector, EMPTY);
        NextCharacter();
        goto again;
        break;
      }
      else
      {
        NextCharacter();
        goto again;
        break;
      }
    case '\n': // TODO: Fix white space after new line bug
      if(token.token_value != NULL)
      {
        FecthAndPushToVec(vector, EMPTY);
        NextCharacter();
        goto again;
      }
      else
      {
        NextCharacter();
        goto again;
      }
      break;
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
      if(token.token_type == DIGIT || token.token_type == EMPTY)
      {
        token.token_type = ALZUR_FALSE;
        token.token_type = DIGIT;
        AddCharToToken(&token, character);
        NextCharacter();
        goto again;
      }
      else
      {
        FecthAndPushToVec(vector, EMPTY);
        AddCharToToken(&token, character);
        token.token_type = ALZUR_FALSE;
        token.token_type = DIGIT;
        NextCharacter();
        goto again;
      }
      /*
      token.token_type = DIGIT;
      AddCharToToken(&token, character);
      NextCharacter();
      goto again;
      break;
      */
    case '+': // TODO: Fix operator handling
      IfTokenNotEmpty(vector, ADD);
      goto again;
      break;
    case '-':
      IfTokenNotEmpty(vector, SUBTRACT);
      goto again;
      break;
    case '*':
      IfTokenNotEmpty(vector, MULTIPLY);
      goto again;
      break;
    case '/':
      IfTokenNotEmpty(vector, DIVIDE);
      goto again;
      break;
    default:
      NextCharacter();
      goto again;
      break;
  }
}

void NextToken()
{
  token = gTokenVector->token_buff[gTokenVectorIndex];
  gTokenVectorIndex++;
}

// Grammar
// <expr> := <term> {("+" | "-") <term>}
// <term> = number {("*" | "/" number)}

// 1 * 2 + 3 * 2 + 1

internal uint8_t Term()
{
  if(token.token_type == DIGIT)
  {
    alzur_ast_add_on_left(&gAST, token);
    NextToken();
    if(token.token_type == MULTIPLY)
    {
      alzur_ast_add_operator(&gAST, token);
      NextToken();
      if(token.token_type == DIGIT)
      {
        if(Term())
          return ALZUR_TRUE;
        else
          return ALZUR_FALSE;
      }
    }
    else if(token.token_type == DIVIDE)
    {
      // something ...
      NextToken();
      if(token.token_type == DIGIT)
      {
        if(Term())
          return ALZUR_TRUE;
        else
          return ALZUR_FALSE;
      }
    }
    // if in term only one digit
  }
  else if(token.token_type == MULTIPLY)
  {
    // something ...
    NextToken();
    if(token.token_type == DIGIT)
    {
      // something ...
      if(Term())
        return ALZUR_TRUE;
      else
        return ALZUR_FALSE;
    }
  }
  else if(token.token_type == DIVIDE)
  {
    // something ...
    if(Term())
        return ALZUR_TRUE;
      else
        return ALZUR_FALSE;
  }
  else if(token.token_type == ADD || token.token_type == SUBTRACT)
    return ALZUR_TRUE;
  else
    return ALZUR_FALSE;
  return 255; // need to explicitly return some uint8 value
}

internal uint8_t Expr()
{
  if(Term())
  {
    if(token.token_type == ADD)
    {
      if(Term())
        return ALZUR_TRUE;
      else
        return ALZUR_FALSE;
    }
    else if(token.token_type == SUBTRACT)
    {
      if(Term())
        return ALZUR_TRUE;
      else
        return ALZUR_FALSE;
    }
    else
    {
      return ALZUR_FALSE;
    }
  }
  else
    return ALZUR_FALSE;
}

internal void Parse(Vector* vector)
{
  size_t SizeOfVector =    0;
  uint64_t left_operand =  0;
  uint64_t right_operand = 0;

  TokenType operator_type;

  while(SizeOfVector < vector->used)
  {
    Token tok = vector->token_buff[SizeOfVector];
    if(tok.token_type == DIGIT)
    {
      // TODO: add insert() in tree
      left_operand = ChrToInt(vector->token_buff[SizeOfVector].token_value);
      SizeOfVector++;
      tok = vector->token_buff[SizeOfVector];
      if(tok.token_type == ADD ||
         tok.token_type == SUBTRACT ||
         tok.token_type == MULTIPLY ||
         tok.token_type == DIVIDE)
      {
        if(tok.token_type == ADD)
          operator_type = ADD;
        else if(tok.token_type == SUBTRACT)
          operator_type = SUBTRACT;
        else if(tok.token_type == MULTIPLY)
          operator_type = MULTIPLY;
        else
          operator_type = DIVIDE;

        SizeOfVector++;
        tok = vector->token_buff[SizeOfVector];
        if(tok.token_type == DIGIT)
        {
          if(operator_type == ADD)
          {
            right_operand = ChrToInt(vector->token_buff[SizeOfVector].token_value);
            printf("result: %d\n", left_operand+right_operand);
          }  
          else if(operator_type == SUBTRACT)
          {
            right_operand = ChrToInt(vector->token_buff[SizeOfVector].token_value);
            printf("result: %d\n", left_operand-right_operand);
          }
          else if(operator_type == MULTIPLY)
          {
            right_operand = ChrToInt(vector->token_buff[SizeOfVector].token_value);
            printf("result: %d\n", left_operand*right_operand);
          }
          else
          {
            right_operand = ChrToInt(vector->token_buff[SizeOfVector].token_value);
            printf("result: %d\n", left_operand/right_operand);
          }
          
          break;
        }
        else
        {
          fprintf(stderr, "syntax error\n");
          break;
        }
      }
      else
      {
        fprintf(stderr, "syntax error\n");
        break;
      }
    }
    else
    {
      fprintf(stderr, "syntax error\n");
      break;
    }

    SizeOfVector++;
  }
}

int main(int argc, char **argv)
{
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
    if(strcmp(_f1, flag) == 0) 
    {
      printf("filename: %s\n", argv[1]);

      printf("Interpretation...\n");

      Vector* vector = VectorInit();

      ScanSourceFile(argv[1]);

      token.token_value = NULL;

      NextCharacter();
      Tokenizer(vector);
      printf("%d\n", VectorSize(vector));
      VectorPrint(vector);

      // global token pointer init
      gTokenVector = vector;

      Parse(vector);

      printf("%d\n", gTokenVectorIndex);
      printf("%p\n\n", (void*)gTokenVector);

      const char *str1 = "123";
      const char *str2 = "321";

      char* tok1 = (char*)malloc(sizeof(char) * (strlen(str1) + 1));
      char* tok2 = (char*)malloc(sizeof(char) * (strlen(str2) + 1));

      memcpy(tok1, str1, strlen(str1));
      memcpy(tok2, str2, strlen(str2));

      tok1[3] = '\0';
      tok2[3] = '\0';

      Token _token1;
      _token1.token_type = DIGIT;
      _token1.token_value = tok1;

      Token _token2;
      _token2.token_type = DIGIT;
      _token2.token_value = tok2;

      // example

      AST* ast = NULL;
      alzur_ast_add_on_left(&ast, _token1);

      alzur_ast_add_on_left(&ast, _token1);
      alzur_ast_add_on_left(&ast, _token2);

      alzur_ast_add_on_right(&ast, _token2);
      alzur_ast_add_on_right(&ast, _token2);

      printf("===---AST Traverse---===\n");
      alzur_ast_traverse(ast);
      printf("===------------------===\n");

      free(gBuffer);
      free(token.token_value);
      VectorFree(vector);
    }
    // Compilation
    else if(strcmp(_f2, flag) == 0) { } 
    else 
    {
      printf("Incorrectly selected flag\n");
    }

    free(flag);
  } else
  {
    printf("Too many filenames, need one\n");
  }

  return 0;
}