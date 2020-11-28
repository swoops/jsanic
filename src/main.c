#include <stdio.h>
#include <stdlib.h> // exit
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "errorcodes.h"
#include "printlines.h"


void die(const char * msg) {
	fprintf(stderr, "%s\n", msg);
	exit(-1);
}

void usage(char *name) {
	printf("%s [OPTIONS] <js_file>\n", name);
}

int main(int argc, char *argv[]) {
	char show_all_types = 0;
	char show_by_type = 0;
	int fd = -1;

	int opt;
	while ((opt = getopt(argc, argv, "hisalt:")) != -1) {
		switch (opt) {
		case 'h':
			usage(argv[0]);
			return 0;
			break;
		case 'i':
			fd = 0;
			break;
		default:
			usage(argv[0]);
			return -1;
			break;
		}
	}
	char *fname = argv[optind];

	if (fd != 0) {
		if (optind >= argc) {
			fprintf(stderr, "Need file if not using stdin\n");
			return ERROR;
		}
		if ((fd = open(fname, O_RDONLY)) < 0) {
			fprintf(stderr, "Can't open %s for reading\n", fname);
			return IOERROR;
		}
	}

	// get tokenizer started
	printlines(fd, 1);
	close(fd);
	return 0;
}
