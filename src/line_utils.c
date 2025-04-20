#include <stdlib.h>
#include <stdio.h>
#include "line_utils.h"

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
