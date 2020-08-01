#include "list.h"
#include <stdlib.h>
#include <unistd.h>

static void empty_sleep(size_t iter){
	usleep(20*iter);
}

static void full_sleep(size_t iter){
	usleep(20*iter);
}

static void list_lock(List *l){
	if (l->locked) {
		pthread_mutex_lock(&l->lock);
	}
}

static void list_unlock(List *l){
	if (l->locked) {
		pthread_mutex_unlock(&l->lock);
	}
}

static void list_element_destroy(List_e *e){
	e->data = NULL;
	free(e);
}

static List_e *list_element_new(void *data){
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
	if (! LIST_IS_EMPTY(s)) {
		l->free(data);
	}
	return s;
}

static void list_clean(List *l) {
	// we *should* have sole ownership, so no need for locks
	if ( l->locked ) {
		pthread_mutex_destroy(&l->lock);
		l->locked = false;
	}

	while (l->length) {
		list_destroy_head(l);
	}
}

void list_destroy(List *l) {
	list_clean(l);
	free(l);
}

void list_destroy_by_consumer(List *l) {
	// tell producer to stop
	list_halt_producer(l);

	// may as well start cleaning while we wait for the producer to finish up
	List_status s = list_destroy_head(l);
	while(!LIST_IS_PRODUCER_FIN(s)) {
		usleep(5);
		s = list_destroy_head(l);
	}
	list_destroy(l);
}

bool list_init(List *l, void (*destructor)(void *ptr), bool locked) {
	if (!l || !destructor) {
		return false;
	}
	if (locked) {
		pthread_mutex_init(&l->lock, NULL);
	}

	l->locked = locked;
	l->free = destructor;
	l->status = LIST_EMPTY;
	l->max = 0;
	l->length = 0;
	return l;
}

List_status list_set_max(List *l, size_t max){
	list_lock(l);
	List_status s = l->status;
	l->max = max;
	list_lock(l);
	return s;
}

List *list_new(void (*destructor)(void *ptr), bool locked) {
	List *l = (List *) malloc(sizeof(List));
	if (!l) {
		return NULL;
	}
	if (!list_init(l, destructor, locked)) {
		free(l);
		return NULL;
	}
	return l;
}

List_status list_append(List *l, void *data) {
	List_e *e = list_element_new(data);
	if (!e) {
		return LIST_MEMFAIL;
	}
	e->data = data;

	list_lock(l);
	List_status status = l->status;
	if (LIST_IS_FULL(status) || LIST_IS_HALT_CONSUMER(status)) {
		list_unlock(l);
		list_element_destroy(e);
		return status;
	}

	List_e *old_tail = l->tail;
	if (old_tail) {
		old_tail->n = e;
		e->p = old_tail;
	} else {
		// no tail so list must be empty
		l->head = e;
		LIST_USET_EMPTY(l->status);
	}

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
		LIST_SET_EMPTY(l->status);
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

List_status list_status_set_flag(List *l, List_status s){
	list_lock(l);
	l->status |= s;
	s = l->status;
	list_unlock(l);
	return s;
}

List_status peek_head(List *l, void **data){
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

void * list_peek_head_block(List *l){
	void *data = NULL;
	List_status s;
	size_t i;
	while (!data){
		s = peek_head(l, &data);
		if (LIST_IS_HALT_CONSUMER(s) || LIST_IS_DONE(s)) {
			return NULL;
		}
		empty_sleep(i++);
	}
	return data;
}

void list_consume_until(List *l, bool (*until)(void *, void *), void *args){
	void *data = list_peek_head_block(l);
	while (!until(data, args)){
		list_destroy_head(l);
		data = list_peek_head_block(l);
	}
}

static inline bool check_flag(List *l, List_status flag){
	list_lock(l);
	List_status s = l->status;
	list_unlock(l);
	if (s & flag) {
		return true;
	}
	return false;
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
