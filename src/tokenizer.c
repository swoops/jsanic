#include <string.h>  //strlen
#include <stdlib.h> // malloc
#include <stdio.h> // fread
#include <assert.h>

#include "tokens.h"
#include "tokenizer.h"
#include "cache.h"

#define locklist(x) pthread_mutex_lock(&x->lock)
#define unlocklist(x) pthread_mutex_unlock(&x->lock)

static void token_list_append(token_list *list, token *tok){
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
	unlocklist(list);
}

static token * init_token(){
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

static int token_list_add_fromdata(token_list *list, tokendata *data, size_t line, size_t charnum ) {
	token* tok = init_token();
	if ( !tok ){
		return -2;
	}
	tok->value = data->name;
	tok->length = strlen(data->name);
	tok->type = data->type;
	tok->nameid = data->nameid;
	tok->linenum = line;
	tok->charnum = charnum;
	token_list_append(list, tok);
	return 0;
}

static int token_list_add_dynamic(token_list *list, char *value, int type, int nameid, size_t line, size_t charnum) {
	token* tok = init_token();
	if ( !tok ){
		return -2;
	}
	TOKEN_SETALLOCED(tok);
	tok->value = value;
	tok->length = strlen(value);
	tok->type = type;
	tok->nameid = nameid;
	tok->linenum = line;
	tok->charnum = charnum;
	token_list_append(list, tok);
	return 0;
}

token_list * init_token_list(){
	token_list * list = (token_list*) malloc(sizeof(token_list));
	if ( list ) {
		list->head = NULL;
		list->tail = NULL;
		list->size = 0;
		pthread_mutex_init(&list->lock, NULL);
	}
	return list;
}

token * tokens_consuehead(token_list *list){
	token *tok, *newhead;
	locklist(list);
	if ( list->size <= 0 ){
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

void destroy_token_list(token_list *list){
	while ( list->size > 0 ){
		token_destroy( tokens_consuehead(list) );
	}
	pthread_mutex_destroy(&list->lock);
	free(list);
}

int is_numeric(int ch){
	return ( ch >= '0' && ch <= '9');
}

int is_lower_alph(int ch){
	return ( ch >= 'a' && ch <= 'z' );
}

int is_upper_alph(int ch){
	return ( ch >= 'A' && ch <= 'Z' );
}

int is_alpha_numeric(int ch){
	return is_upper_alph(ch) || is_lower_alph(ch) || is_numeric(ch);
}

int maybe_punctuator(cache *stream, token_list *list, int dataindex){
	tokendata *data = tokenstrs + dataindex;
	int result;
	size_t linenum = cache_getlinenum(stream);
	size_t charnum = cache_getcharnum(stream);
	int ret = cache_consume_n_match(stream, data->name, data->length, &result);
	if (ret < 0) return ret;
	if ( result == 0 ){
		token_list_add_fromdata( list, data, linenum, charnum);
	}
	return result;
}

int maybe_whitespace(cache *stream, token_list *list, int dataindex){
	int ch = cache_getc(stream);
	if ( ch < 0 ) {
		return ch;
	}
	if ( ch != tokenstrs[dataindex].name[0] ) {
		cache_step_back(stream);
		return 1;
	}
	return 0;
}

int maybe_keyword(cache *stream, token_list *list, int dataindex){
	tokendata *data = tokenstrs + dataindex;
	int result;
	size_t linenum = cache_getlinenum(stream);
	size_t charnum = cache_getcharnum(stream);
	int ret = cache_consume_n_match(stream, data->name, data->length, &result);
	if ( ret < 0 ) return ret;
	if ( result == 0 ){
		// everything matches so far but what if it is "ifvariable" instead of just "if"
		int ch = cache_getc(stream);
		if ( ch < EOF ) return ch; // error
		if ( is_alpha_numeric(ch) || ch == '_' ){ // no good, it is part of a bigger token, so backup
			cache_step_backcount(stream, data->length+1);
			return 1;
		}
		// we go it :)
		cache_step_back(stream); // put ch back
		token_list_add_fromdata( list, data, linenum, charnum);
	}
	return result;
}

int maybe_numeric(cache *stream, token_list *list) {
	size_t linenum = cache_getlinenum(stream);
	size_t charnum = cache_getcharnum(stream);
	size_t size = 32;
	int ch;
	size_t i;
	ch = cache_getc(stream);
	if ( ch < 0 ) return ch;
	if ( ! is_numeric(ch) ) return 0;
	char *buf = (char *) malloc(size);
	if ( !buf ) return -2;
	buf[0] = (char ) ch;
	for ( i=1,ch=0; ch >= 0; i++){
		ch = cache_getc(stream);
		if ( ch < 0 || ! is_alpha_numeric(ch) ) break;
		if ( i >= size-4 ) {
			size += 8;
			buf = realloc(buf, size);
			if ( !buf ) return -2;
		}
		buf[i] = (char ) ch;
	}
	buf[i] = 0;
	buf = realloc(buf, i+2);
	cache_step_back(stream);
	return token_list_add_dynamic(list, buf, NUMERIC, NUMBER, linenum, charnum);;
}


int maybe_string(cache *stream, token_list *list, int dataindex) {
	size_t linenum = cache_getlinenum(stream);
	size_t charnum = cache_getcharnum(stream);
	int start = cache_getc(stream);
	tokendata *data = tokenstrs + dataindex;
	if ( start != data->name[0] ) {
			cache_step_back(stream);
			return 1;
	}
	int skip = 0;
	int ch=0;
	size_t i, size = 32;
	char *buf = (char *) malloc(size);
	if ( !buf ) {
		cache_step_back(stream);
		return -2;
	}
	buf[0] = (char) start;
	for (i=1; ch>=0; i++){
		ch = cache_getc(stream);
		if ( i+4 > size ){
			size*=2;
			buf = realloc(buf, size);
			if ( !buf ) {
				cache_step_backcount(stream, i+1);
				return -2;
			}
		}
		buf[i]= (char) ch;
		if ( skip ) {
			skip = 0;
		}else{
			if ( ch == '\\' ){
				skip = 1;
			}else if  ( ch == start ){
				break;
			}
		}
	}
	if ( ch < 0 ) {
		cache_step_backcount(stream, i+1);
		free(buf);
		return ch;
	}
	buf = realloc(buf, i+4);
	buf[i+1]=0; // null terminate
	return token_list_add_dynamic(list, buf, STRING, data->nameid, linenum, charnum);;
}

static int token_binary_search(cache *stream, tokendata top[], size_t len ){
	if ( len == 1 ){
		return top - tokenstrs;
	}
	int result, ret;
	size_t top_len = len/2;
	size_t bottom_len = len - top_len;
	tokendata *bottom = top+top_len;

	ret = cache_strcmp(stream, bottom[0].name, &result);
	if (ret < 0){ // error
		return ret;
	}

	if (result < 0){ // cache > string
		ret = token_binary_search(stream, top, top_len);
	}else{ // cache <= string
		ret = token_binary_search(stream, bottom, bottom_len);
	}
	return ret;
}

int add_variable(token_list *list, cache *stream, int ch){
	size_t linenum = cache_getlinenum(stream);
	size_t charnum = cache_getcharnum(stream)-1; // ch was given
	size_t size = 16;
	size_t i;
	char *buf = (char *) malloc(16);
	if ( !buf ) {
		cache_step_back(stream);
		return -2;
	}
	buf[0] = (char) ch;
	for ( i=1; ch>0; i++ ){
		ch = cache_getc(stream);
		if ( ! is_alpha_numeric(ch) && ch != '_' ){
			break;
		}
		if ( i+4 > size ){
			size*=2;
			buf = realloc(buf, size);
			if ( !buf ) {
				cache_step_backcount(stream, i+1);
				return -2;
			}
		}
		buf[i] = (char) ch;
	}
	if ( ch < 0 ){
		cache_step_backcount(stream, i+1);
		free(buf);
		return ch;
	}
	buf[i+1] = 0;
	buf = realloc(buf, i+4);
	if ( !buf ){
		cache_step_backcount(stream, i+1);
		return -2;
	}
	cache_step_back(stream);
	return token_list_add_dynamic(list, buf, IDENTIFIER, VARIABLE, linenum, charnum);;
}

int gettokens(int fd, token_list *list){
	cache * stream = cache_init(128, fd);
	int ret = 0;
	int type;
	int ch;
	while (ret >= 0){
		ret = token_binary_search(
			stream, tokenstrs, (sizeof(tokenstrs) / sizeof(tokenstrs[0]))
		);
		if ( ret > 0 ){ // we likely found it, but need to look deeper
			type = tokenstrs[ret].type;
			switch (type){
				case PUNCTUATOR:
					ret = maybe_punctuator(stream, list, ret);
					break;
				case WHITESPACE:
					ret = maybe_whitespace(stream, list, ret);
					break;
				case KEYWORD:
					ret = maybe_keyword(stream, list, ret);
					break;
				case NUMERIC:
					ret = maybe_numeric(stream, list);
					break;
				case STRING:
					ret = maybe_string(stream, list, ret);
					break;
				default:
					fprintf(stderr, "Can't handle typeid %d on %s yet\n", tokenstrs[ret].type, tokenstrs[ret].name);
					abort();
			}
			if ( !ret ) continue; // caught it, start at the top
		}
		if ( ret < 0 ) break;

		ch = cache_getc(stream);
		if ( is_alpha_numeric(ch) || ch == '_' ){
			// assuming it is a variable
			ret = add_variable(list, stream, ch);
			if (ret <= 0 ) continue;
		}
		switch (ch){
		case EOF:
			break;
		default:
			fprintf(stderr, "Unkown character %c 0x%02x at %ld\n", ret, ret, cache_getcharnum(stream));
			break;
		}
	}
	cache_destroy(stream);
	return ret;
}
