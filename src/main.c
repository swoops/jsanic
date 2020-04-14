#include <stdio.h>
#include <stdlib.h> // exit
#include <unistd.h>
#include <fcntl.h>
#include "tokenizer.h"


void die(const char * msg){
		fprintf(stderr, "%s\n", msg);
		exit(-1);
}

int tokenlist_printer(token_list *list){
		int limit = 20;
		token *node = list->head;
		printf("size: %ld\n", list->size);
		printf("line   charnum   length  value\n");
		while ( node && limit > 0) {
				printf("%-4ld   %-8ld  %-6ld  %s\n", node->linenum, node->charnum, node->length, node->value);
				node = node->next;
				limit--;
		}
		return 0;
}

int main(int argc, char *argv[]){
		int ret, fd;
		char *fname = "/tmp/test.js";
		if (argc == 2) {
				fname = argv[1];
		}
		fd = open(fname, O_RDONLY);
		if ( fd < 0 ){
				fprintf(stderr, "Failed to open file %s\n", fname);
				return -1;
		}

		token_list * list = init_token_list();
		ret = gettokens(fd, list);
		printf("gettokens returned %d\n", ret);
		tokenlist_printer(list);
		destroy_token_list(list);
		close(fd);

		return 0;
}
