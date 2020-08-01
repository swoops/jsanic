#include "token_output.h"
#include "errorcodes.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

typedef enum BEAUTY_STATES {
	B_START,
	B_INLINE,
	B_DONE
}BEAUTY_STATES;

typedef struct Beauty {
	BEAUTY_STATES current;
	size_t depth, depth_reset;
	char *tab;
} Beauty;


static int token_type_name(size_t type, char **ret);

int cleanup_pthread_for_tokens(pthread_t *tid, pthread_attr_t *attr){
	void * status;
	if ( pthread_join(*tid, &status) != 0 ){
		fprintf(stderr, "[!!] Join failed\n");
		return ERROR_PTHREAD_FAIL;
	}

	if ( pthread_attr_destroy(attr) != 0 ){
		fprintf(stderr, "Failed to destroy pthread attr\n");
		return ERROR_PTHREAD_FAIL;
	}
	return 0;
}


int threaded_printer(int fd, void *func, void *args){
	if (!func) {
		return ERROR;
	}
	pthread_t tid;
	pthread_attr_t attr;
	if (pthread_attr_init(&attr) != 0){
		perror("Failed pthread_attr_init: ");
		return ERROR;
	}

	List *list = tokenizer_start_thread(&tid, &attr, fd);
	if (!list) {
		return ERROR;
	}

	int (*fun_ptr)(List *, void *) = func;
	int ret = (fun_ptr)(list, args);
	if (ret == EOF) {
		ret = 0;
	}
	cleanup_pthread_for_tokens(&tid, &attr);
	list_destroy(list);
	return ret;
}

int token_output_typeids(){
	size_t i;
	int ret = 0;
	char *name;

	for (i=0; ret==0; i++){
		ret = token_type_name(i, &name);
		if ( !ret ){
			printf("%3ld   :  %s\n", i, name);
		}
	}
	return 0;
}

static char * token_printable(Token *tok){
	char * ret;
	switch (tok->type) {
		case TOKEN_CARRAGE_RETURN:
			ret = "\\r";
			break;
		case TOKEN_TAB:
			ret = "\\t";
			break;
		case TOKEN_SPACE:
			ret = "<<space>>";
			break;
		case TOKEN_NEWLINE:
			ret = "\\n";
			break;
		case TOKEN_EOF:
			ret = "EOF";
			break;
		default:
			ret = tok->value;
			break;
	}
	return ret;
}


static inline void bhelp_dec_depth(Beauty *state){
	if ( state->depth_reset ){
		if ( state->depth > 3 ) {
			state->depth--;
		} else {
			state->depth = 10;
			state->depth_reset--;
		}
	}else {
		if ( state->depth > 0 ) {
			state->depth--;
		}
	}
}

static inline void bhelp_inc_depth(Beauty *state){
	if ( state->depth < 10 ) {
		state->depth++;
	} else {
		state->depth = 3;
		state->depth_reset++;
	}
}

static void bhelp_add_tabs(Beauty *state){
	size_t i;
	for (i=0; i<state->depth; i++){
		printf("%s", state->tab);
	}
}

static void curly_close_add_newline(List *list, Beauty *state){
	size_t t = token_list_consume_white_peek(list);
	switch (t){
		case TOKEN_WHILE:
		case TOKEN_ELSE:
		case TOKEN_COMMA:
		case TOKEN_SEMICOLON:
			state->current = B_INLINE;
			break;
		default:
			state->current = B_START;
			printf("\n");
	}
}

// beautifyers
static void beautify_start(List *list, Beauty *state){
	Token * token = list_dequeue_block(list);
	if (!token) {
		// end of token list for some reason
		state->current = B_DONE;
		return;
	}
	switch (token->type){
		case TOKEN_EOF:
			state->current = B_DONE;
			break;
		case TOKEN_SPACE:
		case TOKEN_NEWLINE:
		case TOKEN_TAB:
			break;
		case TOKEN_OPEN_CURLY:
			bhelp_add_tabs(state);
			printf("{\n");
			bhelp_inc_depth(state);
			break;
		case TOKEN_CLOSE_CURLY:
			bhelp_dec_depth(state);
			bhelp_add_tabs(state);
			printf("}");
			curly_close_add_newline(list, state);
			break;
		case TOKEN_CARRAGE_RETURN:
			break;
		default:
			// got first real Token
			// indent
			bhelp_add_tabs(state);
			// print element
			printf("%s", token->value);
			// change state
			state->current = B_INLINE;
			break;
	}
	list->free(token);
}

static void beautify_in_line(List *list, Beauty *state){
	Token * token = list_dequeue_block(list);
	if (!list) {
		state->current = B_DONE;
	}
	// do we change what we are doing?
	switch (token->type){
		case TOKEN_EOF:
			state->current = B_DONE;
			break;
		case TOKEN_CARRAGE_RETURN:
			break;
		case TOKEN_SEMICOLON:
			printf(";\n");
			state->current = B_START;
			break;
		case TOKEN_OPEN_CURLY:
			printf("{\n");
			state->current = B_START;
			bhelp_inc_depth(state);
			break;
		case TOKEN_CLOSE_CURLY:
			printf("\n");
			bhelp_dec_depth(state);
			bhelp_add_tabs(state);
			printf("}");
			curly_close_add_newline(list, state);
			break;
		case TOKEN_NEWLINE:
			printf("\n");
			state->current = B_START;
			break;
		default:
			printf("%s", token->value);
	}
	list->free(token);
}

// consumers
/*
 * beautifyer is implemented as a state machine
 * each beautify_* function consumes one token and then returns to the below
 * loop
*/
static void consumer_beautify(List *list){
	Beauty state;
	state.depth = 0;
	state.depth_reset = 0;
	state.current = B_START;
	state.tab = "  ";
	while ( state.current != B_DONE ){
		switch (state.current){
			case B_START:
				beautify_start(list, &state);
				break;
			case B_INLINE:
				beautify_in_line(list, &state);
				break;
			case B_DONE:
				break;
		}
	}
}

void consumer_stats(List *list){
	size_t token_count=0;
	size_t token_lines = 1;
	size_t token_unknown = 0;
	size_t token_loops = 0;
	size_t token_vars = 0;
	size_t token_ifs = 0;
	size_t token_charnum = 0;
	size_t token_terinaries = 0;
	while (1){
		Token *token = list_dequeue_block(list);
		if (token_count % 137 == 136 || !token){
			if ( token_count > 136 ){
				printf("\e[8F");
			}
			printf("count:         %8ld\n", token_count);
			printf("lines:         %8ld\n", token_lines);
			printf("characters:    %8ld\n", token_charnum);
			printf("loops:         %8ld\n", token_loops);
			printf("variables:     %8ld\n", token_vars);
			printf("ifs:           %8ld\n", token_ifs);
			printf("ternary:       %8ld\n", token_terinaries);
			printf("unkown tokens: %8ld\n", token_unknown);
			if (!token) {
				break;
			}
		}
		token_count++;

		if ( token->charnum > token_charnum ){
			token_charnum = token->charnum;
		}
		switch ( token->type ){
			case TOKEN_ERROR:
				token_unknown++;
				break;
			case TOKEN_NEWLINE:
				token_lines++;
				break;
			case TOKEN_FOR:
				token_loops++;
				break;
			case TOKEN_VARIABLE:
				token_vars++;
				break;
			case TOKEN_IF:
				token_ifs++;
				break;
			case TOKEN_QUESTIONMARK:
				token_terinaries++;
				break;
		}
		list->free(token);
	}
}

static int consumer_all(List *list, void *unused){
	size_t line=1;
	size_t token_count=0;
	int ret = 0;
	Token *token;
	printf("#     line  charnum  length value\n");
	while ((token = list_dequeue_block(list))){
		token_count++;
		printf(
			"%4ld  %-4ld   %-8ld  %-6ld  %s",
			token_count, line, token->charnum, token->length,
			token_printable(token)
		);
		switch (token->type){
			case TOKEN_ERROR:
				printf(" -> TOKEN_ERROR");
				break;
			case TOKEN_EOF:
				printf("\nNum of lines: %ld\n", line);
				printf("EOF size: %ld\n", token->charnum);
				break;
			case TOKEN_NEWLINE:
				line++;
				break;
		}
		printf("\n");
		list->free(token);
	}
	return ret;
}

static int consumer_by_type(List *list, void *type_id){
	size_t type = *(size_t *)type_id;
	char *name = NULL;
	size_t line = 1;
	size_t i=0;
	Token *token;
	int ret = token_type_name(type, &name);
	if (ret){
		return ERROR;
	}

	fprintf(stderr, "seraching for: %s\n", name);
	while ((token = list_dequeue_block(list))){
		i++;
		if ( token->type == type ){
			printf("%s line:%ld byte_num:%ld token_num:%ld '%s'\n",
				name, line, token->charnum, i, token->value
			);
		}
		if ( token->type == TOKEN_NEWLINE ){
			line++;
		}

		list->free(token);
	}
	return ret;
}


void token_print_consume_unkown(List *list){
	size_t count = 0;
	char * value;
	Token *token;
	while ((token = list_dequeue_block(list)) && count < 30){
		if ( token->type == TOKEN_ERROR ){
			value = token->value;
			count++;
			printf("[%ld] char: %ld uknown token: 0x%02x '%s'\n",
				count, token->charnum, value[0], value
			);
		}
		list->free(token);
	}
}

static int token_type_name(size_t type, char **name_ret){
	int ret = 0;
	char *name = "Unknown token, please add it to "__FILE__;
	switch (type){
		case TOKEN_NONE:
			name =  "TOKEN_NONE";
			break;
		case TOKEN_ERROR:
			name =  "TOKEN_ERROR";
			break;
		case TOKEN_EOF:
			name =  "TOKEN_EOF";
			break;
		case TOKEN_TAB:
			name =  "TOKEN_TAB";
			break;
		case TOKEN_SPACE:
			name =  "TOKEN_SPACE";
			break;
		case TOKEN_NEWLINE:
			name =  "TOKEN_NEWLINE";
			break;
		case TOKEN_CARRAGE_RETURN:
			name =  "TOKEN_CARRAGE_RETURN";
			break;
		case TOKEN_OPEN_PAREN:
			name =  "TOKEN_OPEN_PAREN";
			break;
		case TOKEN_CLOSE_PAREN:
			name =  "TOKEN_CLOSE_PAREN";
			break;
		case TOKEN_OPEN_BRACE:
			name =  "TOKEN_OPEN_BRACE";
			break;
		case TOKEN_CLOSE_BRACE:
			name =  "TOKEN_CLOSE_BRACE";
			break;
		case TOKEN_COMMA:
			name =  "TOKEN_COMMA";
			break;
		case TOKEN_DOT:
			name =  "TOKEN_DOT";
			break;
		case TOKEN_COLON:
			name =  "TOKEN_COLON";
			break;
		case TOKEN_SEMICOLON:
			name =  "TOKEN_SEMICOLON";
			break;
		case TOKEN_OPEN_CURLY:
			name =  "TOKEN_OPEN_CURLY";
			break;
		case TOKEN_CLOSE_CURLY:
			name =  "TOKEN_CLOSE_CURLY";
			break;
		case TOKEN_QUESTIONMARK:
			name =  "TOKEN_QUESTIONMARK";
			break;
		case TOKEN_NULL_COALESCING:
			name =  "TOKEN_NULL_COALESCING";
			break;
		case TOKEN_NOT:
			name =  "TOKEN_NOT";
			break;
		case TOKEN_BITWISE_NOT:
			name =  "TOKEN_BITWISE_NOT";
			break;
		case TOKEN_BITSHIFT_LEFT:
			name =  "TOKEN_BITSHIFT_LEFT";
			break;
		case TOKEN_BITSHIFT_LEFT_ASSIGN:
			name =  "TOKEN_BITSHIFT_LEFT_ASSIGN";
			break;
		case TOKEN_GREATERTHAN_OR_EQUAL:
			name =  "TOKEN_GREATERTHAN_OR_EQUAL";
			break;
		case TOKEN_BITSHIFT_RIGHT_ASSIGN:
			name =  "TOKEN_BITSHIFT_RIGHT_ASSIGN";
			break;
		case TOKEN_ZERO_FILL_RIGHT_SHIFT:
			name =  "TOKEN_ZERO_FILL_RIGHT_SHIFT";
			break;
		case TOKEN_SIGNED_BITSHIFT_RIGHT:
			name =  "TOKEN_SIGNED_BITSHIFT_RIGHT";
			break;
		case TOKEN_BITWISE_AND_ASSIGN:
			name =  "TOKEN_BITWISE_AND_ASSIGN";
			break;
		case TOKEN_LOGICAL_AND:
			name =  "TOKEN_LOGICAL_AND";
			break;
		case TOKEN_BITWISE_AND:
			name =  "TOKEN_BITWISE_AND";
			break;
		case TOKEN_BITWISE_OR_ASSIGN:
			name =  "TOKEN_BITWISE_OR_ASSIGN";
			break;
		case TOKEN_LOGICAL_OR:
			name =  "TOKEN_LOGICAL_OR";
			break;
		case TOKEN_BITWISE_OR:
			name =  "TOKEN_BITWISE_OR";
			break;
		case TOKEN_LINE_COMMENT:
			name =  "TOKEN_LINE_COMMENT";
			break;
		case TOKEN_MULTI_LINE_COMMENT:
			name =  "TOKEN_MULTI_LINE_COMMENT";
			break;
		case TOKEN_REGEX:
			name =  "TOKEN_REGEX";
			break;
		case TOKEN_DIVIDE_ASSIGN:
			name =  "TOKEN_DIVIDE_ASSIGN";
			break;
		case TOKEN_DIVIDE:
			name =  "TOKEN_DIVIDE";
			break;
		case TOKEN_EXPONENT:
			name =  "TOKEN_EXPONENT";
			break;
		case TOKEN_MULTIPLY_ASSIGN:
			name =  "TOKEN_MULTIPLY_ASSIGN";
			break;
		case TOKEN_MULTIPLY:
			name =  "TOKEN_MULTIPLY";
			break;
		case TOKEN_MOD_EQUAL:
			name =  "TOKEN_MOD_EQUAL";
			break;
		case TOKEN_MOD:
			name =  "TOKEN_MOD";
			break;
		case TOKEN_BITWISE_XOR:
			name =  "TOKEN_BITWISE_XOR";
			break;
		case TOKEN_BITWISE_XOR_ASSIGN:
			name =  "TOKEN_BITWISE_XOR_ASSIGN";
			break;
		case TOKEN_EQUAL:
			name =  "TOKEN_EQUAL";
			break;
		case TOKEN_EQUAL_EQUAL:
			name =  "TOKEN_EQUAL_EQUAL";
			break;
		case TOKEN_EQUAL_EQUAL_EQUAL:
			name =  "TOKEN_EQUAL_EQUAL_EQUAL";
			break;
		case TOKEN_ADD:
			name =  "TOKEN_ADD";
			break;
		case TOKEN_INCREMENT:
			name =  "TOKEN_INCREMENT";
			break;
		case TOKEN_PLUS_EQUAL:
			name =  "TOKEN_PLUS_EQUAL";
			break;
		case TOKEN_SUBTRACT:
			name =  "TOKEN_SUBTRACT";
			break;
		case TOKEN_MINUS_EQUAL:
			name =  "TOKEN_MINUS_EQUAL";
			break;
		case TOKEN_DECREMENT:
			name =  "TOKEN_DECREMENT";
			break;
		case TOKEN_LESSTHAN_OR_EQUAL:
			name =  "TOKEN_LESSTHAN_OR_EQUAL";
			break;
		case TOKEN_LESSTHAN:
			name =  "TOKEN_LESSTHAN";
			break;
		case TOKEN_GREATER_THAN:
			name =  "TOKEN_GREATER_THAN";
			break;
		case TOKEN_NUMERIC:
			name =  "TOKEN_NUMERIC";
			break;
		case TOKEN_DOUBLE_QUOTE_STRING:
			name =  "TOKEN_DOUBLE_QUOTE_STRING";
			break;
		case TOKEN_SINGLE_QUOTE_STRING:
			name =  "TOKEN_SINGLE_QUOTE_STRING";
			break;
		case TOKEN_TILDA_STRING:
			name =  "TOKEN_TILDA_STRING";
			break;
		case TOKEN_VAR:
			name =  "TOKEN_VAR";
			break;
		case TOKEN_FOR:
			name =  "TOKEN_FOR";
			break;
		case TOKEN_LET:
			name =  "TOKEN_LET";
			break;
		case TOKEN_FUNCTION:
			name =  "TOKEN_FUNCTION";
			break;
		case TOKEN_RETURN:
			name =  "TOKEN_RETURN";
			break;
		case TOKEN_CATCH:
			name =  "TOKEN_CATCH";
			break;
		case TOKEN_IF:
			name =  "TOKEN_IF";
			break;
		case TOKEN_ELSE:
			name =  "TOKEN_ELSE";
			break;
		case TOKEN_DO:
			name =  "TOKEN_DO";
			break;
		case TOKEN_WHILE:
			name =  "TOKEN_WHILE";
			break;
		case TOKEN_THROW:
			name =  "TOKEN_THROW";
			break;
		case TOKEN_CONST:
			name =  "TOKEN_CONST";
			break;
		case TOKEN_TYPEOF:
			name =  "TOKEN_TYPEOF";
			break;
		case TOKEN_VARIABLE:
			name =  "TOKEN_VARIABLE";
			break;
		default:
			ret = -1;
			break;
	}
	if ( name_ret != NULL  )
		*name_ret = name;
	return ret;
}

int token_output_beauty(int fd){
	return threaded_printer(fd, consumer_beautify, NULL);
}

int token_output_stats(int fd){
	return threaded_printer(fd, consumer_stats, NULL);
}

int token_output_all(int fd){
	return threaded_printer(fd, consumer_all, NULL);
}

int token_output_by_type(int fd, size_t type){
	// check the type is correct before we go making threads n chunks
	// hmmm threads n chunks sounds like a terrible cereal
	if ( token_type_name(type, NULL) != 0 ){
		fprintf(stderr, "Unkown tokend type\n");
		return ERROR;
	}
	return threaded_printer(fd, consumer_by_type, (void*) &type);
}

