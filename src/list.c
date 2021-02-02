#include "list.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#ifdef DEBUG
#include <assert.h>
static void validate_list(List *l) {
	if (!l->thread) {
		assert(LIST_IS_PRODUCER_FIN(l->status));
	}
	if (l->length == 0) {
		assert (l->head == NULL && l->tail == NULL && LIST_IS_EMPTY(l->status));
		return;
	} 
	if (l->length == 1) {
		assert(l->head == l->tail && l->head != NULL );
		assert(l->head->n == NULL && l->head->p == NULL);
		return;
	} 
	List_e *ptr = l->head;
	size_t i = 0;
	while (ptr) {
		if (ptr->n) {
			assert(ptr->n->p == ptr);
		}
		i++;
		ptr = ptr->n;
	}
	assert(i==l->length);
}
#endif

static void empty_sleep(size_t iter) {
	usleep(20*iter);
}

static void full_sleep(size_t iter) {
	usleep(20*iter);
}

static void list_lock(List *l) {
	if (l->thread) {
		pthread_mutex_lock(&l->thread->lock);
	}
#ifdef DEBUG
		validate_list(l);
#endif
}


static void list_unlock(List *l) {
#ifdef DEBUG
		validate_list(l);
#endif
	if (l->thread) {
		pthread_mutex_unlock(&l->thread->lock);
	}
}

static void list_element_destroy(List_e *e) {
	e->data = NULL;
	free(e);
}

static List_e *list_element_new(void *data) {
	if (!data) {
		return NULL;
	}

	List_e *e = (List_e *) calloc(1, sizeof(List_e));
	if (!e) {
		return NULL;
	}
	e->data = data;
	return e;
}

List_status list_destroy_head(List *l) {
	void *data = NULL;
	List_status s = list_dequeue(l, &data);
	l->free(data);
	return s;
}

static inline void list_destroy_tail(List *l) {
#ifdef DEBUG
	validate_list(l);
#endif
	if (l->thread) {
		fprintf(stderr, "[!!] Can't get tail when producers and consuers fight over it!!!\n");
		return;
	} else if (l->length == 0) {
		return;
	} else if (l->length == 1) {
		// head is tail
		list_destroy_head(l);
		return;
	}

	List_e *tail = l->tail;

	// second to last element, becomes last element
	l->tail = tail->p;
	// new last element's next will point to NULL
	l->tail->n = NULL;
	// free up the data in the element
	l->free(tail->data);
	// free up the element envelope
	l->length--;
	list_element_destroy(tail);
}


static void list_clean(List *l) {
	// we *should* have sole ownership, so no need for locks
	// we *should* also be the consumer thread
	if (l->thread) {
		Threadinfo *thread = l->thread;
		pthread_mutex_destroy(&thread->lock);
		void *status;
		pthread_join(thread->tid, &status);
		pthread_attr_destroy(&thread->attr);
		free (thread);
		l->thread = NULL;
	}

	while (l->length) {
		list_destroy_head(l);
	}
}

static void list_free(List *l) {
	if (l) {
		list_clean(l);
		free(l->thread);
		free(l);
	}
}

void list_destroy(List *l) {
	// tell producer to stop
	list_halt_producer(l);

	// may as well start cleaning while we wait for the producer to finish up
	List_status s = list_destroy_head(l);
	while(!LIST_IS_PRODUCER_FIN(s)) {
		usleep(5);
		s = list_destroy_head(l);
	}
	list_free(l);
}

bool list_init(List *l, void (*destructor)(void *ptr), bool locked) {
	if (!l || !destructor) {
		return false;
	}
	l->free = destructor;
	l->status = LIST_EMPTY;
	l->head = NULL;
	l->tail = NULL;
	l->max = 0;
	l->length = 0;

	if (locked) {
		l->thread = malloc(sizeof(Threadinfo));
		if (!l->thread) {
			return false;
		}
		pthread_mutex_init(&l->thread->lock, NULL);
		if (pthread_attr_init(&l->thread->attr) != 0) {
			return false;
		}
		if (pthread_attr_init(&l->thread->attr) != 0) {
			return false;
		}
	} else {
		l->thread = NULL;
		l->status |= LIST_PRODUCER_FIN;
	}

	return true;
}

List_status list_set_max(List *l, size_t max) {
	list_lock(l);
	List_status s = l->status;
	l->max = max;
	list_lock(l);
	return s;
}

List *list_new(void (*destructor)(void *ptr), bool locked) {
	List *l = (List *) malloc(sizeof(List));
	if (!list_init(l, destructor, locked)) {
		free(l);
		return NULL;
	}
	return l;
}

static void add_to_empty_list(List *l, List_e *e) {
	l->head = e;
	l->tail = e;
	l->length = 1;
	LIST_USET_EMPTY(l->status);
}

bool list_push(List *l, void *data) {
	if (l->thread) {
		fprintf(stderr, "Not allowed to push on threaded list\n");
		return false;
	}
	List_e *e = list_element_new(data);
	if (!e) {
		return false;
	}
	e->data = data;

	List_e *old_head = l->head;
	if (!old_head) {
		add_to_empty_list(l, e);
		return true;
	}
	old_head->p = e;
	e->n = old_head;
	l->head = e;
	l->length++;
#ifdef DEBUG
validate_list(l);
#endif
	return true;
}

List_status list_append(List *l, void *data) {
	list_lock(l);
	List_status status = l->status;
	if (LIST_IS_FULL(status) || LIST_IS_HALT_PRODUCER(status)) {
		list_unlock(l);
		return status;
	}

	List_e *e = list_element_new(data);
	if (!e) {
		list_unlock(l);
		return LIST_MEMFAIL;
	}
	e->data = data;

	List_e *old_tail = l->tail;
	if (!old_tail) {
		add_to_empty_list(l, e);
		list_unlock(l);
		return status;
	}
	old_tail->n = e;
	e->p = old_tail;
	l->tail = e;
	l->length++;

	list_unlock(l);
	return status;
}

bool list_append_block(List *l, void *data) {
	List_status s;
	size_t i = 1;
	while (1) {
		s = list_append(l, data);
		if (LIST_IS_HALT_PRODUCER(s)) {
			l->free(data);
			return false;
		}
		if (!LIST_IS_FULL(s)) {
			break;
		}
		full_sleep(i++);
	}
	return true;
}

List_status list_dequeue(List *l, void **data) {
	list_lock(l);
	List_status status = l->status;

	// unlink head
	List_e *old_head = l->head;
	if (!old_head) {
		list_unlock(l);
		return status;
	}
	l->head = old_head->n;
	l->length--;
	if (l->length == 0) {
		l->tail = NULL;
		LIST_SET_EMPTY(l->status);
	} else if (l->length == 1) {
		l->head->p = NULL;
	}
	list_unlock(l);

	// save data
	*data = old_head->data;

	// free data container
	list_element_destroy(old_head);

	return status;
}


// This assume it is a consumer calling
void * list_dequeue_block(List *l) {
	size_t i=1;
	void *data = NULL;
	List_status stat;
	do {
		stat = list_dequeue(l, &data);
		if (LIST_IS_HALT_CONSUMER(stat)) {
			l->free(data);
			return NULL;
		}
		if (LIST_IS_DONE(stat)) {
			l->free(data); // shouldn't be nescissary
			return NULL;
		}
		empty_sleep(i++);
	} while (!data);
	return data;
}

bool list_expended(List *l) {
	list_lock(l);
	List_status s = l->status;
	list_unlock(l);

	if (LIST_IS_DONE(s)) {
		return true;
	}
	return false;
}

List_status list_status_set_flag(List *l, List_status s) {
	list_lock(l);
	l->status |= s;
	s = l->status;
	list_unlock(l);
	return s;
}

List_status peek_head(List *l, void **data) {
	list_lock(l);
	List_status status = l->status;
	if (!l->head) {
		list_unlock(l);
		return status;
	}
	*data = l->head->data;
	list_unlock(l);
	return status;
}

void * list_peek_head_block(List *l) {
	void *data = NULL;
	List_status s;
	size_t i = 0;
	for (;;) {
		s = peek_head(l, &data);
		if (LIST_IS_HALT_CONSUMER(s) || LIST_IS_DONE(s)) {
			l->free(data);
			return NULL;
		}
		if (data) {
			break;
		}
		empty_sleep(i++);
	}
	return data;
}

void *list_peek_tail(List *l) {
	if (l->thread) {
		fprintf(stderr, "[!!] Can't get tail when producers and consuers fight over it!!!\n");
		return NULL;
	}
	return l->tail->data;
}

void list_consume_tail_until(List *l, bool (*until)(void *, void *), void *args) {
	void *data = list_peek_tail(l);
	while (until(data, args)) {
		list_destroy_tail(l);
		data = list_peek_tail(l);
	}
}

void list_consume_until(List *l, bool (*until)(void *, void *), void *args) {
	void *data = list_peek_head_block(l);
	while (until(data, args)) {
		list_destroy_head(l);
		data = list_peek_head_block(l);
	}
}

size_t list_length(List *l) {
	size_t ret = 0;
	if (l)	{
		list_lock(l);
		ret = l->length;
		list_unlock(l);
	}
	return ret;
}

// tell consumer to halt
void list_halt_consumer(List *l) {
	list_status_set_flag(l, LIST_HALT_CONSUMER);
}

// tell producer to halt
void list_halt_producer(List *l) {
	list_status_set_flag(l, LIST_HALT_PRODUCER);
}

// producer relenquishes
void list_producer_fin(List *l) {
	list_status_set_flag(l, LIST_PRODUCER_FIN);
}
