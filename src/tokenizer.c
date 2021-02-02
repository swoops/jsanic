#include <string.h>  //strlen
#include <assert.h>
#include <stdio.h>

#include "threads.h"
#include "errorcodes.h"
#include "tokenizer.h"
#include "cache.h"

// token flags and macros
#define  LINKED   1  <<  0
#define  HEAD     1  <<  1
#define  ALLOCED  1  <<  2

#define TOKEN_ISALLOCED(x)  (x->flags & ALLOCED)
#define TOKEN_ISLINKED(x)     (x->flags & LINKED)
#define TOKEN_SETLINKED(x)    x->flags |= LINKED
#define TOKEN_UNSETLINKED(x)  x->flags &= ~LINKED
#define TOKEN_ISHEAD(x)       (x->flags & HEAD)
#define TOKEN_SETALLOCED(x)   x->flags |= ALLOCED

static bool until_not_white(void *data, void *args) {
	Token *token = (Token *) data;
	if (!data) {
		*(tokentype *) args = TOKEN_STOP;
		return false;
	}
	tokentype type = token->type;
	switch (type) {
		case TOKEN_SPACE:
		case TOKEN_NEWLINE:
		case TOKEN_CARRAGE_RETURN:
		case TOKEN_TAB:
			// continue
			return true;
		default:
			*(tokentype *) args = type;
			return false; // stop
	}
}

tokentype token_list_consume_white_peek(List *tl) {
	tokentype type = TOKEN_NONE;
	list_consume_until(tl, until_not_white, (void *) &type);
	return type;
}

void token_list_snip_white_tail(List *tl) {
	tokentype type = TOKEN_NONE;
	list_consume_tail_until(tl, until_not_white, &type);
}

size_t token_list_peek_type(List *tl) {
	Token *t = (Token *) list_peek_head_block(tl);
	if (t) {
		return t->type;
	}
	return TOKEN_ERROR;
}

static Token * token_init() {
	Token * ret = (Token *) malloc(sizeof(Token));
	if (ret == NULL) return ret;
	ret->value = NULL;
	ret->flags = 0;
	return ret;
}

static void token_destroy(void *v) {
	if (!v) {
		return;
	}
	Token *tok = (Token *) v;
	assert(!TOKEN_ISLINKED(tok));
	assert(!TOKEN_ISHEAD(tok));
	if (TOKEN_ISALLOCED(tok)) {
		free((void *) tok->value);
	}
	free(tok);
}

Token * token_list_dequeue(List *tl) {
	return (Token *) list_dequeue_block(tl);
}

static inline int is_numeric(int ch) {
	return (ch >= '0' && ch <= '9');
}

static inline int is_lower_alpha(int ch) {
	return (ch >= 'a' && ch <= 'z');
}

static inline int is_upper_alpha(int ch) {
	return (ch >= 'A' && ch <= 'Z');
}

static inline int is_alpha(int ch) {
	return (is_upper_alpha(ch) || is_lower_alpha(ch));
}

static inline int is_alpha_numeric(int ch) {
	return is_alpha(ch) || is_numeric(ch);
}


/*
 * createes a buffer, reads into the buffer until a non-identifyer char is encounted.
 *
 * returns a pointer to the string, or NULL on failure
 * failure returns the cache to it's original state
*/
static char * alloc_identifyer(cache *stream, int ch, size_t *len) {
	size_t size = 16;
	size_t i;
	char *buf = (char *) malloc(size);
	*len = 0;
	if (!buf) {
		cache_step_back(stream);
		return NULL;
	}
	buf[0] = (char) ch;
	for (i=1; ch>0; i++) {
		ch = cache_getc(stream);
		if (! is_alpha_numeric(ch) && ch != '_' && ch != '$') {
			break;
		}
		if (i+4 > size) {
			size*=2;
			buf = realloc(buf, size);
			if (!buf) {
				cache_step_backcount(stream, i);
				return NULL;
			}
		}
		buf[i] = (char) ch;
	}
	if (ch < EOF) {
		cache_step_backcount(stream, i);
		free(buf);
		return NULL;
	}
	buf[i] = 0;
	*len = i;
	buf = realloc(buf, i+3);
	if (!buf) {
		cache_step_backcount(stream, i);
		return NULL;
	}
	cache_step_back(stream);
	return buf;
}

static char * alloc_numeric(cache *stream, int ch, size_t *len) {
	// TODO HANDLE 0xXX 0bBB 0oOO formats
	size_t size = 16;
	size_t i;
	char *buf = (char *) malloc(size);
	*len = 0;
	if (!buf) {
		cache_step_back(stream);
		return NULL;
	}
	buf[0] = (char) ch;
	for (i=1; ch>0; i++) {
		ch = cache_getc(stream);
		if (! is_numeric(ch)) {
			break;
		}
		if (i+4 > size) {
			size*=2;
			buf = realloc(buf, size);
			if (!buf) {
				cache_step_backcount(stream, i);
				return NULL;
			}
		}
		buf[i] = (char) ch;
	}
	if (ch < EOF) {
		cache_step_backcount(stream, i);
		free(buf);
		return NULL;
	}
	buf[i] = 0;
	*len = i;
	buf = realloc(buf, i+3);
	if (!buf) {
		cache_step_backcount(stream, i);
		return NULL;
	}
	cache_step_back(stream);
	return buf;
}


Token * new_token_static(char *value, size_t type, size_t length, size_t charnum) {
	Token *tok = token_init();
	if (!tok) return tok;
	tok->value = value;
	tok->length = length;
	tok->type = type;
	tok->charnum = charnum;
	return tok;
}

static Token * new_token_error(int ch, size_t charnum) {
	char buf[2];
	Token *tok = token_init();
	if (!tok) return tok;
	buf[0] = (char) ch;
	buf[1] = '\x00';

	TOKEN_SETALLOCED(tok);
	if ((tok->value = strdup(buf)) == NULL) {
		free(tok);
		return NULL;
	}
	tok->length = 1;
	tok->type = TOKEN_ERROR;
	tok->charnum = charnum;
	return tok;
}

static size_t get_identifyer_type(char *buf) {
	size_t ret = TOKEN_VARIABLE;
	switch (buf[0]) {
		case 'c':
			if (strcmp("catch", buf) == 0)
				ret = TOKEN_CATCH;
			else if (strcmp("const", buf) == 0)
				ret = TOKEN_CONST;
			break;
		case 'd':
			if (strcmp("do", buf) == 0)
				ret = TOKEN_FOR;
			else if (strcmp("function", buf) == 0)
				ret = TOKEN_DO;
			break;
		case 'e':
			if (strcmp("else", buf) == 0)
				ret = TOKEN_ELSE;
			break;
		case 'f':
			if (strcmp("for", buf) == 0)
				ret = TOKEN_FOR;
			else if (strcmp("function", buf) == 0)
				ret = TOKEN_FUNCTION;
			break;
		case 'i':
			if (strcmp("if", buf) == 0)
				ret = TOKEN_IF;
			break;
		case 'l':
			if (strcmp("let", buf) == 0)
				ret = TOKEN_LET;
			break;
		case 'r':
			if (strcmp("return", buf) == 0)
				ret = TOKEN_RETURN;
			break;
		case 't':
			if (strcmp("throw", buf) == 0)
				ret = TOKEN_THROW;
			else if (strcmp("typeof", buf) == 0)
				ret = TOKEN_TYPEOF;
			break;
		case 'v':
			if (strcmp("var", buf) == 0)
				ret = TOKEN_VAR;
			break;
		case 'w':
			if (strcmp("while", buf) == 0)
				ret = TOKEN_WHILE;
			break;
	}
	return ret;
}

static Token * new_token_identifyer(size_t charnum, char *value, size_t len) {
	Token *tok = token_init();
	if (!tok) return tok;
	TOKEN_SETALLOCED(tok);
	tok->length = len;
	tok->value = value;
	tok->type = get_identifyer_type(value);
	tok->charnum = charnum;
	return tok;
}

static Token * new_token_number(size_t charnum, char *value, size_t len) {
	Token *tok = token_init();
	if (!tok) return tok;
	TOKEN_SETALLOCED(tok);
	tok->length = len;
	tok->value = value;
	tok->charnum = charnum;
	tok->type = TOKEN_NUMERIC;
	return tok;
}

static char * alloc_string(cache *stream, int start, size_t *len) {
	size_t size = 128;
	int ch;
	size_t i;
	char *buf = (char *) malloc(size);
	int skip = 0;
	if (!buf) {
		cache_step_back(stream);
		return NULL;
	}
	*len = 0;
	buf[0] = (char) start;
	for (i=1; ; i++) {
		ch = cache_getc(stream);
		if (i+4 > size) {
			size*=2;
			buf = realloc(buf, size);
			if (!buf) {
				cache_step_backcount(stream, i);
				return NULL;
			}
		}
		buf[i] = (char) ch;

		if (skip) {
			skip = 0;
		} else if (ch == '\\') {
			skip = 1;
		} else if (ch == start) {
			break; // found it
		} else if (ch < 0) {
			i--;
			cache_step_back(stream);
			break; // error, just end the string early
		}
	}
	buf[i+1] = 0;
	*len = i+1;
	buf = realloc(buf, i+3);
	if (!buf) {
		cache_step_backcount(stream, i);
		return NULL;
	}
	return buf;
}

static Token * new_token_string(cache *stream, int start, size_t charnum) {
	Token *tok = token_init();
	if (!tok) return tok;
	tok->value = alloc_string(stream, start, &tok->length);
	if (!tok->value) {
		free(tok);
		return NULL;
	}
	TOKEN_SETALLOCED(tok);
	tok->charnum = charnum;
	switch (start) {
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


static char * alloc_line_comment(cache *stream, size_t *len) {
	size_t i;
	int ch = '/';
	size_t size = 90;
	char *buf = (char *) malloc(size);
	if (!buf) return NULL;
	buf[0] = ch;
	buf[1] = ch;
	for (i=2; ch!='\n' && ch > 0; i++) {
		ch = cache_getc(stream);
		if (i+3 > size) {
			size += 16;
			buf = (char *) realloc(buf, size);
			if (!buf) return NULL;
		}
		buf[i] = ch;
	}
	if (ch < 0) {
		// did not get everything, but save what we did get and step cache back
		// to prev position before error
		cache_step_back(stream);
		i--;
	}
	buf = (char *) realloc(buf, i+3);
	if (!buf) return NULL;
	*len = i;
	buf[i] = '\x00';
	return buf;
}

static Token * new_token_line_comment(cache *stream, size_t charnum) {
	Token *tok = token_init();
	if (!tok) return tok;
	tok->value = alloc_line_comment(stream, &tok->length);
	if (!tok->value) {
		free(tok);
		return NULL;
	}
	TOKEN_SETALLOCED(tok);
	tok->charnum = charnum;
	tok->type = TOKEN_LINE_COMMENT;
	return tok;
}

static char * alloc_multi_line_comment(cache *stream, size_t *len) {
	size_t i;
	int prev = '/';
	int ch = '*';
	size_t size = 90;
	char *buf = (char *) malloc(size);
	if (!buf) return NULL;
	buf[0] = (char) prev;
	buf[1] = (char) ch;
	for (i=2; ; i++) {
		ch = cache_getc(stream);
		if (ch < 0) break;
		if (i+3 > size) {
			size += 16;
			buf = (char *) realloc(buf, size);
			if (!buf) return NULL;
		}
		buf[i] = ch;
		if (prev == '*' && ch == '/') break;
		prev = ch;
	}
	if (ch < 0) {
		// did not finish, but save what we have and restore cache
		cache_step_back(stream);
		i--;
	}
	buf = (char *) realloc(buf, i+3);
	if (!buf) return NULL;
	*len = i+1;
	buf[i+1] = '\x00';
	return buf;

}

static char * alloc_regex(cache *stream, size_t *len) {
	size_t i;
	int skip = 0;
	int in_square = 0;
	int end_slash = 0;
	size_t size = 64;
	char *buf = (char *) malloc(size);
	int ch = '/';
	if (!buf) return NULL;
	buf[0] = ch;
	for (i=1; ch > 0; i++) {
		ch = cache_getc(stream);
		if (i+3 > size) {
			size += 16;
			buf = (char *) realloc(buf, size);
			if (!buf) return NULL;
		}
		buf[i] = ch;
		if (end_slash) {
			// flags
			if (! is_alpha(ch)) {
				cache_step_back(stream);
				break;
			}
		} else if (in_square) {
			// trying to escape the square...
			if (skip) {
				skip = 0;
			}else if (ch == '\\') {
				skip=1;
			}else if (ch == ']') {
				in_square = 0;
			}
		}else {
			// looking for ending /
			if (skip) {
				skip = 0;
			}else if (ch == '[') {
				in_square = 1;
			}else if (ch == '\\') {
				skip = 1;
			}else if (ch == '/') {
				end_slash = 1;
			}else {
				skip = 0;
			}

		}
	}
	if (ch < 0) {
		// did not finish, but save what we have and restore cache
		cache_step_back(stream);
		i--;
	}
	buf = (char *) realloc(buf, i+3);
	if (!buf) return NULL;
	*len = i;
	buf[i] = '\x00';
	return buf;

}

static Token * new_regex(cache *stream, size_t charnum) {
	Token *tok = token_init();
	if (!tok) return tok;
	tok->value = alloc_regex(stream, &tok->length);
	if (!tok->value) {
		free(tok);
		return NULL;
	}
	TOKEN_SETALLOCED(tok);
	tok->charnum = charnum;
	tok->type = TOKEN_REGEX;
	return tok;
}


static Token * new_token_multi_line_comment(cache *stream, size_t charnum) {
	Token *tok = token_init();
	if (!tok) return tok;
	tok->value = alloc_multi_line_comment(stream, &tok->length);
	if (!tok->value) {
		free(tok);
		return NULL;
	}
	TOKEN_SETALLOCED(tok);
	tok->charnum = charnum;
	tok->type = TOKEN_MULTI_LINE_COMMENT;
	return tok;
}

#define SIMPLE_TOKEN(value, name) new_token_static(value, name, sizeof(value)-1, charnum);
static Token * scan_token(cache *stream, size_t prev_type) {
	size_t charnum = cache_getcharnum(stream);
	int ch = cache_getc(stream);
	Token *tok = NULL;
	if (is_alpha(ch) || ch == '_' || ch == '$') {
		size_t len;
		char *buf = alloc_identifyer(stream, ch, &len);
		if (!buf) {
			return NULL;
		}
		tok = new_token_identifyer(charnum, buf, len);
		if (!tok) {
			free(buf);
			return NULL;
		}
		return tok;
	} else if (is_numeric(ch)) {
		size_t len;
		char *buf = alloc_numeric(stream, ch, &len);
		if (!buf) {
			return NULL;
		}
		tok = new_token_number(charnum, buf, len);
		if (!tok) {
			free(buf);
			return NULL;
		}
		return tok;
	}
	switch (ch) {
		// simple single characters
		case '\r':
			tok = SIMPLE_TOKEN("\r", TOKEN_CARRAGE_RETURN);
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
			if (ch == '?') {
				tok = SIMPLE_TOKEN("??", TOKEN_NULL_COALESCING);
			}else {
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("?", TOKEN_QUESTIONMARK);
			}
			break;
		case '/':
			ch = cache_getc(stream); // /X
			if (ch == '/') {
				tok = new_token_line_comment(stream, charnum);
			}else if (ch == '*') {
				tok = new_token_multi_line_comment(stream, charnum);
			}else if (prev_type != TOKEN_VARIABLE
				&& prev_type != TOKEN_NUMERIC && prev_type != TOKEN_CLOSE_PAREN
				&& prev_type != TOKEN_CLOSE_BRACE
			) {
				cache_step_back(stream);
				tok = new_regex(stream, charnum);
			}else if (ch == '=') {
				tok = SIMPLE_TOKEN("/=", TOKEN_DIVIDE_ASSIGN);
			}else{
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("/", TOKEN_DIVIDE);
			}
			break;
		case '=':
			ch = cache_getc(stream);
			if (ch == '=') {
				ch = cache_getc(stream);
				if (ch == '=') {
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
			if (ch == '-') {
				tok = SIMPLE_TOKEN("--", TOKEN_DECREMENT);
			} else if (ch == '=') {
				tok = SIMPLE_TOKEN("-=", TOKEN_MINUS_EQUAL);
			} else{
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("-", TOKEN_SUBTRACT);
			}
			break;
		case '%':
			ch = cache_getc(stream);
			if (ch == '=') {
				tok = SIMPLE_TOKEN("%=", TOKEN_MOD_EQUAL);
			} else{
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("%", TOKEN_MOD);
			}
			break;
		case '*':
			ch = cache_getc(stream);
			if (ch == '*') {
				tok = SIMPLE_TOKEN("**", TOKEN_EXPONENT);
			} else if (ch == '=') {
				tok = SIMPLE_TOKEN("*=", TOKEN_MULTIPLY_ASSIGN);
			} else{
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("*", TOKEN_MULTIPLY);
			}
			break;
		case '+':
			ch = cache_getc(stream);
			if (ch == '+') {
				tok = SIMPLE_TOKEN("++", TOKEN_INCREMENT);
			} else if (ch == '=') {
				tok = SIMPLE_TOKEN("+=", TOKEN_PLUS_EQUAL);
			} else{
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("+", TOKEN_ADD);
			}
			break;
		case '^':
			ch = cache_getc(stream);
			if (ch == '=') {
				tok = SIMPLE_TOKEN("^=", TOKEN_BITWISE_XOR_ASSIGN);
			} else{
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("^", TOKEN_BITWISE_XOR);
			}
			break;
		case '|':
			ch = cache_getc(stream);
			if (ch == '=') {
				tok = SIMPLE_TOKEN("|=", TOKEN_BITWISE_OR_ASSIGN);
			} else if (ch == '|') {
				tok = SIMPLE_TOKEN("||", TOKEN_LOGICAL_OR);
			} else{
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("|", TOKEN_BITWISE_OR);
			}
			break;
		case '&':
			ch = cache_getc(stream);
			if (ch == '=') {
				tok = SIMPLE_TOKEN("&=", TOKEN_BITWISE_AND_ASSIGN);
			} else if (ch == '&') {
				tok = SIMPLE_TOKEN("&&", TOKEN_LOGICAL_AND);
			} else{
				cache_step_back(stream);
				tok = SIMPLE_TOKEN("&", TOKEN_BITWISE_AND);
			}
			break;
		case '<':
			ch = cache_getc(stream); // <X
			if (ch == '=') {
				tok = SIMPLE_TOKEN("<=", TOKEN_LESSTHAN_OR_EQUAL);
			} else if (ch == '<') {
				ch = cache_getc(stream); // <<X
				if (ch == '=') {
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
			if (ch == '=') {
				tok = SIMPLE_TOKEN(">=", TOKEN_GREATERTHAN_OR_EQUAL);
			} else if (ch == '>') {
				ch = cache_getc(stream); // >>X
				if (ch == '=') {
					tok = SIMPLE_TOKEN(">>=", TOKEN_BITSHIFT_RIGHT_ASSIGN);
				} else if (ch == '>') {
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

static void * gettokens(void *in) {
	Thread_params *t = (Thread_params *) in;
	int fd = *(int *) t->input;
	free(t->input);
	List *tl = (List *) t->output;
	free(t);

	size_t prev_type;
	Token *token = NULL;

	if (fd < 0) {
		list_status_set_flag(tl, LIST_HALT_CONSUMER | LIST_PRODUCER_FIN);
		return NULL;
	}

	cache * stream = cache_init(128, fd);
	if (!stream) {
		list_status_set_flag(tl,
		  LIST_MEMFAIL | LIST_PRODUCER_FIN | LIST_HALT_CONSUMER);
		return NULL;
	}

	bool status = true;
	bool eof = false;
	while (status && !eof) {
		token = scan_token(stream, prev_type);
		if (token != NULL) {
			switch (token->type) {
			case TOKEN_EOF:
				eof = true;
				break;
			case TOKEN_TAB:
			case TOKEN_SPACE:
			case TOKEN_NEWLINE:
			case TOKEN_CARRAGE_RETURN:
				break;
			default:
				prev_type = token->type;
				break;
			}
			status = list_append_block(tl, token);
		} else {
			status = LIST_PRODUCER_CONTINUE(list_status_set_flag(tl, LIST_MEMFAIL));
		}
	}

	cache_destroy(stream);
	// must set a status <0 to indicate completion
	list_producer_fin(tl);
	return NULL;
}

List *token_list_new(bool locked) {
	return list_new(&token_destroy, locked);
}

List * tokenizer_start_thread(int fd) {
	if (fd < 0) {
		return NULL;
	}
	List *list = token_list_new(true);
	if (!list) {
		return NULL;
	}

	Thread_params *t = (Thread_params *)malloc(sizeof(Thread_params));
	if (!t) {
		list_destroy(list);
		return NULL;
	}

	t->input = malloc(sizeof(int));
	if (!t->input) {
		list_destroy(list);
		free(t);
		return NULL;
	}

	*(int *)t->input = fd;
	t->output = (void *) list;

	if (pthread_create(&list->thread->tid, &list->thread->attr, gettokens, (void *) t) != 0) {
		fprintf(stderr, "[!!] pthread_create failed\n");
		list_destroy(list);
		return NULL;
	}
	return list;
}
