/**
* Alzur - basic programming language. This program is basic translator,
* which can do compilation(maybe interpretation too?).
*/

/*
  -*syntax specification*-
program          = { function | statement }

function         = "func" IDENTIFIER "(" parameters_opt ")" "{" statement_list "}"
parameters_opt   = parameters | ε
parameters       = IDENTIFIER parameter_list
parameter_list   = { "," IDENTIFIER }

statement        = if_stmt
				 | while_stmt
				 | return_stmt
				 | assignment_or_call_stmt ; // Левая факторизация для IDENTIFIER

statement_list   = { statement }

if_stmt          = "if" "(" expr ")" "{" statement_list "}" else_stmt_opt
else_stmt_opt    = "else" "{" statement_list "}" | ε
while_stmt       = "while" "(" expr ")" "{" statement_list "}"
return_stmt      = "return" expr_opt ";"
expr_opt         = expr | ε

// Это ключевое место для LL-совместимости:
// Теперь, когда мы видим IDENTIFIER, мы знаем, что это либо присваивание, либо вызов функции.
// Мы используем следующий токен (lookahead) после IDENTIFIER, чтобы решить.
assignment_or_call_stmt = IDENTIFIER assignment_or_call_suffix ";"
assignment_or_call_suffix = "=" expr // Это assign_stmt
						  | "(" arguments_opt ")" ; // Это call_stmt

// Правила для выражений - преобразованы для устранения левой рекурсии
expr             = logical_or_terminal
logical_or_terminal = logical_and_terminal logical_or_tail
logical_or_tail  = "||" logical_and_terminal logical_or_tail | ε

logical_and_terminal = equality_terminal logical_and_tail
logical_and_tail = "&&" equality_terminal logical_and_tail | ε

equality_terminal = comparison_terminal equality_tail
equality_tail    = ("==" | "!=") comparison_terminal equality_tail | ε

comparison_terminal = term_terminal comparison_tail
comparison_tail  = ("<" | ">" | "<=" | ">=") term_terminal comparison_tail | ε

term_terminal    = factor_terminal term_tail
term_tail        = ("+" | "-") factor_terminal term_tail | ε

factor_terminal  = unary_terminal factor_tail
factor_tail      = ("*" | "/") unary_terminal factor_tail | ε

unary_terminal   = ("!" | "-") unary_terminal
				 | primary_terminal

primary_terminal = NUMBER
				 | IDENTIFIER
				 | "(" expr ")"
				 | call_primary ; // Call теперь является частью primary, чтобы избежать дублирования

call_primary     = IDENTIFIER "(" arguments_opt ")"
arguments_opt    = arguments | ε
arguments        = expr argument_list
argument_list    = { "," expr }

// Правила для токенов (регулярные выражения)
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
#define SYNTAX_ERROR				7

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
	NOT,
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

/**
* token struct
*/

typedef struct Token { // TODO: refactor Token struct
	TokenType token_type;

	union {
		int32_t int_value;
		float   float_value;
		char    char_value;
		char* token_value;
	} value;

	uint32_t  line_count;  // line
	uint32_t  in_line_pos; // col
} Token;

/**
* parse tree struct
*/

typedef struct {
	Token token;
	struct tree** leafs;
} parse_tree;

/**
* input stream struct
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

Token create_eof_token() {
	Token token;

	token.value.token_value = NULL;
	token.token_type = END_OF_FILE;

	return token;
}

Token create_not_terminal_token(TokenType type, uint32_t line, uint32_t col) {
	Token token;

	token.line_count = line;
	token.in_line_pos = col;

	switch (type) {
	case ROOT:
		token.token_type = ROOT;

		char* root = "root";
		token.value.token_value = amalloc(strlen(root) + 1);
		token.value.token_value[strlen(root)] = '\0';

		memcpy(token.value.token_value, root, strlen(root));
		break;
	case FUNC:
		token.token_type = FUNC;

		char* func = "func";
		token.value.token_value = amalloc(strlen(func) + 1);
		token.value.token_value[strlen(func)] = '\0';

		memcpy(token.value.token_value, func, strlen(func));
		break;
	case STATEMENT:
		token.token_type = STATEMENT;

		char* statement = "statement";
		token.value.token_value = amalloc(strlen(statement) + 1);
		token.value.token_value[strlen(statement)] = '\0';

		memcpy(token.value.token_value, statement, strlen(statement));
		break;
	case IF:
		token.token_type = IF;

		char* if_ = "if";
		token.value.token_value = amalloc(strlen(if_) + 1);
		token.value.token_value[strlen(if_)] = '\0';

		memcpy(token.value.token_value, if_, strlen(if_));
		break;
	case WHILE:
		token.token_type = WHILE;

		char* while_ = "while";
		token.value.token_value = amalloc(strlen(while_) + 1);
		token.value.token_value[strlen(while_)] = '\0';

		memcpy(token.value.token_value, while_, strlen(while_));
		break;
	}

	return token;
}

Token create_token(TokenType t, const char* v, uint32_t len, uint32_t line, uint32_t col) {
	Token token;

	token.token_type = t;
	token.line_count = line;
	token.in_line_pos = col;

	if (v == NULL) {
		token.value.token_value = NULL;
		return token;
	}

	token.value.token_value = amalloc(len + 1);
	token.value.token_value[len] = '\0';
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
	is->line = 1;
	is->col = 0;
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

	token = create_token(DIGIT, tmp_buff_for_digit, sbcount(tmp_buff_for_digit) - 1, is->col, is->line);
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
			token = create_token(reserved_keywords[i].type, tmp_buff_for_lexeme, sbcount(tmp_buff_for_lexeme) - 1, is->col, is->line);
			sbfree(tmp_buff_for_lexeme);
			return token;
		}
	}

	token = create_token(IDENTIFIER, tmp_buff_for_lexeme, sbcount(tmp_buff_for_lexeme) - 1, is->col, is->line);
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

Token peek_next_token(input_stream* is) {
	if (is->eof_reached) {
		return create_eof_token();
	}

	uint64_t file_pos = ftell(is->file); // saving position in file before peeking forward
	uint64_t line = is->line;
	uint64_t col = is->col;
	char current_char = is->current_char;
	bool eof_reached = is->eof_reached;

	Token next_token = get_next_token(is);

	fseek(is->file, file_pos, SEEK_SET); // restore file position after peeking next token
	is->line = line;
	is->col = col;
	is->current_char = current_char;
	is->eof_reached = eof_reached;

	return next_token;
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

	if (t->token.value.token_value != NULL) {
		switch(t->token.token_type) {
			case DIGIT:
				printf("TOKEN[ type: DIGIT\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case ADD:
				printf("TOKEN[ type: ADD\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case SUBTRACT:
				printf("TOKEN[ type: SUBTRACT\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case MULTIPLY:
				printf("TOKEN[ type: MULTIPLY\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case DIVIDE:
				printf("TOKEN[ type: DIVIDE\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case IDENTIFIER:
				printf("TOKEN[ type: IDENTIFIER\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case LEFT_BRACKET:
				printf("TOKEN[ type: LEFT_BRACKET\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case RIGHT_BRACKET:
				printf("TOKEN[ type: RIGHT_BRACKET\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case LEFT_BRACE:
				printf("TOKEN[ type: LEFT_BRACE\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case RIGHT_BRACE:
				printf("TOKEN[ type: RIGHT_BRACE\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case EQUALS:
				printf("TOKEN[ type: EQUALS\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case EQUAL_EQUAL:
				printf("TOKEN[ type: EQUAL_EQUAL\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case NOT:
				printf("TOKEN[ type: NOT\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case NOT_EQUAL:
				printf("TOKEN[ type: NOT_EQUAL\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case LOGICAL_NOT:
				printf("TOKEN[ type: LOGICAL_NOT\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case LOGICAL_AND:
				printf("TOKEN[ type: LOGICAL_AND\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case LOGICAL_OR:
				printf("TOKEN[ type: LOGICAL_OR\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case LESS:
				printf("TOKEN[ type: LESS\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case LESS_EQUAL:
				printf("TOKEN[ type: LESS_EQUAL\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case GREATER:
				printf("TOKEN[ type: GREATER\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case GREATER_EQUAL:
				printf("TOKEN[ type: GREATER_EQUAL\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case COLON:
				printf("TOKEN[ type: COLON\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case SEMICOLON:
				printf("TOKEN[ type: SEMICOLON\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case COMMA:
				printf("TOKEN[ type: COMMA\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case CONST:
				printf("TOKEN[ type: CONST\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case VAR:
				printf("TOKEN[ type: VAR\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case INT:
				printf("TOKEN[ type: INT\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case CHAR:
				printf("TOKEN[ type: CHAR\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case BOOl:
				printf("TOKEN[ type: BOOl\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case FLOAT:
				printf("TOKEN[ type: FLOAT\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case IF:
				printf("TOKEN[ type: IF\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case ELSE:
				printf("TOKEN[ type: ELSE\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case WHILE:
				printf("TOKEN[ type: WHILE\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case RETURN:
				printf("TOKEN[ type: RETURN\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case FUNC:
				printf("TOKEN[ type: FUNC\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case ROOT:
				printf("TOKEN[ type: ROOT\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case STATEMENT:
				printf("TOKEN[ type: STATEMENT\t lexeme: %s ] \n", t->token.value.token_value);
				break;
			case END_OF_FILE:
				printf("TOKEN [\n\t type: END_OF_FILE\n\t ] \n");
				break;
			default:
				printf("unknown token\n");
				break;
		};
	}

	for (int i = 0; i < sbcount(t->leafs); i++) {
		tree_traverse(t->leafs[i], depth + 1);
	}
}

/*
* Implementation of terminals
*/

Token current_parsed_token; // global token for parsing functions
bool has_errors = false;

void advance_token(input_stream* is) {
	current_parsed_token = get_next_token(is);
}

void report_error(input_stream* is, const char* msg, Token token) {
    fprintf(stderr, "Error at %u:%u: %s (Found: '%s')\n",
            token.line_count, token.in_line_pos, msg,
            token.value.token_value ? token.value.token_value : "EOF/NULL"); // Обработка NULL для EOF
    has_errors = true; // Устанавливаем флаг ошибки
}

void sync_to(input_stream* is, TokenType* sync_points, size_t count) {
    while (current_parsed_token.token_type != END_OF_FILE) {
        bool found_sync_point = false;
        for (size_t i = 0; i < count; i++) {
            if (current_parsed_token.token_type == sync_points[i]) {
                found_sync_point = true;
                break;
            }
        }
        if (found_sync_point) {
            break; // Найден синхронизирующий токен, выходим
        }
        advance_token(is); // Пропускаем текущий токен
    }
    // Если токен был найден, он останется текущим.
    // Если END_OF_FILE, цикл завершится.
}

bool is_valid_expression_start(TokenType token_type) {
    switch (token_type) {
        // Допустимые начала выражений:
        case DIGIT:        // Число (например, 42)
        case IDENTIFIER:   // Идентификатор (например, x)
        case LEFT_BRACKET: // Открывающая скобка '('
        case SUBTRACT:     // Унарный минус (-x)
        case NOT:          // Логическое НЕ (!x)
        case LOGICAL_NOT:  // Альтернативное НЕ (если есть)
            return true;
        default:
            return false;
    }
}

// --- Forward Declarations for Parser Functions ---

bool parse_function(input_stream* is, parse_tree* root);
void parse_parameters_opt(input_stream* is, parse_tree* parent);
void parse_statement(input_stream* is, parse_tree* parent);
void parse_statement_list(input_stream* is, parse_tree* root);
void parse_expression(input_stream* is, parse_tree* root);
void parse_assignment_or_call_stmt(input_stream* is, parse_tree* root);
void parse_assignment_or_call_suffix(input_stream* is, parse_tree* root);
void parse_else_stmt_opt(input_stream* is, parse_tree* root);
void parse_logical_or_terminal(input_stream* is, parse_tree* root);
void parse_logical_or_tail(input_stream* is, parse_tree* root);
void parse_logical_and_terminal(input_stream* is, parse_tree* root);
void parse_logical_and_tail(input_stream* is, parse_tree* root);
void parse_equality_tail(input_stream* is, parse_tree* root);
void parse_equality_terminal(input_stream* is, parse_tree* root);
void parse_comparison_tail(input_stream* is, parse_tree* root);
void parse_comparison_terminal(input_stream* is, parse_tree* root);
void parse_term_tail(input_stream* is, parse_tree* root);
void parse_term_terminal(input_stream* is, parse_tree* root);
void parse_factor_tail(input_stream* is, parse_tree* root);
void parse_factor_terminal(input_stream* is, parse_tree* root);
void parse_unary_terminal(input_stream* is, parse_tree* root);
void parse_primary_terminal(input_stream* is, parse_tree* root);
void parse_call_primary(input_stream* is, parse_tree* root);
void parse_arguments_opt(input_stream* is, parse_tree* root);
void parse_argument_list(input_stream* is, parse_tree* root);

// --- Parser Implementations ---

parse_tree* parse_program(input_stream* is) {
	parse_tree* root = tree_create_leaf(create_not_terminal_token(ROOT, is->line, is->col));
	advance_token(is);

	while (current_parsed_token.token_type != END_OF_FILE) {
		if (current_parsed_token.token_type == FUNC) {
			parse_function(is, root);
		}
		else if (current_parsed_token.token_type == IF ||
			current_parsed_token.token_type == WHILE ||
			current_parsed_token.token_type == RETURN ||
			current_parsed_token.token_type == IDENTIFIER) {
			parse_statement(is, root);
		}
		else {
			// Если токен не является ни началом функции, ни началом оператора,
            // тогда это синтаксическая ошибка.
            report_error(is, "Expected 'func' or a statement", current_parsed_token);
            // Точки синхронизации для верхнего уровня: func, if, while, return, IDENTIFIER, END_OF_FILE
            TokenType sync_points[] = {FUNC, IF, WHILE, RETURN, IDENTIFIER, END_OF_FILE};
            sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
            if (current_parsed_token.token_type == END_OF_FILE) {
                break; // Выход, если достигнут конец файла во время синхронизации
            }
		}
	}
	return root;
}

bool parse_function(input_stream* is, parse_tree* root) {
    parse_tree* func_node = tree_create_leaf(current_parsed_token); // Создаем узел func раньше, чтобы добавить к нему ошибки

    if (current_parsed_token.token_type != FUNC) {
        report_error(is, "Expected 'func'", current_parsed_token);
        TokenType sync_points[] = {SEMICOLON, RIGHT_BRACE, FUNC, IF, WHILE, RETURN, IDENTIFIER, END_OF_FILE};
        sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
        return false;
    }

    tree_add(root, func_node);
    advance_token(is); // Consume "func"

    if (current_parsed_token.token_type != IDENTIFIER) {
        report_error(is, "Expected IDENTIFIER after 'func'", current_parsed_token);
        TokenType sync_points[] = {LEFT_BRACKET, SEMICOLON, RIGHT_BRACE, FUNC, IF, WHILE, RETURN, IDENTIFIER, END_OF_FILE};
        sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
        return false;
    }
    parse_tree* identifier_node = tree_create_leaf(current_parsed_token);
    tree_add(func_node, identifier_node);
    advance_token(is); // Consume IDENTIFIER

    if (current_parsed_token.token_type != LEFT_BRACKET) {
        report_error(is, "Expected '(' after function identifier", current_parsed_token);
        TokenType sync_points[] = {RIGHT_BRACKET, LEFT_BRACE, SEMICOLON, RIGHT_BRACE, FUNC, IF, WHILE, RETURN, IDENTIFIER, END_OF_FILE};
        sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
        return false;
    }
    parse_tree* left_bracket_node = tree_create_leaf(current_parsed_token);
    tree_add(func_node, left_bracket_node);
    advance_token(is); // Consume "("

    parse_parameters_opt(is, func_node);

    if (current_parsed_token.token_type != RIGHT_BRACKET) {
        report_error(is, "Expected ')' after parameters", current_parsed_token);
        TokenType sync_points[] = {LEFT_BRACE, SEMICOLON, RIGHT_BRACE, FUNC, IF, WHILE, RETURN, IDENTIFIER, END_OF_FILE};
        sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
        return false;
    }
    parse_tree* right_bracket_node = tree_create_leaf(current_parsed_token);
    tree_add(func_node, right_bracket_node);
    advance_token(is); // Consume ")"

    if (current_parsed_token.token_type != LEFT_BRACE) {
        report_error(is, "Expected '{' after function parameters", current_parsed_token);
        TokenType sync_points[] = {RIGHT_BRACE, FUNC, IF, WHILE, RETURN, IDENTIFIER, END_OF_FILE};
        sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
        return false;
    }
    parse_tree* left_brace_node = tree_create_leaf(current_parsed_token);
    tree_add(func_node, left_brace_node);
    advance_token(is);

    parse_tree* statement_list_node = tree_create_leaf(create_not_terminal_token(STATEMENT, is->line, is->col));
    tree_add(func_node, statement_list_node);
    parse_statement_list(is, statement_list_node); // statement_list_node

    if (current_parsed_token.token_type != RIGHT_BRACE) {
        report_error(is, "Expected '}' after function body", current_parsed_token);
        TokenType sync_points[] = {FUNC, IF, WHILE, RETURN, IDENTIFIER, END_OF_FILE}; // Синхронизация до следующего верхнеуровневого элемента
        sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
        return false;
    }
    parse_tree* right_brace_node = tree_create_leaf(current_parsed_token);
    tree_add(func_node, right_brace_node);
    advance_token(is);
    return true;
}

void parse_parameters_opt(input_stream* is, parse_tree* parent) {
	if (current_parsed_token.token_type == RIGHT_BRACKET) {
		return; // Пустой список параметров, скобка будет потреблена в parse_function
	}
	else if (current_parsed_token.token_type == IDENTIFIER) {
		parse_tree* parameter_node = tree_create_leaf(current_parsed_token);
		tree_add(parent, parameter_node);
		advance_token(is); // Потребляем IDENTIFIER

		while (current_parsed_token.token_type == COMMA) {
			parse_tree* comma_node = tree_create_leaf(current_parsed_token);
			tree_add(parent, comma_node);
			advance_token(is); // Потребляем COMMA

			if (current_parsed_token.token_type == IDENTIFIER) {
				parse_tree* next_param = tree_create_leaf(current_parsed_token);
				tree_add(parent, next_param);
				advance_token(is); // Потребляем следующий IDENTIFIER
			}
			else {
				fprintf(stderr, "Error: Expected IDENTIFIER after comma\n");
				exit(SYNTAX_ERROR);
			}
		}
	}
}

void parse_statement(input_stream* is, parse_tree* root) {
    // Создаем временный узел для оператора, если он распознан успешно
    parse_tree* stmt_node = NULL;

    if (current_parsed_token.token_type == IF) {
        stmt_node = tree_create_leaf(current_parsed_token);
        tree_add(root, stmt_node);
        advance_token(is);

        if (current_parsed_token.token_type != LEFT_BRACKET) {
            report_error(is, "Expected '(' after 'if'", current_parsed_token);
            TokenType sync_points[] = {LEFT_BRACE, SEMICOLON, RIGHT_BRACE, FUNC, IF, WHILE, RETURN, IDENTIFIER, END_OF_FILE};
            sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
            return; // Не можем продолжать парсинг if
        }
        parse_tree* left_bracket = tree_create_leaf(current_parsed_token);
        tree_add(stmt_node, left_bracket);
        advance_token(is);

        parse_expression(is, stmt_node);

        if (current_parsed_token.token_type != RIGHT_BRACKET) {
            report_error(is, "Expected ')' after if condition", current_parsed_token);
            TokenType sync_points[] = {LEFT_BRACE, SEMICOLON, RIGHT_BRACE, FUNC, IF, WHILE, RETURN, IDENTIFIER, END_OF_FILE};
            sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
            return;
        }
        parse_tree* right_bracket = tree_create_leaf(current_parsed_token);
        tree_add(stmt_node, right_bracket);
        advance_token(is);

        if (current_parsed_token.token_type != LEFT_BRACE) {
            report_error(is, "Expected '{' after if condition", current_parsed_token);
            TokenType sync_points[] = {RIGHT_BRACE, ELSE, SEMICOLON, FUNC, IF, WHILE, RETURN, IDENTIFIER, END_OF_FILE};
            sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
            return;
        }
        parse_tree* left_brace = tree_create_leaf(current_parsed_token);
        tree_add(stmt_node, left_brace);
        advance_token(is);

        parse_tree* statement_list_node = tree_create_leaf(create_not_terminal_token(STATEMENT, is->line, is->col));
        tree_add(stmt_node, statement_list_node);
        parse_statement_list(is, statement_list_node);

        if (current_parsed_token.token_type != RIGHT_BRACE) {
            report_error(is, "Expected '}' after if block", current_parsed_token);
            TokenType sync_points[] = {ELSE, SEMICOLON, FUNC, IF, WHILE, RETURN, IDENTIFIER, END_OF_FILE};
            sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
            return;
        }
        parse_tree* right_brace = tree_create_leaf(current_parsed_token);
        tree_add(stmt_node, right_brace);
        advance_token(is);

        parse_else_stmt_opt(is, stmt_node); // parse_else_stmt_opt добавит свои узлы к stmt_node

    } // ... (аналогично для WHILE, RETURN, ASSIGNMENT_OR_CALL_STMT)
    else if (current_parsed_token.token_type == WHILE) {
        stmt_node = tree_create_leaf(current_parsed_token);
        tree_add(root, stmt_node);
        advance_token(is);

        if (current_parsed_token.token_type != LEFT_BRACKET) {
            report_error(is, "Expected '(' after 'while'", current_parsed_token);
            TokenType sync_points[] = {LEFT_BRACE, SEMICOLON, RIGHT_BRACE, FUNC, IF, WHILE, RETURN, IDENTIFIER, END_OF_FILE};
            sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
            return;
        }
        parse_tree* left_bracket = tree_create_leaf(current_parsed_token);
        tree_add(stmt_node, left_bracket);
        advance_token(is);

        parse_expression(is, stmt_node);

        if (current_parsed_token.token_type != RIGHT_BRACKET) {
            report_error(is, "Expected ')' after while condition", current_parsed_token);
            TokenType sync_points[] = {LEFT_BRACE, SEMICOLON, RIGHT_BRACE, FUNC, IF, WHILE, RETURN, IDENTIFIER, END_OF_FILE};
            sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
            return;
        }
        parse_tree* right_bracket = tree_create_leaf(current_parsed_token);
        tree_add(stmt_node, right_bracket);
        advance_token(is);

        if (current_parsed_token.token_type != LEFT_BRACE) {
            report_error(is, "Expected '{' after while condition", current_parsed_token);
            TokenType sync_points[] = {RIGHT_BRACE, SEMICOLON, FUNC, IF, WHILE, RETURN, IDENTIFIER, END_OF_FILE};
            sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
            return;
        }
        parse_tree* left_brace = tree_create_leaf(current_parsed_token);
        tree_add(stmt_node, left_brace);
        advance_token(is);

        parse_tree* statement_list_node = tree_create_leaf(create_not_terminal_token(STATEMENT, is->line, is->col));
        tree_add(stmt_node, statement_list_node);
        parse_statement_list(is, statement_list_node);

        if (current_parsed_token.token_type != RIGHT_BRACE) {
            report_error(is, "Expected '}' after while block", current_parsed_token);
            TokenType sync_points[] = {SEMICOLON, FUNC, IF, WHILE, RETURN, IDENTIFIER, END_OF_FILE};
            sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
            return;
        }
        parse_tree* right_brace = tree_create_leaf(current_parsed_token);
        tree_add(stmt_node, right_brace);
        advance_token(is);

    }
    else if (current_parsed_token.token_type == RETURN) {
        stmt_node = tree_create_leaf(current_parsed_token);
        tree_add(root, stmt_node);
        advance_token(is);

        if (current_parsed_token.token_type == SEMICOLON) {
            parse_tree* semicolon = tree_create_leaf(current_parsed_token);
            tree_add(stmt_node, semicolon);
            advance_token(is);
        } else {
            parse_expression(is, stmt_node);
            if (current_parsed_token.token_type != SEMICOLON) {
                report_error(is, "Expected ';' after return statement", current_parsed_token);
                TokenType sync_points[] = {FUNC, IF, WHILE, RETURN, IDENTIFIER, END_OF_FILE};
                sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
                return;
            }
            parse_tree* semicolon = tree_create_leaf(current_parsed_token);
            tree_add(stmt_node, semicolon);
            advance_token(is);
        }
    }
    else if (current_parsed_token.token_type == IDENTIFIER) {
        parse_assignment_or_call_stmt(is, root); // Эта функция сама обрабатывает ошибки
    }
    else {
        report_error(is, "Unexpected token for statement", current_parsed_token);
        TokenType sync_points[] = {SEMICOLON, RIGHT_BRACE, FUNC, IF, WHILE, RETURN, IDENTIFIER, END_OF_FILE};
        sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
        return;
    }
}

void parse_statement_list(input_stream* is, parse_tree* root) {
	while (current_parsed_token.token_type != RIGHT_BRACE &&
		current_parsed_token.token_type != END_OF_FILE) {
		parse_statement(is, root);
	}
}

void parse_assignment_or_call_stmt(input_stream* is, parse_tree* root) {
    if (current_parsed_token.token_type != IDENTIFIER) {
        report_error(is, "Expected IDENTIFIER for assignment or call", current_parsed_token);
        TokenType sync_points[] = {SEMICOLON, RIGHT_BRACE, FUNC, IF, WHILE, RETURN, IDENTIFIER, END_OF_FILE};
        sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
        return;
    }
    
    parse_tree* identifier = tree_create_leaf(current_parsed_token);
    tree_add(root, identifier);
    advance_token(is);

    parse_assignment_or_call_suffix(is, root);

    if (current_parsed_token.token_type != SEMICOLON) {
        report_error(is, "Expected ';' after assignment or call", current_parsed_token);
        TokenType sync_points[] = {RIGHT_BRACE, FUNC, IF, WHILE, RETURN, IDENTIFIER, END_OF_FILE};
        sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
        return;
    }
    
    parse_tree* semicolon = tree_create_leaf(current_parsed_token);
    tree_add(root, semicolon);
    advance_token(is);
}

void parse_assignment_or_call_suffix(input_stream* is, parse_tree* root) {
	if (current_parsed_token.token_type == EQUALS) {
		parse_tree* equals = tree_create_leaf(current_parsed_token);
		tree_add(root, equals);
		advance_token(is);

		parse_expression(is, root);
	}
	else if (current_parsed_token.token_type == LEFT_BRACKET) {
		parse_tree* left_bracket = tree_create_leaf(current_parsed_token);
		tree_add(root, left_bracket);
		advance_token(is);

		parse_arguments_opt(is, root);

		if (current_parsed_token.token_type == RIGHT_BRACKET) {
			parse_tree* right_bracket = tree_create_leaf(current_parsed_token);
			tree_add(root, right_bracket);
			advance_token(is);
		}
		else {
			fprintf(stderr, "Error %u:%u: Expected ')' after function arguments.\n", current_parsed_token.line_count, current_parsed_token.in_line_pos);
			exit(SYNTAX_ERROR);
		}
	}
	else {
		fprintf(stderr, "Error %u:%u: Expected '=' or '(' for assignment or call suffix.\n", current_parsed_token.line_count, current_parsed_token.in_line_pos);
		exit(SYNTAX_ERROR);
	}
}

void parse_else_stmt_opt(input_stream* is, parse_tree* root) {
	if (current_parsed_token.token_type == ELSE) { // ИСПРАВЛЕНО: Проверяем current_parsed_token напрямую
		parse_tree* else_ = tree_create_leaf(current_parsed_token);
		tree_add(root, else_); // Добавляем 'else' к if_node
		advance_token(is); // Потребляем "else"

		if (current_parsed_token.token_type == LEFT_BRACE) {
			parse_tree* left_brace = tree_create_leaf(current_parsed_token);
			tree_add(else_, left_brace); // Добавляем '{' к else_node
			advance_token(is); // ИСПРАВЛЕНО: Потребляем "{"

			parse_tree* statement_list = tree_create_leaf(create_not_terminal_token(STATEMENT, is->line, is->col));
			tree_add(else_, statement_list); // Добавляем statement_list к else_node
			parse_statement_list(is, statement_list); // parse_statement_list оставляет current_parsed_token на '}'

			if (current_parsed_token.token_type == RIGHT_BRACE) {
				parse_tree* right_brace = tree_create_leaf(current_parsed_token);
				tree_add(else_, right_brace); // Добавляем '}' к else_node
				advance_token(is); // Потребляем "}"
			}
			else {
				fprintf(stderr, "Error %u:%u: Expected '}' after else block.\n", current_parsed_token.line_count, current_parsed_token.in_line_pos);
				exit(SYNTAX_ERROR);
			}
		}
		else {
			fprintf(stderr, "Error %u:%u: Expected '{' after 'else'.\n", current_parsed_token.line_count, current_parsed_token.in_line_pos);
			exit(SYNTAX_ERROR);
		}
	}
	// Если не ELSE, это эпсилон-случай, просто возвращаемся.
}

void parse_expression(input_stream* is, parse_tree* root) {
	if (!is_valid_expression_start(current_parsed_token.token_type)) {
        report_error(is, "Invalid expression start", current_parsed_token);
        // Пытаемся синхронизироваться до конца выражения или начала следующего оператора
        TokenType sync_points[] = {RIGHT_BRACKET, SEMICOLON, COMMA, LOGICAL_OR, LOGICAL_AND, 
								   EQUAL_EQUAL, NOT_EQUAL, LESS, GREATER, LESS_EQUAL, GREATER_EQUAL, 
								   ADD, SUBTRACT, MULTIPLY, DIVIDE, END_OF_FILE};
        sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
        return;
    }
    parse_logical_or_terminal(is, root);
}

void parse_logical_or_terminal(input_stream* is, parse_tree* root) {
	parse_logical_and_terminal(is, root);
	parse_logical_or_tail(is, root);
}

void parse_logical_or_tail(input_stream* is, parse_tree* root) {
	if (current_parsed_token.token_type == LOGICAL_OR) {
		parse_tree* logical_or = tree_create_leaf(current_parsed_token);
		tree_add(root, logical_or);
		advance_token(is); // Потребляем "||"

		parse_logical_and_terminal(is, root);
		parse_logical_or_tail(is, root); // Рекурсивный вызов
	}
	// Если не LOGICAL_OR, это эпсилон-случай, просто возвращаемся.
}

void parse_logical_and_terminal(input_stream* is, parse_tree* root) {
	parse_equality_terminal(is, root);
	parse_logical_and_tail(is, root);
}

void parse_logical_and_tail(input_stream* is, parse_tree* root) {
	if (current_parsed_token.token_type == LOGICAL_AND) {
		parse_tree* logical_and = tree_create_leaf(current_parsed_token);
		tree_add(root, logical_and);
		advance_token(is);

		parse_equality_terminal(is, root);
		parse_logical_and_tail(is, root);
	}
}

void parse_equality_terminal(input_stream* is, parse_tree* root) {
	parse_comparison_terminal(is, root);
	parse_equality_tail(is, root);
}

void parse_equality_tail(input_stream* is, parse_tree* root) {
	if(current_parsed_token.token_type == EQUAL_EQUAL) {
		parse_tree* equal_equal = tree_create_leaf(current_parsed_token);
		tree_add(root, equal_equal);
		advance_token(is);

		parse_comparison_terminal(is, root);
		parse_equality_tail(is, root);
	} else if (current_parsed_token.token_type == NOT_EQUAL) {
		parse_tree* not_equal = tree_create_leaf(current_parsed_token);
		tree_add(root, not_equal);
		advance_token(is);

		parse_comparison_terminal(is, root);
		parse_equality_tail(is, root);
	}
	return;
}

void parse_comparison_terminal(input_stream* is, parse_tree* root) {
	parse_term_terminal(is, root);
	parse_comparison_tail(is, root);
}

void parse_comparison_tail(input_stream* is, parse_tree* root) {
	if (current_parsed_token.token_type == LESS) {
		parse_tree* less = tree_create_leaf(current_parsed_token);
		tree_add(root, less);
		advance_token(is);

		parse_term_terminal(is, root);
		parse_comparison_tail(is, root);

		return;
	}
	else if (current_parsed_token.token_type == GREATER) {
		parse_tree* greater = tree_create_leaf(current_parsed_token);
		tree_add(root, greater);
		advance_token(is);

		parse_term_terminal(is, root);
		parse_comparison_tail(is, root);

		return;
	}
	else if (current_parsed_token.token_type == LESS_EQUAL) {
		parse_tree* less_equal = tree_create_leaf(current_parsed_token);
		tree_add(root, less_equal);
		advance_token(is);

		parse_term_terminal(is, root);
		parse_comparison_tail(is, root);

		return;
	}
	else if (current_parsed_token.token_type == GREATER_EQUAL) {
		parse_tree* greater_equal = tree_create_leaf(current_parsed_token);
		tree_add(root, greater_equal);
		advance_token(is);

		parse_term_terminal(is, root);
		parse_comparison_tail(is, root);

		return;
	}
	return;
}

void parse_term_terminal(input_stream* is, parse_tree* root) {
	parse_factor_terminal(is, root);
	parse_term_tail(is, root);
}

void parse_term_tail(input_stream* is, parse_tree* root) {
	if (current_parsed_token.token_type == ADD) {
		parse_tree* add = tree_create_leaf(current_parsed_token);
		tree_add(root, add);
		advance_token(is);

		parse_factor_terminal(is, root);
		parse_term_tail(is, root);

		return;
	} else if (current_parsed_token.token_type == SUBTRACT) {
		parse_tree* subtract = tree_create_leaf(current_parsed_token);
		tree_add(root, subtract);
		advance_token(is);

		parse_factor_terminal(is, root);
		parse_term_tail(is, root);

		return;
	}
	return;
}

void parse_factor_terminal(input_stream* is, parse_tree* root) {
	parse_unary_terminal(is, root);
	parse_factor_tail(is, root);
}

void parse_factor_tail(input_stream* is, parse_tree* root) {
	// TODO: таже самая ошибка, при продвинутом current_parsed_token,
	// пикается дальше следующий токен
	
	if (current_parsed_token.token_type == MULTIPLY) {
		parse_tree* multiply = tree_create_leaf(current_parsed_token);
		tree_add(root, multiply);
		advance_token(is);

		parse_unary_terminal(is, root);
		parse_factor_tail(is, root);

		return;
	}
	else if (current_parsed_token.token_type == DIVIDE) {
		parse_tree* divide = tree_create_leaf(current_parsed_token);
		tree_add(root, divide);
		advance_token(is);

		parse_unary_terminal(is, root);
		parse_factor_tail(is, root);

		return;
	}
	return;
}

void parse_unary_terminal(input_stream* is, parse_tree* root) {
	// TODO: ошибка синтаксического анализа
	// до вызова parse_unary_terminal() токен уже был продвинут,
	// а тут еще пикается следующий, исправлено на проверку сразу current_parse_token

	if (current_parsed_token.token_type == NOT) {
		parse_tree* not = tree_create_leaf(current_parsed_token);
		tree_add(root, not);
		advance_token(is);

		parse_unary_terminal(is, root);
	} else if (current_parsed_token.token_type == SUBTRACT) {
		parse_tree * subtract = tree_create_leaf(current_parsed_token);
		tree_add(root, subtract);
		advance_token(is);

		parse_unary_terminal(is, root);
	} else {
		parse_primary_terminal(is, root);
	}
}

void parse_primary_terminal(input_stream* is, parse_tree* root) {
    if (current_parsed_token.token_type == DIGIT) {
        parse_tree* number = tree_create_leaf(current_parsed_token);
        tree_add(root, number);
        advance_token(is);
    }
    else if (current_parsed_token.token_type == IDENTIFIER) {
        Token next_token = peek_next_token(is);
        if (next_token.token_type == LEFT_BRACKET) {
            parse_call_primary(is, root);
        }
        else {
            parse_tree* identifier = tree_create_leaf(current_parsed_token);
            tree_add(root, identifier);
            advance_token(is);
        }
    }
    else if (current_parsed_token.token_type == LEFT_BRACKET) {
        parse_tree* left_bracket = tree_create_leaf(current_parsed_token);
        tree_add(root, left_bracket);
        advance_token(is);

        parse_expression(is, root);

        if (current_parsed_token.token_type != RIGHT_BRACKET) {
            report_error(is, "Expected ')' after expression", current_parsed_token);
            // После ошибки в выражении внутри скобок, синхронизируемся до ближайшего ')' или ';'
            TokenType sync_points[] = {RIGHT_BRACKET, SEMICOLON, END_OF_FILE};
            sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
            // Если мы нашли ')' после синхронизации, потребляем его.
            if (current_parsed_token.token_type == RIGHT_BRACKET) {
                parse_tree* right_bracket = tree_create_leaf(current_parsed_token);
                tree_add(root, right_bracket);
                advance_token(is);
            }
            return; // Не можем продолжать парсить primary_terminal после ошибки
        }
        parse_tree* right_bracket = tree_create_leaf(current_parsed_token);
        tree_add(root, right_bracket);
        advance_token(is);
    }
    else {
        report_error(is, "Expected NUMBER, IDENTIFIER or '('", current_parsed_token);
        // Синхронизация до токена, который может завершать текущий контекст выражения
        TokenType sync_points[] = {RIGHT_BRACKET, SEMICOLON, COMMA, LOGICAL_OR, LOGICAL_AND, 
								   EQUAL_EQUAL, NOT_EQUAL, LESS, GREATER, LESS_EQUAL, GREATER_EQUAL, 
								   ADD, SUBTRACT, MULTIPLY, DIVIDE, END_OF_FILE};
        sync_to(is, sync_points, sizeof(sync_points) / sizeof(sync_points[0]));
    }
}

void parse_call_primary(input_stream* is, parse_tree* root) {
	parse_tree* identifier = tree_create_leaf(current_parsed_token);
	tree_add(root, identifier);
	advance_token(is);

	if (current_parsed_token.token_type == LEFT_BRACKET) {
		parse_tree* left_bracket = tree_create_leaf(current_parsed_token);
		tree_add(root, left_bracket);
		advance_token(is);

		parse_arguments_opt(is, root);

		if (current_parsed_token.token_type == RIGHT_BRACKET) {
			parse_tree* right_bracket = tree_create_leaf(current_parsed_token);
			tree_add(root, right_bracket);
			advance_token(is);
		}
		else {
			fprintf(stderr, "Error: Expected ')' after arguments\n");
			exit(SYNTAX_ERROR);
		}
	}
	else {
		fprintf(stderr, "Error: Expected '(' after identifier\n");
		exit(SYNTAX_ERROR);
	}
}

void parse_arguments_opt(input_stream* is, parse_tree* root) {
	if (current_parsed_token.token_type == RIGHT_BRACKET) {
		return; // Нет аргументов
	}

	parse_expression(is, root);

	while (current_parsed_token.token_type == COMMA) {
		parse_tree* comma = tree_create_leaf(current_parsed_token);
		tree_add(root, comma);
		advance_token(is);

		parse_expression(is, root);
	}
}

void parse_argument_list(input_stream* is, parse_tree* root) {
	// current_parsed_token уже должен быть токеном после первого expr в parse_arguments_opt,
	// то есть ',' или ')'
	while (current_parsed_token.token_type == COMMA) {
		parse_tree* comma = tree_create_leaf(current_parsed_token);
		tree_add(root, comma);
		advance_token(is); // Consume COMMA

		parse_expression(is, root); // Parse next argument
		// After parse_expression, current_parsed_token is the first token *after* the expression.
	}
	// Loop ends when current_parsed_token is not COMMA (it should be RIGHT_BRACKET)
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
			printf("Compilation of file: %s\n", argv[1]);

			input_stream* is = input_stream_init(argv[1]);
			if (!is->file) { // Проверяем, открылся ли файл
				fprintf(stderr, "Error: Could not open file '%s'\n", argv[1]);
				free(flag);
				return FILE_OPEN_ERROR;
			}
	
			parse_tree* program = parse_program(is);
			if(has_errors)
				fprintf(stderr, "\nParsing finished with errors.\n");
			else {
				printf("\nParsing successful!\n");
				tree_traverse(program, 0);
			}
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

	parse_tree* program = parse_program(is);
	tree_traverse(program, 0);
#endif
	return 0;
}
