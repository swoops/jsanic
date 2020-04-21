#include <stdio.h>
#include <stdlib.h> // exit
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "errorcodes.h"
#include "token_output.h"


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
	int show_stats = 0;
	int show_tokens = 0;
	int use_stdin = 0;
	char *fname;
	int fd = 0; // default to stdin

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

	// validate options
	if ( show_stats && show_tokens ){
		fprintf(stderr, "Cain't show both tokens and stats\n");
		usage(argv[0]);
		return ERROR;
	} else if ( use_stdin == 0 && optind >= argc ){
		fprintf(stderr, "Need a file to parse\n");
		usage(argv[0]);
		return ERROR;
	}

	if ( ! use_stdin ){
		if (optind >= argc) {
			fprintf(stderr, "Need file if not using stdin\n");
			return ERROR;
		}
		fname = argv[optind];
		if ( ( fd = open(fname, O_RDONLY) ) < 0 ){
			fprintf(stderr, "Can't open %s for reading\n", fname);
			return IOERROR;
		}
	}


	// do what the user asked
	if ( show_stats ){
		print_token_stats(fd);
	} else if ( show_tokens ){
		print_tokens(fd);
	}else {
		fprintf(stderr, "Beautifier not implemented yet\n");
		return -1;
	}

	if ( fd > 0 ){
		close(fd);
	}

	return 0;
}
