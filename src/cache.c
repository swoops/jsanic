#include "cache.h"
#include "errorcodes.h"

#define MINCACHESIZE  128
#define MAXSTRLEN  0x400

size_t cache_getcharnum(cache *c){
	return c->charnum;
}

size_t cache_getlinenum(cache *c){
	return c->linenum;
}

cache * cache_init(size_t size, int fd){
	cache *c = (cache *) malloc(sizeof(cache));
	if ( !c ) return NULL;

	if (size <= MINCACHESIZE) size = MINCACHESIZE;
	c->buf = (unsigned char *) malloc(size);
	if ( !c->buf ) {
		free(c);
		return NULL;
	}

	c->charnum = 0;
	c->eof = 0;
	c->index = 0;
	c->start = 0;
	c->size = 0;
	c->behind = 0;
	c->linenum = 1;
	c->real_size = size;
	c->fd = fd;
	return c;
}

void cache_destroy(cache *c){
	free(c->buf);
	free(c);
}

int readchr(int fd){
	ssize_t ret;
	unsigned char buf;
	ret = read(fd, &buf, 1);
	if ( ret == 0 ){
		return EOF;
	}else if ( ret == 1 ){
		return (int) buf;
	}else {
		return IOERROR;
	}
}

int cache_getc(cache *c){
	int ret;
	if ( c->behind ){
		ret = c->buf[c->index];
		c->index++;
		c->behind--;
		c->charnum++;
		if ( ret == '\n' ) {
			c->linenum++;
		}
		return ret;
	}

	ret = readchr(c->fd);
	if ( ret == EOF ){
		c->eof = 1;
		return ret;
	}
	c->buf[c->index] = (unsigned char) ret;
	c->charnum++;
	c->index++;
	if ( c->size < c->real_size ) {
		c->size++;
		if ( ret == '\n' ) {
			c->linenum++;
		}
		return ret;
	}

	// if we hit buf max, so we have to move things arround
	c->index %= c->real_size; // at this point real_size == size
	if ( ret == '\n' ) {
		c->linenum++;
	}
	return ret;
}

int cache_step_back(cache *c){
	if (c->size == 0) return ERROR;
	if ( c->behind >= c->size ) return ERROR;
	c->behind++;
	c->charnum--;
	c->index--;
	if ( c->buf[c->index] == '\n' ){
		c->linenum--;
	}
	return 0;
}

int cache_step_backcount(cache *c, size_t count){
	size_t i;
	int ret;
	// walking back one step at a time is stupid... but it avoids stupid
	// mistakes, like skipping newlines or int underflows
	// TODO: live dangeriously
	for (i=0; i<count; i++){
		ret = cache_step_back(c);
		if ( ret < 0 ) return ret;
	}
	return 0;
}

int cache_eof(cache *c){
	if ( c->eof ) return 1;
	return 0;
}

int cache_match(cache *c, char *str){
	int i;
	int ch, ret;
	// no one shoudl be able too look to far into the future
	size_t max = c->real_size/2;

	for (i=0; str[i] && i<max; i++){
		ch = cache_getc(c);
		if ( ch == EOF || str[i] != ch){
			break;
		}
	}
	if ( str[i] == '\x00' ){
	// reached end of string, we matched
		ret = i;
	} else if ( ch < 0 && ch != EOF ) {
		ret = -1;
	} else {
		ret = 0; // not a match
	}
	if ( ret >= 0 && cache_step_backcount(c, i+1) < 0) {
		return ERROR;
	}
	return ret;
}

static int cache_general_match(cache *c, char *str, size_t n, int *result, int consumematches){
	size_t i;
	int ch, test;
	size_t max = n;
	*result=0;

	if ( max == 0 ){
		max = c->real_size/2;
	}

	for (i=0; i<max; i++){
		ch = cache_getc(c);
		test = str[i];

		// EOF or error
		if ( ch < 0 ) {
			if ( ch != EOF ){
				return ERROR;
			}
			// EOF handling
			if ( i == 0 ) {
				return EOF; // nothing to compare
			}
			if ( test == 0 || n == i+1){
				// ran out of compare material and str at same time, apparently a match
				break;
			}
		}else if ( ch > test ){  // cache is bigger
			i++;
			*result = 1;
			break;
		}else if ( ch < test ){ // cache is smaller
			i++;
			*result = -1;
			break;
		}
	}

	if ( *result  || !consumematches ){
		if ( cache_step_backcount(c, i) < 0) {
			return ERROR;
		}
	}
	return 0;
}

int cache_consume_n_match(cache *c, char *str, size_t n, int *result){
	return cache_general_match(c, str, n, result, 1);
}

int cache_strcmp(cache *c, char *str, int *result){
	return cache_general_match(c, str, 0, result, 0);
}

int cache_strncmp(cache *c, char *str, size_t n, int *result){
	return cache_general_match(c, str, n, result, 0);
}

