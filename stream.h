#ifndef STREAM_H
#define STREAM_H
    
#define BLOCK_SIZE_DEFAULT 256

enum STREAM_STATUS {
   STREAM_SUCCESS,
   STREAM_OOM_HOST,
   STREAM_OOM_DEVICE,
   STREAM_INVALID_RESULTS
};

void do_stream (int rt_array_size, int n_times, int *status);

#endif
