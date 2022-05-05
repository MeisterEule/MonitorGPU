#include <cuda.h>
#include <cuda_runtime_api.h>
#include <cublas_v2.h>

#include <stdio.h>
#include <time.h>

#include "dgemm.h"

void init_matrices (int N, double *mA, double *mB, double *mC) {
   double *init_vec = (double*)aligned_alloc (64, sizeof(double) * N);
   //double *init_vec_rev = (double*)aligned_alloc (64, sizeof(double) * N);

   for (int i = 0; i < N; i++) {
      double value = 2.0 + sin(i);
      init_vec[i] = value;
   }

   for (int i = 0; i < N; i++) {
      for (int j = 0; j < N; j++) {
         mA[i * N + j] = init_vec[j];
         mB[i * N + j] = 1.0 / init_vec[j];
         mC[i * N + j] = 1.0;
      }
   }

   free(init_vec);
}

double get_time_monotonic () {
   struct timespec tp;
   clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
   return (double)tp.tv_sec + (double)tp.tv_nsec * 1e-9;
}

void doDgemm(const int N, const double alpha, const double beta, const int n_repeats,
             double *perf) {
   //const int N = 1000;
   //const double alpha = 1.0;
   //const double beta = 1.0;
   const long long required_memory = 3.0 * N * N * sizeof(double);
   const long long flops_per_step = N * N * (N + 1) * 2;

   cublasHandle_t cublas_handle;
   cublasStatus_t cublas_status = cublasCreate(&cublas_handle);
  
   double *mA, *devA;
   double *mB, *devB;
   double *mC, *devC;
      
   mA = (double*)aligned_alloc(64, N * N * sizeof(double));
   mB = (double*)aligned_alloc(64, N * N * sizeof(double));
   mC = (double*)aligned_alloc(64, N * N * sizeof(double));

   cudaError_t cuda_error;
   cuda_error = cudaMalloc((void**) &devA, N * N * sizeof(double));
   cuda_error = cudaMalloc((void**) &devB, N * N * sizeof(double));
   cuda_error = cudaMalloc((void**) &devC, N * N * sizeof(double));

   init_matrices (N, mA, mB, mC);
   cuda_error = cudaMemcpy(devA, mA, N * N * sizeof(double), cudaMemcpyHostToDevice);
   cuda_error = cudaMemcpy(devB, mB, N * N * sizeof(double), cudaMemcpyHostToDevice);
   cuda_error = cudaMemcpy(devC, mC, N * N * sizeof(double), cudaMemcpyHostToDevice);

   const double t_start = get_time_monotonic();
   cublas_status = cublasDgemm (cublas_handle,
                                CUBLAS_OP_N, CUBLAS_OP_N,
                                N, N, N,
                                &alpha,
                                devB, N,
                                devA, N,
                                &beta,
                                devC, N);

   const double t_end = get_time_monotonic();
   double t_elapsed = t_end - t_start;
   *perf = flops_per_step / t_elapsed / 1e9;

   cublas_status = cublasDestroy(cublas_handle);
   free(mA);
   free(mB);
   free(mC);
   cudaFree(devA);
   cudaFree(devB);
   cudaFree(devC);
}
