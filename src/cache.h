#include <unistd.h>
#include <string.h>  //strlen
#include <stdlib.h> // malloc

#define RBUFSIZE 4096

typedef struct  cache {
	int fd;
	unsigned char *buf;
	size_t charnum, real_size, size;
	size_t start,index, behind;

	// for buffering read
	unsigned char rbuf[RBUFSIZE];
	size_t ri;
	ssize_t rsize;
} cache;


size_t cache_getcharnum(cache *c);
cache * cache_init(size_t size, int fd);
void cache_destroy(cache *c);
int cache_getc(cache *c);
int cache_step_backcount(cache *c, size_t count);
int cache_step_back(cache *c);
