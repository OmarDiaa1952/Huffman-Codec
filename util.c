#include"util.h"
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<stdarg.h>
#include<sys/stat.h>
//#include<x86intrin.h>
static const char file[]=__FILE__;

char g_buf[G_BUF_SIZE]={0};
//double linked list
#if 1
void			dlist_init(DListHandle list, size_t objsize, size_t objpernode, void (*destructor)(void*))
{
	list->i=list->f=0;
	list->objsize=objsize;
	list->objpernode=objpernode;
	list->nnodes=list->nobj=0;//empty
	list->destructor=destructor;
}
#define			DLIST_COPY_NODE(DST, PREV, NEXT, SRC, PAYLOADSIZE)\
	DST=(DNodeHandle)malloc(sizeof(DNodeHeader)+(PAYLOADSIZE)),\
	DST->prev=PREV,\
	DST->next=NEXT,\
	memcpy(DST->data, SRC->data, PAYLOADSIZE)
void			dlist_copy(DListHandle dst, DListHandle src)
{
	DNodeHandle it;
	size_t payloadsize;

	dlist_init(dst, src->objsize, src->objpernode, src->destructor);
	it=dst->i;
	if(it)
	{
		payloadsize=src->objpernode*src->objsize;

		DLIST_COPY_NODE(dst->f, 0, 0, it, payloadsize);
		//dst->f=(DNodeHandle)malloc(sizeof(DNodeHeader)+payloadsize);
		//memcpy(dst->f->data, it->data, payloadsize);

		dst->i=dst->f;

		it=it->next;

		for(;it;it=it->next)
		{
			DLIST_COPY_NODE(dst->f->next, dst->f, 0, it, payloadsize);
			dst->f=dst->f->next;
		}
	}
	dst->nnodes=src->nnodes;
	dst->nobj=src->nobj;
}
void			dlist_clear(DListHandle list)
{
	DNodeHandle it;

	it=list->i;
	if(it)
	{
		while(it->next)
		{
			if(list->destructor)
			{
				for(size_t k=0;k<list->objpernode;++k)
					list->destructor(it->data+k*list->objsize);
				list->nobj-=list->objpernode;
			}
			it=it->next;
			free(it->prev);
		}
		if(list->destructor)
		{
			for(size_t k=0;k<list->nobj;++k)
				list->destructor(it->data+k*list->objsize);
		}
		free(it);
		list->i=list->f=0;
		list->nobj=list->nnodes=0;
	}
}
void			dlist_appendtoarray(DListHandle list, ArrayHandle *dst)
{
	DNodeHandle it;
	size_t payloadsize;

	if(!*dst)
		*dst=array_construct(0, list->objsize, 0, 0, list->nnodes*list->objpernode, list->destructor);
	else
	{
		if(dst[0]->esize!=list->objsize)
		{
			LOG_ERROR("dlist_appendtoarray(): dst->esize=%d, list->objsize=%d", dst[0]->esize, list->objsize);
			return;
		}
		ARRAY_APPEND(*dst, 0, 0, 0, list->nnodes*list->objpernode);
	}
	it=list->i;
	payloadsize=list->objpernode*list->objsize;
	for(size_t offset=dst[0]->count;it;)
	{
		memcpy(dst[0]->data+offset*list->objsize, it->data, payloadsize);
		offset+=list->objpernode;
		it=it->next;
	}
	dst[0]->count+=list->nobj;
}

void*			dlist_push_back(DListHandle list, const void *obj)
{
	size_t obj_idx=list->nobj%list->objpernode;
	if(!obj_idx)//need a new node
	{
		DNodeHandle temp=(DNodeHandle)malloc(sizeof(DNodeHeader)+list->objpernode*list->objsize);
		if(list->nnodes)
		{
			list->f->next=temp;
			temp->prev=list->f;
			temp->next=0;
			list->f=temp;
		}
		else
		{
			temp->prev=temp->next=0;
			list->i=list->f=temp;
		}
		++list->nnodes;
	}
	void *p=list->f->data+obj_idx*list->objsize;
	if(obj)
		memcpy(p, obj, list->objsize);
	else
		memset(p, 0, list->objsize);
	++list->nobj;
	return p;
}
void*			dlist_back(DListHandle list)
{
	size_t obj_idx;

	if(!list->nobj)
		LOG_ERROR("dlist_back() called on empty list");
	obj_idx=(list->nobj-1)%list->objpernode;
	return list->f->data+obj_idx*list->objsize;
}
void			dlist_pop_back(DListHandle list)
{
	size_t obj_idx;

	if(!list->nobj)
		LOG_ERROR("dlist_pop_back() called on empty list");
	if(list->destructor)
		list->destructor(dlist_back(list));
	obj_idx=(list->nobj-1)%list->objpernode;
	if(!obj_idx)//last object is first in the last block
	{
		DNodeHandle last=list->f;
		list->f=list->f->prev;
		free(last);
		--list->nnodes;
		if(!list->nnodes)//last object was popped out
			list->i=0;
	}
	--list->nobj;
}

void			dlist_first(DListHandle list, DListItHandle it)
{
	it->list=list;
	it->node=list->i;
	it->obj_idx=0;
}
void			dlist_last(DListHandle list, DListItHandle it)
{
	it->list=list;
	it->node=list->f;
	it->obj_idx=(list->nobj-1)%list->objpernode;
}
void*			dlist_it_deref(DListItHandle it)
{
	if(it->obj_idx>=it->list->nobj)
		LOG_ERROR("dlist_it_deref() out of bounds");
	if(!it->node)
		LOG_ERROR("dlist_it_deref() node == nullptr");
	return it->node->data+it->obj_idx%it->list->objpernode*it->list->objsize;
}
int				dlist_it_inc(DListItHandle it)
{
	++it->obj_idx;
	if(it->obj_idx>=it->list->objpernode)
	{
		it->obj_idx=0;
		if(!it->node||!it->node->next)
			return 0;
			//LOG_ERROR("dlist_it_inc() attempting to read node == nullptr");
		it->node=it->node->next;
	}
	return 1;
}
int				dlist_it_dec(DListItHandle it)
{
	if(it->obj_idx)
		--it->obj_idx;
	else
	{
		if(!it->node||!it->node->prev)
			return 0;
			//LOG_ERROR("dlist_it_dec() attempting to read node == nullptr");
		it->node=it->node->prev;
		it->obj_idx=it->list->objpernode-1;
	}
	return 1;
}
#endif

BitstringHandle bitstring_construct(const void* src, size_t bitCount, size_t bitOffset, size_t bytePad) {
	BitstringHandle str;
	size_t srcSize, cap;
	unsigned char* srcBytes = (unsigned char*)src;

	srcSize = (bitCount + 7) >> 3;
	cap = srcSize + bytePad;
	str = (BitstringHandle)malloc(sizeof(BitstringHeader) + cap);
	str->bitCount = bitCount;
	str->byteCap = cap;

	memset(str->data, 0, cap);
	if (src) {
		for (size_t b = 0; b < bitCount; ++b) {
			int bit = srcBytes[(bitOffset + b) >> 3] >> ((bitOffset + b) & 7) & 1;
			str->data[b >> 3] |= bit << (b & 7);
		}
	}
	return str;
}

void bitstring_free(BitstringHandle* str) {
	free(*str);
	*str = 0;
}

void bitstring_append(BitstringHandle* str, const void* src, size_t bitCount, size_t bitOffset) {
	size_t reqcap, newcap;
	void* p;
	size_t byteIdx;
	unsigned char* srcBytes = (unsigned char*)src;

	newcap = str[0]->byteCap;
	newcap += !newcap;
	reqcap = (str[0]->bitCount + bitCount + 7) / 8;
	for (; newcap < reqcap; newcap <<= 1);

	if (str[0]->byteCap < newcap) {
		p = realloc(*str, sizeof(BitstringHeader) + newcap);
		if (!p) {
			LOG_ERROR("Realloc returned nullptr");
		}
		*str = p;
		str[0]->byteCap = newcap;
	}

	byteIdx = (str[0]->bitCount + 7) / 8;
	memset(str[0]->data + byteIdx, 0, newcap - byteIdx);
	if (src) {
		for (size_t b = 0; b < bitCount; ++b) {
			int bit = srcBytes[(bitOffset + b) >> 3] >> ((bitOffset + b) & 7) & 1;
			str[0]->data[(str[0]->bitCount + b) >> 3] |= bit << ((str[0]->bitCount + b) & 7);
		}
	}

	str[0]->bitCount += bitCount;
}

int bitstring_get(BitstringHandle* str, size_t bitIdx) {

	if (!*str) {
		LOG_ERROR("bitstring_get OOB: str = %p", *str);
		return 0;
	}

	if (bitIdx >= str[0]->bitCount) {
		LOG_ERROR("bitstring_get OOB: Count = %lld, bitIdx = %lld", (long long)str[0]->bitCount, (long long)bitIdx);
		return 0;
	}

	return str[0]->data[bitIdx >> 3] >> (bitIdx & 7) & 1;
}

void bitstring_set(BitstringHandle* str, size_t bitIdx, int bit) {

	if (!*str) {
		LOG_ERROR("bitstring_get OOB: str = %p", *str);
		return;
	}

	if (bitIdx >= str[0]->bitCount) {
		LOG_ERROR("bitstring_get OOB: Count = %lld, bitIdx = %lld", (long long)str[0]->bitCount, (long long)bitIdx);
		return;
	}

	if (bit)
		str[0]->data[bitIdx >> 3] |= 1 << (bitIdx & 7);
	else
		str[0]->data[bitIdx >> 3] &= 1 << (bitIdx & 7);
}

void bitstring_print(BitstringHandle str) {

	for (int i = 0; i < str->bitCount; ++i) {
		printf("%d", bitstring_get(&str, i));
	}
	printf("\n");
}

int		nErrors=0;
int		log_error(const char *file, int line, const char *format, ...)
{
	va_list args;

	printf("[%d] %s(%d)", nErrors, file, line);
	if(format)
	{
		printf(" ");
		va_start(args, format);
		vprintf(format, args);
		va_end(args);
		printf("\n");
	}
	else
		printf("Unknown error\n");
	++nErrors;
	return 0;
}
int					valid(const void *p)
{
	size_t val=(size_t)p;

	if(sizeof(size_t)==4)
	{
		switch(val)
		{
		case 0:
		case 0xCCCCCCCC:
		case 0xFEEEFEEE:
		case 0xEEFEEEFE:
		case 0xCDCDCDCD:
		case 0xFDFDFDFD:
		case 0xBAAD0000:
			return 0;
		}
	}
	else
	{
		if(val==0xCCCCCCCCCCCCCCCC)
			return 0;
		if(val==0xFEEEFEEEFEEEFEEE)
			return 0;
		if(val==0xEEFEEEFEEEFEEEFE)
			return 0;
		if(val==0xCDCDCDCDCDCDCDCD)
			return 0;
		if(val==0xBAADF00DBAADF00D)
			return 0;
		if(val==0xADF00DBAADF00DBA)
			return 0;
	}
	return 1;
}

void				memfill(void *dst, const void *src, size_t dstbytes, size_t srcbytes)
{
	unsigned copied;
	char *d=(char*)dst;
	const char *s=(const char*)src;
	if(dstbytes<srcbytes)
	{
		memcpy(dst, src, dstbytes);
		return;
	}
	copied=srcbytes;
	memcpy(d, s, copied);
	while(copied<<1<=dstbytes)
	{
		memcpy(d+copied, d, copied);
		copied<<=1;
	}
	if(copied<dstbytes)
		memcpy(d+copied, d, dstbytes-copied);
}
void				memswap_slow(void *p1, void *p2, size_t size)
{
	unsigned char *s1=(unsigned char*)p1, *s2=(unsigned char*)p2, *end=s1+size;
	for(;s1<end;++s1, ++s2)
	{
		const unsigned char t=*s1;
		*s1=*s2;
		*s2=t;
	}
}
void 				memswap(void *p1, void *p2, size_t size, void *temp)
{
	memcpy(temp, p1, size);
	memcpy(p1, p2, size);
	memcpy(p2, temp, size);
}
void				memreverse(void *p, size_t count, size_t esize)
{
	size_t totalsize=count*esize;
	unsigned char *s1=(unsigned char*)p, *s2=s1+totalsize-esize;
	void *temp=malloc(esize);
	while(s1<s2)
	{
		memswap(s1, s2, esize, temp);
		s1+=esize, s2-=esize;
	}
	free(temp);
}
void 				memrotate(void *p, size_t byteoffset, size_t bytesize, void *temp)
{
	unsigned char *buf=(unsigned char*)p;
	
	if(byteoffset<bytesize-byteoffset)
	{
		memcpy(temp, buf, byteoffset);
		memmove(buf, buf+byteoffset, bytesize-byteoffset);
		memcpy(buf+bytesize-byteoffset, temp, byteoffset);
	}
	else
	{
		memcpy(temp, buf+byteoffset, bytesize-byteoffset);
		memmove(buf+bytesize-byteoffset, buf, byteoffset);
		memcpy(buf, temp, bytesize-byteoffset);
	}
}
void				memshuffle(void *base, ptrdiff_t count, size_t esize, int (*rand_fn)(void))
{
	ptrdiff_t k, k2;
	void *temp;

	temp=malloc(esize);
	for(k=0;k<count-1;++k)
	{
		k2=rand_fn()%count;
		memswap(&base+k*esize, &base+k2*esize, esize, temp);
	}
	free(temp);
}
int 				binary_search(void *base, size_t count, size_t esize, int (*threeway)(const void*, const void*), const void *val, size_t *idx)
{
	unsigned char *buf=(unsigned char*)base;
	ptrdiff_t L=0, R=(ptrdiff_t)count-1, mid;
	int ret;

	while(L<=R)
	{
		mid=(L+R)>>1;
		ret=threeway(buf+mid*esize, val);
		if(ret<0)
			L=mid+1;
		else if(ret>0)
			R=mid-1;
		else
		{
			if(idx)
				*idx=mid;
			return 1;
		}
	}
	if(idx)
		*idx=L+(L<(ptrdiff_t)count&&threeway(buf+L*esize, val)<0);
	return 0;
}
void 				isort(void *base, size_t count, size_t esize, int (*threeway)(const void*, const void*))
{
	unsigned char *buf=(unsigned char*)base;
	size_t k;
	void *temp;

	if(count<2)
		return;

	temp=malloc((count>>1)*esize);
	for(k=1;k<count;++k)
	{
		size_t idx=0;
		binary_search(buf, k, esize, threeway, buf+k*esize, &idx);
		if(idx<k)
			memrotate(buf+idx*esize, (k-idx)*esize, (k+1-idx)*esize, temp);
	}
	free(temp);
}


//C array
#if 1
static void		array_realloc(ArrayHandle *arr, size_t count, size_t pad)//CANNOT be nullptr, array must be initialized with array_alloc()
{
	ArrayHandle p2;
	size_t size, newcap;

	ASSERT_P(*arr);
	size=(count+pad)*arr[0]->esize, newcap=arr[0]->esize;
	for(;newcap<size;newcap<<=1);
	if(newcap>arr[0]->cap)
	{
		p2=(ArrayHandle)realloc(*arr, sizeof(ArrayHeader)+newcap);
		ASSERT_P(p2);
		*arr=p2;
		if(arr[0]->cap<newcap)
			memset(arr[0]->data+arr[0]->cap, 0, newcap-arr[0]->cap);
		arr[0]->cap=newcap;
	}
	arr[0]->count=count;
}

//Array API
ArrayHandle		array_construct(const void *src, size_t esize, size_t count, size_t rep, size_t pad, void (*destructor)(void*))
{
	ArrayHandle arr;
	size_t srcsize, dstsize, cap;
	
	srcsize=count*esize;
	dstsize=rep*srcsize;
	for(cap=esize+pad*esize;cap<dstsize;cap<<=1);
	arr=(ArrayHandle)malloc(sizeof(ArrayHeader)+cap);
	ASSERT_P(arr);
	arr->count=count;
	arr->esize=esize;
	arr->cap=cap;
	arr->destructor=destructor;
	if(src)
	{
		ASSERT_P(src);
		memfill(arr->data, src, dstsize, srcsize);
	}
	else
		memset(arr->data, 0, dstsize);
		
	if(cap-dstsize>0)//zero pad
		memset(arr->data+dstsize, 0, cap-dstsize);
	return arr;
}
ArrayHandle		array_copy(ArrayHandle *arr)
{
	ArrayHandle a2;
	size_t bytesize;

	if(!*arr)
		return 0;
	bytesize=sizeof(ArrayHeader)+arr[0]->cap;
	a2=(ArrayHandle)malloc(bytesize);
	ASSERT_P(a2);
	memcpy(a2, *arr, bytesize);
	return a2;
}
void			array_clear(ArrayHandle *arr)//can be nullptr
{
	if(*arr)
	{
		if(arr[0]->destructor)
		{
			for(size_t k=0;k<arr[0]->count;++k)
				arr[0]->destructor(array_at(arr, k));
		}
		arr[0]->count=0;
	}
}
void			array_free(ArrayHandle *arr)//can be nullptr
{
	if(*arr&&arr[0]->destructor)
	{
		for(size_t k=0;k<arr[0]->count;++k)
			arr[0]->destructor(array_at(arr, k));
	}
	free(*arr);
	*arr=0;
}
void			array_fit(ArrayHandle *arr, size_t pad)//can be nullptr
{
	ArrayHandle p2;
	if(!*arr)
		return;
	arr[0]->cap=(arr[0]->count+pad)*arr[0]->esize;
	p2=(ArrayHandle)realloc(*arr, sizeof(ArrayHeader)+arr[0]->cap);
	ASSERT_P(p2);
	*arr=p2;
}

void*			array_insert(ArrayHandle *arr, size_t idx, const void *data, size_t count, size_t rep, size_t pad)//cannot be nullptr
{
	size_t start, srcsize, dstsize, movesize;
	
	ASSERT_P(*arr);
	start=idx*arr[0]->esize;
	srcsize=count*arr[0]->esize;
	dstsize=rep*srcsize;
	movesize=arr[0]->count*arr[0]->esize-start;
	array_realloc(arr, arr[0]->count+rep*count, pad);
	memmove(arr[0]->data+start+dstsize, arr[0]->data+start, movesize);
	if(data)
		memfill(arr[0]->data+start, data, dstsize, srcsize);
	else
		memset(arr[0]->data+start, 0, dstsize);
	return arr[0]->data+start;
}
void*			array_erase(ArrayHandle *arr, size_t idx, size_t count)
{
	size_t k;

	ASSERT_P(*arr);
	if(arr[0]->count<idx+count)
	{
		LOG_ERROR("array_erase() out of bounds: idx=%lld count=%lld size=%lld", (long long)idx, (long long)count, (long long)arr[0]->count);
		if(arr[0]->count<idx)
			return 0;
		count=arr[0]->count-idx;//erase till end of array if OOB
	}
	if(arr[0]->destructor)
	{
		for(k=0;k<count;++k)
			arr[0]->destructor(array_at(arr, idx+k));
	}
	memmove(arr[0]->data+idx*arr[0]->esize, arr[0]->data+(idx+count)*arr[0]->esize, (arr[0]->count-(idx+count))*arr[0]->esize);
	arr[0]->count-=count;
	return arr[0]->data+idx*arr[0]->esize;
}

size_t			array_size(ArrayHandle const *arr)//can be nullptr
{
	if(!arr[0])
		return 0;
	return arr[0]->count;
}
void*			array_at(ArrayHandle *arr, size_t idx)
{
	if(!arr[0])
		return 0;
	if(idx>=arr[0]->count)
		return 0;
	return arr[0]->data+idx*arr[0]->esize;
}
const void*		array_at_const(ArrayConstHandle *arr, int idx)
{
	if(!arr[0])
		return 0;
	return arr[0]->data+idx*arr[0]->esize;
}
void*			array_back(ArrayHandle *arr)
{
	if(!*arr||!arr[0]->count)
		return 0;
	return arr[0]->data+(arr[0]->count-1)*arr[0]->esize;
}
const void*		array_back_const(ArrayConstHandle *arr)
{
	if(!*arr||!arr[0]->count)
		return 0;
	return arr[0]->data+(arr[0]->count-1)*arr[0]->esize;
}
#endif


ArrayHandle		load_bin(const char *filename, int pad)
{
	struct stat info={0};
	FILE *f;
	ArrayHandle str;

	int error=stat(filename, &info);
	if(error)
	{
		strerror_s(g_buf, G_BUF_SIZE, errno);
		LOG_ERROR("Cannot open %s\n%s", filename, g_buf);
		return 0;
	}
	//fopen_s(&f, filename, "rb");
	//f=fopen(filename, "r");
	f=fopen(filename, "rb");
	//f=fopen(filename, "r, ccs=UTF-8");//gets converted to UTF-16 on Windows

	str=array_construct(0, 1, info.st_size, 1, pad+1, 0);
	str->count=fread(str->data, 1, info.st_size, f);
	fclose(f);
	memset(str->data+str->count, 0, str->cap-str->count);
	return str;
}
void				pqueue_heapifyup(PQueueHandle *pq, size_t idx , void *temp){
	for(;idx!=0;){
		size_t parent=(idx-1)/2;
		if(pq[0]->less(pq[0]->data + parent * pq[0]->esize, pq[0]->data+idx*pq[0]->esize))
			memswap(pq[0]->data+parent*pq[0]->esize,pq[0]->data + idx*pq[0]->esize,pq[0]->esize,temp);
		else
			break;
		idx=parent;
	}
}
void 				pqueue_heapifydown(PQueueHandle *pq,size_t idx , void *temp)
{
	size_t L,R,largest;
	for(;idx<pq[0]->count;){
		L=(idx<<1)+1 , R=L+1;
		if(L<pq[0]->count&&pq[0]->less(pq[0]->data+idx*pq[0]->esize,pq[0]->data+L*pq[0]->esize))
		{
			largest=L;
		}
		else{
			largest=idx;
		}
		if(R<pq[0]->count&&pq[0]->less(pq[0]->data+largest*pq[0]->esize,pq[0]->data+R*pq[0]->esize))
		{
			largest=R;
		}
		if(largest==idx)
		{
			break;
		}
		memswap(pq[0]->data+idx*pq[0]->esize,pq[0]->data + largest*pq[0]->esize,pq[0]->esize,temp);
		idx=largest;
	}
}
void pqueue_buildheap(PQueueHandle *pq)
{
	void *temp;
	temp = malloc(pq[0]->esize);
    for(size_t i=pq[0]->count/2-1;i>=0;--i)
        pqueue_heapifydown(pq,i, temp);

	free(temp);
}




//Max heap based Priorty Queue
#if 1
static void		pqueue_realloc(PQueueHandle* pq, size_t count, size_t pad)//CANNOT be nullptr, array must be initialized with array_alloc()
{
	PQueueHandle p2;
	size_t reqSize, newcap;

	ASSERT_P(*pq);
	reqSize = (count + pad) * pq[0]->esize, newcap = pq[0]->esize;
	for (; newcap < reqSize; newcap <<= 1);
	if (newcap > pq[0]->byteCap)
	{
		p2 = (PQueueHandle)realloc(*pq, sizeof(PQueueHeader) + newcap);
		ASSERT_P(p2);
		*pq = p2;
		if (pq[0]->byteCap < newcap)
			memset(pq[0]->data + pq[0]->byteCap, 0, newcap - pq[0]->byteCap);
		pq[0]->byteCap = newcap;
	}
	pq[0]->count = count;
}

//Array API
PQueueHandle	pqueue_construct( size_t esize, size_t pad, int (*less)(const void*, const void*), void (*destructor)(void*))
{
	PQueueHandle pq;
	size_t  cap;

    cap=esize+pad*esize;
	pq=(PQueueHandle)malloc(sizeof(PQueueHeader)+cap);
	ASSERT_P(pq);
	pq->count=0;
	pq->esize=esize;
	pq->byteCap=cap;
	pq->destructor=destructor;
    pq->less=less;
	
    memset(pq->data, 0, cap);

    return pq;
}

void			pqueue_free(PQueueHandle *pq)//can be nullptr
{
	if(*pq&&pq[0]->destructor)
	{
		for(size_t k=0;k<pq[0]->count;++k)
			pq[0]->destructor(pq[0]->data+k*pq[0]->esize);
	}
	free(*pq);
	*pq=0;
}

void			pqueue_enqueue(PQueueHandle *pq,const void *src )
{
	size_t start;
    void *temp;

	ASSERT_P(*pq);
	start=pq[0]->count*pq[0]->esize;
    pqueue_realloc(pq, pq[0]->count+1, 0);

    memcpy(pq[0]->data+start, src, pq[0]->esize);
    temp=malloc(pq[0]->esize);
    pqueue_heapifyup(pq, pq[0]->count-1, temp);
    free(temp);

}

void			pqueue_dequeue(PQueueHandle *pq)
{
    void *temp;
	ASSERT_P(*pq);
	if(!pq[0]->count)
	{
		LOG_ERROR("pqueue_erase() out of bounds: size=%lld", (long long)pq[0]->count);
		return;
	}

	if(pq[0]->destructor)
        pq[0]->destructor(pq[0]->data);

    temp=malloc(pq[0]->esize); 
    memswap(pq[0]->data, pq[0]->data+(pq[0]->count-1)*pq[0]->esize, pq[0]->esize, temp);
    --pq[0]->count;   
	
    pqueue_heapifydown(pq,0,temp);
    free(temp);
	
}

void* 			pqueue_front(PQueueHandle *pq){
	return pq[0]->data;
}
#endif

ArrayHandle		load_text(const char *filename, int pad)
{
	struct stat info={0};
	FILE *f;
	ArrayHandle str;

	int error=stat(filename, &info);
	if(error)
	{
		LOG_ERROR("Cannot open %s\n%s", filename, strerror(errno));
		return 0;
	}
	f=fopen(filename, "r");
	//f=fopen(filename, "rb");
	//f=fopen(filename, "r, ccs=UTF-8");//gets converted to UTF-16 on Windows

	str=array_construct(0, 1, info.st_size, 1, pad+1, 0);
	str->count=fread(str->data, 1, info.st_size, f);
	fclose(f);
	memset(str->data+str->count, 0, str->cap-str->count);
	return str;
}
int				save_text(const char *filename, const char *text, size_t len)
{
	FILE *f=fopen(filename, "w");
	if(!f)
		return 0;
	fwrite(text, 1, len, f);
	fclose(f);
	return 1;
}

/*int 			save_file_bin(const char *filename,const unsigned char *src , size_t srcSize)
{
	FILE *f;
	size_t bytesRead;
	f=fopen(filename, "wb");
	//fopen_s(&f , filename , "wb");
	if(!f){
		printf("Failed to save %s\n",filename);
		return 0;
	}
	bytesRead=fwrite(src,1,srcSize,f);
	fclose(f);
	if(bytesRead!=srcSize)
	{
		printf("Failed to save %s\n" , filename);
		return 0;
	}
	return 1;
}*/
