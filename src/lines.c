#include <stdlib.h>
#include <stdio.h>
#include "tokenizer.h"
#include "lines.h"
#include "threads.h"

typedef enum {
	LRET_HALT_ERR,
	LRET_HALT,
	LRET_END,
	LRET_CONTINUE,
} lineret;

static inline void set_line_has(Line *line, tokentype t) {
	switch (t) {
	case TOKEN_COMMA:
		line->cnt_comma++;
		break;
	case TOKEN_LOGICAL_OR:
	case TOKEN_LOGICAL_AND:
		line->cnt_logic++;
		break;
	case TOKEN_QUESTIONMARK:
		line->cnt_ternary++;
		break;
	default:
		break;
	}
}

// return false means you should halt
static inline bool line_append(Line *line, Token *token) {
	if (token) {
		set_line_has (line, token->type); \
		return list_append_block (line->tokens, token);
	}
	return false;
}

#define LINE_APPEND(line, token) { \
	if (!line_append (line, token)) { \
		return LRET_HALT_ERR; \
	} \
}

static void line_destroy(Line *l) {
	if (l) {
		list_destroy(l->tokens);
	}
}

static tokentype line_peek_last_type(Line *line) {
	Token *tok = (Token *) list_peek_tail(line->tokens);
	if (!tok) {
		return TOKEN_NONE;
	}
	return tok->type;
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

static lineret append_until_paren_fin(List *tokens, Line *line) {
	Token *tok = token_list_dequeue(tokens);

	LINE_APPEND (line, tok)
	if (tok->type != TOKEN_OPEN_PAREN) {
		return LRET_CONTINUE;
	}

	if (token_list_consume_white_peek(tokens) == TOKEN_FUNCTION) {
		return LRET_END;
	}

	size_t depth = 1; // appended the first open paren already
	while ((tok = token_list_dequeue(tokens)) != NULL) {
		switch (tok->type) {
		case TOKEN_CARRAGE_RETURN:
			// remove
			tokens->free(tok);
			break;
		case TOKEN_SEMICOLON:
		case TOKEN_COMMA:
			LINE_APPEND (line, tok)
			token_list_consume_white_peek(tokens);
			LINE_APPEND (line, token_space ());
			break;
		case TOKEN_OPEN_PAREN:
			LINE_APPEND (line, tok);
			depth++;
			break;
		case TOKEN_CLOSE_PAREN:
			LINE_APPEND (line, tok);
			if (--depth == 0) {
				return LRET_END;
			}
			depth--;
			break;
		case TOKEN_EOF:
		case TOKEN_NEWLINE:
			// TODO delete newline?
			LINE_APPEND  (line, tok);
			return LRET_END;
		default:
			LINE_APPEND (line, tok);
			break;
		}
	}
	return LRET_CONTINUE;
}

static bool is_valid_op_serpator(tokentype t) {
	switch (t) {
	case TOKEN_MULTI_LINE_COMMENT:
	case TOKEN_CLOSE_PAREN:
	case TOKEN_CLOSE_CURLY:
	case TOKEN_CLOSE_BRACE:
	case TOKEN_DOUBLE_QUOTE_STRING:
	case TOKEN_SINGLE_QUOTE_STRING:
	case TOKEN_TILDA_STRING:
	case TOKEN_NUMERIC:
	case TOKEN_VARIABLE:
		return true;
	default:
		return false;
	}
}

static bool maybe_space_surround(List *tokens, Line *line) {
	if (is_valid_op_serpator (line_peek_last_type (line))) {
		if (!line_append (line, token_space ())) {
			return false;
		}
	}
	LINE_APPEND (line, token_list_dequeue (tokens));
	if (is_valid_op_serpator( token_list_peek_type (tokens))) {
		if (!line_append (line, token_space ())) {
			return false;
		}
	}
	return true;
}

static inline size_t line_length(Line *line) {
	return list_length(line->tokens);
}

static lineret curly_end(List *tokens, Line *line) {
	if (line_length (line)) {
		token_list_snip_white_tail(line->tokens);
		return LRET_END; // newline before CURLY CLOSE
	}

	LINE_APPEND (line, token_list_dequeue(tokens));
	tokentype t = token_list_consume_white_peek(tokens);
	switch (t) {
	case TOKEN_COMMA:
		LINE_APPEND (line, token_list_dequeue(tokens));
		return LRET_CONTINUE;
	case TOKEN_ELSE:
	case TOKEN_WHILE:
		LINE_APPEND (line, token_space ());
		return LRET_CONTINUE;
	default:
		return LRET_CONTINUE;
	}
}

static lineret finish_line(List *tokens, Line *line) {
	tokentype t;
	while ((t = token_list_peek_type (tokens)) != TOKEN_ERROR) {
		switch (t) {
		case TOKEN_QUESTIONMARK:
		case TOKEN_ADD:
		case TOKEN_SUBTRACT:
		case TOKEN_MULTIPLY:
		case TOKEN_DIVIDE:
		case TOKEN_EXPONENT:
		case TOKEN_LOGICAL_OR:
		case TOKEN_LOGICAL_AND:
		case TOKEN_NULL_COALESCING:
		case TOKEN_BITWISE_AND:
		case TOKEN_BITWISE_XOR:
		case TOKEN_BITWISE_OR:
		case TOKEN_BITSHIFT_LEFT:
		case TOKEN_ZERO_FILL_RIGHT_SHIFT:
		case TOKEN_SIGNED_BITSHIFT_RIGHT:
		case TOKEN_MOD:
		case TOKEN_BITWISE_XOR_ASSIGN:
		case TOKEN_MULTIPLY_ASSIGN:
		case TOKEN_DIVIDE_ASSIGN:
		case TOKEN_BITWISE_OR_ASSIGN:
		case TOKEN_BITWISE_AND_ASSIGN:
		case TOKEN_BITSHIFT_LEFT_ASSIGN:
		case TOKEN_BITSHIFT_RIGHT_ASSIGN:
		case TOKEN_MINUS_ASSIGN:
		case TOKEN_MOD_ASSIGN:
		case TOKEN_ASSIGN:
		case TOKEN_ARROW_FUNC:
		case TOKEN_EQUAL_EQUAL:
		case TOKEN_NOT_EQUAL:
		case TOKEN_PLUS_EQUAL:
		case TOKEN_DECREMENT:
		case TOKEN_LESSTHAN_OR_EQUAL:
		case TOKEN_LESSTHAN:
		case TOKEN_GREATER_THAN:
		case TOKEN_GREATERTHAN_OR_EQUAL:
		case TOKEN_EQUAL_EQUAL_EQUAL:
		case TOKEN_NOT_EQUAL_EQUAL:
			if (!maybe_space_surround (tokens, line)) {
				return LRET_HALT_ERR;
			}
			break;
		case TOKEN_SPACE:
			// prevent double spaces
			token_list_consume_white_peek (tokens);
			LINE_APPEND (line, token_space ());
			break;
		case TOKEN_NEWLINE:
		case TOKEN_CARRAGE_RETURN:
			// no need for newline or whitespace at end of lines
			token_list_consume_white_peek (tokens);
			return LRET_END;
		case TOKEN_OPEN_PAREN:
			append_until_paren_fin(tokens, line);
			break;
		case TOKEN_CLOSE_CURLY:
			return curly_end(tokens, line);
		case TOKEN_OPEN_CURLY:
			if (line_peek_last_type(line) == TOKEN_CLOSE_PAREN) {
				LINE_APPEND (line, token_space ());
			}
			LINE_APPEND (line, token_list_dequeue(tokens));
			break;
		case TOKEN_EOF:
		case TOKEN_COMMA:
		case TOKEN_SEMICOLON:
			LINE_APPEND (line, token_list_dequeue(tokens));
			break;
		default:
			LINE_APPEND (line, token_list_dequeue(tokens));
			break;
		}
	}
	return LRET_END;
}

static lineret make_else_line(List *tokens, Line *line) {
	Token *tok = token_list_dequeue(tokens);
	if (!tok || tok->type != TOKEN_ELSE) {
		return LRET_CONTINUE;
	}
	LINE_APPEND (line, tok);

	// eat whitespace
	tokentype t = token_list_consume_white_peek(tokens);
	if (t == TOKEN_OPEN_CURLY) {
		LINE_APPEND (line, token_space ());
		LINE_APPEND (line, token_list_dequeue(tokens));
	}
	return true;
}

static lineret make_logic_line(List *tokens, Line *line) {
	// append logic token for,wihle,if, etc
	Token *tok = token_list_dequeue (tokens);
	if (!tok) {
		return LRET_HALT_ERR;
	}

	LINE_APPEND (line, tok);

	switch (tok->type) {
	case TOKEN_FOR:
	case TOKEN_IF:
	case TOKEN_WHILE:
		break;
	default:
		fprintf (stderr, "[!!] This is not a control flow token\n");
		return LRET_CONTINUE;
	}

	// eat whitespace
	tokentype t = token_list_consume_white_peek(tokens);

	if (t == TOKEN_STOP) {
		return LRET_END;
	} else if (t != TOKEN_OPEN_PAREN) {
		line->type = LINE_INVALID;
		return finish_line (tokens, line);
	}
	LINE_APPEND (line, token_space ());

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
		LINE_APPEND (line, token_space ());
		if (!(tok = token_list_dequeue(tokens))) {
			return false;
		}
		LINE_APPEND (line, tok);
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
		make_logic_line(tokens, line);
	case TOKEN_IF:
		line->type = LINE_IF;
		return make_logic_line(tokens, line);
	case TOKEN_WHILE:
		line->type = LINE_WHILE;
		return make_logic_line(tokens, line);
	case TOKEN_ELSE:
		line->type = LINE_ELSE;
		make_else_line(tokens, line);
	default:
		return finish_line(tokens, line);
	}
	return LRET_END;
}

static inline Line *line_new(size_t n, int indent) {
	Line *l = (Line *) malloc(sizeof(Line));
	if (l) {
		if ((l->tokens = token_list_new(false))) {
			l->type = LINE_NONE;
			l->num = n;
			l->indent = indent;
			return l;
		}
		line_destroy(l);
	}
	return NULL;
}

static inline void make_lines(List *tokens, List *lines) {
	size_t n = 0;
	int indent = 0;
	do {
		Line *line = line_new (n++, indent);
		if (!line) {
			fprintf (stderr, "[!!] Failed to alloc\n");
			return;
		}

		lineret ret = fill_line (tokens, line);
		if (!line_length (line)) {
			fprintf (stderr, "[!!] Empty line %ld?\n", line->num);
			line_destroy (line);
		} else if (!list_append_block (lines, line)) {
			return;
		}

		switch (ret) {
		// STOP thread!
		case LRET_HALT_ERR:
			fprintf (stderr, "[!!] Line creation failed, output is incomplete!\n");
		case LRET_HALT:
			return;

		// Keep making lines
		case LRET_CONTINUE:
			fprintf (stderr, "[!!] Unexpected continue in line ret\n");
			// fallthrough
		default:
			break;
		}
	} while (true);
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

List *lines_new() {
	return list_new (&_line_destroy, true);
}

List *lines_creat_start_thread(List *tokens) {
	if (!tokens) {
		return NULL;
	}

	List *lines = lines_new ();
	if (!lines) {
		list_destroy (tokens);
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
		list_destroy (tokens);
		list_destroy (lines);
		free (t);
		return NULL;
	}
	return lines;
}
