#include <stdio.h>
#include <stdlib.h> // exit
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "errorcodes.h"
#include "tokenizer.h"
#include "decoders.h"
#include "lines.h"
#include "lines_beautify.h"
#include "ugly_lines.h"
#include "printlines.h"


void die(const char * msg) {
	fprintf(stderr, "%s\n", msg);
	exit(-1);
}

void usage(char *name) {
	printf("%s [-hd] <js_file>\n", name);
	printf("\n");
	printf("\t-h\t help menu\n");
	printf("\t-d\t do deobfuscation\n");
	printf("\t-p\t pretty -> try to do more pretty stuff, increase chance of breaking code\n");
}

int main(int argc, char *argv[]) {
	int fd = -1;
	bool deobf = false;
	bool pretty = false;

	int opt;
	while ((opt = getopt(argc, argv, "hdp")) != -1) {
		switch (opt) {
		case 'h':
			usage(argv[0]);
			return 0;
			break;
		case 'd':
			deobf = true;
			break;
		case 'p':
			pretty = true;
			break;
		default:
			usage(argv[0]);
			return -1;
			break;
		}
	}

	if (optind == argc) { // no file provided
		if (isatty(fileno(stdin))) {
			usage(argv[0]);
			fprintf(stderr, "Need file\n");
			return IOERROR;
		}
		fd = 0; // input is stdin
	} else if (optind + 1 == argc) {
		if ((fd = open(argv[optind], O_RDONLY)) < 0) {
			fprintf(stderr, "Can't open %s for reading\n", argv[optind]);
			return IOERROR;
		}
	} else {
		usage(argv[0]);
		fprintf(stderr, "Invalid number of files specified\n");
		return -1;
	}

	// l is a list of tokens
	List *l = tokenizer_start_thread (fd); // token list
	if (deobf) {
		l = decoder_creat_start_thread (l); // token list (deobfuscated)
	}

	if (pretty) {
		// l becomes a list of lines
		l = lines_creat_start_thread (l); // makes basic lines
		l = lines_beautify (l); // deeper beautification
	} else {
		l = ugly_lines_start_thread (l); // makes basic lines
	}
	printlines(l, stdout);

	close(fd);
	return 0;
}
