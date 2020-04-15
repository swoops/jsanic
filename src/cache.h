#include <unistd.h>
#include <string.h>  //strlen
#include <stdlib.h> // malloc

typedef struct  cache {
	int fd;
	unsigned char *buf;
	size_t charnum, real_size, size, linenum;
	size_t start,index, behind;
	int eof;
} cache;


size_t cache_getcharnum(cache *c);
size_t cache_getlinenum(cache *c);
cache * cache_init(size_t size, int fd);
void cache_destroy(cache *c);
int cache_getc(cache *c);
int cache_step_backcount(cache *c, size_t count);
int cache_step_back(cache *c);

/*
 * look ahead and see if the next part matches string. Does not consume
 * characters.
 * returns:
 *		 >0  yes
 *		 0   no
 *     <0  error
 */
int cache_match(cache *c, char *str);
/*
 * Compare cache to what is next in the string.
 *		 >0  cache is bigger
 *		 0   matches
 *     <0  cache is smaller
 */

int cache_strcmp(cache *c, char *str, int *result);
int cache_strncmp(cache *c, char *str, size_t n, int *result);
int cache_consume_n_match(cache *c, char *str, size_t n, int *result);
