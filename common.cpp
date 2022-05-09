#include "time.h"

double get_time_monotonic () {
   struct timespec tp;
   clock_gettime(CLOCK_MONOTONIC_RAW, &tp);
   return (double)tp.tv_sec + (double)tp.tv_nsec * 1e-9;
}


