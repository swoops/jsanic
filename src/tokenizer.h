#include <pthread.h>

typedef struct token token;
struct  token {
	char *value;
	size_t length;
	size_t charnum;
	size_t linenum;
	int type;
	int nameid;
	unsigned int flags;
	token * next;
	token * prev;
};

typedef struct token token;
#define  LINKED   1  <<  0
#define  HEAD     1  <<  1
#define  ALLOCED  1  <<  2

#define TOKEN_HASSTR(x) ( x->value != NULL )

#define TOKEN_ISLINKED(x)    ( x->flags & LINKED )
#define TOKEN_SETLINKED(x)   x->flags |= LINKED
#define TOKEN_UNSETLINKED(x) x->flags &= ~LINKED

#define TOKEN_ISHEAD(x)  ( x->flags & HEAD )
#define TOKEN_SETHEAD(x) ( x->flags |= HEAD > 0)

#define TOKEN_ISALLOCED(x)  ( x->flags & ALLOCED )
#define TOKEN_SETALLOCED(x)   x->flags |= ALLOCED
#define TOKEN_UNSETALLOCED(x) x->flags &= ~ALLOCED

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
