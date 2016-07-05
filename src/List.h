struct List;

typedef void (*ListAction)(void *const);
typedef int (*ListPredicate)(void *const element, void *const param);

struct List *List(const size_t width);
void List_(struct List **const l_ptr);
int ListEnsureCapacity(struct List *const l, const int min_capacity);
int ListSize(const struct List *const l);

int ListAdd(struct List *const l, const void *const e);
void *ListGet(const struct List *const l, const int index);
void ListRemove(struct List *const l, const int index);
void ListClear(struct List *const l);

void *ListIterate(struct List *const l);
void ListForEach(struct List *const l, const ListAction action);
int ListShortCircuit(struct List *const l, const ListPredicate predicate, void *const param);
int ListRemoveIf(struct List *const l, const ListPredicate predicate, void *const param);

int ListIndex(const struct List *const l);
void ListSetParam(struct List *const l, void *const param);
void *ListGetParam(struct List *const l);

char *ListError(struct List *const l);
