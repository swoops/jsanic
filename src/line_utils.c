#include <stdlib.h>
#include <stdio.h>
#include "line_utils.h"

tokentype line_peek_last_type(Line *line) {
	Token *tok = (Token *) list_peek_tail (line->tokens);
	if (!tok) {
		return TOKEN_NONE;
	}
	return tok->type;
}
