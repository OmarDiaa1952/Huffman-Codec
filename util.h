#ifndef DS_H
#define DS_H
#if defined _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include<stddef.h>//for size_t

#define		COUNTOF(ARR)		(sizeof(ARR)/sizeof(*(ARR)))
#define		G_BUF_SIZE	2048
extern char g_buf[G_BUF_SIZE];

int			log_error(const char *file, int line, const char *format, ...);
int			valid(const void *p);
#define		LOG_ERROR(format, ...)	log_error(file, __LINE__, format, ##__VA_ARGS__)
#define		ASSERT(SUCCESS)			((SUCCESS)!=0||log_error(file, __LINE__, #SUCCESS))
#define		ASSERT_P(POINTER)		(valid(POINTER)||log_error(file, __LINE__, #POINTER " == 0"))

void		memfill(void *dst, const void *src, size_t dstbytes, size_t srcbytes);
void		memswap_slow(void *p1, void *p2, size_t size);//TODO improve the simple byte loop
void 		memswap(void *p1, void *p2, size_t size, void *temp);
void		memreverse(void *p, size_t count, size_t size);//calls memswap
void 		memrotate(void *p, size_t byteoffset, size_t bytesize, void *temp);//temp buffer is min(byteoffset, bytesize-byteoffset)
void		memshuffle(void *base, ptrdiff_t count, size_t esize, int (*rand_fn)(void));
int 		binary_search(void *base, size_t count, size_t esize, int (*threeway)(const void*, const void*), const void *val, size_t *idx);//returns true if found, otherwise the idx is where val should be inserted
void 		isort(void *base, size_t count, size_t esize, int (*threeway)(const void*, const void*));//binary insertion sort


//array
#if 1
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4200)//no default-constructor for struct with zero-length array
#endif
typedef struct ArrayHeaderStruct
{
	size_t count, esize, cap;//cap is in bytes
	void (*destructor)(void*);
	unsigned char data[];
} ArrayHeader, *ArrayHandle;
typedef const ArrayHeader *ArrayConstHandle;
#ifdef _MSC_VER
#pragma warning(pop)
#endif
ArrayHandle		array_construct(const void *src, size_t esize, size_t count, size_t rep, size_t pad, void (*destructor)(void*));
ArrayHandle		array_copy(ArrayHandle *arr);//shallow
void			array_clear(ArrayHandle *arr);//keeps allocation
void			array_free(ArrayHandle *arr);
void			array_fit(ArrayHandle *arr, size_t pad);

void*			array_insert(ArrayHandle *arr, size_t idx, const void *data, size_t count, size_t rep, size_t pad);//cannot be nullptr
void*			array_erase(ArrayHandle *arr, size_t idx, size_t count);

size_t			array_size(ArrayHandle const *arr);
void*			array_at(ArrayHandle *arr, size_t idx);
const void*		array_at_const(ArrayConstHandle *arr, int idx);
void*			array_back(ArrayHandle *arr);
const void*		array_back_const(ArrayConstHandle *arr);

#define			ARRAY_ALLOC(ELEM_TYPE, ARR, COUNT, PAD, DESTRUCTOR)	ARR=array_construct(0, sizeof(ELEM_TYPE), COUNT, 1, PAD, DESTRUCTOR)
#define			ARRAY_APPEND(ARR, DATA, COUNT, REP, PAD)			array_insert(&(ARR), (ARR)->count, DATA, COUNT, REP, PAD)
#define			ARRAY_DATA(ARR)			(ARR)->data
#define			ARRAY_I(ARR, IDX)		*(int*)array_at(&ARR, IDX)
#define			ARRAY_U(ARR, IDX)		*(unsigned*)array_at(&ARR, IDX)
#define			ARRAY_F(ARR, IDX)		*(double*)array_at(&ARR, IDX)


//null terminated array
#define			ESTR_ALLOC(TYPE, STR, DATA, LEN)	STR=array_construct(DATA, sizeof(TYPE), LEN, 1, 1, 0)
#define			STR_APPEND(STR, SRC, LEN, REP)		array_insert(&(STR), (STR)->count, SRC, LEN, REP, 1)
#define			STR_FIT(STR)						array_fit(&STR, 1)
#define			ESTR_AT(TYPE, STR, IDX)				*(TYPE*)array_at(&(STR), IDX)

#define			STR_ALLOC(STR, LEN)				ESTR_ALLOC(char, STR, 0, LEN)
#define			STR_COPY(STR, DATA, LEN)		ESTR_ALLOC(char, STR, DATA, LEN)
#define			STR_AT(STR, IDX)				ESTR_AT(char, STR, IDX)

#define			WSTR_ALLOC(STR, LEN)			ESTR_ALLOC(wchar_t, STR, 0, LEN)
#define			WSTR_COPY(STR, DATA, LEN)		ESTR_ALLOC(wchar_t, STR, DATA, LEN)
#define			WSTR_AT(STR, IDX)				ESTR_AT(wchar_t, STR, IDX)
#endif

ArrayHandle		load_text(const char *filename, int pad);//don't forget to free string
int				save_text(const char *filename, const char *text, size_t len);


//single-linked list, queue and stack
#if 1
typedef struct SNodeStruct
{
	struct SNodeStruct *prev;
	unsigned char data[];//4-byte aligned on 32-bit, not suitable for double on 32-bit
} SNode, *SNodeHandle;
typedef struct SListStruct
{
	//[front] -> ... -> [back] -> nullptr
	size_t esize, count;
	void (*destructor)(void*);
	SNodeHandle
		front,	//can remove from or append to front
		back;	//prev always nullptr, can only append to back
} SList, *SListHandle;
//Double Linked List
typedef struct DNodeStruct
{
	struct DNodeStruct *prev, *next;
	unsigned char data[];
} DNodeHeader, *DNodeHandle;
typedef struct DListStruct
{
	DNodeHandle i, f;
	size_t
		objsize,	//size of one contained object
		objpernode,	//object count per node,		recommended value 128
		nnodes,		//node count
		nobj;		//total object count
	void (*destructor)(void*);
} DList, *DListHandle;
//iterator: seamlessly iterate through contained objects
typedef struct DListIteratorStruct
{
	DListHandle list;
	DNodeHandle node;
	size_t obj_idx;
} DListIterator, *DListItHandle;
void			dlist_first(DListHandle list, DListItHandle it);
void			dlist_last(DListHandle list, DListItHandle it);
void*			dlist_it_deref(DListItHandle it);
int				dlist_it_inc(DListItHandle it);
int				dlist_it_dec(DListItHandle it);
void			dlist_init(DListHandle list, size_t objsize, size_t objpernode, void (*destructor)(void*));
void			dlist_copy(DListHandle dst, DListHandle src);
void			dlist_clear(DListHandle list);
void			dlist_appendtoarray(DListHandle list, ArrayHandle *dst);
void*			dlist_push_back(DListHandle list, const void *obj);//shallow copy of obj	TODO dlist_push_back(array)
void*			dlist_back(DListHandle list);//returns address of last object
void			dlist_pop_back(DListHandle list);
void			dlist_first(DListHandle list, DListItHandle it);
void			dlist_last(DListHandle list, DListItHandle it);
//API
void slist_init(SListHandle list, size_t esize, void (*destructor)(void*));
void slist_clear(SListHandle list);
void* slist_push_front(SListHandle list, const void *data);
void* slist_push_back(SListHandle list, const void *data);
void* slist_front(SListHandle list);
void* slist_back(SListHandle list);
void slist_pop_front(SListHandle list);
void slist_print(SListHandle list, void (*printer)(const void*));

//list-based stack
#define STACK_PUSH(LIST, DATA)	dlist_push_back(LIST, DATA)
#define STACK_TOP(LIST)			dlist_back(LIST)
#define STACK_POP(LIST)			dlist_pop_back(LIST)

//list-based queue
#define QUEUE_ENQUEUE(LIST, DATA)	slist_push_back(LIST, DATA)
#define QUEUE_FRONT(LIST)			slist_front(LIST)
#define QUEUE_DEQUEUE(LIST)			slist_pop_front(LIST)
#endif


//bit-string
#if 1
typedef struct BitstringStruct
{
	size_t bitCount, byteCap;
	unsigned char data[];//8bits/element
	//unsigned data[];//32bits/element
} BitstringHeader, *BitstringHandle;
//-> [bitCount], [byteCap], data...
//arr->data		((char*)arr+sizeof(Header))

//bitstring_construct: allocates a new bit-string
//src: If 0, initialize memory with 0
//bitCount:  The number of bits in resulting array
//bytePad:  Number of zero-pad bytes
BitstringHandle bitstring_construct(const void *src, size_t bitCount, size_t bitOffset, size_t bytePad);

//bitstring_free: Frees allocated buffer and sets pointer to zero
//str: The bit-string to be freed
void bitstring_free(BitstringHandle *str);

//bitstring_append: Appends bits to bit-string
//str: The destination bit-string
//src: A pointer to the source data (bytes)
//bitCount:  The number of bits to read from src
//  Actually, this will read (bitCount+7)/8 bytes
//bitOffset: The number of bits to skip at the start of src. Must be between 0 and 7.
//For example, if s1 & s2 are bit-strings, the following should work properly:
//  bitstring_append(&s2, s1->data, s1->bitCount, 0);
void bitstring_append(BitstringHandle *str, const void *src, size_t bitCount, size_t bitOffset);

//bitstring_get: Get bit from bit-string
//str: The source bit-string
//bitIdx:  The index of bit to get
int bitstring_get(BitstringHandle *str, size_t bitIdx);

//bitstring_set: Set a certain bit in bit-string. Does not resize the buffer and calls some error function on out-of-bounds access.
//str: The destination bit-string
//bitIdx:  The index of bit to set
//bit: The bit value to set
void bitstring_set(BitstringHandle *str, size_t bitIdx, int bit);

//bitstring_print: Prints the bits in the bit-string
void bitstring_print(BitstringHandle str);
#endif


//Max-heap-based priority queue
#if 1
typedef struct PQueueStruct
{
	size_t count, //number of elements
	esize, //total element size in bytes
	byteCap;  //allocated buffer size in bytes
	int (*less)(const void*, const void*);//comparator function pointer
	void (*destructor)(void*);//element destructor, can be 0
	unsigned char data[];//each contained object begins with the key
} PQueueHeader, *PQueueHandle;

//pqueue_construct: allocates a new bit-string
//src:		If 0, initialize elements with 0
//esize:	Element size
//pad:		Initial array size
//less:		The comparator, returns 1 if the left argument is less
//destructor: Optional element destructor
PQueueHandle pqueue_construct(
	size_t esize,
	size_t pad,
	int (*less)(const void*, const void*),
	void (*destructor)(void*)
);
#define PQUEUE_ALLOC(TYPE, Q, PAD, LESS, DESTRUCTOR)	Q=pqueue_construct(sizeof(TYPE), PAD, LESS, DESTRUCTOR)

//pqueue_free: Frees allocated buffer and sets pointer to zero
void pqueue_free(PQueueHandle *pq);

//pqueue_enqueue:  Inserts one element to the queue
//pq: The destination priority queue
//src: Pointer to the source data
void pqueue_enqueue(PQueueHandle *pq, const void *src);

//pqueue_front: Returns address of the maximum element in priority queue
void* pqueue_front(PQueueHandle *pq);

//pqueue_dequeue: Removes heap root from priority queue
//Return type is void
void pqueue_dequeue(PQueueHandle *pq);

//pqueue_print:	Prints the contents of the heap array given an element printing function
void pqueue_print(PQueueHandle *pq, void (*printer)(const void*));

//pqueue_print_heap: Prints contents as heap
void pqueue_print_heap(PQueueHandle *pq, void (*printer)(const void*));
#endif


ArrayHandle		load_bin(const char *filename, int pad);

#endif

