#include <stdlib.h>
#include <stdio.h>
#include "lines.h"
#include "threads.h"

#define line_append_or_ret(line, token) \
	if (!(list_append_block(line->tokens, token))) {\
		return false; \
	}

static void line_destroy(Line *l) {
	if (l) {
		if (l->tokens) {
			list_destroy(l->tokens);
		}
		free(l);
	}
}

static bool append_until_paren_fin(List *tokens, Line *line) {
	Token *tok = NULL;
	size_t depth = 0;
	while ((tok = token_list_dequeue(tokens)) != NULL) {
		line_append_or_ret(line, tok);
		if (tok->type == TOKEN_CLOSE_PAREN) {
			if (depth == 1) {
				return true;
			} else if (depth == 0) {
				return false;
			}
			depth--;
		} else if (tok->type == TOKEN_OPEN_PAREN) {
			depth++;
		}
	}
	return false;
}

static bool line_add_space(Line *line) {
	Token *tok = new_token_static(" ", TOKEN_SPACE, sizeof(" ")-1, 0);
	line_append_or_ret(line, tok);
	return true;
}

static bool curly_end(List *tokens, Line *line) {
	if (list_length(line->tokens)) {
		token_list_snip_white_tail(line->tokens);
		return true; // newline before CURLY CLOSE
	}

	line_append_or_ret(line, token_list_dequeue(tokens));
	tokentype t = token_list_consume_white_peek(tokens);
	switch (t) {
	case TOKEN_COMMA:
		line_append_or_ret(line, token_list_dequeue(tokens));
		return false;
	case TOKEN_ELSE:
	case TOKEN_WHILE:
		line_add_space(line);
		return false;
	default:
		return true;
	}
}

static bool finish_line(List *tokens, Line *line) {
	tokentype t;
	while ((t = token_list_peek_type(tokens)) != TOKEN_ERROR) {
		switch (t) {
		case TOKEN_NEWLINE:
		case TOKEN_CARRAGE_RETURN:
			// no need for newline or whitespace at end of lines
			token_list_consume_white_peek(tokens);
			return true;
		case TOKEN_OPEN_PAREN:
			append_until_paren_fin(tokens, line);
			break;
		case TOKEN_CLOSE_CURLY:
			if (curly_end(tokens, line)) {
				return true;
			}
			break;
		case TOKEN_COMMA:
		case TOKEN_SEMICOLON:
		case TOKEN_OPEN_CURLY:
			line_append_or_ret(line, token_list_dequeue(tokens));
			return true;
		default:
			line_append_or_ret(line, token_list_dequeue(tokens));
			break;
		}
	}
	return false;
}

static bool make_else_line(List *tokens, Line *line) {
	Token *tok = token_list_dequeue(tokens);
	if (!tok || tok->type != TOKEN_ELSE) {
		return false;
	}
	line_append_or_ret(line, tok);

	// eat whitespace
	tokentype t = token_list_consume_white_peek(tokens);
	if (t == TOKEN_OPEN_CURLY) {
		line_add_space(line);
		line_append_or_ret(line, token_list_dequeue(tokens));
	}
	return true;
}

static bool make_logic_line(List *tokens, Line *line) {
	// append logic token for,wihle,if, etc
	Token *tok = token_list_dequeue(tokens);
	if (!tok) {
		return false;
	}
	switch (tok->type) {
	case TOKEN_FOR:
	case TOKEN_IF:
	case TOKEN_WHILE:
		break;
	default:
		fprintf(stderr, "This is not a control flow token\n");
		tokens->free(tok);
		return false;
	}
	line_append_or_ret(line, tok);

	// eat whitespace
	tokentype t = token_list_consume_white_peek(tokens);

	if (t == TOKEN_STOP) {
		return false;
	} else if (t != TOKEN_OPEN_PAREN) {
		line->type = LINE_INVALID;
		return finish_line(tokens, line);
	}
	line_add_space(line);

	// get paren and everything in it
	if (!append_until_paren_fin(tokens, line)) {
		line->type = LINE_NONE;
		return false;
	}

	// remove whitespace
	t = token_list_consume_white_peek(tokens);
	if (t == TOKEN_STOP) {
		return false;
	} else if (t == TOKEN_OPEN_CURLY) {
		// append curly and end line
		line_add_space(line);
		if (!(tok = token_list_dequeue(tokens))) {
			return false;
		}
		line_append_or_ret(line, tok);
	}

	return true;
}

static inline bool fill_line(List *tokens, Line *line) {
	tokentype t = token_list_consume_white_peek(tokens);
	switch (t) {
	case TOKEN_STOP:
		return false;
	case TOKEN_FOR:
		line->type = LINE_FOR;
		if (!make_logic_line(tokens, line)) {
			return false;
		}
		break;
	case TOKEN_IF:
		line->type = LINE_IF;
		if (!make_logic_line(tokens, line)) {
			return false;
		}
		break;
	case TOKEN_WHILE:
		line->type = LINE_WHILE;
		if (!make_logic_line(tokens, line)) {
			return false;
		}
		break;
	case TOKEN_ELSE:
		line->type = LINE_ELSE;
		if (!make_else_line(tokens, line)) {
			return false;
		}
		break;
	default:
		if (!finish_line(tokens, line)) {
			return false;
		}
		break;
	}
	return true;
}

static Line *line_new(size_t n, int indent) {
	Line *l = (Line *) malloc(sizeof(Line));
	if (l) {
		if ((l->tokens = token_list_new(false))) {
			l->type = LINE_NONE;
			l->num = n;
			l->indent = indent;
			return l;
		}
	}
	line_destroy(l);
	return NULL;
}

#define MAXIND 0x70000000 // for overflow reasons
static int adjust_indent(Line *line, int indent) {
	Token *tok = (Token *) list_peek_tail(line->tokens);
	switch (line->type) {
	default:
		break;
	case LINE_FOR:
	case LINE_WHILE:
	case LINE_IF:
		if (tok->type == TOKEN_CLOSE_PAREN && indent < MAXIND) {
			return indent+1;
		}
	}

	switch (tok->type) {
	case TOKEN_OPEN_CURLY:
		if (indent < MAXIND) {
			return indent+1;
		}
		return indent;
	case TOKEN_CLOSE_CURLY:
		if (line->indent > 0) {
			line->indent--;
		}
		if (indent > 0) {
			return indent-1;
		}
	default:
		break;
	}
	return indent;
}

static inline void make_lines(List *tokens, List *lines) {
	Line *line = NULL;
	size_t n = 0;
	int indent = 0;
	while ((line = line_new(n++, indent))) {
		if (!fill_line(tokens, line)) {
			line_destroy(line);
			break;
		}
		indent = adjust_indent(line, indent);
		if (!list_append_block(lines, line)) {
			break;
		}
	}
}

static void *getlines(void *in) {
	Thread_params *t = (Thread_params *) in;
	List *tokens = (List *)t->input;
	List *lines = (List *)t->output;
	free (t);
	make_lines(tokens, lines);
	list_destroy(tokens);
	list_producer_fin(lines);
	return NULL;
}

static void _line_destroy(void *l) {
	line_destroy((Line *) l);
}

List *lines_creat_start_thread(List *tokens) {
	if (!tokens) {
		return NULL;
	}

	List *lines = list_new(&_line_destroy, true);
	if (!lines) {
		list_destroy(tokens);
		return NULL;
	}

	Thread_params *t = (Thread_params *)malloc(sizeof(Thread_params));
	if (!t) {
		list_destroy(tokens);
		list_destroy(lines);
		return NULL;
	}

	t->input = (void *)tokens;
	t->output = (void *)lines;

	if (pthread_create(&lines->thread->tid, &lines->thread->attr, getlines, (void *) t) != 0) {
		fprintf(stderr, "[!!] pthread_create failed\n");
		list_destroy(tokens);
		list_destroy(lines);
		free(t);
		return NULL;
	}
	return lines;
}

List *lines_fd(int fd) {
	List *l = NULL;
	List *tl = tokenizer_start_thread(fd);
	if (tl) {
		 l = lines_creat_start_thread(tl);
	}
	return l;
}
