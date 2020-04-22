#include "token_output.h"
#include "errorcodes.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>

int cleanup_pthread_for_tokens(pthread_t tid, pthread_attr_t *attr){
	void * status;
	if ( pthread_join(tid, &status) != 0 ){
		fprintf(stderr, "[!!] Join failed\n");
		return ERROR_PTHREAD_FAIL;
	}

	if ( pthread_attr_destroy(attr) != 0 ){
		fprintf(stderr, "Failed to destroy pthread attr\n");
		return ERROR_PTHREAD_FAIL;
	}
	return 0;
}

int start_token_thread(pthread_t *tid, pthread_attr_t *attr, token_list *list){
	if ( pthread_attr_init(attr) != 0){
		perror("Failed pthread_attr_init: ");
		token_list_destroy(list);
		return ERROR_PTHREAD_FAIL;
	}

	if ( pthread_create(tid, attr, gettokens, (void *) list) != 0){
		fprintf(stderr, "[!!] pthread_create failed\n");
		token_list_destroy(list);
		return ERROR_PTHREAD_FAIL;
	}
	return 0;
}

int threaded_printer(int fd, void *func){
	pthread_t tid;
	pthread_attr_t attr;
	token_list *list = NULL;
	int ret;
	int (*fun_ptr)(token_list *) = func;
	if ( fd < 0 ){
		return IOERROR;
	}
	if ( (list = token_list_init(fd)) == NULL ) {
		return MALLOCFAIL;
	}
	if ( start_token_thread(&tid, &attr, list) == 0 ){
		ret = (fun_ptr)(list);
		if ( ret == EOF ) ret = 0;
	}

	token_list_destroy(list);
	return ret;
}

int print_tokens(int fd){
	return threaded_printer(fd, token_list_print_consume);
	return 0;
}

int print_token_stats(int fd){
	return threaded_printer(fd, token_list_stats_consume);
	return 0;
}

int print_token_unknown(int fd){
	return threaded_printer(fd, token_print_consume_unkown);
	return 0;
}

static char * token_printable(token *tok){
	char * ret;
	switch (tok->type) {
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

int token_list_print_consume(token_list *list){
	size_t line=1;
	size_t token_count=0;
	int ret = 0;
	token *node;
	printf("#     line  charnum  length value\n");
	while (1){
		node = token_list_pop(list, &ret);
		if ( !node ){
			if ( ret ) break;
			usleep(20);
			continue;
		}
		token_count++;
		printf(
			"%4ld  %-4ld   %-8ld  %-6ld  %s",
			token_count, line, node->charnum, node->length,
			token_printable(node)
		);
		switch (node->type){
			case TOKEN_ERROR:
				printf(" -> TOKEN_ERROR");
				break;
			case TOKEN_EOF:
				printf("\nNum of lines: %ld\n", line);
				printf("EOF size: %ld\n", node->charnum);
				break;
			case TOKEN_NEWLINE:
				line++;
				break;
		}
		printf("\n");
		token_destroy(node);
	}
	return ret;
}

int token_print_consume_unkown(token_list *list){
	size_t count = 0;
	char * value;
	int ret = 0;
	token *node;
	while (count < 30){
		node = token_list_pop(list, &ret);
		if ( !node ){
			if ( ret ) break;
			usleep(20);
			continue;
		}
		if ( node->type == TOKEN_ERROR ){
			value = node->value;
			count++;
			printf("[%ld] char: %ld uknown token: 0x%02x '%s'\n",
				count, node->charnum, value[0], value
			);
		}
		token_destroy(node);
	}
	return ret;
}

int token_list_stats_consume(token_list *list){
	size_t token_count=1;
	size_t token_lines = 1;
	size_t token_unknown = 0;
	size_t token_loops = 0;
	size_t token_vars = 0;
	size_t token_ifs = 0;
	size_t token_charnum = 0;
	size_t token_terinaries = 0;
	int ret = 0;
	token *node;
	while (1){
		node = token_list_pop(list, &ret);
		if ( token_count % 137 == 0 || (!node && ret) ){
			if ( token_count > 137 ){
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
		}
		if ( !node ){
			if ( ret ) break;
			usleep(20);
			continue;
		}

		token_count++;
		if ( node->charnum > token_charnum ){
			token_charnum = node->charnum;
		}
		switch ( node->type ){
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
		token_destroy(node);
	}
	return ret;
}

