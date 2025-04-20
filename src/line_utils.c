#include <stdlib.h>
#include <stdio.h>
#include "line_utils.h"

static inline void update_line_stats(Line *line, Token *tok, bool added) {
	int add = added? 1: -1;
	switch (tok->type) {
	case TOKEN_COMMA:
		line->cnt_comma += add;
		break;
	case TOKEN_LOGICAL_OR:
	case TOKEN_LOGICAL_AND:
		line->cnt_logic += add;
		break;
	case TOKEN_QUESTIONMARK:
		line->cnt_ternary += add;
		break;
	default:
		break;
	}

	if (added) {
		line->char_len += tok->length;
	} else {
		line->char_len -= tok->length;
	}
}

static inline Token *line_dequeue_token(Line *line) {
	Token *tok = (Token *)list_dequeue_block (line->tokens);
	if (tok) {
		update_line_stats (line, tok, false);
	}
	return tok;
}

static inline Token *token_space() {
	static Token fake_space = {
		.value = " ",
		.length = 1,
		.charnum = 0,
		.type = TOKEN_SPACE,
		.fake = true,
		.isalloc = false,
	};
	return &fake_space;
}


// return false means you should halt
bool line_append(Line *line, Token *token) {
	if (token) {
		update_line_stats (line, token, true);
		if (list_append_block (line->tokens, token)) {
			return true;
		}
	}
	return false;
}

bool line_append_space(Line *line) {
	return line_append (line, token_space ());
}

// switch token from line in to line out, returns token type
tokentype line_switch(Line *in, Line *out) {
	Token *tok = line_dequeue_token (in);
	if (!tok) {
		return TOKEN_STOP;
	}

	tokentype t = tok->type;
	if (!line_append (out, tok)) {
		return TOKEN_ERROR;
	}

	return t;
}


void line_free(Line *l) {
	if (l) {
		list_destroy (l->tokens);
		free (l); // right
	}
}

Line *line_new(size_t n, int indent) {
	Line *l = (Line *)malloc (sizeof (Line));
	if (l) {
		if ((l->tokens = token_list_new (false))) {
			l->type = LINE_NONE;
			l->num = n;
			l->indent = indent;
			return l;
		}
		line_free (l);
	}
	return NULL;
}

tokentype line_peek_last_type(Line *line) {
	Token *tok = (Token *)list_peek_tail (line->tokens);
	if (!tok) {
		return TOKEN_NONE;
	}
	return tok->type;
}
