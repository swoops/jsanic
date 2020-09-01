#include "tokenizer.h"

/*
 * beautify some js in the file descriptor fd
*/
int token_output_beauty(List *list);

/*
 * set up token_list and fileio, pthread, prints the stats
*/
int token_output_stats(List *list);

/*
 * set up token_list and fileio, pthread, prints the tokens
*/
int token_output_all(List *list);

/*
 * given a type, outputs all the seen tokens of that type
*/
int token_output_by_type(List *list, size_t type);

int token_output_typeids();
