#include <string.h>  //strlen
#include <assert.h>
#include <stdio.h>

#include "errorcodes.h"
#include "tokenizer.h"
#include "cache.h"

#define locklist(x) pthread_mutex_lock(&x->lock)
#define unlocklist(x) pthread_mutex_unlock(&x->lock)

// token flags and macros
#define  LINKED   1  <<  0
#define  HEAD     1  <<  1
#define  ALLOCED  1  <<  2

#define TOKEN_ISALLOCED(x)  ( x->flags & ALLOCED )
#define TOKEN_ISLINKED(x)     ( x->flags & LINKED )
#define TOKEN_SETLINKED(x)    x->flags |= LINKED
#define TOKEN_UNSETLINKED(x)  x->flags &= ~LINKED
#define TOKEN_ISHEAD(x)       ( x->flags & HEAD )
#define TOKEN_SETALLOCED(x)   x->flags |= ALLOCED

static int token_list_append(token_list *list, token *tok){
	int ret = 0;
	assert(tok->next == NULL); // sanity

	locklist(list);
	token *tail = list->tail;
	list->tail = tok;
	tok->prev = tail;
	if ( tail != NULL ){
		assert(tail->next == NULL); // sanity
		tail->next = tok;
	}
	if ( list->head == NULL ){
		list->head = tok;
	}
	list->size++;
	TOKEN_SETLINKED(tok);
	if ( tok->type == TOKEN_EOF ){
		ret = list->status = EOF;
	}else{
		ret = list->status;
	}
	unlocklist(list);
	return ret;
}


size_t token_list_peek_type(token_list *list){
	size_t ret = TOKEN_NONE;
	locklist(list);
	if ( list->size > 0 ){
		ret = list->head->type;
	}
	unlocklist(list);
	return ret;
}

static token * token_init(){
	token * ret = (token *) malloc(sizeof(token));
	if ( ret == NULL ) return ret;
	ret->value = NULL;
	ret->next = NULL;
	ret->prev = NULL;
	ret->flags = 0;
	return ret;
}

void token_destroy(token *tok){
	assert(!TOKEN_ISLINKED(tok));
	assert(!TOKEN_ISHEAD(tok));
	if ( TOKEN_ISALLOCED(tok) ){
		free(tok->value);
	}
	free(tok);
}

token_list * token_list_init(int fd){
	if ( fd < 0 ) return NULL;
	token_list * list = (token_list*) malloc(sizeof(token_list));
	if ( list ) {
		list->status = 0;
		list->head = NULL;
		list->tail = NULL;
		list->size = 0;
		list->fd = fd;
		pthread_mutex_init(&list->lock, NULL);
	}
	return list;
}

token * token_list_pop(token_list *list, int *status){
	token *tok, *newhead;
	locklist(list);
	*status = list->status;
	if ( list->size == 0 ){
		unlocklist(list);
		return NULL;
	}

	tok = list->head;
	assert(tok->prev == NULL); // sanity
	list->size--;
	TOKEN_UNSETLINKED(tok);

	// re-linking
	newhead = tok->next;
	tok->next = NULL;
	list->head = newhead;
	if ( newhead ){
		newhead->prev = NULL;
	} else if ( list->tail == tok ){
		list->tail=NULL;
	}
	unlocklist(list);
	return tok;
}

void token_list_set_status(token_list *list, int status){
	locklist(list);
	list->status = status;
	unlocklist(list);
}

static void token_list_wait_done(token_list *list){
	locklist(list);
	if (list->status < 0){
		// tokenizer is done already
		unlocklist(list);
		return;
	}

	if ( list->status == 0 ){
		// need to tell it to stop
		list->status = 1;
	}

	// wait for tokenizer to finish
	while (1){
		unlocklist(list);
		usleep(10);
		locklist(list);
		if ( list->status < 0 ){
			unlocklist(list);
			break;
		}
	}
}

void token_list_destroy(token_list *list){
	int ret;
	token_list_wait_done(list);
	while ( list->size > 0 ){
		token_destroy( token_list_pop(list, &ret));
	}
	pthread_mutex_destroy(&list->lock);
	free(list);
}

static inline int is_numeric(int ch){
	return ( ch >= '0' && ch <= '9');
}

static inline int is_lower_alpha(int ch){
	return ( ch >= 'a' && ch <= 'z' );
}

static inline int is_upper_alpha(int ch){
	return ( ch >= 'A' && ch <= 'Z' );
}

static inline int is_alpha(int ch){
	return ( is_upper_alpha(ch) || is_lower_alpha(ch) );
}

static inline int is_alpha_numeric(int ch){
	return is_alpha(ch) || is_numeric(ch);
}


/*
 * createes a buffer, reads into the buffer until a non-identifyer char is encounted.
 *
 * returns a pointer to the string, or NULL on failure
 * failure returns the cache to it's original state
*/
static char * alloc_identifyer(cache *stream, int ch, size_t *len){
	size_t size = 16;
	size_t i;
	char *buf = (char *) malloc(size);
	*len = 0;
	if ( !buf ) {
		cache_step_back(stream);
		return NULL;
	}
	buf[0] = (char) ch;
	for ( i=1; ch>0; i++ ){
		ch = cache_getc(stream);
		if ( ! is_alpha_numeric(ch) && ch != '_' && ch != '$' ){
			break;
		}
		if ( i+4 > size ){
			size*=2;
			buf = realloc(buf, size);
			if ( !buf ) {
				cache_step_backcount(stream, i);
				return NULL;
			}
		}
		buf[i] = (char) ch;
	}
	if ( ch < EOF ){
		cache_step_backcount(stream, i);
		free(buf);
		return NULL;
	}
	buf[i] = 0;
	*len = i;
	buf = realloc(buf, i+3);
	if ( !buf ){
		cache_step_backcount(stream, i);
		return NULL;
	}
	cache_step_back(stream);
	return buf;
}

static char * alloc_numeric(cache *stream, int ch, size_t *len){
	// TODO HANDLE 0xXX 0bBB 0oOO formats
	size_t size = 16;
	size_t i;
	char *buf = (char *) malloc(size);
	*len = 0;
	if ( !buf ) {
		cache_step_back(stream);
		return NULL;
	}
	buf[0] = (char) ch;
	for ( i=1; ch>0; i++ ){
		ch = cache_getc(stream);
		if ( ! is_numeric(ch) ){
			break;
		}
		if ( i+4 > size ){
			size*=2;
			buf = realloc(buf, size);
			if ( !buf ) {
				cache_step_backcount(stream, i);
				return NULL;
			}
		}
		buf[i] = (char) ch;
	}
	if ( ch < EOF ){
		cache_step_backcount(stream, i);
		free(buf);
		return NULL;
	}
	buf[i] = 0;
	*len = i;
	buf = realloc(buf, i+3);
	if ( !buf ){
		cache_step_backcount(stream, i);
		return NULL;
	}
	cache_step_back(stream);
	return buf;
}


static token * new_token_static(char *value, size_t type, size_t length, size_t charnum){
	token *tok = token_init();
	if ( !tok ) return tok;
	tok->value = value;
	tok->length = length;
	tok->type = type;
	tok->charnum = charnum;
	return tok;
}

static token * new_token_error(int ch, size_t charnum){
	char buf[2];
	token *tok = token_init();
	if ( !tok ) return tok;
	buf[0] = (char) ch;
	buf[1] = '\x00';

	TOKEN_SETALLOCED(tok);
	if (( tok->value = strdup(buf) ) == NULL){
		free(tok);
		return NULL;
	}
	tok->length = 1;
	tok->type = TOKEN_ERROR;
	tok->charnum = charnum;
	return tok;
}

static size_t get_identifyer_type(char *buf){
	size_t ret = TOKEN_VARIABLE;
	switch (buf[0]){
		case 'c':
			if ( strcmp("catch", buf) == 0)
				ret = TOKEN_CATCH;
			break;
		case 'f':
			if ( strcmp("for", buf) == 0)
				ret = TOKEN_FOR;
			else if ( strcmp("function", buf) == 0)
				ret = TOKEN_FUNCTION;
			break;
		case 'i':
			if ( strcmp("if", buf) == 0)
				ret = TOKEN_IF;
			break;
		case 'l':
			if ( strcmp("let", buf) == 0)
				ret = TOKEN_LET;
			break;
		case 'r':
			if ( strcmp("return", buf) == 0)
				ret = TOKEN_RETURN;
			break;
		case 'v':
			if ( strcmp("var", buf) == 0)
				ret = TOKEN_VAR;
			break;
	}
	return ret;
}

static token * new_token_identifyer(size_t charnum, char *value, size_t len){
	token *tok = token_init();
	if ( !tok ) return tok;
	TOKEN_SETALLOCED(tok);
	tok->length = len;
	tok->value = value;
	tok->type = get_identifyer_type(value);
	tok->charnum = charnum;
	return tok;
}

static token * new_token_number(size_t charnum, char *value, size_t len){
	token *tok = token_init();
	if ( !tok ) return tok;
	TOKEN_SETALLOCED(tok);
	tok->length = len;
	tok->value = value;
	tok->charnum = charnum;
	tok->type = TOKEN_NUMERIC;
	return tok;
}

static char * alloc_string(cache *stream, int start, size_t *len){
	size_t size = 128;
	int ch;
	size_t i;
	char *buf = (char *) malloc(size);
	int skip = 0;
	if ( !buf ) {
		cache_step_back(stream);
		return NULL;
	}
	*len = 0;
	buf[0] = (char) start;
	for ( i=1; ; i++ ){
		ch = cache_getc(stream);
		if ( i+4 > size ){
			size*=2;
			buf = realloc(buf, size);
			if ( !buf ) {
				cache_step_backcount(stream, i);
				return NULL;
			}
		}
		buf[i] = (char) ch;

		if ( skip ){
			skip = 0;
		} else if ( ch == '\\' ){
			skip = 1;
		} else if ( ch == start ) {
			break; // found it
		} else if ( ch < 0 ){
			i--;
			cache_step_back(stream);
			break; // error, just end the string early
		}
	}
	buf[i+1] = 0;
	*len = i+1;
	buf = realloc(buf, i+3);
	if ( !buf ){
		cache_step_backcount(stream, i);
		return NULL;
	}
	return buf;
}

static token * new_token_string(cache *stream, int start, size_t charnum){
	token *tok = token_init();
	if ( !tok ) return tok;
	tok->value = alloc_string(stream, start, &tok->length);
	if ( !tok->value ){
		free(tok);
		return NULL;
	}
	TOKEN_SETALLOCED(tok);
	tok->charnum = charnum;
	switch ( start ){
		case '"':
			tok->type = TOKEN_DOUBLE_QUOTE_STRING;
			break;
		case '`':
			tok->type = TOKEN_TILDA_STRING;
			break;
		default:
			tok->type = TOKEN_SINGLE_QUOTE_STRING;
			break;
	}
	return tok;
}


static char * alloc_line_comment(cache *stream, size_t *len){
	size_t i;
	int ch = '/';
	size_t size = 90;
	char *buf = (char *) malloc(size);
	if ( !buf ) return NULL;
	buf[0] = ch;
	buf[1] = ch;
	for (i=2; ch!='\n' && ch > 0; i++){
		ch = cache_getc(stream);
		if ( i+3 > size ){
			size += 16;
			buf = (char *) realloc(buf, size);
			if ( !buf ) return NULL;
		}
		buf[i] = ch;
	}
	if ( ch < 0 ){
		// did not get everything, but save what we did get and step cache back
		// to prev position before error
		cache_step_back(stream);
		i--;
	}
	buf = (char *) realloc(buf, i+3);
	if ( !buf ) return NULL;
	*len = i;
	buf[i] = '\x00';
	return buf;
}

static token * new_token_line_comment(cache *stream, size_t charnum){
	token *tok = token_init();
	if ( !tok ) return tok;
	tok->value = alloc_line_comment(stream, &tok->length);
	if ( !tok->value ){
		free(tok);
		return NULL;
	}
	TOKEN_SETALLOCED(tok);
	tok->charnum = charnum;
	tok->type = TOKEN_LINE_COMMENT;
	return tok;
}

static char * alloc_multi_line_comment(cache *stream, size_t *len){
	size_t i;
	int prev = '/';
	int ch = '*';
	size_t size = 90;
	char *buf = (char *) malloc(size);
	if ( !buf ) return NULL;
	buf[0] = (char) prev;
	buf[1] = (char) ch;
	for (i=2; ch > 0; i++){
		ch = cache_getc(stream);
		if ( i+3 > size ){
			size += 16;
			buf = (char *) realloc(buf, size);
			if ( !buf ) return NULL;
		}
		buf[i] = ch;
		if ( prev == '*' && ch == '/' ) break;
		prev = ch;
	}
	if ( ch < 0 ) {
		// did not finish, but save what we have and restore cache
		cache_step_back(stream);
		i--;
	}
	buf = (char *) realloc(buf, i+3);
	if ( !buf ) return NULL;
	*len = i;
	buf[i] = '\x00';
	return buf;

}

static char * alloc_regex(cache *stream, size_t *len){
	size_t i;
	int skip = 0;
	int in_square = 0;
	int end_slash = 0;
	size_t size = 64;
	char *buf = (char *) malloc(size);
	int ch = '/';
	if ( !buf ) return NULL;
	buf[0] = ch;
	for (i=1; ch > 0; i++){
		ch = cache_getc(stream);
		if ( i+3 > size ){
			size += 16;
			buf = (char *) realloc(buf, size);
			if ( !buf ) return NULL;
		}
		buf[i] = ch;
		if ( end_slash ){
			// flags
			if ( ! is_alpha(ch) ){
				break;
			}
		} else if ( in_square ){
			// trying to escape the square...
			if ( skip ){
				skip = 0;
			}else if (ch == '\\'){
				skip=1;
			}else if (ch == ']'){
				in_square = 0;
			}
		}else {
			// looking for ending /
			if ( skip ){
				skip = 0;
			}else if ( ch == '[' ){
				in_square = 1;
			}else if ( ch == '\\' ){
				skip = 1;
			}else if ( ch == '/' ){
				end_slash = 1;
			}else {
				skip = 0;
			}

		}
	}
	if ( ch < 0 ) {
		// did not finish, but save what we have and restore cache
		cache_step_back(stream);
		i--;
	}
	buf = (char *) realloc(buf, i+3);
	if ( !buf ) return NULL;
	*len = i;
	buf[i] = '\x00';
	return buf;

}

static token * new_regex(cache *stream, size_t charnum){
	token *tok = token_init();
	if ( !tok ) return tok;
	tok->value = alloc_regex(stream, &tok->length);
	if ( !tok->value ){
		free(tok);
		return NULL;
	}
	TOKEN_SETALLOCED(tok);
	tok->charnum = charnum;
	tok->type = TOKEN_MULTI_LINE_COMMENT;
	return tok;
}


static token * new_token_multi_line_comment(cache *stream, size_t charnum){
	token *tok = token_init();
	if ( !tok ) return tok;
	tok->value = alloc_multi_line_comment(stream, &tok->length);
	if ( !tok->value ){
		free(tok);
		return NULL;
	}
	TOKEN_SETALLOCED(tok);
	tok->charnum = charnum;
	tok->type = TOKEN_MULTI_LINE_COMMENT;
	return tok;
}

static token * scan_token(cache *stream, size_t prev_type){
	size_t charnum = cache_getcharnum(stream);
	int ch = cache_getc(stream);
	token *tok = NULL;
	if ( is_alpha(ch) || ch == '_' || ch == '$'){
		size_t len;
		char *buf = alloc_identifyer(stream, ch, &len);
		if ( !buf ){
			return NULL;
		}
		tok = new_token_identifyer(charnum, buf, len);
		if ( !tok ){
			free(buf);
			return NULL;
		}
		return tok;
	} else if ( is_numeric(ch) ){
		size_t len;
		char *buf = alloc_numeric(stream, ch, &len);
		if ( !buf ){
			return NULL;
		}
		tok = new_token_number(charnum, buf, len);
		if ( !tok ){
			free(buf);
			return NULL;
		}
		return tok;
	}
#define SIMPLE_TOKEN(value, name) new_token_static(value, name, sizeof(value), charnum);
	switch (ch) {
		// simple single characters
		case '\r':
			tok = SIMPLE_TOKEN("\r ", TOKEN_CARRAGE_RETURN);
			break;
		case ' ':
			tok = SIMPLE_TOKEN(" ", TOKEN_SPACE);
			break;
		case '\t':
			tok = SIMPLE_TOKEN("\t", TOKEN_TAB);
			break;
		case '\n':
			tok = SIMPLE_TOKEN("\n", TOKEN_NEWLINE);
			break;
		case '{':
			tok = SIMPLE_TOKEN("{", TOKEN_OPEN_CURLY);
			break;
		case '}':
			tok = SIMPLE_TOKEN("}", TOKEN_CLOSE_CURLY);
			break;
		case '(':
			tok = SIMPLE_TOKEN("(", TOKEN_OPEN_PAREN);
			break;
		case ')':
			tok = SIMPLE_TOKEN(")", TOKEN_CLOSE_PAREN);
			break;
		case '[':
			tok = SIMPLE_TOKEN("[", TOKEN_OPEN_BRACE);
			break;
		case ']':
			tok = SIMPLE_TOKEN("]", TOKEN_CLOSE_BRACE);
			break;
		case ',':
			tok = SIMPLE_TOKEN(",", TOKEN_COMMA);
			break;
		case '.':
			tok = SIMPLE_TOKEN(".", TOKEN_DOT);
			break;
		case ':':
			tok = SIMPLE_TOKEN(":", TOKEN_COLON);
			break;
		case ';':
			tok = SIMPLE_TOKEN(";", TOKEN_SEMICOLON);
			break;
		case '!':
			tok = SIMPLE_TOKEN("!", TOKEN_NOT);
			break;
		case '~':
			tok = SIMPLE_TOKEN("~", TOKEN_BITWISE_NOT);
			break;

		// 1 or more chars
		case '?':
			ch = cache_getc(stream);
			if ( ch == '?' ){
				tok = SIMPLE_TOKEN("??", TOKEN_NULL_COALESCING);
			}else {
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("?", TOKEN_QUESTIONMARK);
			}
			break;
		case '/':
			ch = cache_getc(stream); // /X
			if ( ch == '/' ){
				tok = new_token_line_comment(stream, charnum);
			}else if ( ch == '*' ){
				tok = new_token_multi_line_comment(stream, charnum);
			}else if ( prev_type != TOKEN_VARIABLE
				&& prev_type != TOKEN_NUMERIC && prev_type != TOKEN_CLOSE_PAREN
				&& prev_type != TOKEN_CLOSE_BRACE
			){
				cache_step_back(stream);
				tok = new_regex(stream, charnum);
			}else if ( ch == '=' ){
				tok = SIMPLE_TOKEN("/=", TOKEN_DIVIDE_ASSIGN);
			}else{
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("/", TOKEN_DIVIDE);
			}
			break;
		case '=':
			ch = cache_getc(stream);
			if ( ch == '='){
				ch = cache_getc(stream);
				if ( ch == '='){
					tok = SIMPLE_TOKEN("===", TOKEN_EQUAL_EQUAL_EQUAL);
				} else {
					cache_step_back(stream);
					tok = SIMPLE_TOKEN("==", TOKEN_EQUAL_EQUAL);
				}
			} else{
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("=", TOKEN_EQUAL);
			}
			break;
		case '-':
			ch = cache_getc(stream);
			if ( ch == '-'){
				tok = SIMPLE_TOKEN("--", TOKEN_DECREMENT);
			} else if ( ch == '='){
				tok = SIMPLE_TOKEN("-=", TOKEN_MINUS_EQUAL);
			} else{
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("-", TOKEN_SUBTRACT);
			}
			break;
		case '%':
			ch = cache_getc(stream);
			if ( ch == '='){
				tok = SIMPLE_TOKEN("%=", TOKEN_MOD_EQUAL);
			} else{
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("%", TOKEN_MOD);
			}
			break;
		case '*':
			ch = cache_getc(stream);
			if ( ch == '*'){
				tok = SIMPLE_TOKEN("**", TOKEN_EXPONENT);
			} else if ( ch == '='){
				tok = SIMPLE_TOKEN("*=", TOKEN_MULTIPLY_ASSIGN);
			} else{
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("*", TOKEN_MULTIPLY);
			}
			break;
		case '+':
			ch = cache_getc(stream);
			if ( ch == '+'){
				tok = SIMPLE_TOKEN("++", TOKEN_INCREMENT);
			} else if ( ch == '='){
				tok = SIMPLE_TOKEN("+=", TOKEN_PLUS_EQUAL);
			} else{
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("+", TOKEN_ADD);
			}
			break;
		case '^':
			ch = cache_getc(stream);
			if ( ch == '='){
				tok = SIMPLE_TOKEN("^=", TOKEN_BITWISE_XOR_ASSIGN);
			} else{
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("^", TOKEN_BITWISE_XOR);
			}
			break;
		case '|':
			ch = cache_getc(stream);
			if ( ch == '='){
				tok = SIMPLE_TOKEN("|=", TOKEN_BITWISE_OR_ASSIGN);
			} else if ( ch == '|'){
				tok = SIMPLE_TOKEN("||", TOKEN_LOGICAL_OR);
			} else{
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("|", TOKEN_BITWISE_OR);
			}
			break;
		case '&':
			ch = cache_getc(stream);
			if ( ch == '='){
				tok = SIMPLE_TOKEN("&=", TOKEN_BITWISE_AND_ASSIGN);
			} else if ( ch == '&'){
				tok = SIMPLE_TOKEN("&&", TOKEN_LOGICAL_AND);
			} else{
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("&", TOKEN_BITWISE_AND);
			}
			break;
		case '<':
			ch = cache_getc(stream); // <X
			if ( ch == '='){
				tok = SIMPLE_TOKEN("<=", TOKEN_LESSTHAN_OR_EQUAL);
			} else if ( ch == '<'){
				ch = cache_getc(stream); // <<X
				if ( ch == '=' ){
					tok = SIMPLE_TOKEN("<<=", TOKEN_BITSHIFT_LEFT_ASSIGN);
				}
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("<<", TOKEN_BITSHIFT_LEFT);
			}else{
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("<", TOKEN_LESSTHAN);
			}
			break;
		case '>':
			ch = cache_getc(stream); // >X
			if ( ch == '='){
				tok = SIMPLE_TOKEN(">=", TOKEN_GREATERTHAN_OR_EQUAL);
			} else if ( ch == '>'){
				ch = cache_getc(stream); // >>X
				if ( ch == '=' ){
					tok = SIMPLE_TOKEN(">>=", TOKEN_BITSHIFT_RIGHT_ASSIGN);
				} else if ( ch == '>' ){
					tok = SIMPLE_TOKEN(">>>", TOKEN_ZERO_FILL_RIGHT_SHIFT);
				}
				cache_step_back(stream);
				tok = SIMPLE_TOKEN(">>", TOKEN_SIGNED_BITSHIFT_RIGHT);
			}else{
				cache_step_back(stream);
				tok = SIMPLE_TOKEN(">", TOKEN_GREATER_THAN);
			}
			break;

		case '\'':
		case '`':
		case '"':
			tok = new_token_string(stream, ch, charnum);
			break;

		case EOF:
			tok = SIMPLE_TOKEN("\xff", TOKEN_EOF);
			break;
		default:
			tok = new_token_error(ch, charnum);
#undef SIMPLE_TOKEN
	}
	return tok;
}


void * gettokens(void *l){
	int ret = TOKEN_NONE;
	size_t prev_type;
	token *tok = NULL;
	token_list *list = (token_list *) l;
	if ( list->fd < 0 ){
		token_list_set_status(list, IOERROR);
		return NULL;
	}

	cache * stream = cache_init(128, list->fd);
	if ( !stream ) {
		token_list_set_status(list, MALLOCFAIL);
		return NULL;
	}

	while ( ret == 0 ){
		tok = scan_token(stream, prev_type);
		if ( tok != NULL ) {
			switch ( tok->type ){
				case TOKEN_TAB:
				case TOKEN_SPACE:
				case TOKEN_NEWLINE:
				case TOKEN_CARRAGE_RETURN:
					break;
				default:
					prev_type = tok->type;
					break;
			}
			ret = token_list_append(list, tok);
		} else {
			ret = ERROR;
		}
	}

	cache_destroy(stream);
	// must set a status <0 to indicate completion
	token_list_set_status(list, ret == EOF ? EOF : ERROR);
	return NULL;
}
