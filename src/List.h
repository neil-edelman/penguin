struct List;

typedef void (*ListAction)(void *const);
typedef int (*ListPredicate)(void *const element, void *const param);

struct List *List(const size_t width);
void List_(struct List **al_ptr);
int ListAdd(struct List *al, const void *const e);
int ListIndexAdd(struct List *al, const int index, void *const e);
int ListAddAll(struct List *al, const struct List *c);
int ListIndexAddAll(struct List *al, const int index, const struct List *c);
void ListClear(struct List *al);
int ListEnsureCapacity(struct List *al, const int min_capacity);
void *ListIterate(struct List *const l);
void ListForEach(struct List *al, ListAction action);
int ListIndex(void);
void ListSetParam(void *const param);
void *ListGetParam(void);
int ListShortCircuit(struct List *al, const ListPredicate predicate, void *const param);
void *ListGet(const struct List *al, const int index);
void *ListRemove(struct List *al, const int index);
int ListRemoveIf(struct List *al, int (* const filter)(void *), void  (* const remove)(void *));
void *ListSet(struct List *al, const int index, void *const element);
int ListSize(const struct List *al);
void ListSort(const struct List *al, int (* const c)(const void *, const void *));
void ListTrimToSize(struct List *al);
char *ListError(struct List *const al);
