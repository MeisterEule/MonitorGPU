# include <stdio.h>
# include <unistd.h>
# include <math.h>
# include <float.h>
# include <limits.h>
# include <sys/time.h>

#include <cuda.h>

#include "common.h"
#include "stream.h"

#define CUDA_CALL_SAFE(cuda_func)                                 \
do {                                                         \
   cudaError_t err = cuda_func;                              \
   if (err != cudaSuccess) {                                 \
      printf ("Cuda Error: %s\n", cudaGetErrorString(err));  \
      exit(err);                                             \
   }                                                         \
} while (0)                                                  \

#define CUDA_CALL_UNSAFE(cuda_func) cuda_func

#ifdef _SAFE
#define CUDA_CALL CUDA_CALL_SAFE
#else
#define CUDA_CALL CUDA_CALL_UNSAFE
#endif

#define STREAM_ARRAY_SIZE_DEFAULT 10000000
#define NREPS_DEFAULT 10
 

//#ifdef NTIMES
//#if NTIMES<=1
//#   define NTIMES	10
//#endif
//#endif

#ifndef NTIMES
#   define NTIMES	10
#endif

# define HLINE "-------------------------------------------------------------\n"

# ifndef MIN
# define MIN(x,y) ((x)<(y)?(x):(y))
# endif
# ifndef MAX
# define MAX(x,y) ((x)>(y)?(x):(y))
# endif

#ifndef STREAM_TYPE
#define STREAM_TYPE double
#endif

#define SCALE_SCALAR 3.0
#define TRIAD_SCALAR 3.0

// *************************
// **** The CUDA kernels ***
// *************************

__global__ void vector_copy (STREAM_TYPE *out, STREAM_TYPE *in, long long n) {
   long long tid = blockIdx.x * blockDim.x + threadIdx.x;
   if (tid < n) out[tid] = in[tid];
}

__global__ void vector_scale (STREAM_TYPE *out, STREAM_TYPE *in,  long long n) {
   long long tid = blockIdx.x * blockDim.x + threadIdx.x;
   if (tid < n) out[tid] = SCALE_SCALAR * in[tid];
}

__global__ void vector_add (STREAM_TYPE *out, STREAM_TYPE *a, STREAM_TYPE *b, long long n) {
   long long tid = blockIdx.x * blockDim.x + threadIdx.x;
   if (tid < n) out[tid] = a[tid] + b[tid];
}

__global__ void vector_triad (STREAM_TYPE *out, STREAM_TYPE *b, STREAM_TYPE *c, long long n) {
   long long tid = blockIdx.x * blockDim.x + threadIdx.x;
   if (tid < n) out[tid] = b[tid] + TRIAD_SCALAR * c[tid];
}

static int rt_nreps;
static double rt_bytes[4] = {0.0};
static int rt_block_size;

void print_help () {
  printf ("Stream GPU options: \n");
  printf ("-s: Nr. of elements\n");
  printf ("-n: Nr. of repetitions\n");
}

//void print_header () {
//   printf(HLINE);
//   printf("STREAM CUDA $\n");
//   printf(HLINE);
//   printf ("Arrays have %d elements with %d bytes each\n", rt_array_size, sizeof(STREAM_TYPE));
//   printf ("   Total: %lf GiB\n", 3.0 * rt_array_size * sizeof(STREAM_TYPE) / 1024 / 1024 / 1024);
//}

STREAM_TYPE *a, *b, *c;
STREAM_TYPE *d_a, *d_b, *d_c;

bool setup_fields (int array_size) {
   printf ("Required memory: %lf BiB\n", (double)array_size * sizeof(STREAM_TYPE) / 1024 / 1024 / 1024);
   a = (STREAM_TYPE*) malloc (array_size * sizeof(STREAM_TYPE)); 
   b = (STREAM_TYPE*) malloc (array_size * sizeof(STREAM_TYPE)); 
   c = (STREAM_TYPE*) malloc (array_size * sizeof(STREAM_TYPE)); 
   for (int j = 0; j < array_size; j++) {
     a[j] = 2.0; b[j] = 2.0; c[j] = 0.0;
   }

   if ((cudaMalloc ((void**)&d_a, sizeof(STREAM_TYPE) * array_size)) == cudaErrorMemoryAllocation) {
      printf ("Failed allocate field on device. Out of memory!\n");
      return false;
   }
   if ((cudaMalloc ((void**)&d_b, sizeof(STREAM_TYPE) * array_size)) == cudaErrorMemoryAllocation) {
      printf ("Failed allocate field on device. Out of memory!\n");
      return false;
   }
   if ((cudaMalloc ((void**)&d_c, sizeof(STREAM_TYPE) * array_size)) == cudaErrorMemoryAllocation) {
      printf ("Failed allocate field on device. Out of memory!\n");
      return false;
   }

   cudaMemcpy (d_a, a, sizeof(STREAM_TYPE) * array_size, cudaMemcpyHostToDevice);
   cudaMemcpy (d_b, b, sizeof(STREAM_TYPE) * array_size, cudaMemcpyHostToDevice);
   cudaMemcpy (d_c, c, sizeof(STREAM_TYPE) * array_size, cudaMemcpyHostToDevice);
   return true;
}

static double	avgtime[4] = {0}, maxtime[4] = {0},
		mintime[4] = {FLT_MAX,FLT_MAX,FLT_MAX,FLT_MAX};


//extern double mysecond();


void display_summary (double **times, int n_reps) {
   const char *label[4] = {"Copy:      ", "Scale:     ", "Add:       ", "Triad:     "};
   // First iteration is skipped
   for (int k = 1; k < NTIMES; k++) { 
       for (int j = 0; j < 4; j++) {
           avgtime[j] = avgtime[j] + times[j][k];
           mintime[j] = MIN(mintime[j], times[j][k]);
           maxtime[j] = MAX(maxtime[j], times[j][k]);
       }
   }
   
   printf ("Bytes per operation[GiB]: %lf %lf %lf %lf\n",
           rt_bytes[0] / 1024 / 1024 / 1024,
           rt_bytes[1] / 1024 / 1024 / 1024,
           rt_bytes[2] / 1024 / 1024 / 1024,
           rt_bytes[3] / 1024 / 1024 / 1024);
   printf("Function    Best Rate GB/s  Avg time     Min time     Max time\n");
   for (int j = 0; j < 4; j++) {
       	avgtime[j] = avgtime[j] / (double)(rt_nreps - 1);
       	printf("%s%12.1f  %11.6f  %11.6f  %11.6f\n", label[j],
               1.0E-09 * rt_bytes[j] / mintime[j],
               avgtime[j],
               mintime[j],
               maxtime[j]);
   }
   printf(HLINE);
}

#ifndef abs
#define abs(a) ((a) >= 0 ? (a) : -(a))
#endif
bool check_results (double **times, int array_size, int n_reps) {

   //cudaMemcpy (a, d_a, sizeof(STREAM_TYPE) * array_size, cudaMemcpyDeviceToHost);
   //cudaMemcpy (b, d_b, sizeof(STREAM_TYPE) * array_size, cudaMemcpyDeviceToHost);
   //cudaMemcpy (c, d_c, sizeof(STREAM_TYPE) * array_size, cudaMemcpyDeviceToHost);

   STREAM_TYPE aj ,bj, cj;
   STREAM_TYPE aSumErr,bSumErr,cSumErr;
   STREAM_TYPE aAvgErr,bAvgErr,cAvgErr;

   double epsilon;
   int	ierr,err;

    /* reproduce initialization */
	aj = 1.0;
	bj = 2.0;
	cj = 0.0;
    /* a[] is modified during timing check */
	aj = 2.0E0 * aj;
    /* now execute timing loop */
	for (int k = 0; k < n_reps; k++) {
            cj = aj;
            bj = SCALE_SCALAR * cj;
            cj = aj + bj;
            aj = bj + TRIAD_SCALAR * cj;
        }

    /* accumulate deltas between observed and expected results */
	aSumErr = 0.0;
	bSumErr = 0.0;
	cSumErr = 0.0;
	for (int j = 0; j < array_size; j++) {
           //printf ("Compare: %lf %lf\n", a[j], aj);
	   aSumErr += abs(a[j] - aj);
	   bSumErr += abs(b[j] - bj);
	   cSumErr += abs(c[j] - cj);
	}
	aAvgErr = aSumErr / (STREAM_TYPE) array_size;
	bAvgErr = bSumErr / (STREAM_TYPE) array_size;
	cAvgErr = cSumErr / (STREAM_TYPE) array_size;

	if (sizeof(STREAM_TYPE) == 4) {
           epsilon = 1.e-6;
	}
	else if (sizeof(STREAM_TYPE) == 8) {
           epsilon = 1.e-13;
	}
	else {
	   printf("WEIRD: sizeof(STREAM_TYPE) = %lu\n",sizeof(STREAM_TYPE));
	   epsilon = 1.e-6;
	}

        bool oka = abs(aAvgErr / aj) <= epsilon;
        bool okb = abs(bAvgErr / bj) <= epsilon;
        bool okc = abs(cAvgErr / cj) <= epsilon;
        printf ("Err: %lf %lf %lf\n", aAvgErr, bAvgErr, cAvgErr);
        printf ("oka: %d, okb: %d, okc: %d\n", oka, okb, okc);

        return oka && okb && okc;
}

void cleanup_pass () {
   cudaFree(d_a);
   cudaFree(d_b);
   cudaFree(d_c);
   free(a);
   free(b);
   free(c);
}

//int main(int argc, char *argv[]) {
void do_stream (int array_size, int n_times, int *status) {

    int block_size = BLOCK_SIZE_DEFAULT;
    int grid_size = (array_size + block_size) / block_size;
    double **times;

    times = (double**) malloc (4 * sizeof(double*));
    for (int i = 0; i < 4; i++) {
       times[i] = (double*) malloc (n_times * sizeof(double));
    }

    for (int k = 0; k < n_times; k++) {
       times[0][k] = 0.0;
       times[1][k] = 0.0;
       times[2][k] = 0.0;
       times[3][k] = 0.0;
    }

    double test_bytes[4];
    test_bytes[0] = 2 * sizeof(STREAM_TYPE) * array_size;
    test_bytes[1] = 2 * sizeof(STREAM_TYPE) * array_size;
    test_bytes[2] = 3 * sizeof(STREAM_TYPE) * array_size;
    test_bytes[3] = 3 * sizeof(STREAM_TYPE) * array_size;

    // Allocate host fields
    if (!(a = (STREAM_TYPE*) malloc (array_size * sizeof(STREAM_TYPE)))) {
      *status = STREAM_OOM_HOST;
      return;
    }
    if (!(b = (STREAM_TYPE*) malloc (array_size * sizeof(STREAM_TYPE)))) {
      *status = STREAM_OOM_HOST;
      return;
    } 
    if (!(c = (STREAM_TYPE*) malloc (array_size * sizeof(STREAM_TYPE)))) {
      *status = STREAM_OOM_HOST;
      return;
    }

    if ((cudaMalloc ((void**)&d_a, sizeof(STREAM_TYPE) * array_size)) == cudaErrorMemoryAllocation) {
      *status = STREAM_OOM_DEVICE;
      return;
    }
    if ((cudaMalloc ((void**)&d_b, sizeof(STREAM_TYPE) * array_size)) == cudaErrorMemoryAllocation) {
      *status = STREAM_OOM_DEVICE;
      return;
    }
    if ((cudaMalloc ((void**)&d_c, sizeof(STREAM_TYPE) * array_size)) == cudaErrorMemoryAllocation) {
      *status = STREAM_OOM_DEVICE;
      return;
    }

    for (int i = 0; i < array_size; i++) {
       a[i] = 2.0;
       b[i] = 2.0;
       c[i] = 0.0; 
    }

    cudaMemcpy (d_a, a, sizeof(STREAM_TYPE) * array_size, cudaMemcpyHostToDevice);
    cudaMemcpy (d_b, b, sizeof(STREAM_TYPE) * array_size, cudaMemcpyHostToDevice);
    cudaMemcpy (d_c, c, sizeof(STREAM_TYPE) * array_size, cudaMemcpyHostToDevice);

    double t1, t2;
    for (int k = 0; k < n_times; k++) {
       times[0][k] = get_time_monotonic();
       vector_copy<<<grid_size,block_size>>> (d_c, d_a, array_size);
       cudaDeviceSynchronize();
       times[0][k] = get_time_monotonic() - times[0][k];

       times[1][k] = get_time_monotonic();
       vector_scale<<<grid_size,block_size>>> (d_b, d_c, array_size);
       cudaDeviceSynchronize();
       times[1][k] = get_time_monotonic() - times[1][k];

       times[2][k] = get_time_monotonic();
       vector_add<<<grid_size,block_size>>> (d_c, d_a, d_b, array_size);
       cudaDeviceSynchronize();
       times[2][k] = get_time_monotonic() - times[2][k];

       times[3][k] = get_time_monotonic();
       vector_triad<<<grid_size,block_size>>> (d_a, d_b, d_c, array_size);
       cudaDeviceSynchronize();
       times[3][k] = get_time_monotonic() - times[3][k];
    }

    cudaMemcpy (a, d_a, sizeof(STREAM_TYPE) * array_size, cudaMemcpyDeviceToHost);
    cudaMemcpy (b, d_b, sizeof(STREAM_TYPE) * array_size, cudaMemcpyDeviceToHost);
    cudaMemcpy (c, d_c, sizeof(STREAM_TYPE) * array_size, cudaMemcpyDeviceToHost);

    bool check = check_results (times, array_size, n_times);
    if (!check) {
       *status = STREAM_INVALID_RESULTS;
       return;
    }

    for (int i = 0; i < n_times; i++) {
       for (int j = 0; j < 4; j++) {
          avgtime[j] = avgtime[j] + times[j][i];
          if (times[j][i] < mintime[j]) mintime[j] = times[j][i];
       }
    }
    for (int i = 0; i < 4; i++) {
       printf ("Avg: %lf\n", avgtime[i] / (n_times - 1));
       printf ("Min: %lf\n", mintime[i]);
       printf ("Rate: %lf\n", 1.0e-9 * test_bytes[i] / mintime[i]);
       printf ("***************************\n");
    }
    printf(HLINE);

    cudaFree(d_a);
    cudaFree(d_b);
    cudaFree(d_c);
    free(a);
    free(b);
    free(c);

    *status = STREAM_SUCCESS;
}

/* A gettimeofday routine to give access to the wall
   clock timer on most UNIX-like systems.  */

//#include <sys/time.h>
//
//double mysecond()
//{
//        struct timeval tp;
//        struct timezone tzp;
//        int i;
//
//        i = gettimeofday(&tp,&tzp);
//        return ( (double) tp.tv_sec + (double) tp.tv_usec * 1.e-6 );
//}


