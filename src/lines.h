#include "list.h"
typedef enum {
	LINE_FOR, LINE_IF, LINE_WHILE, LINE_NONE, LINE_ELSE, LINE_INVALID,
} LineType;

typedef struct {
	List *tokens;
	LineType type;
	size_t num;
	int indent;

	size_t cnt_logic, cnt_comma,  cnt_ternary;
} Line;

List *lines_new();

/*
 * consumer of tokens, producer of a line
*/
List *lines_creat_start_thread(List *tokens);
