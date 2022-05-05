#ifndef DGEMM_H
#define DGEMM_H

void doDgemm(const int N, const double alpha, const double beta, const int n_repeats,
             double *perf);

#endif
