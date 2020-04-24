#include "tokenizer.h"

/*
 * beautify some js in the file descriptor fd
*/
int token_output_beauty(int fd);

/*
 * set up token_list and fileio, pthread, prints the stats
*/
int token_output_stats(int fd);

/*
 * set up token_list and fileio, pthread, prints the tokens
*/
int token_output_all(int fd);

/*
 * given a type, outputs all the seen tokens of that type
*/
int token_output_by_type(int fd, size_t type);

int token_output_typeids();
