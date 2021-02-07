#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "printlines.h"
#include <unistd.h>

static bool put_token(Token *tok, FILE *fp) {
	if (fwrite(tok->value, sizeof(tok->value[0]), tok->length, fp) == tok->length) {
		return true;
	}
	return false;
}

static bool put_newline(FILE *fp) {
	if (fwrite("\n", 1, 1, fp) == 1) {
		return true;
	}
	return false;
}

static bool put_indent(int count, FILE *fp) {
	if (count > 16) {
		count = (count % 15) + 1;
	}
	int i;
	for (i=0; i<count; i++) {
		if (fwrite("\t", 1, 1, fp) != 1) {
			return false;
		}
	}
	return true;
}

static int print_one_line(List *tokens, int indent, FILE *fp) {
	if (!put_indent(indent, fp)) {
		return false;
	}
	Token *t;
	while ((t = list_dequeue_block(tokens)) != NULL) {
		bool ret = put_token(t, fp);
		tokens->free(t);
		if (!ret) {
			return false;
		}
	}
	if (!put_newline(fp)) {
		return false;
	}
	return true;
}

bool printlines(int in, FILE *out) {
	bool ret = true;
	List *lines = lines_fd(in);
	if (!lines) {
		return false;
	}

	Line *l = NULL;
	while (ret && (l = list_dequeue_block(lines)) != NULL) {
		ret = print_one_line(l->tokens, l->indent, out);
		lines->free(l);
	}
	list_destroy(lines);
	if (!ret) {
		fflush(out);
		fprintf(stderr, "Error while writing\n");
	}
	return ret;
}
