#ifndef DGEMM_H
#define DGEMM_H

constexpr unsigned int DGEMM_CUBLAS_SUCCESS{0};
constexpr unsigned int DGEMM_CUBLAS_ERR_OOM{1};
constexpr unsigned int DGEMM_CUBLAS_ERR_OTHER{2};

int dgemmMaxSize (const double memory_size);
double get_time_monotonic ();

void doDgemm(const int N, const double alpha, const double beta, const int n_repeats,
             double *gflops_agv, double *gflops_min, double *gflops_max, double *gflops_stddev,
             unsigned int *status);

#endif
