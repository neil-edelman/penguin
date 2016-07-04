/** Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <string.h>	/* memcpy memmove strerror */
#include <errno.h>	/* global errno */
#include "List.h"

/** List like in Java, but dangerous and unsafe because C. The structs are
 stored in a contagious array, not pointers. You specify sizeof in the
 constructor.
 <p>
 Incomplete and untested!

 @author	Neil
 @version	1.1; 2016-06
 @since		2015 */

static const int debug = 0;

static const int fibonacci6 = 8;
static const int fibonacci7 = 13;

enum Error {
	E_NO_ERROR, E_ERRNO, E_TOO_LARGE, E_LOCKED
} error = E_NO_ERROR;
int errno_copy;

struct List {
	int    capacity[2];	/* Fibonacci */
	int    size;		/* the size of the list */
	size_t width;		/* the size of the elements */
	int    is_locked;	/* eg ForEach locks it to prevent changes to ??? fuck you apasia...genus tologemy */
	char   *iterator;	/* yay */
	char   *array;		/* the actual list, implemented as bytes * width */
	enum Error error;	/* errors in this file */
	int errno_copy;		/* copy of errno when when error == E_ERRNO */
};

/* hhhmmmm :[ */
static int last_index;
static void *last_param;

/* private prototypes */

static void grow(int *const a, int *const b);

/* public */

/** "Constructs an empty list with the specified initial capacity."
 @param width	The size of the objects that go in the list.
 @param compare	Metric that compares; can be null if you don't want to sort.
 @return		An object.
 @throws		width <= 0, E_ERRNO; */
struct List *List(const size_t width) {
	struct List *al;

	if(width <= 0) return 0;
	if(!(al = malloc(sizeof(struct List)))) {
		error = E_ERRNO;
		errno_copy = errno;
		return 0;
	}
	al->capacity[0] = fibonacci6;
	al->capacity[1] = fibonacci7;
	al->size        = 0;
	al->array       = 0;
	al->width       = width;
	al->is_locked   = 0;
	al->iterator    = 0;
	al->array       = 0;
	al->error       = E_NO_ERROR;
	al->errno_copy  = 0;
	if(debug) fprintf(stderr, "List: new, capacity %d, #%p.\n", al->capacity[0], (void *)al);
	if(!(al->array = malloc(al->capacity[0] * width))) {
		error = E_ERRNO;
		errno_copy = errno;
		List_(&al);
		return 0;
	}

	return al;
}

/** Destructor.
 @param al_ptr	A reference to the object that is to be deleted. */
void List_(struct List **al_ptr) {
	struct List *al;

	if(!al_ptr || !(al = *al_ptr)) return;
	if(al->is_locked) fprintf(stderr, "~List: erasing #%p while it is "
		"locked; you will probably experience a crash now.\n", (void *)al);
	if(debug) fprintf(stderr, "~List: erase, #%p.\n", (void *)al);
	free(al->array);
	free(al);
	*al_ptr = 0;
}

/** "Increases the capacity of this List instance, if necessary, to ensure
 that it can hold at least the number of elements specified by the minimum
 capacity argument."
 @param min_capacity
 @return				True if the capacity increase was vaible; otherwise
						the list is not touched.
 @throws				!al; E_LOCKED, E_TOO_LARGE, E_ERRNO */
int ListEnsureCapacity(struct List *al, const int min_capacity) {
	void *array;
	int c0, c1;

	if(!al) return 0;
	if(al->is_locked) { al->error = E_LOCKED; return 0; }

	/* we're good */
	if(al->capacity[0] >= min_capacity) return -1;

	/* re-alloc */
	c0 = al->capacity[0];
	c1 = al->capacity[1];
	while(min_capacity > c0) {
		grow(&c0, &c1);
		if(c0 < 0) { al->error = E_TOO_LARGE; return 0; }
	}
	if(!(array = realloc(al->array, c0 * al->width))) {
		al->error = E_ERRNO;
		al->errno_copy = errno;
		return 0;
	}
	al->array = array;
	al->capacity[0] = c0;
	al->capacity[1] = c1;

	if(debug) fprintf(stderr, "ListEnsureCapacity: #%p expanded to %d to fit %d elements.\n", (void *)al, c0, min_capacity);
	return -1;
}

/** "Appends the specified element to the end of this list."
 @param al	The List.
 @param e	"element to be appended to this list"
 @return	True if the element could be added.
 @throws	!al, !e; E_LOCKED, E_TOO_LARGE, E_ERRNO */
int ListAdd(struct List *al, const void *const e) {
	if(!al || !e) return 0;
	if(al->is_locked) { al->error = E_LOCKED; return 0; }
	if(!ListEnsureCapacity(al, al->size + 1)) return 0;
	memcpy(al->array + al->size * al->width, e, al->width);
	al->size++;
	return -1;
}

/** "Removes all of the elements from this list."
 @param al	The List.
 @throws	!al; E_LOCKED */
void ListClear(struct List *al) {
	if(!al) return;
	if(al->is_locked) { al->error = E_LOCKED; return; }

	al->size = 0;
}

/** This is the only function that lets is_locked be non-zero outside.
 @fixme		Allow deletions/insertions.
 @param l	List.
 @throws	!l; E_LOCKED */
void *ListIterate(struct List *const l) {
	if(!l) return 0;
	if(!l->iterator) {
		if(l->is_locked) { l->error = E_LOCKED; return 0; }
		l->is_locked = -1;
		l->iterator = l->array;
	} else {
		l->iterator += l->width;
	}
	if(l->iterator >= l->array + l->size * l->width) {
		l->iterator = 0;
		l->is_locked = 0;
	}
	return l->iterator;
}

/** "Performs the given action for each element." Sets Index.
 @param al		List.
 @param action	"The action to be performed for each element."
				<p>
				void (*)(void *const).
 @throws		!al, !action; E_LOCKED */
void ListForEach(struct List *al, ListAction action) {
	int i;
	char *a;

	if(!al || !action) return;
	if(al->is_locked) { al->error = E_LOCKED; return; }
	al->is_locked = -1;

	for(i = 0, a = al->array; i < al->size; i++, a += al->width) {
		last_index = i;
		(*action)(a);
	}
	last_index = 0;

	al->is_locked = 0;
}

/** Intended to be used inside a locked callback. Extremly not thread-safe.
 @return		The last index. */
int ListIndex(void) {
	return last_index;
}

/** @param param	Sets the global parameter. */
void ListSetParam(void *const param) {
	last_param = param;
}

/** Intended to be used inside a locked callback. Extremly not thread-safe.
 @return		The param set by SetParam. */
void *ListGetParam(void) {
	return last_param;
}

/**
 @return	All true, true; one false, false.
 @throws	!al, !predicate; E_LOCKED */
int ListShortCircuit(struct List *al, const ListPredicate predicate, void *const param) {
	int i, ret = -1;
	char *a;

	if(!al || !predicate) return 0;
	if(al->is_locked) { al->error = E_LOCKED; return 0; }
	al->is_locked = -1;

	for(i = 0, a = al->array; i < al->size; i++, a += al->width) {
		last_index = i;
		if(!(*predicate)(a, param)) { ret = 0; break; }
	}
	last_index = 0;

	al->is_locked = 0;

	return ret;
}

/** "Returns the element at the specified position in this list."
 @param al		The List.
 @param index	"index of the element to return"
 @return		"the element at the specified position in this list"
 @throws		!al, 0 > index || index <= size; */
void *ListGet(const struct List *al, const int index) {
	if(!al || index < 0 || index >= al->size) return 0;
	return al->array + index * al->width;
}

/** indexOf, lastIndexOf, boring */

/** "Returns the number of elements in this list."
 @param al	The List.
 @return	"the number of elements in this list"
 @throws	!al */
int ListSize(const struct List *al) {
	if(!al) return 0;
	return al->size;
}

#if 0
/** "Inserts the specified element at the specified position in this list."
 @param al		The List.
 @param index
 @param e		"element to be appended to this list"
 @return		True if the element could be added. */
int ListIndexAdd(struct List *al, const int index, void *const e) {
	if(!al || index < 0 || index > al->size) return 0;
	if(al->is_locked) { error = E_LOCKED; return 0; }
	if(!ListEnsureCapacity(al, al->size + 1)) return 0;
	memmove(al->array + index * al->width, al->array + (index + 1) * al->width,
			(al->size - index) * al->width);
	memcpy(al->array + index * al->width, e, al->width);
	return -1;
}

/** Concatenates another List onto this one.
 <p>
 Fixme: untested.
 @param al	The List.
 @param c	Another List.
 @return	Success. */
int ListAddAll(struct List *al, const struct List *c) {
	int i, j;

	assert(0);

	if(!al) return 0;
	if(!c) return -1;

	if(!ListEnsureCapacity(al, al->size + c->size)) return 0;
	for(i = al->size, j = 0; j < c->size; i++, j++) {
		al->array[i] = c->array[j];
		al->size++;
	}

	return -1;
}

/** "Inserts all of the elements in the specified collection into this list,
 starting at the specified position. Shifts the element currently at that
 position (if any) and any subsequent elements to the right."
 <p>
 Fixme: untested.
 @param al		The List.
 @param index	"index at which to insert the first element"
 @param c		"element to be appended to this list"
 @return		Success. */
int ListIndexAddAll(struct List *al, const int index, const struct List *c) {
	int i, j;

	assert(0);

	if(!al || index < 0 || index > al->size) return 0;
	if(!c) return -1;

	if(!ListEnsureCapacity(al, al->size + c->size)) return 0;
	assert(0);
	for(i = al->size - 1, j = al->size - 1 + c->size - 1; i >= index; i--, j--) {
		al->array[j] = al->array[i];
	}
	for(j = 0; j < c->size; i++, j++) {
		al->array[i] = c->array[j];
		al->size++;
	}

	return -1;
}

/*this is removeif...*/
void ListForEach(struct List *al, ListAction action) {
	/* a to b, delete, b to c, keep */
	char *a = 0, *b = 0, *c, *end;
	enum { KEEPING, DELETING } state = KEEPING;

	if(!al || !action) return;

	end  = al->array + al->size * al->width;

	for(c = al->array; c < end; c += al->width) {
		if((*action)(c, argument)) {
			if(state == DELETING) {
				/* changes from deleting to keeping */
				state = KEEPING;
				b = c;
			}
		} else {
			if(state == KEEPING) {
				/* changes from keeping to deleting */
				state = DELETING;
				if(a) {
					memmove(a, b, c - b);
					a += c - b;
				} else {
					a = c;
				}
			}
			al->size--;
		}
	}
	/* fixme: insert while foreach */
}

/** "Removes the element at the specified position in this list. Shifts any
 subsequent elements to the left (subtracts one from their indices)."
 <p>
 Fixme: untested.
 @param al		The List.
 @param index	"the index of the element to be removed"
 @return		The element removed. */
void *ListRemove(struct List *al, const int index) {
	void *object;

	assert(0);
	if(!al || index < 0 || index > al->size - 1) return 0;
	object = al->array + index * al->width;
	/*memmove();*/
	al->size--;
	return object;
}

/** "Removes all of the elements of this collection that satisfy the given
 predicate."
 <p>
 Fixme: untested.
 @param al		The List.
 @param filter	The predicate.
 @param remove	Done to the removed elements (eg, free?)
 @return		The number of elements were removed. */
int ListRemoveIf(struct List *al, int (* const filter)(void *), void  (* const remove)(void *)) {
	void *r;
	int ret = 0;
	int i;

	if(!al || !filter || !remove) return 0;
	assert(0);

	for(i = 0; i < al->size; i++) {
		if((*filter)(al->array[i])) {
			r = ListRemove(al, i);
			(*remove)(r);
			i--;
			ret++;
		}
	}
	return ret;
}

/** void removeRange(int fromIndex, int toIndex),
 void replaceAll(UnaryOperator<E> operator),
 int retainAll(Collection<?> c), boring */

/** "Replaces the element at the specified position in this list with the
 specified element."
 @param al		The List.
 @param index	"index of the element to replace"
 @param element	"element to be stored at the specified position"
 @return		"the element previously at the specified position" */
void *ListSet(struct List *al, const int index, void *const element) {
	void *prev;

	assert(0);
	if(!al || index < 0 || index >= al->size) return 0;

	prev = al->array[index];
	al->array[index] = element;

	return prev;
}

/** "Sorts this list according to the order induced by the specified
 Comparator."
 @param al		The List.
 @param c	"c - the Comparator used to compare list elements." */
void ListSort(const struct List *al, int (* const c)(const void *, const void *)) {
	if(!al || !c) return;
	qsort(al->array, al->size, al->width, c);
}

/** "Trims the capacity of this List instance to be the list's current
 size. An application can use this operation to minimize the storage of an
 List instance."
 @param al	The List. */
void ListTrimToSize(struct List *al) {
	void *array;
	int c0, c1;

	if(!al) return;

	/* we're good */
	if(al->capacity[0] >= al->size) return;

	/* re-alloc */
	c0 = al->size;
	c1 = c0 * 2;
	if(!(array = realloc(al->array, c0 * al->width))) {
		fprintf(stderr, "List: failed shrinking space for capacity %d, #%p.\n", c0, (void *)al);
		return;
	}
	al->array       = array;
	al->capacity[0] = c0;
	al->capacity[1] = c1;
}
#endif

/** See what's the error if something goes wrong. The error is reset by this
 action.
 @param al	List
 @return	The last error string.
 @throws	!al, E_NO_ERR */
char *ListError(struct List *const al) {
	char *str = 0;
	enum Error *error_p;
	int *errno_p;

	error_p = al ? &al->error : &error;
	errno_p = al ? &al->errno_copy : &errno_copy;
	switch(*error_p) {
		case E_NO_ERROR:
			break;
		case E_ERRNO:
			str = strerror(*errno_p);
			*errno_p = 0;
			break;
		case E_TOO_LARGE:
			str = "too large for index";
			break;
		case E_LOCKED:
			str = "modification not permitted";
			break;
	}
	*error_p = 0;
	return str;
}

/* private */

/** Fibonacci growing thing. */
static void grow(int *const a, int *const b) {
	*a ^= *b;
	*b ^= *a;
	*a ^= *b;
	*b += *a;
}
