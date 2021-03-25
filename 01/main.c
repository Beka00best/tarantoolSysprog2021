#include <ucontext.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/mman.h>
#include <limits.h>
#include <time.h>                                             


static ucontext_t *uctx_func1 = NULL, uctx_main;             
static ucontext_t uctx_start;

#define handle_error(msg) \
  do { perror(msg); exit(EXIT_FAILURE); } while (0)

#define stack_size 1024*1024                                            
int *point = NULL;
double *times = NULL;

static void *
allocate_stack_sig()
{
	void *stack = malloc(stack_size);
	stack_t ss;
	ss.ss_sp = stack;
	ss.ss_size = stack_size;
	ss.ss_flags = 0;
	sigaltstack(&ss, NULL);
	return stack;
}

static void *
allocate_stack_mmap()
{
	return mmap(NULL, stack_size, PROT_READ | PROT_WRITE | PROT_EXEC,
	        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
}

static void *
allocate_stack_mprot()                                                      
{
	void *stack = malloc(stack_size);
	mprotect(stack, stack_size, PROT_READ | PROT_WRITE | PROT_EXEC);
	return stack;
}

enum stack_type {
	STACK_MMAP,
	STACK_SIG,
	STACK_MPROT
};

static void *
allocate_stack(enum stack_type t)
{
	switch(t) {
	case STACK_MMAP:
		return allocate_stack_mmap();
	case STACK_SIG:
		return allocate_stack_sig();
	case STACK_MPROT:
		return allocate_stack_mprot();
	}
}

void merge(int *a, int n, int index, int size) {         
    clock_t start = clock();
    int begin[150], end[150], LEFT, RIGHT;
    int i = 0;
    begin[0] = 0;
    end[0] = n;
    while (i >= 0) {
        LEFT = begin[i];
        RIGHT = end[i];
        if (LEFT + 1 < RIGHT--) {
            int piv = a[LEFT];
            while (LEFT < RIGHT) {
                while (a[RIGHT] >= piv && LEFT < RIGHT)
                    RIGHT--;
                if (LEFT < RIGHT)
                    a[LEFT++] = a[RIGHT];
                while (a[LEFT] <= piv && LEFT < RIGHT)
                    LEFT++;
                if (LEFT < RIGHT)
                    a[RIGHT--] = a[LEFT];
            }
            a[LEFT] = piv;
            if (LEFT - begin[i] > end[i] - RIGHT) { 
                begin[i + 1] = LEFT + 1;
                end[i + 1] = end[i];
                end[i++] = LEFT;
            } else {
                begin[i + 1] = begin[i];
                end[i + 1] = LEFT;
                begin[i++] = LEFT + 1;
            }
        } else {
            i--;
        }
          
      if (activeCoroutine(size, index) != -1){                             
          if(swapcontext(&uctx_func1[index], &uctx_func1[nextCoroutine(size, index)]) == -1) {
              handle_error("swapcontext");
          }
      }
    }
    clock_t stop = clock();
    times[index] = (double)(stop - start) / (CLOCKS_PER_SEC);
    point[index] = 1;
}

int activeCoroutine(int n, int index){                                  
    for(int i = 0; i < n; i++){
        if ((point[i] == 0) && (i != index)){
            return i;
        }
    }
    return -1;
}

int nextCoroutine(int n, int index){                            
    int tmp = index % n;
    for(int i = 0; i < 2 * n; i++){
        if ((point[i % n] == 0) && (i % n != index) && ((i % n > tmp) || (i >= n))){
            return i % n;
        }
    }
    return -1;
}

int minIndex(int n, int *a) {              
    int min = INT_MAX;
    int index = -1;
    for (int i = 0; i < n; i++){
        if((a[i] < min) && (a[i]!=-1)){
            min = a[i];
            index = i;
        }
    }
    return index;
}

void procCoroutine(int n){                                          
    while(1){
        if (activeCoroutine(n, -1) == -1){
            swapcontext(&uctx_start, &uctx_main);
            break;
        }
        else{
            swapcontext(&uctx_start, &uctx_func1[activeCoroutine(n, -1)]);
        }
    }
}

void finalMerge(int **arr, int n, char *file, int *sizes){ 
    int *p = (int *)malloc(n * sizeof(int));                                        
    int *tmp = (int*)malloc(n * sizeof(int));                                       
    for(int i = 0; i < n; i++){
        p[i] = 0;
    }
    for (int i = 0; i < n; i++){
        tmp[i] = arr[i][p[i]];
    }
    FILE *fd = fopen(file, "w");
    int min;
    int j = 0;
    while (j < n){
        min = minIndex(n, tmp);
        fprintf(fd, "%d ", arr[min][p[min]]);
        p[min]++;
        if (p[min] == sizes[min]){
            j++;
            tmp[min] = -1;                                                           
        }                                                                                
        else{                                                                               
            tmp[min] = arr[min][p[min]];                            
        }
    }
    free(p);
    free(tmp);
    fclose(fd);
}

int main(int argc, char **argv) {
    clock_t start = clock();
    point = (unsigned int *)malloc(sizeof(unsigned int)*(argc-1));
    times = (double *)malloc(sizeof(double)*(argc-1));  

    for (int i = 0; i < argc-1; i++){
        point[i] = 0;
    }

    int *sizes = NULL;
    sizes = (int*)malloc(sizeof(int) * (argc-1));
    FILE *fp = NULL;
    int i = 0;
    int size = 0;
    int num;
    int  **numbers;
    numbers = (int **) malloc((argc-1) * sizeof(numbers));

    for(int i = 0; i < argc - 1; i++){
        numbers[i] = NULL;
    }

    uctx_func1 = (ucontext_t *)malloc((argc) * sizeof(ucontext_t));
    for (int i = 0; i < argc-1; i++){                                                       
        size = 0;
        if ((fp = fopen(argv[i+1], "r")) < 0){
            handle_error("fopen");
        }; 
        while (fscanf(fp, "%d", &num) == 1 ){
            if ((numbers[i] = (int*)realloc(numbers[i], sizeof(int) * (size + 1)) ) == NULL){
                perror("realloc");
                exit(EXIT_FAILURE);
            }
            numbers[i][size++] = num;
        }
        sizes[i] = size;
        
    }
    fclose(fp);

    getcontext(&uctx_start);                                                       
    uctx_start.uc_stack.ss_sp = allocate_stack_mprot();
    uctx_start.uc_stack.ss_size = stack_size;
    uctx_start.uc_link = &uctx_main;
    makecontext(&uctx_start, procCoroutine, 1, (argc-1));

    for(int i = 0; i < argc-1; i++){                                                
        if(getcontext(&uctx_func1[i]) == -1){
            handle_error("getcontext");
        }
        uctx_func1[i].uc_stack.ss_sp = allocate_stack_mprot();
        uctx_func1[i].uc_stack.ss_size = stack_size;
        uctx_func1[i].uc_link = &uctx_start;
        makecontext(&uctx_func1[i], merge, 4, numbers[i], sizes[i], i, argc-1);
    }

    swapcontext(&uctx_main, &uctx_start);
    
    int size_of_last_one = 0;
    for (int k = 0; k < argc-1; k++){
        size_of_last_one = size_of_last_one + sizes[k];
    }
    
    finalMerge(numbers, argc-1, "res.txt", sizes);                         
    clock_t stop = clock();
    double spent = (double)(stop - start)/(CLOCKS_PER_SEC);
    printf("Spent time %f second\n", spent);
    for(int i = 0; i < argc - 1; i++){
        printf("#%d coroutine spent %f second\n", i, times[i]);
    }
    for(int i = 0; i < argc - 1; i++){
        free(uctx_func1[i].uc_stack.ss_sp);
        free(numbers[i]);
    }

    free(uctx_func1);
    free(sizes);
    free(point);
    free(numbers);
    free(times);
    return 0;
}