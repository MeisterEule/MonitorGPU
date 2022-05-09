#ifndef STREAM_H
#define STREAM_H
    
#define BLOCK_SIZE_DEFAULT 256
#ifndef STREAM_TYPE
#define STREAM_TYPE double
#endif

enum stream_status {
   STREAM_SUCCESS,
   STREAM_OOM_HOST,
   STREAM_OOM_DEVICE,
   STREAM_INVALID_RESULTS
};

#define SCALE_SCALAR 3.0
#define TRIAD_SCALAR 3.0

enum stream_test_id {
   STREAM_COPY,
   STREAM_SCALE,
   STREAM_ADD,
   STREAM_TRIAD
};

double stream_max_vector_size (long memory_size);
void do_stream (int rt_array_size, int n_times,
                double *best_copy, double *best_scale,
                double *best_add, double *best_triad, int *status);

#endif
