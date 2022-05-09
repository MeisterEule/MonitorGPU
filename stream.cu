# include <stdio.h>
# include <unistd.h>
# include <math.h>
# include <float.h>
# include <limits.h>
# include <sys/time.h>

#include <cuda.h>

#include "common.h"
#include "stream.h"

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



//void display_summary (double **times, int n_reps) {
//   const char *label[4] = {"Copy:      ", "Scale:     ", "Add:       ", "Triad:     "};
//   // First iteration is skipped
//   for (int k = 1; k < NTIMES; k++) { 
//       for (int j = 0; j < 4; j++) {
//           avgtime[j] = avgtime[j] + times[j][k];
//           mintime[j] = MIN(mintime[j], times[j][k]);
//           maxtime[j] = MAX(maxtime[j], times[j][k]);
//       }
//   }
//   
//   printf ("Bytes per operation[GiB]: %lf %lf %lf %lf\n",
//           rt_bytes[0] / 1024 / 1024 / 1024,
//           rt_bytes[1] / 1024 / 1024 / 1024,
//           rt_bytes[2] / 1024 / 1024 / 1024,
//           rt_bytes[3] / 1024 / 1024 / 1024);
//   printf("Function    Best Rate GB/s  Avg time     Min time     Max time\n");
//   for (int j = 0; j < 4; j++) {
//       	avgtime[j] = avgtime[j] / (double)(rt_nreps - 1);
//       	printf("%s%12.1f  %11.6f  %11.6f  %11.6f\n", label[j],
//               1.0E-09 * rt_bytes[j] / mintime[j],
//               avgtime[j],
//               mintime[j],
//               maxtime[j]);
//   }
//   printf(HLINE);
//}

bool check_results (STREAM_TYPE *a, STREAM_TYPE *b, STREAM_TYPE *c,
                    double **times, int array_size, int n_reps) {

   STREAM_TYPE aj ,bj, cj;
   STREAM_TYPE a_sum_err, b_sum_err, c_summ_err;
   STREAM_TYPE a_avg_err, b_avg_err, c_agv_err;

   double epsilon;
   int	ierr,err;

    aj = 1.0;
    bj = 2.0;
    cj = 0.0;
    // a[] is modified during timing check
    aj = 2.0E0 * aj;
    for (int k = 0; k < n_reps; k++) {
       cj = aj;
       bj = SCALE_SCALAR * cj;
       cj = aj + bj;
       aj = bj + TRIAD_SCALAR * cj;
    }

    /* accumulate deltas between observed and expected results */
    a_sum_err = 0.0;
    b_sum_err = 0.0;
    c_summ_err = 0.0;

#ifndef abs
#define abs(a) ((a) >= 0 ? (a) : -(a))
#endif
    for (int j = 0; j < array_size; j++) {
       a_sum_err += abs(a[j] - aj);
       b_sum_err += abs(b[j] - bj);
       c_summ_err += abs(c[j] - cj);
    }
    a_avg_err = a_sum_err / (STREAM_TYPE) array_size;
    b_avg_err = b_sum_err / (STREAM_TYPE) array_size;
    c_agv_err = c_summ_err / (STREAM_TYPE) array_size;

    if (sizeof(STREAM_TYPE) == 4) {
        epsilon = 1.e-6;
    } else if (sizeof(STREAM_TYPE) == 8) {
        epsilon = 1.e-13;
    }

    bool oka = abs(a_avg_err / aj) <= epsilon;
    bool okb = abs(b_avg_err / bj) <= epsilon;
    bool okc = abs(c_agv_err / cj) <= epsilon;
    return oka && okb && okc;
}

void do_stream (int array_size, int n_times,
                double *best_copy, double *best_scale, 
                double *best_add, double *best_triad, int *status) {
 
    int block_size = BLOCK_SIZE_DEFAULT;
    int grid_size = (array_size + block_size) / block_size;

    double **times = (double**) malloc (4 * sizeof(double*));
    for (int i = 0; i < 4; i++) {
       times[i] = (double*) malloc (n_times * sizeof(double));
    }

    for (int i = 0; i < 4; i++) {
       for (int j = 0; j < n_times; j++) {
          times[i][j] = 0.0;
       }
    }

    double test_bytes[4];
    test_bytes[STREAM_COPY] = 2 * sizeof(STREAM_TYPE) * array_size;
    test_bytes[STREAM_SCALE] = 2 * sizeof(STREAM_TYPE) * array_size;
    test_bytes[STREAM_ADD] = 3 * sizeof(STREAM_TYPE) * array_size;
    test_bytes[STREAM_TRIAD] = 3 * sizeof(STREAM_TYPE) * array_size;

    // Host fields
    STREAM_TYPE *a, *b, *c;
    // Device fields
    STREAM_TYPE *d_a, *d_b, *d_c;

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
       times[STREAM_COPY][k] = get_time_monotonic();
       vector_copy<<<grid_size,block_size>>> (d_c, d_a, array_size);
       cudaDeviceSynchronize();
       times[STREAM_COPY][k] = get_time_monotonic() - times[STREAM_COPY][k];

       times[STREAM_SCALE][k] = get_time_monotonic();
       vector_scale<<<grid_size,block_size>>> (d_b, d_c, array_size);
       cudaDeviceSynchronize();
       times[STREAM_SCALE][k] = get_time_monotonic() - times[STREAM_SCALE][k];

       times[STREAM_ADD][k] = get_time_monotonic();
       vector_add<<<grid_size,block_size>>> (d_c, d_a, d_b, array_size);
       cudaDeviceSynchronize();
       times[STREAM_ADD][k] = get_time_monotonic() - times[STREAM_ADD][k];

       times[STREAM_TRIAD][k] = get_time_monotonic();
       vector_triad<<<grid_size,block_size>>> (d_a, d_b, d_c, array_size);
       cudaDeviceSynchronize();
       times[STREAM_TRIAD][k] = get_time_monotonic() - times[STREAM_TRIAD][k];
    }

    cudaMemcpy (a, d_a, sizeof(STREAM_TYPE) * array_size, cudaMemcpyDeviceToHost);
    cudaMemcpy (b, d_b, sizeof(STREAM_TYPE) * array_size, cudaMemcpyDeviceToHost);
    cudaMemcpy (c, d_c, sizeof(STREAM_TYPE) * array_size, cudaMemcpyDeviceToHost);

    bool check = check_results (a, b, c, times, array_size, n_times);
    if (!check) {
       *status = STREAM_INVALID_RESULTS;
       printf ("Fields not OK\n");
       return;
    }

    double avgtime[4] = {0};
    double maxtime[4] = {0};
    double mintime[4] = {FLT_MAX,FLT_MAX,FLT_MAX,FLT_MAX};

    for (int i = 0; i < n_times; i++) {
       for (int j = 0; j < 4; j++) {
          avgtime[j] = avgtime[j] + times[j][i];
          if (times[j][i] < mintime[j]) mintime[j] = times[j][i];
          if (times[j][i] > maxtime[j]) maxtime[j] = times[j][i];
       }
    }

    *best_copy = test_bytes[STREAM_COPY] / mintime[STREAM_COPY] * 1e-9;
    *best_scale = test_bytes[STREAM_SCALE] / mintime[STREAM_SCALE] * 1e-9;
    *best_add = test_bytes[STREAM_ADD] / mintime[STREAM_ADD] * 1e-9;
    *best_triad = test_bytes[STREAM_TRIAD] / mintime[STREAM_TRIAD] * 1e-9;

    cudaFree(d_a);
    cudaFree(d_b);
    cudaFree(d_c);
    free(a);
    free(b);
    free(c);

    *status = STREAM_SUCCESS;
}

