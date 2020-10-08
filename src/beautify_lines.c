#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "beautify_lines.h"
#include <unistd.h>

typedef struct {
	char *ind_str;	// tab, spaces, etc
	size_t ind_max;	// max indent length
	size_t ind_len;	// strlen(str_indent)
	List *ind_stack; // how far to indent, when to un-indent
	int fd;
	size_t maxlen;
} Bstate;

static inline void bstate_free(Bstate *st){
	if (st) {
		list_destroy(st->ind_stack);
		free(st->ind_str);
		free(st);
	}
}

static Bstate *bstate_new(int fd, char *ind_str) {
	if (fd < 0 || !ind_str) {
		return NULL;
	}
	Bstate *st = (Bstate *)malloc(sizeof(Bstate));
	if (!st) {
		return NULL;
	}
	if (!(st->ind_stack = list_new(&free, false))) {
		bstate_free(st);
		return NULL;
	}
	if (!(st->ind_str = strdup(ind_str))) {
		bstate_free(st);
		return NULL;
	}
	st->ind_len = strlen(ind_str);
	st->ind_max = 16;
	st->maxlen = 80;
	return st;
}

static void bstate_indent_check_pop(tokentype t, Bstate *st) {
	List *l = st->ind_stack;
	Token *head = list_peek_head_block(l);
	if (head) {
		switch (head->type) {
		case TOKEN_OPEN_CURLY:
			if (t == TOKEN_CLOSE_CURLY) {
				list_destroy_head(l);
			}
			break;
		default:
			break;
		}
	}
}

static bool put_indent(Bstate *st) {
	size_t len = st->ind_len;
	size_t count = list_length(st->ind_stack);
	if (count >= st->ind_max-1) {
		count = (count % st->ind_max) + 1;
	}
	while (count-- > 0) {
		if (write(st->fd, st->ind_str, len) != len) {
			return false;
		}
	}
	return true;
}

static bool bstate_push_indent(tokentype t, Bstate *st) {
	tokentype *type = (tokentype *) malloc(sizeof(tokentype));
	if (!type) {
		return false;
	}
	if (!list_push(st->ind_stack, type)){
		return false;
	}
	return true;
}

static bool put_token(Token *tok, int fd) {
	if (write(fd, tok->value, tok->length) == tok->length) {
		return true;
	}
	return false;
}

static bool put_space(int fd) {
	if (write(fd, " ", 1) == 1) {
		return true;
	}
	return false;
}

static bool put_newline(int fd) {
	if (write(fd, "\n", 1) == 1) {
		return true;
	}
	return false;
}

static bool put_next_token_if(Line *l, Bstate *st, tokentype t) {
	Token *tok = token_list_dequeue(l->tokens);
	if (tok && tok->type == t) {
		put_token(tok, st->fd);
		l->tokens->free(tok);
		return true;
	}
	return false;
}

static bool line_too_big(Line *l, Bstate *st) {
	if (st->maxlen && list_length(l->tokens) > st->maxlen) {
		return true;
	}
	return false;
}

static tokentype put_rest_of_line(Line *l, Bstate *st) {
	Token *tok = NULL;
	tokentype t;
	while ((tok = token_list_dequeue(l->tokens)) != NULL) {
		put_token(tok, st->fd);
		t = tok->type;
		l->tokens->free(tok);
	}
	return t;
}

static bool print_till_end(Line *l, Bstate *st) {
	tokentype t = put_rest_of_line(l, st);
	bstate_push_indent(t, st);
	return true;
}

static bool print_parens(Line *l, Bstate *st) {
	if (!put_next_token_if(l, st, TOKEN_OPEN_PAREN)) {
		return print_till_end(l, st);
	}
	return print_till_end(l, st);
}

static bool print_for_line(Line *l, Bstate *st) {
	// print for, consume any space after
	if (
	  !put_next_token_if(l, st, TOKEN_FOR)
	  || token_list_consume_white_peek(l->tokens) != TOKEN_OPEN_PAREN
	) {
		return print_till_end(l, st);
	}
	put_space(st->fd);
	print_parens(l, st);
	return true;
}


static bool print_one_lines(Line *l, Bstate *st) {
	bool ret = false;
	put_indent(st);
	switch (l->type) {
	case LINE_FOR: 
		ret = print_for_line(l, st);
		break;
	default:
		ret = print_till_end(l, st);
		break;
	}
	return ret;
}

static void print_lines(List *lines, Bstate *st){
	Line *l = NULL;
	while ((l = list_dequeue_block(lines)) != NULL) {
		print_one_lines(l, st);
		put_newline(st->fd);
		lines->free(l);
	}
}

void beautify_fd(int fd, int out) {
	List *lines = lines_fd(fd);
	Bstate *st = bstate_new(out, "\t");
	if (lines && st) {
		print_lines(lines, st);
	}
	list_destroy(lines);
	bstate_free(st);
}
