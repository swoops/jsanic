#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include "list.h"
#include "lines.h"
#include "tokenizer.h"

typedef enum {
	LINE_FOR, LINE_IF, LINE_WHILE, LINE_NONE, LINE_ELSE, LINE_INVALID,
} LineType;

typedef struct {
	List *tokens;
	LineType type;
	size_t num;
	int indent;
	size_t char_len;

	size_t cnt_logic, cnt_comma,  cnt_ternary;
} Line;

#define line_dec_indent(line) (line->indent && line->indent--)
#define line_inc_indent(line) (line->indent++)

tokentype line_peek_last_type(Line *line);
bool line_ends_with_type(Line *line);
