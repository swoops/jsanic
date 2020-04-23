#include "tokenizer.h"

/*
 * set up token_list and fileio, pthread, prints the stats
*/
int token_output_stats(int fd);

/*
 * set up token_list and fileio, pthread, prints the tokens
*/
int token_output_all(int fd);

/*
 * consumes tokens in list, printing each
 * return 0 on success
*/
int token_list_print_consume(token_list *list);
int token_list_stats_consume(token_list *list);
int token_print_consume_unkown(token_list *list);
int token_output_unkown(int fd);
int token_type_name(size_t type, char **ret);
int token_output_types();
