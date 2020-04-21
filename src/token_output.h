#include "tokenizer.h"

/*
 * set up token_list and fileio, pthread, prints the stats
*/
int print_token_stats(int fd);

/*
 * set up token_list and fileio, pthread, prints the tokens
*/
int print_tokens(int fd);

/*
 * consumes tokens in list, printing each
 * return 0 on success
*/
int token_list_print_consume(token_list *list);
int token_list_stats_consume(token_list *list);
