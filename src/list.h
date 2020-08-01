#include <stdbool.h>
#include <pthread.h>

typedef unsigned char List_status;

typedef struct list_element {
	void *data;
	struct list_element *n, *p;
} List_e;

typedef struct {
	List_e *head;
	List_e *tail;
	void (*free)(void *ptr);
	size_t length;
	size_t max;
	pthread_mutex_t lock;
	bool locked;
	List_status status;
} List;

// List_status flags
#define LIST_EMPTY          1 << 0
#define LIST_FULL           1 << 1
#define LIST_HALT_CONSUMER  1 << 2
#define LIST_HALT_PRODUCER  1 << 3
#define LIST_PRODUCER_FIN   1 << 4
#define LIST_MEMFAIL        1 << 5

#define LIST_IS_EMPTY(x)          (x & LIST_EMPTY)
#define LIST_IS_FULL(x)           (x & LIST_FULL)
#define LIST_IS_HALT_CONSUMER(x)  (x & LIST_HALT_CONSUMER)
#define LIST_IS_HALT_PRODUCER(x)  (x & LIST_HALT_PRODUCER)
#define LIST_IS_PRODUCER_FIN(x)   (x & LIST_PRODUCER_FIN)
#define LIST_IS_MEMFAIL(x)        (x & LIST_MEMFAIL)

#define LIST_IS_DONE(x)           (LIST_IS_EMPTY(x) && LIST_IS_PRODUCER_FIN(x))
#define LIST_PRODUCER_CONTINUE(x) (!LIST_IS_HALT_PRODUCER(x) && !LIST_IS_MEMFAIL(x) )


#define LIST_SET_EMPTY(x)  (x |= LIST_EMPTY)
#define LIST_USET_EMPTY(x) (x ^= LIST_EMPTY)

// interface for threaded consumer producer
bool list_append_block(List *l, void *data);
void * list_dequeue_block(List *l);
void list_consume_until(List *l, bool (*until)(void *, void *), void *args);
void list_destroy_by_consumer(List *l);
void * list_peek_head_block(List *l);
List_status list_status_set_flag(List *l, List_status s);
List_status list_set_max(List *l, size_t max);

// alloc and init
List *list_new(void (*destructor)(void *ptr), bool locked);
// init and already allocated list
bool list_init(List *l, void (*destructor)(void *ptr), bool locked);
// clean and free list
void list_destroy(List *l);
List_status list_append(List *l, void *data);
List_status list_dequeue(List *l, void **data);

// tell consumer to halt
void list_halt_consumer(List *l);
// tell producer to halt
void list_halt_producer(List *l);
void list_producer_fin(List *l);
