#include "tokenizer.h"
typedef enum {
	LINE_FOR, LINE_IF, LINE_WHILE, LINE_NONE, LINE_INVALID,
} LineType;

typedef struct {
	List *tokens;
	LineType type;
	size_t num;
} Line;

/*
 * consumer of tokens, producer of lines
*/
List *lines_creat_start_thread(List *tokens);
List *auto_start_lines(int fd);
void print_lines(List *lines);
