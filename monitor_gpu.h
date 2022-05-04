#ifndef MONITOR_GPU_H
#define MONITOR_GPU_H

#include <Python.h>
#include "structmember.h"

#include <string>
#include "nvml_interface.h"

typedef struct {
   PyObject_HEAD
   std::string gpu_name;
   int num_cores;
   int temp;
   int freq;
   int pcie_rate;
   int power_usage; //mW
   unsigned int gpu_util;
   unsigned int mem_util;
} device_info;

static int deviceInfo_tp_init (device_info *self, PyObject *args, PyObject *kwargs);
static PyObject *deviceInfo_tp_new (PyTypeObject *type, PyObject *args, PyObject *kwargs);
static int deviceInfo_tp_clear (device_info *self);
static int deviceInfo_tp_traverse (device_info *self, visitproc visit, void *arg);
static void deviceInfo_tp_dealloc (device_info *self);


#endif
