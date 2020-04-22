#include <pthread.h>

typedef struct token token;
struct  token {
	char *value;
	size_t length;
	size_t charnum;
	int type;
	unsigned int flags;
	token * next;
	token * prev;
};


typedef struct token token;

typedef enum {

	// special chars
	TOKEN_NONE = 0,
	TOKEN_ERROR, TOKEN_EOF,

	// whiltespace
	TOKEN_TAB, TOKEN_SPACE, TOKEN_NEWLINE,

	// Single-character tokens.
	TOKEN_OPEN_PAREN, TOKEN_CLOSE_PAREN, TOKEN_OPEN_BRACE, TOKEN_CLOSE_BRACE,
	TOKEN_COMMA, TOKEN_DOT, TOKEN_COLON, TOKEN_SEMICOLON, TOKEN_OPEN_CURLY, TOKEN_CLOSE_CURLY, TOKEN_QUESTIONMARK,

	//
	TOKEN_EQUAL, TOKEN_EQUAL_EQUAL, TOKEN_EQUAL_EQUAL_EQUAL,
	TOKEN_ADD, TOKEN_INCREMENT, TOKEN_PLUS_EQUAL, TOKEN_SUBTRACT, TOKEN_MINUS_EQUAL, TOKEN_DECREMENT,
	TOKEN_LESSTHAN_OR_EQUAL, TOKEN_LESSTHAN,

	TOKEN_NUMERIC,

	// strings
	TOKEN_DOUBLE_QUOTE_STRING, TOKEN_SINGLE_QUOTE_STRING, TOKEN_TILDA_STRING,

	// identifiers
	TOKEN_VAR, TOKEN_FOR, TOKEN_LET, TOKEN_FUNCTION, TOKEN_RETURN, TOKEN_CATCH,
	TOKEN_IF, TOKEN_VARRIABLE,
} tokentype;


/*
 * token_list.status
 * <0   tokenizer is done, no more appending, safe to remove
 * >0   tells tokenizer to stop, but it has not done so yet
 * ==0  tokenizer is still going
 * == EOF done and processed to EOF
*/
typedef struct token_list {
	token *head;
	token *tail;
	size_t size;
	int fd;
	int status;
	pthread_mutex_t lock;
} token_list;

#define LIST_LOCK 1 << 0

/*
 * generate a empty token list.
*/
token_list * token_list_init(int fd);

/*
 * lexes from list->fd, if ( list->fd < 0 ) it will open list->fanme and lex
 * from there
 *
*/
void * gettokens(void *list);

/*
 * destroy a token_list, the calling thread should have sole access to the
 * token_list or there will be a race
*/
void token_list_destroy(token_list *list);

/*
 * unlinks first element of token list and returns a pointer to it
 *
 * NULL means failure or no more tokens. If all went well, the last token
 * should be a EOF. If the token parsing thread (gettokens_...) thread errored,
 * this may never return EOF
 *
 * returns NULL on failure
*/
token * token_list_pop(token_list *list, int *status);

/*
 * peeks the type of the next token in the list, does not consume the token
*/
size_t token_list_peek_type(token_list *list);

/*
 * remove token structure from memmory
*/
void token_destroy(token *tok);
