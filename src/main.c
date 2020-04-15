#include <stdio.h>
#include <stdlib.h> // exit
#include "tokenizer.h"
#include "errorcodes.h"


void die(const char * msg){
	fprintf(stderr, "%s\n", msg);
	exit(-1);
}

int main(int argc, char *argv[]){
	int ret;
	char *fname = "/tmp/test.js";
	if (argc == 2) {
		fname = argv[1];
	}
	token_list * list = init_token_list();
	ret = gettokens_fromfname(fname, list);
	if ( ret == EOF ){
		fprintf(stderr, "[**] success\n");
	}else{
		fprintf(stderr, "[!!] gettokens returned %d\n", ret);
	}
	token_list_print_consume(list);
	token_list_destroy(list);

	return 0;
}
