#include<pthread.h>
#include <stdio.h>

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

int gettokens(int fd, token_list *list);

/*
 * generate a empty token list. Use a userlock paramter for thread safety
*/
token_list * init_token_list();

/*
 * destroy a toke_list, the calling thread should have sole access to the
 * token_list or there will be a race
*/
void destroy_token_list(token_list *list);

/*
 * removes first element of tokenlist and returns it.
 * returns NULL on failure.
*/
token * tokens_consuehead(token_list *list);
void token_destroy(token *tok);
