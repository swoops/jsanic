#include <stdlib.h>
#include <stdio.h>
#include "tokenizer.h"
#include "lines.h"
#include "threads.h"
#include "line_utils.h"

typedef enum {
	LRET_HALT_ERR,
	LRET_HALT,
	LRET_END_INC_INDENT,
	LRET_END_DEC_INDENT,
	LRET_END,
	LRET_CONTINUE,
} lineret;

#define LINE_APPEND(line, token) { \
	if (!line_append (line, token)) { \
		return LRET_HALT_ERR; \
	} \
}

static inline lineret handle_curly_close(List *tokens, Line *line) {
	if (line->indent > 0) {
		line->indent--;
	}

	tokentype t = token_list_peek_type (tokens);

	switch (t) {
	case TOKEN_COMMA:
	case TOKEN_SEMICOLON:
		LINE_APPEND (line, token_list_dequeue (tokens));
		break;
	default:
		break;
	}
	return LRET_END_DEC_INDENT;
}

static inline lineret fill_line(List *tokens, Line *line) {
	for (;;) {
		Token *tok = token_list_dequeue (tokens);
		if (!tok || tok->type == TOKEN_STOP) {
			return LRET_HALT;
		}

		switch (tok->type) {
		case TOKEN_NEWLINE:
			// split on newline
			tokens->free (tok);
			return LRET_END;
		case TOKEN_SEMICOLON:
		case TOKEN_OPEN_CURLY:
			LINE_APPEND (line, tok);
			return LRET_END_INC_INDENT;
		case TOKEN_CLOSE_CURLY:
			LINE_APPEND (line, tok);
			return handle_curly_close (tokens, line);

		case TOKEN_EOF:
			return LRET_END;
		default:
			LINE_APPEND (line, tok);
			break; // continue making the line
		}
	}
}

static inline void make_lines(List *tokens, List *lines) {
	size_t n = 0;
	int indent = 0;
	do {
		Line *line = line_new (n++, indent);
		if (!line) {
			return;
		}

		lineret ret = fill_line (tokens, line);
		if (!list_append_block (lines, line)) {
			return;
		}

		switch (ret) {
		// STOP thread!
		case LRET_HALT_ERR:
			fprintf (stderr, "[!!] Got halt??? Developer mistake %s:%d\n", __FUNCTION__, __LINE__);
		case LRET_END_INC_INDENT:
			indent++;
			break;
		case LRET_END_DEC_INDENT:
			if (indent) {
				indent--;
			}
			break;
		case LRET_HALT:
			return;

		// Keep making lines
		case LRET_CONTINUE:
			fprintf (stderr, "[!!] Developer mistake %s:%d\n", __FUNCTION__, __LINE__);
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
	make_lines (tokens, lines);
	list_destroy (tokens);
	list_producer_fin (lines);
	return NULL;
}

List *ugly_lines_start_thread(List *tokens) {
	if (!tokens) {
		return NULL;
	}

	List *lines = lines_list_new ();
	if (!lines) {
		list_destroy (tokens);
		return NULL;
	}

	Thread_params *t = (Thread_params *)malloc (sizeof (Thread_params));
	if (!t) {
		list_destroy (tokens);
		list_destroy (lines);
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
