#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "tokenizer.h"
#include "lines.h"
#include "printlines.h"

static bool put_token(Token *tok, FILE *fp) {
	if (fwrite(tok->value, sizeof(tok->value[0]), tok->length, fp) == tok->length) {
		return true;
	}
	return false;
}

static bool put_newline(FILE *fp) {
	if (fputc('\n', fp) == EOF) {
		return false;
	}
	return true;
}

static bool put_indent(int count, FILE *fp) {
	if (count > 16) {
		count = (count % 15) + 1;
	}
	int i;
	for (i=0; i<count; i++) {
		if (fputc('\t', fp) == EOF) {
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

bool printlines(List *lines, FILE *fp) {
	bool ret = true;
	if (!lines) {
		return false;
	}

	Line *l = NULL;
	while (ret && (l = list_dequeue_block(lines)) != NULL) {
		ret = print_one_line(l->tokens, l->indent, fp);
		lines->free(l);
	}
	list_destroy(lines);

	if (!ret) {
		fflush(fp);
		fprintf(stderr, "Error while writing\n");
	}

	return ret;
}
