#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "printlines.h"
#include <unistd.h>

static bool put_token(Token *tok, int fd) {
	if (write(fd, tok->value, tok->length) == tok->length) {
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

static void put_indent(int count, int fd) {
	if (count > 16) {
		count = (count % 15) + 1;
	}
	int i;
	for (i=0; i<count; i++) {
		write(fd, "  ", 2);
	}
}

static int print_one_line(List *tokens, int fd) {
	Token *t;
	while ((t = list_dequeue_block(tokens)) != NULL) {
		bool ret = put_token(t, fd);
		tokens->free(t);
		if (!ret) {
			return false;
		}
	}
	if (!put_newline(fd)) {
		return false;
	}
	return true;
}

bool printlines(int in, int out) {
	bool ret = false;
	List *lines = lines_fd(in);
	if (lines) {
		Line *l = NULL;
		while ((l = list_dequeue_block(lines)) != NULL) {
			put_indent(l->indent, out);
			ret = print_one_line(l->tokens, out);
			lines->free(l);
			if (!ret) {
				break;
			}
		}
	}
	list_destroy(lines);
	return ret;
}
