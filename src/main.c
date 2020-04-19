#include <pthread.h>
#include <stdio.h>
#include <stdlib.h> // exit
#include "tokenizer.h"
#include "errorcodes.h"


void die(const char * msg){
	fprintf(stderr, "%s\n", msg);
	exit(-1);
}

int main(int argc, char *argv[]){
	void *ret;
	pthread_t lexer_tid;
	pthread_attr_t attr;

	token_list * list = init_token_list();
	if ( !list ) {
		fprintf(stderr, "Failed init token_list\n");
		return -1;
	}

	if (argc == 2) {
		list->fname = argv[1];
	}else {
		list->fname = "/tmp/test.js";
	}

	if ( pthread_attr_init(&attr) != 0){
		perror("Failed pthread_attr_init: ");
		token_list_destroy(list);
		return -1;
	}

	if ( pthread_create(&lexer_tid, &attr, gettokens, (void *) list) != 0){
		fprintf(stderr, "[!!] pthread_create failed\n");
		token_list_destroy(list);
		return -1;
	}
	token_list_print_consume(list);

	if ( pthread_join(lexer_tid, &ret) != 0 ){
		fprintf(stderr, "[!!] Join failed\n");
		token_list_destroy(list);
		return -1;
	}

	if ( pthread_attr_destroy(&attr) != 0 ){
		fprintf(stderr, "Failed to destroy pthread attr\n");
	}
	token_list_destroy(list);
	return 0;
}
