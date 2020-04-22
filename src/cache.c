#include "cache.h"
#include "errorcodes.h"

#define MINCACHESIZE  128
#define MAXSTRLEN  0x400

size_t cache_getcharnum(cache *c){
	return c->charnum;
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
		return ret;
	}

	// if we hit buf max, so we have to move things arround
	c->index %= c->real_size; // at this point real_size == size
	return ret;
}

int cache_step_back(cache *c){
	if (c->size == 0) return ERROR;
	if ( c->behind >= c->size ) return ERROR;
	c->behind++;
	c->charnum--;
	if ( c->index == 0 )
		c->index = c->size-1;
	else
		c->index--;
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

int cache_str_match(cache *c, char *str){
	int ch, test;
	size_t i;
	size_t max = c->real_size/2;
	for (i=0; i<max; i++){
		test = str[i];
		if ( test == 0 ) return 0; // matches

		ch = cache_getc(c);
		if ( ch < EOF ) break;
		if ( ch != test ) break;
	}
	// failed to match so step back
	if( cache_step_backcount(c, i+1) < 0)
		return ERROR;
	if ( i == max || ch < EOF )
		return ERROR;
	return 0;
}
