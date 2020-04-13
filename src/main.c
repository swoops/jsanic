#include <error.h>
#include <stdio.h>
#include <stdlib.h>
#include "tokenizer.h"

void die(const char * msg){
		fprintf(stderr, "%s\n", msg);
		exit(-1);
}

int tokenlist_printer(token_list *list){
		int limit = 20;
		token *node = list->head;
		printf("size: %ld\n", list->size);
		while ( node && limit > 0) {
				printf("line: %4ld byte: %6ld len: %3ld value: %s\n", node->linenum, node->charnum, node->length, node->value);
				node = node->next;
				limit--;
		}
		return 0;
}

int main(int argc, char *argv[]){
		FILE *fp;
		int ret;
		char *fname = "/tmp/test.js";
		if (argc == 2) {
				fname = argv[1];
		}
		if ( !( fp = fopen(fname, "r") ) )
				die("No fp\n");

		token_list * list = init_token_list();
		ret = gettokens(fp, list);
		printf("gettokens returned %d\n", ret);
		tokenlist_printer(list);
		destroy_token_list(list);
		fclose(fp);

		return 0;
}
