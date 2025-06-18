/**
* I just want to write my first compiler.
* Alzur - basic programming language. This program is basic translator,
* which can do compilation(maybe interpretation too?).
*/

/*

  -*syntax specification*-
program      = { function | statement } ;

function     = "func" IDENTIFIER "(" [ parameters ] ")" "{" { statement } "}" ;
parameters   = IDENTIFIER { "," IDENTIFIER } ;

statement    = if_stmt | while_stmt | return_stmt | assign_stmt | expr_stmt ;

if_stmt      = "if" "(" expr ")" "{" { statement } "}" [ "else" "{" { statement } "}" ] ;
while_stmt   = "while" "(" expr ")" "{" { statement } "}" ;
return_stmt  = "return" [ expr ] ";" ;
assign_stmt  = IDENTIFIER "=" expr ";" ;
expr_stmt    = expr ";" ;

expr         = logical_or ;
logical_or   = logical_and { "||" logical_and } ;
logical_and  = equality { "&&" equality } ;
equality     = comparison { ("==" | "!=") comparison } ;
comparison   = term { ("<" | ">" | "<=" | ">=") term } ;
term         = factor { ("+" | "-") factor } ;
factor       = unary { ("*" | "/") unary } ;
unary        = ("!" | "-") unary | primary ;
primary      = NUMBER | IDENTIFIER | "(" expr ")" | call ;
call         = IDENTIFIER "(" [ arguments ] ")" ;
arguments    = expr { "," expr } ;

identifier := letter {letter | number}.
letter := "a" ... "z" | "A" ... "Z".
number := "1" ... "9".
*/

#define _CRT_SECURE_NO_WARNINGS

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

#define FALSE 0
#define TRUE  1

#define PRINT_TABS(x) for(size_t i = 0; i < x; i++) { printf("\t"); } // macro for ast traverse

/**
* alzur's core data structs and typedef's
*/

typedef enum {
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
	EQUAL_EQUAL,
	NOT_EQUAL,
	LOGICAL_NOT,
	LOGICAL_AND,
	LOGICAL_OR,
	LESS,
	LESS_EQUAL,
	GREATER,
	GREATER_EQUAL,
	COLON,
	SEMICOLON,
	COMMA,
	CONST,
	VAR,
	INT,
	CHAR,
	BOOl,
	FLOAT,
	IF,
	ELSE,
	WHILE,
	RETURN,
	FUNC,
	ERROR,
	ROOT,
	STATEMENT,
	END_OF_FILE,

	EMPTY = 255
} TokenType;

typedef struct arena_alloc {
	size_t index;
	size_t size;
	char* data;
} arena_alloc;

typedef struct Token { // TODO: refactor Token struct
	TokenType token_type;

	union {
		int32_t int_value;
		float   float_value;
		char    char_value;
		char* token_value;
	} value;

	uint32_t  line_count;
	uint32_t  in_line_pos;
} Token;

typedef struct Vector {
	Token* token_buff; // Array that contain all tokens

	size_t capacity;
	size_t used;
} Vector;

typedef struct AST {
	Token item;

	struct AST* left;
	struct AST* right;
} AST;

typedef struct abstract_syntax_tree {
	// Token item;
	TokenType token_type;

	union {
		size_t  integer;
		uint8_t operator;
	};

	// add more than two node pointers in onde node
	struct abstract_syntax_tree** nodes;
	struct abstract_syntax_tree* left;
	struct abstract_syntax_tree* right;
} abstract_syntax_tree;

typedef struct {
	Token token;
	struct tree** leafs;
} parse_tree;

/**
* interface for input_stream
*/

typedef struct input_stream {
	FILE* file;
	char current_char;
	uint32_t line;
	uint32_t col;
	bool eof_reached; // end_of_file flag
} input_stream;

/**
* array of reserved words
*/

#define RESERVED_KEYWORDS_COUNT 5

typedef struct reserved_keyword {
	const char* keyword;
	TokenType type;
} reserved_keyword;

reserved_keyword reserved_keywords[RESERVED_KEYWORDS_COUNT] =
{
	{"func", FUNC},
	{"if", IF},
	{"else", ELSE},
	{"while", WHILE},
	{"return", RETURN}
};

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

static void sbgrowf(void** arr, int increment, int itemsize)
{
	int m = *arr ? 2 * sbm(*arr) + increment : increment + 1;
	void* p = realloc(*arr ? sbraw(*arr) : 0, itemsize * m + sizeof(int) * 2);
	if (p)
	{
		if (!*arr) ((int*)p)[1] = 0;
		*arr = (void*)((int*)p + 2);
		sbm(*arr) = m;
	}
}

/*
* hash table implementation
* TODO: finish it
*/

struct node {
	char* key; // identifier
	/*union {
	TokenType t_type;
	int64_t int_value;
	char* value;
	};*/
	char* value;

	struct node* next;
};

typedef struct hash_table {
	size_t capacity, count;

	struct node** buf;
} h_t;

void set_node(struct node* node, const char* key, const char* value) {
	size_t key_str_size = strlen(key);
	size_t value_str_size = strlen(value);

	// copy key string in node->key
	node->key = (char*)malloc(key_str_size + 1);

	memcpy(node->key, key, strlen(key));
	node->key[key_str_size] = '\0';

	// copy value string in node->value
	node->value = (char*)malloc(value_str_size + 1);

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

	if (ht->buf[hash_index] == NULL)
		ht->buf[hash_index] = new_node;
	else {
		new_node->next = ht->buf[hash_index];
		ht->buf[hash_index] = new_node;
	}
}

char* hash_search(struct hash_table* ht, const char* key) {
	size_t hash_index = hash_function(ht, key);

	struct node* traverse = ht->buf[hash_index];
	while (traverse != NULL) {
		if (strcmp(traverse->key, key) == 0) {
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

	while (curr_node != NULL) {
		if (strcmp(curr_node->key, key) == 0) {
			if (curr_node == ht->buf[hash_index]) {
				ht->buf[hash_index] = curr_node->next;
			}
			else {
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

void* amalloc(size_t n_bytes) {
	void* ptr = malloc(n_bytes);
	if (!ptr) {
		fprintf(stderr, "error: amalloc failed");
		exit(1);
	}

	return ptr;
}

void* arealloc(void* source, size_t n_bytes) {
	void* ptr = realloc(source, n_bytes);
	if (!ptr) {
		fprintf(stderr, "error: arealloc failed");
		exit(1);
	}

	return ptr;
}

/**
* Arena allocator prototype(for now)
*/

arena_alloc* arena_create(size_t size) {
	arena_alloc* new_arena = malloc(sizeof(arena_alloc));
	if (new_arena == NULL) {
		fprintf(stderr, "error: bad allocation new_arena");
		return NULL;
	}

	new_arena->data = malloc(size);
	if (new_arena->data == NULL) {
		fprintf(stderr, "error: bad allocation new_arena->data");
		return NULL;
	}

	new_arena->index = 0;
	new_arena->size = size;

	return new_arena;
}

void* arena_allocate(arena_alloc* arena, size_t size) {
	if (arena == NULL) {
		fprintf(stderr, "error: cant alloc from empty arena");
		return NULL;
	}

	arena->index += size;
	return arena->data + (arena->index - size);
}

void arena_clear(arena_alloc* arena) {
	if (arena == NULL)
		return;

	arena->index = 0;
}

void arena_destroy(arena_alloc* arena) {
	if (arena == NULL) {
		return;
	}

	if (arena->data != NULL)

		free(arena->data);
	free(arena);
}

/**
*
*/

abstract_syntax_tree* create_node_for_integer(size_t value)
{
	abstract_syntax_tree* node = malloc(sizeof(abstract_syntax_tree));
	if (node == NULL)
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
	if (node == NULL)
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

abstract_syntax_tree* create_tree_for_binary(abstract_syntax_tree* left, uint8_t binary_op, abstract_syntax_tree* right)
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
	if (tree == NULL)
		return;

	space += 5;

	print_abstract_syntax_tree(tree->right, space);

	printf("\n");
	for (int i = 5; i < space; i++)
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

Token create_eof_token() {
	Token token;

	token.value.token_value = NULL;
	token.token_type = END_OF_FILE;

	return token;
}

Token create_token(TokenType t, const char* v, uint32_t len, uint32_t line_c, uint32_t line_p) {
	Token token;

	token.token_type = t;
	token.line_count = line_c;
	token.in_line_pos = line_p;

	token.value.token_value = amalloc(len + 1);

	// copying characters to token
	memcpy(token.value.token_value, v, len);

	return token;
}

Token create_error_token() {
	Token err_tok;

	err_tok.token_type = ERROR;

	return err_tok;
}

void push_token(Token* token, Token* tokens) {
	sbpush(tokens, *token);
}

input_stream* input_stream_init(const char* filename) {
	input_stream* is = amalloc(sizeof(input_stream));

	is->file = fopen(filename, "r");
	if (!is->file)
		printf("error while opening file: %s\n", filename);

	is->current_char = '\0';
	is->line = 0;
	is->col = 1;
	is->eof_reached = FALSE;

	return is;
}

void input_stream_free(input_stream* is) {
	fclose(is->file);
	free(is);
}

char input_stream_next_char(input_stream* is) {
	if (is->eof_reached) {
		return EOF;
	}

	int c = fgetc(is->file);

	if (c == EOF) {
		is->eof_reached = TRUE;
		is->current_char = EOF;
		return EOF;
	}

	is->current_char = (char)c;
	if (is->current_char == '\n') {
		is->line++;
		is->col = 0;
	}
	else
		is->col++;

	return is->current_char;
}

char input_stream_peek_char(input_stream* is) {
	if (is->eof_reached) {
		return EOF;
	}

	int c = fgetc(is->file);

	if (c == EOF) {
		is->eof_reached = TRUE;
		return EOF;
	}

	ungetc(c, is->file);

	return (char)c;
}

void input_stream_error(input_stream* is, const char* msg) {
	fprintf(stderr, "%d:%d ", is->line, is->col);
	fprintf(stderr, msg);
}

size_t ascii_to_int(const char* str)
{
	size_t index = 0;
	size_t strSize = strlen(str);

	size_t result = 0;

	if (strSize == 1)
	{
		result = str[index] - '0';
		return result;
	}

	while (index < strSize)
	{
		result += str[index] - '0';

		if ((index + 1) == strSize)
			break;

		result *= 10;
		index++;
	}

	return result;
}

/**
* Lexer, function that break expression into tokens
* and put them in vector of tokens
*/

Token tokenize_number(input_stream* is) {
	Token token;

	// buffer for digit symbols
	char* tmp_buff_for_digit = NULL;

	sbpush(tmp_buff_for_digit, is->current_char);

	while (isdigit(input_stream_peek_char(is))) {
		is->current_char = input_stream_next_char(is);
		sbpush(tmp_buff_for_digit, is->current_char);
	}
	sbpush(tmp_buff_for_digit, '\0');

	token = create_token(DIGIT, tmp_buff_for_digit, sbcount(tmp_buff_for_digit), is->col, is->line);
	sbfree(tmp_buff_for_digit);

	return token;
}

Token tokenize_identifier_or_keyword(input_stream* is) {
	Token token;

	// buffer for lexeme symbols
	char* tmp_buff_for_lexeme = NULL;

	sbpush(tmp_buff_for_lexeme, is->current_char);

	while (isalpha(input_stream_peek_char(is)) || isdigit(input_stream_peek_char(is))) {
		is->current_char = input_stream_next_char(is);
		sbpush(tmp_buff_for_lexeme, is->current_char);
	}
	sbpush(tmp_buff_for_lexeme, '\0');

	// Comparing strings to find collision with reserved keyword
	for (uint8_t i = 0; i < RESERVED_KEYWORDS_COUNT; i++) {
		if (strcmp(tmp_buff_for_lexeme, reserved_keywords[i].keyword) == 0) {
			token = create_token(reserved_keywords[i].type, tmp_buff_for_lexeme, sbcount(tmp_buff_for_lexeme), is->col, is->line);
			sbfree(tmp_buff_for_lexeme);
			return token;
		}
	}

	token = create_token(IDENTIFIER, tmp_buff_for_lexeme, sbcount(tmp_buff_for_lexeme), is->col, is->line);
	sbfree(tmp_buff_for_lexeme);

	return token;
}

Token get_next_token(input_stream* is) {
	// 1. Пропускаем пробелы и комментарии
	while (isspace(input_stream_peek_char(is)) ||
		input_stream_peek_char(is) == '/') { // Пример для комментариев
		if (isspace(input_stream_peek_char(is))) {
			input_stream_next_char(is);
		}
		else if (input_stream_peek_char(is) == '/') {
			// Обработка комментариев (однострочные // или многострочные /* */)
			input_stream_next_char(is); // Съедаем первый /
			if (input_stream_peek_char(is) == '/') { // Однострочный
				input_stream_next_char(is); // Съедаем второй /
				while (input_stream_peek_char(is) != '\n' &&
					!is->eof_reached) {
					input_stream_next_char(is);
				}
			}
			else if (input_stream_peek_char(is) == '*') { // Многострочный
				input_stream_next_char(is); // Съедаем *
				char prev_char = 0;
				while (!is->eof_reached && !(prev_char == '*' && input_stream_peek_char(is) == '/')) {
					prev_char = input_stream_next_char(is);
				}
				input_stream_next_char(is); // Съедаем закрывающий /
			}
			else {
				// Это просто оператор деления
				return create_token(DIVIDE, "/", 1, is->line, is->col);
			}
		}
	}

	// Проверка на EOF
	if (is->eof_reached) {
		return create_eof_token(); // Специальный токен конца файла
	}

	// 2. Считываем текущий символ
	char current_char = input_stream_next_char(is);
	uint32_t token_line = is->line;
	uint32_t token_col = is->col - 1; // Токен начинается с предыдущей позиции

	// 3. Сборка лексемы и определение типа токена (switch-case по current_char)
	if (isdigit(current_char)) {
		// Логика для чисел
		// Используем вспомогательную функцию для сбора числа
		// и проверяем, не идет ли следом 'a' (некорректное число)
		return tokenize_number(is);
	}
	else if (isalpha(current_char) || current_char == '_') {
		// Логика для идентификаторов и зарезервированных слов
		// Используем вспомогательную функцию для сбора лексемы
		// и затем проверяем, является ли она зарезервированным словом.
		return tokenize_identifier_or_keyword(is);
	}
	else {
		// Логика для операторов и разделителей
		switch (current_char) {
		case '+': return create_token(ADD, "+", 1, token_line, token_col);
		case '-': return create_token(SUBTRACT, "-", 1, token_line, token_col);
		case '*': return create_token(MULTIPLY, "*", 1, token_line, token_col);
		case '(': return create_token(LEFT_BRACKET, "(", 1, token_line, token_col); // Исправил с BRACE на BRACKET
		case ')': return create_token(RIGHT_BRACKET, ")", 1, token_line, token_col); // Исправил с BRACE на BRACKET
		case '{': return create_token(LEFT_BRACE, "{", 1, token_line, token_col);
		case '}': return create_token(RIGHT_BRACE, "}", 1, token_line, token_col);
		case ';': return create_token(SEMICOLON, ";", 1, token_line, token_col);
		case ',': return create_token(COMMA, ",", 1, token_line, token_col);
		case '=':
			if (input_stream_peek_char(is) == '=') {
				input_stream_next_char(is); // Съедаем второй '='
				return create_token(EQUAL_EQUAL, "==", 2, token_line, token_col); // Новый тип токена
			}
			else {
				return create_token(EQUALS, "=", 1, token_line, token_col);
			}
		case '!':
			if (input_stream_peek_char(is) == '=') {
				input_stream_next_char(is); // Съедаем '='
				return create_token(NOT_EQUAL, "!=", 2, token_line, token_col); // Новый тип токена
			}
			else {
				// Это может быть оператор логического НЕ (!)
				return create_token(LOGICAL_NOT, "!", 1, token_line, token_col); // Добавить в TokenType
			}
			// ... Добавить остальные операторы: <, <=, >, >=, &&, ||
		case '&':
			if (input_stream_peek_char(is) == '&') {
				input_stream_next_char(is);
				return create_token(LOGICAL_AND, "&&", 2, token_line, token_col);
			}
			else {
				// Ошибка или другой оператор (& битовая)
				input_stream_error(is, "Unexpected character '&'");
				return create_error_token();
			}
		case '|':
			if (input_stream_peek_char(is) == '|') {
				input_stream_next_char(is);
				return create_token(LOGICAL_OR, "||", 2, token_line, token_col);
			}
			else {
				// Ошибка или другой оператор (| битовая)
				input_stream_error(is, "Unexpected character '|'");
				return create_error_token();
			}
		case '<':
			if (input_stream_peek_char(is) == '=') {
				input_stream_next_char(is);
				return create_token(LESS_EQUAL, "<=", 2, token_line, token_col);
			}
			else {
				return create_token(LESS, "<", 1, token_line, token_col);
			}
		case '>':
			if (input_stream_peek_char(is) == '=') {
				input_stream_next_char(is);
				return create_token(GREATER_EQUAL, ">=", 2, token_line, token_col);
			}
			else {
				return create_token(GREATER, ">", 1, token_line, token_col);
			}
		default:
			input_stream_error(is, "Unexpected character");
			return create_error_token(); // Создать токен ошибки
		}
	}
}

parse_tree* tree_create_leaf(Token token) {
	parse_tree* t = amalloc(sizeof(parse_tree));

	t->token = token;
	t->leafs = NULL;

	return t;
}

void tree_add(parse_tree* t, parse_tree* child) {
	sbpush(t->leafs, child);
}

void tree_traverse(parse_tree* t, int depth) {
	if (t == NULL)
		return;

	for (int i = 0; i < depth; i++) {
		printf("  ");
	}

	printf("value - %s, leafs - %d\n", t->token.value.token_value, sbcount(t->leafs));

	for (int i = 0; i < sbcount(t->leafs); i++) {
		tree_traverse(t->leafs[i], depth + 1);
	}
}

/*
* implementation of terminals
*/

void parse_function(input_stream* is, parse_tree* root);
void parse_parametrs_opt(input_stream* is, parse_tree* parent);
void parse_statement_list(input_stream* is, parse_tree* root);

parse_tree* parse_program(input_stream* is) {
	Token token;
	token.token_type = ROOT;

	parse_tree* root = tree_create_leaf(token);

	parse_function(is, root);

	return root;
}

void parse_function(input_stream* is, parse_tree* root) {
	Token token = get_next_token(is);

	if (token.token_type == FUNC) {
		parse_tree* func = tree_create_leaf(token);
		tree_add(root, func);

		token = get_next_token(is);
		if (token.token_type == IDENTIFIER) {
			parse_tree* identifier = tree_create_leaf(token);
			tree_add(root, identifier);

			token = get_next_token(is);
			if (token.token_type == LEFT_BRACKET) {
				parse_tree* left_bracket = tree_create_leaf(token);
				tree_add(root, left_bracket);
				
				parse_parametrs_opt(is, root);
				
				token = get_next_token(is);
				if(token.token_type == RIGHT_BRACE) {
					parse_tree* left_brace = tree_create_leaf(token);
					tree_add(root, left_brace);
					
					token = create_token(STATEMENT, NULL, 0, token.line_count, token.in_line_pos);
					parse_tree* statement = tree_create_leaf(token);
					tree_add(root, statement);
					parse_statement_list(is, statement);
				}
				
			} else {
				token = create_error_token();
				parse_tree* error_tree = tree_create_leaf(token);
				tree_add(root, error_tree);

				printf("error: supposed to be left_bracket\n");
			}
		}
		else {
			token = create_error_token();
			parse_tree* error_tree = tree_create_leaf(token);
			tree_add(root, error_tree);

			printf("error: supposed to be identifier\n");
		}
	}
	else {
		token = create_error_token();
		parse_tree* error_tree = tree_create_leaf(token);
		tree_add(root, error_tree);

		printf("error: supposed to be 'func'\n");
	}
}

void parse_parametrs_opt(input_stream* is, parse_tree* parent) {
	Token token = get_next_token(is);
	
	if(token.token_type == IDENTIFIER) {
		parse_tree* parametr = tree_create_leaf(token);
		tree_add(parent, parametr);
		
		while((token = get_next_token(is)).token_type == COMMA) {
			parse_tree* comma = tree_create_leaf(token);
			tree_add(parent, comma);
			
			token = get_next_token(is);
			if(token.token_type == IDENTIFIER) {
				parse_tree* parametr_ = tree_create_leaf(token);
				tree_add(parent, parametr_);
			}
		}
		
		if(token.token_type == RIGHT_BRACKET) {
			parse_tree* right_bracket = tree_create_leaf(token);
			tree_add(parent, right_bracket);
			return;
		}
	} else if(token.token_type == RIGHT_BRACKET) {
		parse_tree* right_bracket = tree_create_leaf(token);
		tree_add(parent, right_bracket);
		return;
	}
}

void parse_statement_list(input_stream* is, parse_tree* root) {
	Token token = get_next_token(is);
	
	if(token.token_type = IF) {
		parse_tree* if_ = tree_create_leaf(token);
		tree_add(root, if_);
		
		token = get_next_token(is);
		if(token.token_type == LEFT_BRACKET) {
			parse_tree* left_bracket = tree_create_leaf(token);
			tree_add(root, left_bracket);
			
			token = get_next_token(is);
		}
	}
}

int main(int argc, char** argv) {
	char f[64];

	if (argc == 1) {
		printf("Type 1 filename to interpret\n");
	}
	else if (argc == 2) {
		printf("Need to chose one of two flags [i, c] to continue\n");
	}
	else if (argc == 3) {
		// ...
		uint8_t _size_of_console_arg_ = strlen(argv[2]);
		char* flag = (char*)malloc((sizeof(char) * _size_of_console_arg_) + sizeof(char));

		for (uint8_t i = 0; i < _size_of_console_arg_; ++i) {
			flag[i] = argv[2][i];
		}

		flag[_size_of_console_arg_] = '\0';
		// ...

		// Interpretation 
		if (strcmp("c", flag) == 0) {
			printf("filename: %s\n", argv[1]);

			printf("Compilation...\n\n");

			// setup for lexing and parsing
			input_stream* is;

			is = input_stream_init(argv[1]);

			parse_tree* program = parse_program(is);
			tree_traverse(program, 0);
		
#if 1
			Token token = get_next_token(is);
			while (token.token_type != END_OF_FILE) {
				printf("lexeme: %s; type: %d\n", token.value.token_value, token.token_type);
				token = get_next_token(is);
			}
#endif
		}
		// Compilation
		else if (strcmp("c", flag) == 0) {}
		else {
			printf("Incorrectly selected flag\n");
		}

		free(flag);
	}
	else {
		printf("Too many filenames, need one\n");
	}
#if 0
	printf(":>");
	if (fgets(f, sizeof(f), stdin) != NULL) {
		f[strcspn(f, "\n")] = '\0';
		printf("%s\n", f);
	}
	else
		printf("error: reading with fgets or EOF\n");

	input_stream* is;

	is = input_stream_init(f);

	Token token = get_next_token(is);
	while (token.token_type != END_OF_FILE) {
		printf("lexeme: %s; type: %d\n", token.value.token_value, token.token_type);
		token = get_next_token(is);
	}
#endif
	return 0;
}
