#ifndef MONITOR_GPU_H
#define MONITOR_GPU_H

#include <Python.h>
#include "structmember.h"

#include <string>
#include "nvml_interface.h"

typedef struct {
   PyObject_HEAD
   int num_devices;
   std::vector<std::string> gpu_names;
   std::vector<int> num_cores;
   std::vector<int> temp;
   std::vector<int> freq;
   std::vector<int> pcie_rate;
   std::vector<int> power_usage; //mW
   std::vector<unsigned int> gpu_util;
   std::vector<unsigned int> mem_util;
   nvml_memory_t* memory; 
   unsigned int* max_running_processes;
   unsigned int* current_processes;
   int **process_ids;
} device_manager_t;

static int deviceManager_tp_init (device_manager_t *self, PyObject *args, PyObject *kwargs);
static PyObject *deviceManager_tp_new (PyTypeObject *type, PyObject *args, PyObject *kwargs);
static int deviceManager_tp_clear (device_manager_t *self);
static int deviceManager_tp_traverse (device_manager_t *self, visitproc visit, void *arg);
static void deviceManager_tp_dealloc (device_manager_t *self);


#endif
