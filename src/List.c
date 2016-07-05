/** Copyright 2015 Neil Edelman, distributed under the terms of the
 GNU General Public License, see copying.txt */

#include <stdlib.h> /* malloc free */
#include <stdio.h>  /* fprintf */
#include <string.h>	/* memcpy memmove strerror */
#include <errno.h>	/* global errno */
#include "List.h"

/** List like ArrayList in Java, but dangerous and unsafe because C. The
 structs are stored in a contagious array, not pointers. You specify sizeof in
 the constructor.
 <p>
 Incomplete and untested!
 
 @fixme		Have an extra level of indirection to avoid the mess of deletion,
			sorting, etc: have the indices change, not the data.
 @author	Neil
 @version	1.1; 2016-06
 @since		2015 */

static const int fibonacci6 = 8;
static const int fibonacci7 = 13;

enum Error {
	E_NO_ERROR,
	E_ERRNO,
	E_TOO_LARGE,
	E_LOCKED
};

enum Flags {
	F_NO_FLAGS = 0,
	F_LOCKED = 1,
	F_DEBUG = 2
};

struct List {

	size_t width;		/* the size of the elements in bytes */
	int    size;		/* the size of the list */
	int    capacity[2];	/* Fibonacci, [0] is the capacity, [1] is the next */

	char   *array;		/* the actual list, implemented as bytes * width */

	char   *iterator;	/* controls iteration */
	int    last_index;
	void   *last_param;

	enum Flags flags;	/* the flags, above */
	enum Error error;	/* errors defined by enum Error */
	int errno_copy;		/* copy of errno when when error == E_ERRNO */

};

/* global errors for allocation */
static enum Error global_error;
static int        global_errno_copy;

/* private prototypes */

static void grow(int *const a, int *const b);

/* public */

/** "Constructs an empty list with the specified initial capacity."
 @param width	The size of the objects that go in the list.
 @return		An object.
 @throws		width <= 0; call @see{ListGetError(0)} to see errors */
struct List *List(const size_t width) {
	struct List *l;

	if(width <= 0) return 0;

	if(!(l = malloc(sizeof(struct List)))) {
		global_error = E_ERRNO;
		global_errno_copy = errno;
		return 0;
	}

	l->width       = width;
	l->size        = 0;
	l->capacity[0] = fibonacci6;
	l->capacity[1] = fibonacci7;

	l->array       = 0;

	l->iterator    = 0;
	l->last_index  = 0;
	l->last_param  = 0;

	l->flags       = F_NO_FLAGS;
	l->error       = E_NO_ERROR;
	l->errno_copy  = 0;

	/* fixme: doesn't do anything, actually */
	if((l->flags & F_DEBUG)) fprintf(stderr, "List: new, capacity %d, #%p.\n",
		l->capacity[0], (void *)l);

	if(!(l->array = malloc(l->capacity[0] * width))) {
		global_error = E_ERRNO;
		global_errno_copy = errno;
		List_(&l);
		return 0;
	}

	return l;
}

/** Destructor.
 @param l_ptr	A reference to the object that is to be deleted. */
void List_(struct List **const l_ptr) {
	struct List *l;

	if(!l_ptr || !(l = *l_ptr)) return;

	if((l->flags & F_LOCKED)) fprintf(stderr, "~List: erasing #%p while it is "
		"locked.\n", (void *)l);
	if((l->flags & F_DEBUG)) fprintf(stderr, "~List: erase, #%p.\n", (void *)l);

	free(l->array);
	free(l);

	*l_ptr = 0;
}

/** "Increases the capacity of this List instance, if necessary, to ensure
 that it can hold at least the number of elements specified by the minimum
 capacity argument."
 @param min_capacity
 @return				True if the capacity increase was vaible; otherwise
						the list is not touched.
 @throws				!l; E_LOCKED, E_TOO_LARGE, E_ERRNO */
int ListEnsureCapacity(struct List *const l, const int min_capacity) {
	void *array;
	int c0, c1;

	if(!l) return 0;
	if((l->flags & F_LOCKED)) { l->error = E_LOCKED; return 0; }

	/* we're good */
	if(l->capacity[0] >= min_capacity) return -1;

	/* re-lloc */
	c0 = l->capacity[0];
	c1 = l->capacity[1];
	while(min_capacity > c0) {
		grow(&c0, &c1);
		if(c0 < 0) { l->error = E_TOO_LARGE; return 0; }
	}
	if(!(array = realloc(l->array, c0 * l->width))) {
		l->error = E_ERRNO;
		l->errno_copy = errno;
		return 0;
	}
	l->array = array;
	l->capacity[0] = c0;
	l->capacity[1] = c1;

	if((l->flags & F_DEBUG)) fprintf(stderr, "ListEnsureCapacity: #%p expanded "
		"to %d to fit %d elements.\n", (void *)l, c0, min_capacity);

	return -1;
}

/** "Returns the number of elements in this list."
 @param l	The List.
 @return	"the number of elements in this list"
 @throws	!l */
int ListSize(const struct List *const l) {
	if(!l) return 0;
	return l->size;
}

/* ----- */

/** "Appends the specified element to the end of this list."
 @param l	The List.
 @param e	"element to be appended to this list" (ie, copied)
 @return	True if the element could be added.
 @throws	!l, !e; E_LOCKED, E_TOO_LARGE, E_ERRNO */
int ListAdd(struct List *const l, const void *const e) {

	if(!l || !e) return 0;

	if((l->flags & F_LOCKED)) { l->error = E_LOCKED; return 0; }

	if(!ListEnsureCapacity(l, l->size + 1)) return 0;

	memcpy(l->array + l->size * l->width, e, l->width);
	l->size++;

	return -1;
}

/** "Returns the element at the specified position in this list."
 @param l		The List.
 @param index	"index of the element to return"
 @return		"the element at the specified position in this list"
 @throws		!l, 0 > index || index <= size; */
void *ListGet(const struct List *const l, const int index) {
	if(!l || index < 0 || index >= l->size) return 0;
	return l->array + index * l->width;
}

/** @fixme	Add. */
void ListRemove(struct List *const l, const int index) {
	if(!l || index < 0 || index >= l->size) return;
}

/** "Removes all of the elements from this list."
 @param l	The List.
 @throws	!l; E_LOCKED */
void ListClear(struct List *const l) {

	if(!l) return;

	if((l->flags & F_LOCKED)) { l->error = E_LOCKED; return; }

	l->size = 0;
}

/* ----- */

/** This is the only function that lets F_LOCKED outside.
 @fixme		Allow deletions/insertions.
 @param l	List.
 @throws	!l; E_LOCKED */
void *ListIterate(struct List *const l) {

	if(!l) return 0;

	if(!l->iterator) {
		if((l->flags & F_LOCKED)) { l->error = E_LOCKED; return 0; }
		l->flags |= F_LOCKED;
		l->iterator = l->array;
	} else {
		l->iterator += l->width;
	}
	if(l->iterator >= l->array + l->size * l->width) {
		l->iterator = 0;
		l->flags &= ~F_LOCKED;
	}

	return l->iterator;
}

/** "Performs the given action for each element." The topology of the list
 cannot be changed with Remove, Add, etc.
 @param l		List.
 @param action	"The action to be performed for each element."
				<p>
				void (*)(void *const).
 @throws		!l, !action; E_LOCKED */
void ListForEach(struct List *const l, const ListAction action) {
	int i;
	char *a;

	if(!l || !action) return;

	if((l->flags & F_LOCKED)) { l->error = E_LOCKED; return; }
	l->flags |= F_LOCKED;

	for(i = 0, a = l->array; i < l->size; i++, a += l->width) {
		l->last_index = i;
		(*action)(a);
	}
	l->last_index = 0;

	l->flags &= ~F_LOCKED;
}

/**
 @return	For all items true, true; one false, false, and quit looking.
 @throws	!l, !predicate; E_LOCKED */
int ListShortCircuit(struct List *const l, const ListPredicate predicate, void *const param) {
	int i, ret = -1;
	char *a;

	if(!l || !predicate) return 0;

	if((l->flags & F_LOCKED)) { l->error = E_LOCKED; return 0; }
	l->flags |= F_LOCKED;

	for(i = 0, a = l->array; i < l->size; i++, a += l->width) {
		l->last_index = i;
		if(!(*predicate)(a, param)) { ret = 0; break; }
	}
	l->last_index = 0;

	l->flags &= ~F_LOCKED;

	return ret;
}

/* "Removes all of the elements of this [List] that satisfy the given
 predicate."
 @param predicate	The predicate.
 @return			The number of deletions. */
int ListRemoveIf(struct List *const l, const ListPredicate predicate, void *const param) {
	int i, no = 0;
	char *a;

	if(!l || !predicate) return 0;

	if((l->flags & F_LOCKED)) { l->error = E_LOCKED; return 0; }
	l->flags |= F_LOCKED;

	for(i = 0, a = l->array; i < l->size; ) {
		l->last_index = i;
		if((*predicate)(a, param)) {
			no++;
			if(i >= --l->size) break;
			memcpy(l->array + l->size * l->width, l->array + i * l->width, l->width);
		} else {
			i++;
			a += l->width;
		}
	}
	l->last_index = 0;

	l->flags &= ~F_LOCKED;

	return no;
}

/* ----- */

/** Intended to be used inside a locked callback iteration.
 @return		The last index. */
int ListIndex(const struct List *const l) {
	if(!l) return 0;
	return l->last_index;
}

/** @param param	Sets the global parameter. */
void ListSetParam(struct List *const l, void *const param) {
	if(!l) return;
	l->last_param = param;
}

/** I always use it for something (usally debugging string.)
 @return		The param set by SetParam. */
void *ListGetParam(struct List *const l) {
	if(!l) return 0;
	return l->last_param;
}

/* ----- */

#if 0

/* "Removes all of the elements of this [List] that satisfy the given
 predicate." boring
 @param predicate	The predicate. */
void ListRemoveIf(struct List *const l, ListAction predicate) {
	/* a to b, delete, b to c, keep */
	char *a = 0, *b = 0, *c, *end;
	enum { KEEPING, DELETING } state = KEEPING;
	
	if(!l || !action) return;
	
	end  = l->array + l->size * l->width;
	
	for(c = l->array; c < end; c += l->width) {
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
			l->size--;
		}
	}
}

/** "Inserts the specified element at the specified position in this list."
 @param l		The List.
 @param index
 @param e		"element to be appended to this list"
 @return		True if the element could be added. */
int ListIndexAdd(struct List *l, const int index, void *const e) {
	if(!l || index < 0 || index > l->size) return 0;
	if((l->flags & F_LOCKED)) { error = E_LOCKED; return 0; }
	if(!ListEnsureCapacity(l, l->size + 1)) return 0;
	memmove(l->array + index * l->width, l->array + (index + 1) * l->width,
			(l->size - index) * l->width);
	memcpy(l->array + index * l->width, e, l->width);
	return -1;
}

/** Concatenates another List onto this one.
 <p>
 Fixme: untested.
 @param l	The List.
 @param c	Another List.
 @return	Success. */
int ListAddAll(struct List *l, const struct List *c) {
	int i, j;

	assert(0);

	if(!l) return 0;
	if(!c) return -1;

	if(!ListEnsureCapacity(l, l->size + c->size)) return 0;
	for(i = l->size, j = 0; j < c->size; i++, j++) {
		l->array[i] = c->array[j];
		l->size++;
	}

	return -1;
}

/** "Inserts all of the elements in the specified collection into this list,
 starting at the specified position. Shifts the element currently at that
 position (if any) and any subsequent elements to the right."
 <p>
 Fixme: untested.
 @param l		The List.
 @param index	"index at which to insert the first element"
 @param c		"element to be appended to this list"
 @return		Success. */
int ListIndexAddAll(struct List *l, const int index, const struct List *c) {
	int i, j;

	assert(0);

	if(!l || index < 0 || index > l->size) return 0;
	if(!c) return -1;

	if(!ListEnsureCapacity(l, l->size + c->size)) return 0;
	assert(0);
	for(i = l->size - 1, j = l->size - 1 + c->size - 1; i >= index; i--, j--) {
		l->array[j] = l->array[i];
	}
	for(j = 0; j < c->size; i++, j++) {
		l->array[i] = c->array[j];
		l->size++;
	}

	return -1;
}


/** "Removes the element at the specified position in this list. Shifts any
 subsequent elements to the left (subtracts one from their indices)."
 <p>
 Fixme: untested.
 @param l		The List.
 @param index	"the index of the element to be removed"
 @return		The element removed. */
void *ListRemove(struct List *l, const int index) {
	void *object;

	assert(0);
	if(!l || index < 0 || index > l->size - 1) return 0;
	object = l->array + index * l->width;
	/*memmove();*/
	l->size--;
	return object;
}

/** "Removes all of the elements of this collection that satisfy the given
 predicate."
 <p>
 Fixme: untested.
 @param l		The List.
 @param filter	The predicate.
 @param remove	Done to the removed elements (eg, free?)
 @return		The number of elements were removed. */
int ListRemoveIf(struct List *l, int (* const filter)(void *), void  (* const remove)(void *)) {
	void *r;
	int ret = 0;
	int i;

	if(!l || !filter || !remove) return 0;
	assert(0);

	for(i = 0; i < l->size; i++) {
		if((*filter)(l->array[i])) {
			r = ListRemove(l, i);
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
 @param l		The List.
 @param index	"index of the element to replace"
 @param element	"element to be stored at the specified position"
 @return		"the element previously at the specified position" */
void *ListSet(struct List *l, const int index, void *const element) {
	void *prev;

	assert(0);
	if(!l || index < 0 || index >= l->size) return 0;

	prev = l->array[index];
	l->array[index] = element;

	return prev;
}

/** "Sorts this list according to the order induced by the specified
 Comparator."
 @param l		The List.
 @param c	"c - the Comparator used to compare list elements." */
void ListSort(const struct List *l, int (* const c)(const void *, const void *)) {
	if(!l || !c) return;
	qsort(l->array, l->size, l->width, c);
}

/** "Trims the capacity of this List instance to be the list's current
 size. An application can use this operation to minimize the storage of an
 List instance."
 @param l	The List. */
void ListTrimToSize(struct List *l) {
	void *array;
	int c0, c1;

	if(!l) return;

	/* we're good */
	if(l->capacity[0] >= l->size) return;

	/* re-lloc */
	c0 = l->size;
	c1 = c0 * 2;
	if(!(array = relloc(l->array, c0 * l->width))) {
		fprintf(stderr, "List: failed shrinking space for capacity %d, #%p.\n", c0, (void *)l);
		return;
	}
	l->array       = array;
	l->capacity[0] = c0;
	l->capacity[1] = c1;
}
#endif

/** See what's the error if something goes wrong. The error is reset by this
 action.
 @param l	List
 @return	The last error string.
 @throws	!l, E_NO_ERR */
char *ListError(struct List *const l) {
	char *str = 0;
	enum Error *error_p;
	int *errno_p;

	error_p = l ? &l->error      : &global_error;
	errno_p = l ? &l->errno_copy : &global_errno_copy;
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
