#include <pthread.h>
#include "list.h"

typedef enum {
	// special, fake, tokens
	TOKEN_NONE = 0,
	TOKEN_STOP,
	TOKEN_ERROR,
	TOKEN_EOF,

	// whiltespace, meaningless
	TOKEN_TAB,
	TOKEN_SPACE,
	TOKEN_NEWLINE,
	TOKEN_CARRAGE_RETURN,

	// comments
	TOKEN_LINE_COMMENT,
	TOKEN_MULTI_LINE_COMMENT,

	// puncts
	TOKEN_OPEN_PAREN,
	TOKEN_CLOSE_PAREN,
	TOKEN_OPEN_CURLY,
	TOKEN_CLOSE_CURLY,
	TOKEN_OPEN_BRACE,
	TOKEN_CLOSE_BRACE,
	TOKEN_COMMA,
	TOKEN_DOT,
	TOKEN_COLON,
	TOKEN_SEMICOLON,

	// trinary
	TOKEN_QUESTIONMARK,

	// binary operators, (some could be unary too (ie -1))
	TOKEN_ADD,
	TOKEN_SUBTRACT,
	TOKEN_MULTIPLY,
	TOKEN_DIVIDE,
	TOKEN_EXPONENT,
	TOKEN_LOGICAL_OR,
	TOKEN_LOGICAL_AND,
	TOKEN_NULL_COALESCING,
	TOKEN_BITWISE_AND,
	TOKEN_BITWISE_XOR,
	TOKEN_BITWISE_OR,
	TOKEN_BITSHIFT_LEFT,
	TOKEN_ZERO_FILL_RIGHT_SHIFT,
	TOKEN_SIGNED_BITSHIFT_RIGHT,
	TOKEN_MOD,


	// assignments
	TOKEN_BITWISE_XOR_ASSIGN,
	TOKEN_MULTIPLY_ASSIGN,
	TOKEN_DIVIDE_ASSIGN,
	TOKEN_BITWISE_OR_ASSIGN,
	TOKEN_BITWISE_AND_ASSIGN,
	TOKEN_BITSHIFT_LEFT_ASSIGN,
	TOKEN_BITSHIFT_RIGHT_ASSIGN,
	TOKEN_MINUS_ASSIGN,
	TOKEN_MOD_ASSIGN,
	TOKEN_ASSIGN,
	TOKEN_ARROW_FUNC,

	// comparisons
	TOKEN_EQUAL_EQUAL,
	TOKEN_NOT_EQUAL,
	TOKEN_PLUS_EQUAL,
	TOKEN_DECREMENT,
	TOKEN_LESSTHAN_OR_EQUAL,
	TOKEN_LESSTHAN,
	TOKEN_GREATER_THAN,
	TOKEN_GREATERTHAN_OR_EQUAL,
	TOKEN_EQUAL_EQUAL_EQUAL,
	TOKEN_NOT_EQUAL_EQUAL,

	// unary operators
	TOKEN_NOT,
	TOKEN_BITWISE_NOT,
	TOKEN_INCREMENT,


	// strings
	TOKEN_DOUBLE_QUOTE_STRING,
	TOKEN_SINGLE_QUOTE_STRING,
	TOKEN_TILDA_STRING,

	TOKEN_NUMERIC,

	// identifiers
	TOKEN_REGEX,
	TOKEN_VAR,
	TOKEN_FOR,
	TOKEN_LET,
	TOKEN_FUNCTION,
	TOKEN_RETURN,
	TOKEN_CATCH,
	TOKEN_IF,
	TOKEN_ELSE,
	TOKEN_DO,
	TOKEN_WHILE,
	TOKEN_THROW,
	TOKEN_CONST,
	TOKEN_TYPEOF,
	TOKEN_VARIABLE,

} tokentype;

typedef struct token_data Token;
struct  token_data {
	const char *value;
	size_t length;
	size_t charnum;
	tokentype type;
	unsigned int flags;
	bool fake; // for space tokens created during beautification, fake tokens lack origin locations and are not allocated.
	bool isalloc, ishead;
};

/*
 * kick off the token producer, tokens will be added to the returned locked
 * list
*/
List * tokenizer_start_thread(int fd);

/*
 * unlinks first element of token list and puts it in the tok pointer
 *
 * If an element is not yet ready, this funciton blocks. The function will
 * return 0 on success. Non-zero means the list is empty and won't get any more
 * elements.
 *
 * Input is Thread_params ponter
*/
Token * token_list_dequeue(List *tl);

/*
 * consumes whitespace characters, returns the next token type
*/
tokentype token_list_consume_white_peek(List *tl);

/*
 * removes whitespace from tail of token list
*/
void token_list_snip_white_tail(List *tl);

Token * new_token_static(char *value, size_t type, size_t length, size_t charnum);

/*
 * peeks the type of the next token in the list, does not consume the token
*/
tokentype token_list_peek_type(List *tl);

List *token_list_new(bool locked);
