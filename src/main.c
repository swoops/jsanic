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
	printf(" -h        help menu and exit\n");
	printf(" -i        read from stdin (pipe)\n");
	printf(" -s        show stats\n");
	printf(" -a        show all tokens\n");
	printf(" -l        list known token types\n");
	printf(" -t X      list tokens with id X (get X from -l)\n");
}

int main(int argc, char *argv[]){
	char show_stats = 0;
	char show_all_tokens = 0;
	char use_stdin = 0;
	char show_all_types = 0;
	char show_by_type = 0;

	char *fname;
	int fd = 0; // default to stdin
	char * type_list = NULL;

	int opt;
	while ( ( opt = getopt(argc, argv, "hisalt:") ) != -1){
		switch (opt){
			case 'h':
				usage(argv[0]);
				return 0;
				break;
			case 'i':
				use_stdin = 1;
				break;
			case 's':
				show_stats = 1;
				break;
			case 'a':
				show_all_tokens = 1;
				break;
			case 'l':
				show_all_types = 1;
				break;
			case 't':
				show_by_type = 1;
				type_list = optarg;
				break;
			default:
				usage(argv[0]);
				return -1;
				break;
		}
	}

	if ( show_all_types == 1){
		return token_output_typeids();
	}

	// validate options
	int sum =  show_stats + show_all_tokens + show_by_type;
	if ( sum > 1){
		fprintf(stderr, "Pick only one of s,a,l,t options");
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
		token_output_stats(fd);
	} else if ( show_all_tokens ){
		token_output_all(fd);
	} else if ( show_by_type ){
		int t = atoi(type_list);
		if ( t < 0 ){
			fprintf(stderr, "Invalid type\n");
		}else {
			token_output_by_type(fd, (size_t) t);
		}
	}else {
		token_output_beauty(fd);
	}

	if ( fd > 0 ){
		close(fd);
	}

	return 0;
}
