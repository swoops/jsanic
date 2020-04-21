#include <pthread.h>
#include <stdio.h>
#include <stdlib.h> // exit
#include <unistd.h>
#include "tokenizer.h"
#include "errorcodes.h"


void die(const char * msg){
	fprintf(stderr, "%s\n", msg);
	exit(-1);
}

void usage(char *name){
	printf("%s [OPTIONS] <js_file>\n", name);
	printf("\n");
	printf("options:\n");
	printf(" -s    show stats\n");
	printf(" -t    show tokens\n");
	printf(" -i    use stdin\n");
}

int main(int argc, char *argv[]){
	void *ret;
	pthread_t lexer_tid;
	pthread_attr_t attr;
	int show_stats = 0;
	int show_tokens = 0;
	int use_stdin = 0;

	int opt;
	while ( ( opt = getopt(argc, argv, "sti") ) != -1){
		switch (opt){
			case 's':
				show_stats = 1;
				break;
			case 't':
				show_tokens = 1;
				break;
			case 'i':
				use_stdin = 1;
				break;
			default:
				usage(argv[0]);
				return -1;
		}
	}

	if ( show_stats && show_tokens ){
		fprintf(stderr, "Cain't show both tokens and stats\n");
		usage(argv[0]);
		return -1;
	} else if ( use_stdin == 0 && optind >= argc ){
		fprintf(stderr, "Need a file to parse\n");
		usage(argv[0]);
		return -1;
	}

	token_list * list = init_token_list();
	if ( use_stdin ){
		list->fd = 0;
	} else {
		list->fname = argv[optind];
	}

	if ( !list ) {
		fprintf(stderr, "Failed init token_list\n");
		return -1;
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

	if ( show_stats )
		token_list_stats_consume(list);
	else if ( show_tokens )
		token_list_print_consume(list);
	else
		printf("Can't make anything pretty yet :(\n");

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
