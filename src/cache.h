#include <unistd.h>
#include <string.h>  //strlen
#include <stdlib.h> // malloc

typedef struct  cache {
	int fd;
	unsigned char *buf;
	size_t charnum, real_size, size;
	size_t start,index, behind;
	int eof;
} cache;


size_t cache_getcharnum(cache *c);
cache * cache_init(size_t size, int fd);
void cache_destroy(cache *c);
int cache_getc(cache *c);
int cache_step_backcount(cache *c, size_t count);
int cache_step_back(cache *c);
int cache_str_match();
