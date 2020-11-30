#include "tokenizer.h"
typedef enum {
	LINE_FOR, LINE_IF, LINE_WHILE, LINE_NONE, LINE_ELSE, LINE_INVALID,
} LineType;

typedef struct {
	List *tokens;
	LineType type;
	size_t num;
	int indent;
} Line;

/*
 * consumer of tokens, producer of lines
*/
List *lines_creat_start_thread(List *tokens);
List *lines_fd(int fd);
