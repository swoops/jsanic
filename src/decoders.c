#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "decoders.h"
#include "threads.h"
#include "tokenizer.h"

static bool b64_buf_add_byte(char **buf, size_t *index, size_t *size, char c) {
	if (*index >= *size) {
		fprintf(stderr, "OVERFLOW in b64\n");
		return false;
	}
	if (c == '"') { // must encode it so it can be put in "" later
		const char input[] = "\\x22";
		const size_t ilen = sizeof(input) - 1;
		*size += ilen - 1; // -1 for null, -1 for space already allocated for char
		if ((*buf = realloc(*buf, *size)) == NULL) {
			return false;
		}
		memcpy(*buf + *index, input, ilen);
		*index += ilen;
		return true;
	}

	// ascii check
	if (c != '\t' && c != '\n' && (c > '~' || c < ' ' )) {
		return false;
	}
	*buf[(*index)++] = c;
	return true;
}

static char * decode_b64(const char *b64, size_t b64_len, size_t *bufsize) {
	if (b64_len < 4) {
		return NULL;
	}
	{
		size_t bitsize = b64_len * 6;
		if (b64[b64_len - 1] == '=') {
			if (b64[b64_len - 2] == '=') {
				bitsize -= 16;
				b64_len -= 2;
			} else {
				bitsize -= 8;
				b64_len -= 1;
			}
		}
		if (bitsize % 8 != 0) {
			return NULL;
		}
		*bufsize = (bitsize / 8) + 1;
	}

	char *buf = (char *) malloc(*bufsize);

	if (!buf) {
		return NULL;
	}

	unsigned short tmp_hldr = 0;		 // hold addition 2 bytes
	short int bits_in = 0;
	size_t i, j=0;
	for (i=0; i<b64_len; i++){
		tmp_hldr <<= 6;
		if (b64[i] >= 'A' && b64[i] <= 'Z') {
			tmp_hldr |= b64[i] - 0x41;	
		} else if (b64[i] >= 'a' && b64[i] <= 'z') {
			tmp_hldr |= b64[i] - 0x47; // 0x41 + 0x6 because 6 chars between z and A
		} else if ( b64[i] >= '0' && b64[i] <= '9') {
			tmp_hldr |= b64[i] + 0x04; // ascii('0') = 0x30 and base64('0') = 0x34, so to speak 
		} else if (b64[i] == '+') {
			tmp_hldr |= 0x3e;
		} else if (b64[i] == '/') {
			tmp_hldr |= 0x3f;
		} else{ 
			free(buf);
			return NULL;
		}

		bits_in += 6;
		if (bits_in >= 8){
			bits_in -= 8;
			char c = tmp_hldr >> bits_in;
			if (!b64_buf_add_byte(&buf, &j, bufsize, c)) {
				free (buf);
				return NULL;
			}
		}
	}
	buf[*bufsize] = '\0'; 
	return buf;
}

static bool decode_string(Token *tok) {
	size_t size = 0;
	char *b64 = decode_b64(tok->value + 1, tok->length - 2, &size);
	if (!b64) {
		return false;
	}
	const char start[] = "btoa('";
	const char end[] = "')";
	size_t alloc_size = sizeof(start) - 1 // size of start without NULL sentinal
		+ sizeof(end) - 1 // size of end without NULL sentinal
		+ size; // size of buffer with NULL sentinal
	char *buf = (char *) malloc(alloc_size);
	if (!buf) {
		free(b64);
		return false;
	}

	free((void *) tok->value);
	tok->value = buf;
	tok->length = alloc_size - 1; // NULL term not included in length

	memcpy(buf, start, sizeof(start) - 1);
	size_t off = sizeof(start) - 1;

	memcpy(buf + off, b64, size - 1);
	off += size - 1;
	free (b64);

	memcpy(buf + off, end, sizeof(end)); // will NULL terminate
	return true;
}

static bool decoder(List *in, List *out) {
	Token *tok = NULL;
	while ((tok = token_list_dequeue(in)) != NULL) {
		switch (tok->type) {
			case TOKEN_DOUBLE_QUOTE_STRING:
			case TOKEN_SINGLE_QUOTE_STRING:
			case TOKEN_TILDA_STRING:
				// potentially decode content of tok->value in place
				decode_string(tok);
				break;
			default:
				break;
		}
		if (!list_append_block(out, tok)) {
			return false;
		}
	}
	return true;
}

static void *decoder_start(void *args) {
	Thread_params *t = (Thread_params *) args;
	List *in = (List *)t->input;
	List *out = (List *)t->output;
	free (t);
	decoder(in, out);
	list_destroy(in);
	list_producer_fin(out);
	return NULL;
}

List *decoder_creat_start_thread(List *tokens) {
	if (!tokens) {
		return NULL;
	}

	List *out = list_new(tokens->free, true);
	if (!out) {
		list_destroy(tokens);
		return NULL;
	}

	Thread_params *t = (Thread_params *)malloc(sizeof(Thread_params));
	if (!t) {
		list_destroy(tokens);
		list_destroy(out);
		return NULL;
	}

	t->input = (void *)tokens;
	t->output = (void *)out;

	if (pthread_create(&out->thread->tid, &out->thread->attr, decoder_start, (void *) t) != 0) {
		fprintf(stderr, "[!!] pthread_create failed\n");
		list_destroy(tokens);
		list_destroy(out);
		free(t);
		return NULL;
	}
	return out;
}
