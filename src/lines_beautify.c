#include <stdlib.h>
#include <stdio.h>
#include "tokenizer.h"
#include "lines.h"
#include "threads.h"

static inline Line *get_line(List *tl) {
	return (Line *) list_dequeue_block(tl);
}

static bool line_simple(Line *line) {
	if (line->char_len > 160) {
		return false;
	}
	if (line->cnt_comma + line->cnt_logic + line->cnt_ternary > 3) {
		return false;
	}
	return true;
}

static inline bool deep_beauty(Line *line) {
	return true;
}

static inline bool beautify_lines(List *inlines, List *outlines) {
	Line *line = NULL;
	while ((line = get_line (inlines))) {
		if (!line_simple (line)) {
			deep_beauty (line);
		}
		if (!list_append_block (outlines, line)) {
			return false;
		}
	}
	return true;
}

static void *threadup_beautifyer(void *in) {
	Thread_params *t = (Thread_params *) in;
	List *inlines = (List *)t->input;
	List *outlines = (List *)t->output;
	free (t);

	beautify_lines (inlines, outlines);
	list_destroy (inlines);
	list_producer_fin (outlines);
	return NULL;
}

List* lines_beautify(List *lines) {
	List *outlines = NULL;
	if (lines && (outlines = lines_new ())) {
		Thread_params *t = (Thread_params *)malloc (sizeof (Thread_params));
		if (!t) {
			list_destroy (lines);
			list_destroy (outlines);
			return NULL;
		}

		t->input = (void *)lines;
		t->output = (void *)outlines;

		if (pthread_create(&lines->thread->tid, &lines->thread->attr, threadup_beautifyer, (void *) t) != 0) {
			fprintf (stderr, "[!!] line beautify pthread_create failed\n");
			list_destroy (lines);
			list_destroy (outlines);
			free (t);
			return NULL;
		}
	}

	return outlines;
}
