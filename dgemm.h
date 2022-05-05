#ifndef DGEMM_H
#define DGEMM_H

void doDgemm(const int N, const double alpha, const double beta, const int n_repeats,
             double *gflops_agv, double *gflops_min, double *gflops_max, double *gflops_stddev);

#endif
