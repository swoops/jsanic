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
	// Single-character tokens.
	TOKEN_OPEN_PAREN, TOKEN_CLOSE_PAREN, TOKEN_OPEN_BRACE, TOKEN_CLOSE_BRACE,
	TOKEN_COMMA, TOKEN_DOT, TOKEN_SEMICOLON, TOKEN_OPEN_CURLY, TOKEN_CLOSE_CURLY,

	//
	TOKEN_EQUAL, TOKEN_EQUAL_EQUAL, TOKEN_EQUAL_EQUAL_EQUAL,
	TOKEN_ADD, TOKEN_INCREMENT,
	TOKEN_LESSTHAN_OR_EQUAL, TOKEN_LESSTHAN,

	TOKEN_NUMERIC,

	// identifiers
	TOKEN_VAR, TOKEN_FOR, TOKEN_LET, TOKEN_FUNCTION, TOKEN_RETURN, TOKEN_CATCH,
	TOKEN_VARRIABLE,

	// special chars
	TOKEN_ERROR, TOKEN_NEWLINE,
	TOKEN_EOF
} tokentype;


typedef struct token_list {
	token *head;
	token *tail;
	size_t size;
	pthread_mutex_t lock;
} token_list;

#define LIST_LOCK 1 << 0

/*
 * consumes tokens in list, printing each
 * return 0 on success
*/
int token_list_print_consume(token_list *list);

/*
 * generate a empty token list.
*/
token_list * init_token_list();

/*
 * opens fname file readonly, lexes file appending tokens to list closes file
 * automaticly
 *
 * returns EOF when it reaches EOF
 * <0 return means error
*/
int gettokens_fromfname(char *fname, token_list *list);

/*
 * lexes from file descriptor returns EOF when it reaches EOF
 *
 * use gettokens_fromfname if you have a real file, use this for stdin or pipes
 *
 * <0 return means error
*/
int gettokens_fromfd(int fd, token_list *list);

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
token * token_list_pop(token_list *list);

/*
 * remove token structure from memmory
*/
void token_destroy(token *tok);
