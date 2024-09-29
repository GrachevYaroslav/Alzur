/**
* I just want to write my first compiler.
* Alzur - basic programming language. This program is basic translator, 
* which can do compilation(maybe interpretation too?).
*/

/*
  main { } <---- entry point of every alzur program

  -*syntax specification*-

  Entry_Point := "main" "{" Declarations Complex_Operators "}".
  
  Declarations := { "CONST" {Const_Declaration ";"} | "VAR" {Variable_Declaration ";"} }.

  Const_Declaration := Identifier "=" Const_Expression.
  Variable_Declaration := Identifier {"," Identifier} ":" Type_Specifier ";".
  Type_Specifier := Identifier.

  Complex_Operators := Operator { ";" Operator}.
  Operator := Variable ":=" [Expression | [Identifier "."] Identifier ["(" [Parameter {"," Parameter}] ")"]
  | "IF" Expression "THEN"
    Complex_Operators
  {"ELSEIF" Expression "THEN"
    Complex_Operators}
  ["ELSE" 
    Complex_Operators]
  "END"
  | "WHILE" Expression "DO"
    Complex_Operators
  "END"
  ].

  Parameter := Variable | Expression.
  Variable := Identifier.
  Const_Expression := ["+" | "-"] (Number | Identifier).
  Expression := Simple_Expression [Attitude Simple_Expression].
  Simple_Expression := ["+" | "-"] Term {Add_Operator Term}.
  Term := Factor {Mul_Operator Factor}.
  Factor := Identifier ["(" Expression | Type_Specifier ")"]
  | Number
  | "(" Expression ")".
  Attitude := "=" | "#" | "<" | "<=" | ">" | ">=".
  Add_Operator := "+" | "-".
  Mul_Operator := "*" | "DIV" | "MOD".
  Identifier := Letter {Letter | Number}.
  Letter := "a" ... "z" | "A" ... "Z".
  Number := "1" ... "9".
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <ctype.h>

/**
* usefull macro, macro-constants
*/

#define MEMORY_ALLOCATION_FAILURE   2
#define MEMORY_REALLOCATION_FAILURE 3
#define SYNTAX_ERROR                4
#define EMPTY_VECTOR                5
#define FILE_OPEN_ERROR             6

#define global_variable static
#define internal        static

#define PRINT_TABS(x) for(size_t i = 0; i < x; i++) { printf("\t"); } // macro for ast traverse

/**
* alzur's core data structs and typedef's
*/

typedef enum TokenType
{
  DIGIT,
  OPERATOR, // deprecated
  ADD,
  SUBTRACT,
  MULTIPLY,
  DIVIDE,

  NUMBER,
  IDENTIFIER,

  LEFT_BRACKET,
  RIGHT_BRACKET,
  LEFT_BRACE,
  RIGHT_BRACE,

  EQUALS,
  COLON,
  SEMICOLON,
  COMMA,

  CONST,
  VAR,

  INT,
  CHAR,
  BOOl,
  FLOAT,

  MAIN,

  EMPTY =       255
} TokenType;

typedef struct arena_alloc
{
  size_t index;
  size_t size;
  char*  data;
} arena_alloc;

typedef struct Token // TODO: refactor Token struct
{
  TokenType token_type;
  char*     token_value;
  
  union
  {
    int32_t int_value;
    float   float_value;
    char    char_value;
  };

  uint32_t  line_count;
  uint32_t  in_line_pos;
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

typedef struct abstract_syntax_tree
{
  // Token item;
  TokenType token_type;

  union
  {
    size_t  integer;
    uint8_t operator; 
  };

  struct abstract_syntax_tree* left;
  struct abstract_syntax_tree* right;
} abstract_syntax_tree;

/**
* some usefull global's
*/

global_variable char* gBuffer;  // Global buffer, that contain text from source file
global_variable char character; // Global character

global_variable Vector* gTokenVector; // global vector of lexed tokens
global_variable Token gToken; // Global token
global_variable size_t gCurrentTokenIndex = 0; // index(count) of current token used in gTokenVector

/**
* array of reserved words
*/

const char* RESERVED_WORDS[] = 
{
  "MAIN",
  "INT",
  "CHAR",
  "BOOL",
  "FLOAT",
  "IF",
  "THEN",
  "ELSE",
  "ELSEIF",
  "END",
  "FOR",
  "WHILE",
};

/**
*
*/

internal abstract_syntax_tree* parse_single_binary(abstract_syntax_tree*, uint8_t); 
internal abstract_syntax_tree* arithmetic_expr();

/**
* dynamic array implementation(strechy buffer), invented and implemented by Sean Barret.
*/

#define sbfree(a)         ((a) ? free(sbraw(a)),0 : 0)
#define sbpush(a,v)       (sbmaybegrow(a,1), (a)[sbn(a)++] = (v))
#define sbcount(a)        ((a) ? sbn(a) : 0)
#define sbadd(a,n)        (sbmaybegrow(a,n), sbn(a)+=(n), &(a)[sbn(a)-(n)])
#define sblast(a)         ((a)[sbn(a)-1])

#define sbraw(a) ((int *) (a) - 2)
#define sbm(a)   sbraw(a)[0]
#define sbn(a)   sbraw(a)[1]

#define sbneedgrow(a,n)  ((a)==0 || sbn(a)+n >= sbm(a))
#define sbmaybegrow(a,n) (sbneedgrow(a,(n)) ? sbgrow(a,n) : 0)
#define sbgrow(a,n)  sbgrowf((void **) &(a), (n), sizeof(*(a)))

static void sbgrowf(void **arr, int increment, int itemsize)
{
  int m = *arr ? 2*sbm(*arr)+increment : increment+1;
  void *p = realloc(*arr ? sbraw(*arr) : 0, itemsize * m + sizeof(int)*2);
  if (p)
  {
    if (!*arr) ((int *) p)[1] = 0;
    *arr = (void *) ((int *) p + 2);
    sbm(*arr) = m;
  }
}

/*
* hash table implementation
* TODO: finish it 
*/

struct node {
  char* key; // identifier
  char* value;

  struct node* next;
};

struct hash_table {
  size_t capacity, count;

  struct node** buf;
};

void set_node(struct node* node, const char* key, const char* value) {
  size_t key_str_size = strlen(key);
  size_t value_str_size = strlen(value);

  // copy key string in node->key
  node->key = (char*)malloc(key_str_size+1);

  memcpy(node->key, key, strlen(key));
  node->key[key_str_size] = '\0';

  // copy value string in node->value
  node->value = (char*)malloc(value_str_size+1);

  memcpy(node->value, value, value_str_size);
  node->value[value_str_size] = '\0';

  // node setting
  node->next = NULL;
}

struct hash_table* hash_table_init(struct hash_table* ht, size_t size) {
  ht->capacity = size;
  ht->count = 0;

  ht->buf = (struct node**)calloc(size, sizeof(struct node*) * size);

  return ht;
}

int hash_function(struct hash_table* ht, const char* key) {
  uint64_t hash_index;
  uint64_t sum = 0, factor = 31;

  for (size_t i = 0; i < strlen(key); i++) {

    // sum = sum + (ascii value of
    // char * (primeNumber ^ x))...
    // where x = 1, 2, 3....n
    sum = ((sum % ht->capacity) + (((uint64_t)key[i]) * factor) % ht->capacity) % ht->capacity;

    // factor = factor * prime
    // number....(prime
    // number) ^ x
    factor = ((factor % INT16_MAX) * (31 % INT16_MAX)) % INT16_MAX;
  }
 
  hash_index = sum;
  return hash_index;
}

void hash_insert(struct hash_table* ht, const char* key, const char* value) {
  int hash_index = hash_function(ht, key);
  
  struct node* new_node = (struct node*)malloc(sizeof(struct node));
  set_node(new_node, key, value);

  if(ht->buf[hash_index] == NULL)
    ht->buf[hash_index] = new_node;
  else {
    new_node->next = ht->buf[hash_index];
    ht->buf[hash_index] = new_node;
  }
}

char* hash_search(struct hash_table* ht, const char* key) {
  size_t hash_index = hash_function(ht, key);

  struct node* traverse = ht->buf[hash_index];
  while(traverse != NULL) {
    if(strcmp(traverse->key, key) == 0) {
      return traverse->value;
    }
    traverse = traverse->next;
  }

  char* error_msg = (char*)malloc(40);
  error_msg = "hash_table error: Oops! No data found.\n";
  return error_msg;
}

void hash_delete(struct hash_table* ht, const char* key) {
  int hash_index = hash_function(ht, key);

  struct node* prev_node = NULL;
  struct node* curr_node = ht->buf[hash_index];

  while(curr_node != NULL) {
    if(strcmp(curr_node->key, key) == 0) {
      if(curr_node == ht->buf[hash_index]) {
        ht->buf[hash_index] = curr_node->next;
      } else {
        prev_node->next = curr_node->next;
      }
      
      free(curr_node);
      break;
    }
    prev_node = curr_node;
    curr_node = curr_node->next;
  }
}

/*
* usefull malloc/realloc wrappers
*/

void* amalloc(size_t n_bytes)
{
  void* ptr = malloc(n_bytes);
  if(!ptr)
  {
    fprintf(stderr, "error: amalloc failed");
    exit(1);
  }

  return ptr;
}

void* arealloc(void* source, size_t n_bytes)
{
  void* ptr = realloc(source, n_bytes);
  if(!ptr)
  {
    fprintf(stderr, "error: arealloc failed");
    exit(1);
  }

  return ptr;
}

/**
* Arena allocator prototype(for now)
*/

arena_alloc* arena_create(size_t size)
{
  arena_alloc* new_arena = malloc(sizeof(arena_alloc));
  if(new_arena == NULL)
  {
    fprintf(stderr,"error: bad allocation new_arena");
    return NULL;
  }

  new_arena->data = malloc(size);
  if(new_arena->data == NULL)
  {
    fprintf(stderr,"error: bad allocation new_arena->data");
    return NULL;
  }

  new_arena->index = 0;
  new_arena->size = size;

  return new_arena;
};

void* arena_allocate(arena_alloc* arena, size_t size)
{
  if(arena == NULL)
  {
    fprintf(stderr, "error: cant alloc from empty arena");
    return NULL;
  }

  arena->index += size;
  return arena->data + (arena->index - size); 
}

void arena_clear(arena_alloc* arena)
{
  if(arena == NULL)
  {
    return;
  }
  arena->index = 0;
}

void arena_destroy(arena_alloc* arena)
{
  if(arena == NULL)
  {
    return;
  }

  if(arena->data != NULL)
    free(arena->data);

  free(arena);
}

/**
*
*/

FILE* err_out_file_init()
{
  FILE* file = fopen("alzur_error_log.txt", "w+");
  if(!file)
  {
    fprintf(stderr, "error while opening alzur_error_log\n");
  }

  return file;
}

void err_out_file_close(FILE* file)
{
  fclose(file);
}

abstract_syntax_tree* create_node_for_integer(size_t value)
{ 
  abstract_syntax_tree* node = malloc(sizeof(abstract_syntax_tree));
  if(node == NULL)
  {
    fprintf(stderr, "error: bad allocation in ast: %p\n", (void*)node);
    exit(MEMORY_ALLOCATION_FAILURE);
  }

  node->integer = value;
  node->left = NULL;
  node->right = NULL;
  node->token_type = DIGIT;

  printf("create_node_for_integer(): %d\n", node->integer);

  return node;
}

abstract_syntax_tree* create_node_for_operator(uint8_t op)
{ 
  abstract_syntax_tree* node;

  node = malloc(sizeof(abstract_syntax_tree));
  if(node == NULL)
  {
    fprintf(stderr, "error: bad allocation in ast: %p\n", (void*)node);
    exit(MEMORY_ALLOCATION_FAILURE);
  }

  node->operator = op;
  node->left = NULL;
  node->right = NULL;

  printf("create_node_for_operator(): %d\n", node->integer);

  return node;
}

abstract_syntax_tree* create_tree_for_binary
(
abstract_syntax_tree* left,   
uint8_t binary_op,                 
abstract_syntax_tree* right
)
{
  abstract_syntax_tree* ast_root;

  // constructing root node
  
  ast_root = create_node_for_operator(binary_op);

  ast_root->left = left;
  ast_root->right = right;
  ast_root->token_type = binary_op;
  printf("\n");
  printf("[ast_root]->%d\n", ast_root->operator);
  printf("\n");

  return ast_root;
}

void print_abstract_syntax_tree(abstract_syntax_tree* tree, size_t space)
{
  if(tree == NULL)
    return;
  
  space += 5;

  print_abstract_syntax_tree(tree->right, space);

  printf("\n");
  for(int i = 5; i < space; i++)
    printf(" ");
  
  switch (tree->token_type)
  {
  case DIGIT:
    printf("(d: %d )\n", tree->integer);
    break;
  case ADD:
    printf("(op: + )\n");
    break;
  case SUBTRACT:
    printf("(op: - )\n");
    break;
  case MULTIPLY:
    printf("(op: * )\n");
    break;
  case DIVIDE:
    printf("(op: / )\n");
    break;      
  default:
    break;
  }

  print_abstract_syntax_tree(tree->left, space);
}

///////////////////////////

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
      printf("[VEC_DEBUG]: value of %d token: value - %s, type - ", (i+1), vector->token_buff[i].token_value);
      switch (vector->token_buff[i].token_type)
      {
      case DIGIT:
        printf("digit\n");
        break;
      case ADD:
        printf("add\n");
        break;
      case SUBTRACT:
        printf("subtract\n");
        break;
      case MULTIPLY:
        printf("multiply\n");
        break;
      case DIVIDE:
        printf("divide\n");
        break;
      case FLOAT:
        printf("float\n");
        break;
      case LEFT_BRACE:
        printf("left brace\n");
        break;
      case RIGHT_BRACE:
        printf("right brace\n");
        break;
      case EQUALS:
        printf("equals\n");
        break;
      case SEMICOLON:
        printf("semicolon\n");
        break;
      case COLON:
        printf("colon\n");
        break;
      case IDENTIFIER:
        printf("identifier\n");
        break;
      case COMMA:
        printf("comma\n");
        break;
      default:
        printf("\n");
        break;
      };
    }
  }
}

void VectorFree(Vector* vector)
{
  free(vector->token_buff);
  free(vector);
}

size_t ascii_to_int(const char* str)
{
  size_t index = 0;
  size_t strSize = strlen(str);

  size_t result = 0;

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
  VectorAdd(gToken, vector);
  free(gToken.token_value);
  gToken.token_value = NULL;
  gToken.token_type = tok_type;
}

internal void IfTokenNotEmpty(Vector* vector, TokenType tok_type)
{
  if(gToken.token_value != NULL)
  {
    FecthAndPushToVec(vector, tok_type);
    AddCharToToken(&gToken, character);
    VectorAdd(gToken, vector);
    free(gToken.token_value);
    gToken.token_value = NULL;
    gToken.token_type = EMPTY;
    //NextCharacter();
  }
  else
  {
    gToken.token_type = tok_type;
    AddCharToToken(&gToken, character);
    VectorAdd(gToken, vector);
    free(gToken.token_value);
    gToken.token_value = NULL;
    gToken.token_type = EMPTY;
    //NextCharacter();
  }
}

internal bool collision_with_reserved_symbols() {
  for(size_t i = 0; i < (sizeof(RESERVED_WORDS) / sizeof(char*)); i++) {
    if(strcmp(RESERVED_WORDS[i], gToken.token_value) == false)
      return true;
  }
  return false;
}

internal void push_token(Vector* vector_of_tokens, TokenType token_t) {
  gToken.token_type = token_t;

  switch (token_t) {
  case DIGIT:
    gToken.int_value = ascii_to_int(gToken.token_value);
    break;
  default:
    break;
  }
  
  VectorAdd(gToken, vector_of_tokens);
  free(gToken.token_value);
  gToken.token_value = NULL;
  gToken.token_type = EMPTY;
}

internal void next_lexeme(Vector* vector_of_tokens, FILE* file) {
  AddCharToToken(&gToken, character);

  while(isalnum(character = getc(file)) || character == '_') {
    gToken.in_line_pos++;
    AddCharToToken(&gToken, character);
  }

  switch (character)
  {
  case '\n':
  case '\r':
    gToken.in_line_pos = 0;
    gToken.line_count++;
    break;
  case ' ':
    gToken.in_line_pos++;
    break;
  case '{':
    gToken.in_line_pos++;
    push_token(vector_of_tokens, IDENTIFIER);
    AddCharToToken(&gToken, character);
    push_token(vector_of_tokens, LEFT_BRACE);
    break;
  case '}':
    gToken.in_line_pos++;
    push_token(vector_of_tokens, IDENTIFIER);
    AddCharToToken(&gToken, character);
    push_token(vector_of_tokens, RIGHT_BRACE);
    break;
  case ';':
    gToken.in_line_pos++;
    push_token(vector_of_tokens, IDENTIFIER);
    AddCharToToken(&gToken, character);
    push_token(vector_of_tokens, SEMICOLON);
    break;
  case ':':
    gToken.in_line_pos++;
    push_token(vector_of_tokens, IDENTIFIER);
    AddCharToToken(&gToken, character);
    push_token(vector_of_tokens, COLON);
    break;
  case ',':
    gToken.in_line_pos++;
    push_token(vector_of_tokens, IDENTIFIER);
    AddCharToToken(&gToken, character);
    push_token(vector_of_tokens, COMMA);
    break;
  default:
    gToken.in_line_pos++;
    break;
  }

  // if some character retain in vector
  if(gToken.token_value != NULL)
   push_token(vector_of_tokens, IDENTIFIER);
  
}

internal void next_number(Vector* vector_of_tokens, FILE* file) {
  AddCharToToken(&gToken, character);

  while(isdigit(character = getc(file))) {
    AddCharToToken(&gToken, character);
  }

  if(isalpha(character)) {
    fprintf(stderr, "incorrect digit at: %d:%d\n", gToken.line_count, gToken.in_line_pos);
    exit(EXIT_FAILURE);
  }
  
  switch (character)
  {
  case '\n':
  case '\r':
    gToken.in_line_pos = 0;
    gToken.line_count++;
    break;
  case ' ':
    gToken.in_line_pos++;
    break;
  case '{':
    gToken.in_line_pos++;
    push_token(vector_of_tokens, DIGIT);
    AddCharToToken(&gToken, character);
    push_token(vector_of_tokens, LEFT_BRACE);
    break;
  case '}':
    gToken.in_line_pos++;
    push_token(vector_of_tokens, DIGIT);
    AddCharToToken(&gToken, character);
    push_token(vector_of_tokens, RIGHT_BRACE);
    break;
  case ';':
    gToken.in_line_pos++;
    push_token(vector_of_tokens, DIGIT);
    AddCharToToken(&gToken, character);
    push_token(vector_of_tokens, SEMICOLON);
    break;
  case ':':
    gToken.in_line_pos++;
    push_token(vector_of_tokens, DIGIT);
    AddCharToToken(&gToken, character);
    push_token(vector_of_tokens, COLON);
    break;
  case ',':
    gToken.in_line_pos++;
    push_token(vector_of_tokens, DIGIT);
    AddCharToToken(&gToken, character);
    push_token(vector_of_tokens, COMMA);
    break;
  default:
    gToken.in_line_pos++;
    break;
  }

  if(gToken.token_value != NULL) {
    push_token(vector_of_tokens, DIGIT);
  }
}

/** 
* Lexer, function that break expression into tokens
* and put them in vector of tokens
*/
internal void lexer(Vector* vec_of_tokens, const char* filename) { 
  FILE* file = fopen(filename, "r");
  if(file == NULL) {
    fprintf(stderr, "file was not opened");
    exit(FILE_OPEN_ERROR);
  }

  gToken.token_type = EMPTY;
  gToken.token_value = NULL;
  gToken.in_line_pos = 1;
  gToken.line_count = 1;

  while((character = getc(file)) != EOF) {
    gToken.in_line_pos++;

    switch (character) { 
    case ' ':
      break;
    case '\n': case '\r':
      gToken.line_count++;    //  new line
      gToken.in_line_pos = 0; //  position in row reset
      
      if(gToken.token_type != EMPTY) {
        FecthAndPushToVec(vec_of_tokens, EMPTY);
      }
      break;
    case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
      next_number(vec_of_tokens, file);
      break;
    case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g': case 'h': case 'i': case 'j':
    case 'k': case 'l': case 'm': case 'n': case 'o': case 'p': case 'q': case 'r': case 's': case 't':
    case 'u': case 'v': case 'w': case 'x': case 'y': case 'z':
    case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': case 'H': case 'I': case 'J':
    case 'K': case 'L': case 'M': case 'N': case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T':
    case 'U': case 'V': case 'W': case 'X': case 'Y': case 'Z':
    case '_':
      next_lexeme(vec_of_tokens, file);
      break;
    case '+':
      IfTokenNotEmpty(vec_of_tokens, ADD);
      break;
    case '-':
      IfTokenNotEmpty(vec_of_tokens, SUBTRACT);
      break;
    case '*':
      IfTokenNotEmpty(vec_of_tokens, MULTIPLY);
      break;
    case '/':
      IfTokenNotEmpty(vec_of_tokens, DIVIDE);
      break;
    case '(':
      IfTokenNotEmpty(vec_of_tokens, LEFT_BRACE);
      break;
    case ')':
      IfTokenNotEmpty(vec_of_tokens, RIGHT_BRACE);
      break;
    case '=':
      IfTokenNotEmpty(vec_of_tokens, EQUALS);
      break;
    case ';':
      IfTokenNotEmpty(vec_of_tokens, SEMICOLON);
      break;
    case '{':
      IfTokenNotEmpty(vec_of_tokens, LEFT_BRACE);
      break;
    case '}':
      IfTokenNotEmpty(vec_of_tokens, RIGHT_BRACE);
      break;
    case ',':
      IfTokenNotEmpty(vec_of_tokens, COMMA);
      break;
    default:
      break;
    }
  }
  if(gToken.token_value != NULL) {
    FecthAndPushToVec(vec_of_tokens, EMPTY);
  }

  fclose(file);
}

Token* create_empty_token()
{
  Token* empty = malloc(sizeof(Token));
  empty->token_type = EMPTY;
  empty->token_value = NULL;

  return empty;
}

//
// next_token() work with gToken, *gTokenVector
//

internal void next_token()
{
  gToken = gTokenVector->token_buff[gCurrentTokenIndex];
  gCurrentTokenIndex++;
}

internal Token get_next_token()
{
  static size_t count = 1;
  static size_t i = 0;

  gCurrentTokenIndex = i;

  if(count > gTokenVector->used)
  {
    // вернуть пустой токен, как сигнал о том, что токены кончились 
    fprintf(stderr, "error: token_vector is empty\n");
    Token empty_token = {EMPTY, .token_value = NULL, 0, 0};

    return empty_token; 
  }

  Token lToken = gTokenVector->token_buff[i];
  fprintf(stderr, "token[%d] fetched: %s\n", count, lToken.token_value);

  i++;
  count++;
  gCurrentTokenIndex++;

  return lToken;
}

internal size_t parse_digit_d()
{
  Token next = get_next_token();

  size_t value = 0;

  if(next.token_type == DIGIT)
  {
    value = ascii_to_int(next.token_value);
    fprintf(stderr,"digit parsed: %d\n", value);
  }
  else if(next.token_type == EMPTY)
  {
    fprintf(stderr,"[DEBUG]: Empty token vector\n");
  }
  else
  {
    fprintf(stderr, "[DEBUG]: syntax error - seems like incorrect token type of digit\n");
  }

  return value;
}

bool is_operator(Token* operator) {
  if(operator->token_type == ADD ||
     operator->token_type == SUBTRACT ||
     operator->token_type == DIVIDE ||
     operator->token_type == MULTIPLY)
  {
    return true;
  }

  return false;
}

void check(TokenType tok_t, const char* lex) {
  if(gToken.token_type != tok_t) {
    fprintf(stderr, "syntax error: expected %s\n", lex);
    exit(EXIT_FAILURE);
  } else 
    next_token();
}

/*
* function declarations to solve forward declaration
*/
void parse_declarations(void);
void parse_complex_operators(void);
void parse_const_declaration(void);
void parse_variable_declaration(void);
void parse_type_specifier(void);
void parse_operator(void);

/*
* 
*/

void parse_entry_point(void) {
  next_token();
  check(MAIN, "main");
  check(LEFT_BRACE, "{");
  
  parse_declarations();
  parse_complex_operators();

  check(RIGHT_BRACE, "}");
}

void parse_declarations(void) {
  if(gToken.token_type == CONST) {
    parse_const_declaration();
    check(SEMICOLON, ";");
  } else if(gToken.token_type == VAR) {
    parse_variable_declaration();
    check(SEMICOLON, ";");
  }
}


void parse_const_declaration(void) {
  check(IDENTIFIER, "Identifier");
  check(EQUALS, "=");
  if(gToken.token_type != DIGIT) {
    fprintf(stderr, "syntax error: expected digit\n");
    exit(EXIT_FAILURE);
  } else if (gToken.token_type != IDENTIFIER) {
    fprintf(stderr, "syntax error: expected identifier\n");
    exit(EXIT_FAILURE);
  }
  check(SEMICOLON, ";");
}

void parse_variable_declaration(void) {
  check(IDENTIFIER, "Identifier");
  while(gToken.token_type == COMMA) {
    check(IDENTIFIER, "Identifier");
  }

  check(COLON, ":");
  parse_type_specifier();
  check(SEMICOLON, ";");
}

void parse_type_specifier(void) {
  if(gToken.token_type == INT || gToken.token_type == CHAR || gToken.token_type == FLOAT) {
    //...
  } else {
    fprintf(stderr, "syntax error: expected type_specifier\n");
    exit(EXIT_FAILURE);
  }
}

void parse_complex_operators(void) {
  parse_operator(); 
}

void parse_operator(void) {
  
}

void parse_parameter(void);
void parse_variable(void);
void parse_const_expression(void);
void parse_expression(void);
void parse_simple_expression(void);
void parse_term(void);
void parse_factor(void);
void parse_attitude(void);
void parse_add_operator(void);
void parse_mul_operator(void);

/*
abstract_syntax_tree* parse_factor_d()
{
  printf("parse_F start\n");

  abstract_syntax_tree* result;

  if(gToken.token_type == DIGIT)
  {
    result = create_node_for_integer(ascii_to_int(gToken.token_value));
    if(result == NULL)
    {
      printf("error while creating node for integer\n");
      return result;
    }

    next_token();
  }
  else if(gToken.token_type == LEFT_BRACE)
  {
    next_token();
    result = arithmetic_expr();

    if(gToken.token_type != RIGHT_BRACE)
    {
      printf("syntax error: ')' is missing\n");
      return NULL;
    }

    next_token();
  }
  else
  {
    printf("parsing error in factor\n");
    return NULL;
  }

  return result;
}

abstract_syntax_tree* parse_term_d()
{
  printf("parse_T start\n");
  
  abstract_syntax_tree* result = parse_factor();
  if(result == NULL)
  {
    printf("error in factor parsing\n");
    return result;
  }

  while(gToken.token_type == MULTIPLY || gToken.token_type == DIVIDE)
  {
    uint8_t op = gToken.token_type;
    next_token();
    abstract_syntax_tree* right = parse_factor();
    if(right == NULL)
    {
      printf("error in factor parsing\n");
      return result;
    }

    result = create_tree_for_binary(result, op, right);
    if(result == NULL)
    {
      printf("error while creating node for binary\n");
      return result;
    }
  }

  return result;
}

abstract_syntax_tree* arithmetic_expr()
{
  printf("parse_expression start\n");
  
  abstract_syntax_tree* result = parse_term();
  if(result == NULL)
  {
    printf("error in term parsing\n");
    return result;
  }

  while(gToken.token_type == ADD || gToken.token_type == SUBTRACT)
  {
    uint8_t op = gToken.token_type;
    next_token();
    abstract_syntax_tree* right = parse_term();
    if(right == NULL)
    {
      printf("error in term parsing\n");
      return result;
    }

    result = create_tree_for_binary(result, op, right);
    if(result == NULL)
    {
      printf("error while creating node for binary\n");
      return result;
    }
  }

  return result;
}
*/

//////////////////////////

int main(int argc, char **argv) {
  if(argc == 1) {
    printf("Type 1 filename to interpret\n");
  } else if(argc == 2) { 
    printf("Need to chose one of two flags [i, c] to continue\n");
  } else if(argc == 3) {
    // ...
    uint8_t _size_of_console_arg_ = strlen(argv[2]);
    char* flag = (char*)malloc((sizeof(char)*_size_of_console_arg_)+sizeof(char));

    for(uint8_t i = 0; i < _size_of_console_arg_; ++i) {
      flag[i] = argv[2][i];
    }

    flag[_size_of_console_arg_] = '\0';
    // ...

    // Interpretation 
    if(strcmp("i", flag) == 0) {
      printf("filename: %s\n", argv[1]);

      printf("Interpretation...\n\n");
      
      // setup for lexing and parsing
      Vector* vector_of_tokens = VectorInit();

      ScanSourceFile(argv[1]);
      lexer(vector_of_tokens, argv[1]);
      printf("vector_of_tokens size: %d\n\n", VectorSize(vector_of_tokens));

      VectorPrint(vector_of_tokens);
      printf("\n\n");
      
      // global token pointer init
      gTokenVector = vector_of_tokens;

      // parsing
#if 0      
      next_token();
      abstract_syntax_tree* expr_tree = parse_expr();
      if(expr_tree == NULL)
      {
        printf("something went wrong while parsing\n");
      }
      printf("\n\n");
      print_abstract_syntax_tree(expr_tree, 0);
      free(expr_tree);
#endif      
      free(gBuffer);
      free(gToken.token_value);
      VectorFree(vector_of_tokens);
    }
    // Compilation
    else if(strcmp("c", flag) == 0) { } 
    else {
      printf("Incorrectly selected flag\n");
    }

    free(flag);
  } else {
    printf("Too many filenames, need one\n");
  }

  return 0;
}
