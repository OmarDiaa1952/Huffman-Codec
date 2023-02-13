#include"util.h"
#include<stdio.h>
#include<stdlib.h>
#include<stdarg.h>
#include<string.h>
//#include<x86intrin.h>

const int huffTag ='H'|'U'<<8|'F'<<16|'F'<<24;

static const char file[] = __FILE__;


#if 1
typedef struct HuffFileHeaderStruct
{
	int magic, zeros;
	size_t u_size, c_size;
	size_t hist[256];
	unsigned char data[];
} HuffFileHeader , *HuffFileHandle;
size_t hist[256] = { 0 };
HuffFileHeader header={0};
BitstringHandle alphabet[256] = { 0 };
typedef struct HuffNodeStruct {
	struct HuffNodeStruct* left, * right; //0 and 1
	size_t symbol, freq;
	BitstringHandle code;
} HuffNode, *HuffNodeHandle;
int huffnode_less(const void *left, const void *right);
void huffnode_print(const void *data);
void print_tree(HuffNodeHandle root, int depth) ;
void free_tree(HuffNodeHandle *root);
void gen_histogram(const unsigned char *data, size_t nBytes, size_t *hist);
HuffNodeHandle gen_tree(size_t *hist);
void gen_alphapet(HuffNodeHandle root, BitstringHandle *alphapet);
void print_hist(size_t *hist) ;
void print_alphapet(BitstringHandle *alphabet, size_t *hist, size_t nsymbols);
int huff_compress(const unsigned char *src,size_t srcsize,ArrayHandle *dst);
int huff_decompress(const unsigned char *src,size_t srcsize,ArrayHandle *dst);
int 			save_file_bin(const char *filename,const unsigned char *src , size_t srcSize);
ArrayHandle		load_text(const char *filename, int pad);
#endif
void	pause()
{
	int k;

	printf("Enter 0 to continue: ");
	scanf("%d", &k);
}


int huffnode_less(const void *left, const void *right) {
	HuffNode *L = *(HuffNode**)left, *R = *(HuffNode**)right;
	return L->freq > R->freq; //for min heap
}

void huffnode_print(const void *data) {
	HuffNodeHandle p = *(HuffNodeHandle*)data;

	if (p->symbol)
		printf("%d:%d", p->freq, p->symbol);
	else
		printf("%d", p->freq);

	if (p->code) {
		printf("Code = ");
		bitstring_print(p->code);
	}

	printf("\n");
}

void print_tree(HuffNodeHandle root, int depth) {
	
	if (root) {
		print_tree(root->left, depth + 1);

		printf("%3d %*s", depth, depth, "");
		huffnode_print(&root);

		print_tree(root->right, depth + 1);
	}
}

void free_tree(HuffNodeHandle *root) {

	if (*root) {
		free_tree(&root[0]->left);
		free_tree(&root[0]->right);
		bitstring_free(&root[0]->code);
		free(*root);
		*root = 0;
	}
}

void gen_histogram(const unsigned char *data, size_t nBytes, size_t *hist) {
	memset(hist, 0, 256 * sizeof(size_t));
	for (int i = 0; i < nBytes; ++i)
		++hist[data[i]];
}

HuffNodeHandle gen_tree(size_t *hist) {
	PQueueHandle q;
	HuffNodeHandle node, L, R;

	PQUEUE_ALLOC(HuffNodeHandle, q, 0, huffnode_less, 0);
	for (int k = 0; k < 256; ++k) {
		if (hist[k]) {
			node = (HuffNodeHandle)malloc(sizeof(HuffNode));
			node->left = node->right = 0;
			node->symbol = k;
			node->freq = hist[k];
			node->code = 0;
			pqueue_enqueue(&q, &node);
		}
	}

	for (int it = 0; q->count > 1; ++it) {

		L = *(HuffNodeHandle*)q->data;
		pqueue_dequeue(&q);

		R = *(HuffNodeHandle*)q->data;
		pqueue_dequeue(&q);


		//-------------------
		node = (HuffNodeHandle)malloc(sizeof(HuffNode));
		node->symbol = 0;
		node->freq = L->freq + R->freq;
		node->left = L;
		node->right = R;
		node->code = 0;

		pqueue_enqueue(&q, &node);
	}

	node = *(HuffNodeHandle*)q->data;
	pqueue_free(&q);
	return node;
}

void gen_alphapet(HuffNodeHandle root, BitstringHandle *alphapet) {
	DList s;
	HuffNodeHandle node;
	BitstringHandle str;
	char bit;

	dlist_init(&s, sizeof(HuffNodeHandle), 1, 0);
	STACK_PUSH(&s, &root);
	while (s.nnodes) {
		node = *(HuffNodeHandle*)STACK_TOP(&s);
		STACK_POP(&s);

		if (!node->left && !node->right) {//leaf node: move leaf code to alphabet
			alphabet[node->symbol] = node->code;
			node->code = 0;
			continue;
		}

		if(node->left){//append false-bit
			bit = 0;
			if (node->code)
				node->left->code = bitstring_construct(node->code->data, node->code->bitCount, 0, 1);
			else
				node->left->code = bitstring_construct(0, 0, 0, 1);

			bitstring_append(&node->left->code, &bit, 1, 0);
			STACK_PUSH(&s, &node->left);
		}

		if (node->right) {//append true-bit
			bit = 1;
			if (node->code)
				node->right->code = bitstring_construct(node->code->data, node->code->bitCount, 0, 1);
			else
				node->right->code = bitstring_construct(0, 0, 0, 1);

			bitstring_append(&node->right->code, &bit, 1, 0);
			STACK_PUSH(&s, &node->right);
		}
	}
	dlist_clear(&s);
}

void print_hist(size_t *hist) {
	for (int i = 0; i < 256; ++i) {
		if (hist[i])
			printf("0x%02X \'%c\': %lld\n", i, i, (long long)hist[i]);
	}
	printf("\n");
}

void print_alphapet(BitstringHandle *alphabet, size_t *hist, size_t nsymbols) {
	for (int k = 0; k < nsymbols; ++k) {
		if (alphabet[k]) {
			printf("\'%c\' %d ", (char)k, hist[k]);
			bitstring_print(alphabet[k]);
			printf("\n");
		}
	}
}

////////////////////////////////////////////////Compress & Decompress///////////////////////////////////////////
int huff_compress(const unsigned char *src,size_t srcsize,ArrayHandle *dst)
{
    HuffNodeHandle root ;
    BitstringHandle d2 ;
    int success;

    success =1;
    gen_histogram(src,srcsize,header.hist);
    root = gen_tree(header.hist);
    gen_alphapet(root,alphabet);
    free_tree(&root);

    d2 =bitstring_construct(0,0,0,srcsize);
    for(size_t i=0;i<srcsize;++i)//for each symbol
    {
        int sym =src[i];
        BitstringHandle code =alphabet[sym];//fetch symbol code
        
#ifdef DEBUG_HUFF
	printf("[%d] 0x%02X : " , i ,sym);
	bitstring_print(code);
	printf("\n");//
#endif
	
	bitstring_append(&d2,code->data,code->bitCount,0);
	
#ifdef DEBUG_HUFF
	bitstring_print(d2);
	printf("\n");//
#endif
	}
	//write to dst
    header.magic =huffTag;
    header.zeros=0;
    header.u_size=srcsize;
    header.c_size =(d2->bitCount+7)/8;

    if(!*dst)
        ARRAY_ALLOC(unsigned char ,*dst,0,sizeof(HuffFileHeader)+header.c_size,0);
    if(dst[0]->esize!=1)
    {
        success =0;
        goto finish;
    }    
    ARRAY_APPEND(*dst,&header,sizeof(header),1,0);
    ARRAY_APPEND(*dst,d2->data,header.c_size,1,0);
finish:
    for(int i=0;i<256;++i)
        bitstring_free(alphabet+i);
    bitstring_free(&d2);    
    return success;
}
int huff_decompress(const unsigned char *src,size_t srcsize,ArrayHandle *dst)
{
    HuffNodeHandle root ;
    size_t bitIdx,nsym;
    int success;

    success=1;
    if(srcsize<sizeof(header))//compressed file must contain the header with histogram
        return 0;

    memcpy(&header,src,sizeof(header));
    root =gen_tree(header.hist);

    
    if(!*dst)
    {
        ARRAY_ALLOC(unsigned char,*dst,0,header.u_size,0);
    }
    else
    {
        if(dst[0]->esize!=1)
        {
            free_tree(&root);
            return 0;
        }
        ARRAY_APPEND(*dst,0,0,1,header.u_size);
    }
    src+=sizeof(header);//skip header 
    srcsize-=sizeof(header);
    for(bitIdx=0,nsym=0;bitIdx<(srcsize<<3)&&nsym<header.u_size;++nsym)
    {
        HuffNodeHandle node =root ;
        while (node->left||node->right)//while not leaf
        {
            int bit=src[bitIdx>>3]>>(bitIdx&7)&1;
#ifdef DEBUG_HUFF
			printf("%d",bit);//
#endif
            if(bit)
                node=node->right;
            else
                node=node->left;  
            ++bitIdx;      
            
        }
#ifdef DEBUG_HUFF
			printf("\n0x%02X\n",node->symbol);//
#endif
        ARRAY_APPEND(*dst,&node->symbol,1,1,0);
        
    }
    free_tree(&root);
    return 1;
}

//---------------------Load & Save Binary files------------------
int 			save_file_bin(const char *filename,const unsigned char *src , size_t srcSize)
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
}
//--------------------------------------------------
//			Main Function
int main(int argc , char **argv)
{
	#if 1
		long long t1,t2;
		ArrayHandle text,src,dst;
		HuffNodeHandle root;
		int success;
		/*text=load_text("main.c",0);
		gen_histogram(text->data,text->count,hist);
		print_hist(hist);
		root=gen_tree(hist);
		print_tree(root,0);
		gen_alphapet(root,alphabet);
		free_tree(&root);
		printf("Alphabet:\n");print_alphapet(alphabet,hist,256);
		array_free(&text);*/
										//encode
/*										
#ifdef _WIN32
    system("cd");
#else
	system("pwd");
#endif
*/
    if(argc!=4)
    {
        printf(
            "Usage:\n"
            "program -c infile outfile\n"
            "\tCompress infile\n"
            "program -d infile outfile\n"
			"\tDecompress infile\n"
			"program -m infile outfile\n"
			"\tCompare files\n"
        );
        return 1;
    }        
    src =load_bin(argv[2],0);
    if(!src)
    {
        printf("Failed  to open %s",argv[2]);
        return 1;
    }
    if(!strcmp(argv[1],"-c"))//compress
    {
        dst=0;
		t1=__rdtsc();
        success=huff_compress(src->data,src->count,&dst);
		t2=__rdtsc();
		printf("Compress elapsed: %lld cycles \n",t2-t1);
        if(!success)
        {
            printf("Failed to compress %s",argv[2]);
            exit(1);
        }
    	save_file_bin(argv[3],dst->data,dst->count);
    }
    else if(!strcmp(argv[1],"-d"))//decompress
    {
        dst=0;
		t1=__rdtsc();
        success=huff_decompress(src->data,src->count,&dst);
		t2=__rdtsc();
		printf("Decompress elapsed: %lld cycles \n",t2-t1);
        if(!success)
        {
            printf("Failed to decompress %s",argv[2]);
            exit(1);
        }
        save_file_bin(argv[3],dst->data,dst->count);
    }
    else if(!strcmp(argv[1],"-m"))//compare files
    {
        dst=load_bin(argv[3],0);
        if(!dst)
            return 1;
        for(int k=0;k<(int)src->count&&k<(int)dst->count;++k)
        {
            if(src->data[k]!=dst->data[k])
            {
                printf("ERROR at %d: %02X !=%02X\n", k ,src->data[k],dst->data[k]);
                return 1;
            }
        }
		printf("Successfull both files are the same. \n");  
    }	
	#endif
	return 0;
}