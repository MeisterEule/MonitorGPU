/*
 * Copyright 2015 NVIDIA Corporation. All rights reserved
 *
 * Sample CUPTI library for OpenACC data collection.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <cupti.h>
#include <cuda.h>
#include <openacc.h>
#include <mpi.h>

// helper macros

#define CUPTI_CALL(call)                                                \
  do {                                                                  \
    CUptiResult _status = call;                                         \
    if (_status != CUPTI_SUCCESS) {                                     \
      const char *errstr;                                               \
      cuptiGetResultString(_status, &errstr);                           \
      fprintf(stderr, "%s:%d: error: function %s failed with error %s.\n", \
              __FILE__, __LINE__, #call, errstr);                       \
      exit(-1);                                                         \
    }                                                                   \
  } while (0)

#define BUF_SIZE (32 * 1024)
#define ALIGN_SIZE (8)
#define ALIGN_BUFFER(buffer, align)                                            \
  (((uintptr_t) (buffer) & ((align)-1)) ? ((buffer) + (align) - ((uintptr_t) (buffer) & ((align)-1))) : (buffer))


typedef struct event_list_st {
  int kind;
  int n_calls;
  uint64_t total_duration;
  const char *kernelName;
  const char *srcFile;
  const char *funcName;
  int line_start;
  int line_end;
  int func_line_start;
  int func_line_end;
  uint64_t total_bytes;
  event_list_st *next;
} event_list_t;

static event_list_t *event_list;
int callback_counter;
int mpi_rank;

int count_registered_events (int kind) {
  int n = 0;
  printf ("event list exists? %d\n", event_list != NULL);
  event_list_t *ev = event_list;
  do {
     if (ev->kind == kind) n++;
     ev = ev->next;
  } while (ev != NULL);
  return n;
}

int compare_event_duration (const void *p1, const void *p2) {
   event_list_t *e1 = *(event_list_t **)p1;
   event_list_t *e2 = *(event_list_t **)p2;
   if (!e1) return -1;
   if (!e2) return 1;
   uint64_t t1 = e1->total_duration;
   uint64_t t2 = e2->total_duration;
   int64_t diff = t2 - t1;
   if (diff > 0) return  1;
   if (diff < 0) return -1;
   return 0;
}

void print_event_list (bool sort_by_duration, int kind) {
   int n_events = count_registered_events(kind);
   event_list_t **ev_array = (event_list_t**)malloc(n_events * sizeof(event_list_t*));
   event_list_t *ev = event_list;

   int ii = 0;
   while (ev != NULL) {
     if (ev->kind == kind) {
        ev_array[ii] = (event_list_t*) malloc (sizeof(event_list_t));
        memcpy (ev_array[ii], ev, sizeof(event_list_t));
        ii++;
     }
     ev = ev->next; 
   }

   if (sort_by_duration) {
      qsort ((void*)ev_array, (size_t)n_events, sizeof(event_list_t*), compare_event_duration);
   }

   long long total_summed_time = 0;
   for (int i = 0; i < n_events; i++) {
      total_summed_time += ev_array[i]->total_duration;
   }

   if (kind == CUPTI_ACTIVITY_KIND_OPENACC_DATA) {
      printf ("TOTAL DATA: %lf s\n", (double)total_summed_time / 1000000000);
   } else if (kind == CUPTI_ACTIVITY_KIND_OPENACC_LAUNCH) {
      printf ("TOTAL KERNEL: %lf s\n", (double)total_summed_time / 1000000000);
   } else if (kind == CUPTI_ACTIVITY_KIND_OPENACC_OTHER) {
      printf ("TOTAL OTHER: %lf s\n", (double)total_summed_time / 1000000000);
   }
   
   for (int i = 0; i < n_events; i++) { 
      printf ("Nr. %d\n", i);
      //printf ("kind: %d\n", ev_array[i]->kind);
      printf ("n_calls: %d\n", ev_array[i]->n_calls);
      long long t = ev_array[i]->total_duration;
      printf ("accumulated time: %lld ms (%.2lf%%)\n", ev_array[i]->total_duration, (double)t / total_summed_time * 100);
      printf ("kernel: %s\n", ev_array[i]->kernelName);
      printf ("source file: %s\n", ev_array[i]->srcFile);
      printf ("function: %s\n", ev_array[i]->funcName);
      printf ("lines: %d - %d\n", ev_array[i]->line_start, ev_array[i]->line_end);
      printf ("function lines: %d - %d\n", ev_array[i]->func_line_start, ev_array[i]->func_line_end);
      if (ev_array[i]->kind == CUPTI_ACTIVITY_KIND_OPENACC_DATA) 
         printf ("bytes transferred: %lld\n", ev_array[i]->total_bytes);
      printf ("**********************************\n");
   }

   free (ev_array);
}

bool event_matches (event_list_t *ev, int kind, const char *srcFile, const char *funcName,
                    const char *kernelName, int line_start, int line_end,
                    int func_line_start, int func_line_end) {
   bool match = ev->kind == kind;
   match &= !strcmp(ev->srcFile, srcFile);
   match &= !strcmp(ev->funcName, funcName);
   if (kernelName != NULL && ev->kernelName != NULL) {
      match &= !strcmp(ev->kernelName, kernelName);
   } else {
      match = false;
   }
   match &= ev->line_start == line_start;
   match &= ev->line_end == line_end;
   match &= ev->func_line_start == func_line_start;
   match &= ev->func_line_end == func_line_end;
   return match;
}

void register_event (CUpti_Activity *record) {
   int kind = record->kind;
   uint64_t start, end;
   const char *kernelName;
   const char *srcFile;
   const char *funcName;
   int line_start, line_end;
   int func_line_start, func_line_end;
   uint64_t bytes;
   callback_counter++;
   if (kind == CUPTI_ACTIVITY_KIND_OPENACC_LAUNCH) {
      CUpti_ActivityOpenAccLaunch *r = (CUpti_ActivityOpenAccLaunch*)record;
      start = r->start;
      end = r->end;
      kernelName = r->kernelName;
      srcFile = r->srcFile;
      funcName = r->funcName;
      line_start = r->lineNo;
      line_end = r->endLineNo;
      func_line_start = r->funcLineNo;
      func_line_end = r->funcEndLineNo;
   } else if (kind == CUPTI_ACTIVITY_KIND_OPENACC_OTHER) {
      CUpti_ActivityOpenAccOther *r = (CUpti_ActivityOpenAccOther*)record;  
      if (r->eventKind == CUPTI_OPENACC_EVENT_KIND_DEVICE_INIT) return;
      start = r->start;
      end = r->end;
      kernelName = NULL;
      srcFile = r->srcFile;
      funcName = r->funcName;
      line_start = r->lineNo;
      line_end = r->endLineNo;
      func_line_start = r->funcLineNo;
      func_line_end = r->funcEndLineNo;
   } else if (kind == CUPTI_ACTIVITY_KIND_OPENACC_DATA) {
      CUpti_ActivityOpenAccData *r = (CUpti_ActivityOpenAccData*)record;
      start = r->start;
      end = r->end;
      bytes = r->bytes; 
      kernelName = NULL;
      srcFile = r->srcFile;
      funcName = r->funcName;
      line_start = r->lineNo;
      line_end = r->endLineNo;
      func_line_start = r->funcLineNo;
      func_line_end = r->funcLineNo; 
   } else { 
      // Unknown event type
      return;
   }
   if (event_list == NULL) {
      event_list = (event_list_t*) malloc (sizeof(event_list_t));
      event_list->kind = kind;
      event_list->n_calls = 1;
      event_list->total_duration = end - start;
      if (kernelName != NULL) {
         event_list->kernelName = kernelName;
      } else {
         event_list->kernelName = NULL;
      }
      if (kind == CUPTI_ACTIVITY_KIND_OPENACC_DATA) {
         event_list->total_bytes = bytes;
      } else {
         event_list->total_bytes = 0;
      }
      event_list->srcFile = srcFile;
      event_list->funcName = funcName;
      event_list->line_start = line_start;
      event_list->line_end = line_end;
      event_list->func_line_start = func_line_start;
      event_list->func_line_end = func_line_end;
      event_list->next = NULL;
   }
      event_list_t *ev = event_list;
      while (ev->next != NULL && !event_matches (ev, kind, srcFile, funcName, kernelName, line_start, line_end, func_line_start, func_line_end)) {
         ev = ev->next; 
      }
      if (ev->next == NULL) { // Event not registered
         ev->next = (event_list_t*) malloc (sizeof(event_list_t));
         ev = ev->next;
         ev->kind = kind;
         if (kind == CUPTI_ACTIVITY_KIND_OPENACC_LAUNCH) printf ("Set n_calls = 1: %s\n", funcName);
         ev->n_calls = 1;
         ev->total_duration = end - start;
         if (kernelName != NULL) {
            ev->kernelName = kernelName;
         } else {
            ev->kernelName = NULL;
         }
         if (kind == CUPTI_ACTIVITY_KIND_OPENACC_DATA) {
            ev->total_bytes += bytes;
         }
         ev->srcFile = srcFile;
         ev->funcName = funcName;
         ev->line_start = line_start;
         ev->line_end = line_end;
         ev->func_line_start = func_line_start;
         ev->func_line_end = func_line_end;
         ev->next = NULL;
      } else { // Event matches
         ev->n_calls++;
         ev->total_duration += end - start;
      }
}


void print_registered_events () {
}

static size_t openacc_records = 0;
static void printActivityOther (CUpti_ActivityOpenAccOther *r) {
     printf ("Other event: \n");
     printf ("start: %lld\n", r->start);
     printf ("end: %lld\n", r->end);
     printf ("srcFile: %s\n", r->srcFile);
     printf ("funcName: %s\n", r->funcName);
     printf ("lineNo: %d\n", r->lineNo);
     printf ("endLineNo: %d\n", r->endLineNo);
     printf ("funcLineNo: %d\n", r->funcLineNo);
     printf ("funcEndLineNo: %d\n", r->funcEndLineNo);
     printf ("**********************************\n");
}

static void printActivityLaunch (CUpti_ActivityOpenAccLaunch *r) {
   printf ("Launch event: \n");
   printf ("Kernel Name: %s\n", r->kernelName);
   printf ("start: %lld\n", r->start);
   printf ("end: %lld\n", r->end); 
   printf ("srcFile: %s\n", r->srcFile);
   printf ("funcName: %s\n", r->funcName);
   printf ("lineNo: %d\n", r->lineNo);
   printf ("endLineNo: %d\n", r->endLineNo);
   printf ("funcLineNo: %d\n", r->funcLineNo);
   printf ("funcEndLineNo: %d\n", r->funcEndLineNo);
   printf ("numGangs: %d\n", r->numGangs);
   printf ("numWorkers: %d\n", r->numWorkers);
   printf ("vectorLength: %d\n", r->vectorLength);
}


static void
printActivity(CUpti_Activity *record)
{
  switch (record->kind) {
      case CUPTI_ACTIVITY_KIND_OPENACC_DATA:
      case CUPTI_ACTIVITY_KIND_OPENACC_LAUNCH:
      case CUPTI_ACTIVITY_KIND_OPENACC_OTHER:
          {
              CUpti_ActivityOpenAcc *oacc =
                     (CUpti_ActivityOpenAcc *)record;

              if (oacc->deviceType != acc_device_nvidia) {
                  printf("Error: OpenACC device type is %u, not %u (acc_device_nvidia)\n",
                         oacc->deviceType, acc_device_nvidia);
                  exit(-1);
              }

              openacc_records++;
          }
          break;

      default:
          ;
  }
  if (record->kind == CUPTI_ACTIVITY_KIND_OPENACC_OTHER) {
     printf ("OTHER\n");
     printActivityOther((CUpti_ActivityOpenAccOther*)record);
  } else if (record->kind == CUPTI_ACTIVITY_KIND_OPENACC_LAUNCH) {
     printf ("LAUNCH\n");
     printActivityLaunch((CUpti_ActivityOpenAccLaunch*)record);
  }
}

// CUPTI buffer handling

void CUPTIAPI bufferRequested(uint8_t **buffer, size_t *size, size_t *maxNumRecords)
{
  //printf ("Buffer requested\n");
  uint8_t *bfr = (uint8_t *) malloc(BUF_SIZE + ALIGN_SIZE);
  if (bfr == NULL) {
    printf("Error: out of memory\n");
    exit(-1);
  }

  *size = BUF_SIZE;
  *buffer = ALIGN_BUFFER(bfr, ALIGN_SIZE);
  *maxNumRecords = 0;
}

void CUPTIAPI bufferCompleted(CUcontext ctx, uint32_t streamId, uint8_t *buffer, size_t size, size_t validSize)
{
  CUptiResult status;
  CUpti_Activity *record = NULL;

  if (validSize > 0) {
    do {
      status = cuptiActivityGetNextRecord(buffer, validSize, &record);
      if (status == CUPTI_SUCCESS) {
        register_event (record);     
      }
      else if (status == CUPTI_ERROR_MAX_LIMIT_REACHED)
        break;
      else {
        CUPTI_CALL(status);
      }
    } while (1);

    // report any records dropped from the queue
    size_t dropped;
    CUPTI_CALL(cuptiActivityGetNumDroppedRecords(ctx, streamId, &dropped));
    if (dropped != 0) {
      printf("Dropped %u activity records\n", (unsigned int) dropped);
    }
  }

  free(buffer);
}

void finalize() {
  cuptiActivityFlushAll(0);
  printf ("Finalize CUPTI\n");
  if (mpi_rank == 0) {
      printf ("KERNEL EVENTS: \n");
      print_event_list(true, CUPTI_ACTIVITY_KIND_OPENACC_LAUNCH);
      printf ("OTHER EVENTS: \n");
      print_event_list(true, CUPTI_ACTIVITY_KIND_OPENACC_OTHER);
      printf ("DATA EVENTS: \n");
      print_event_list(true, CUPTI_ACTIVITY_KIND_OPENACC_DATA);
  }
}

// acc_register_library is defined by the OpenACC tools interface
// and allows to register this library with the OpenACC runtime.

extern "C" void
acc_register_library(void *profRegister, void *profUnregister, void *profLookup)
{
  // once connected to the OpenACC runtime, initialize CUPTI's OpenACC interface
  if (cuptiOpenACCInitialize(profRegister, profUnregister, profLookup) != CUPTI_SUCCESS) {
    printf("Error: Failed to initialize CUPTI OpenACC support\n");
    exit(-1);
  }

  printf("Initialized CUPTI OpenACC\n");
  event_list = NULL;
  callback_counter = 0;

  MPI_Comm_rank (MPI_COMM_WORLD, &mpi_rank);

  // only interested in OpenACC activity records
  CUPTI_CALL(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_OPENACC_DATA));
  CUPTI_CALL(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_OPENACC_LAUNCH));
  CUPTI_CALL(cuptiActivityEnable(CUPTI_ACTIVITY_KIND_OPENACC_OTHER));

  // setup CUPTI buffer handling
  CUPTI_CALL(cuptiActivityRegisterCallbacks(bufferRequested, bufferCompleted));

  // at program exit, flush CUPTI buffers and print results
  atexit(finalize);
}

