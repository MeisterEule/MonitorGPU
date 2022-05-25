#include "monitor_gpu.h"
#include "dgemm.h"
#include "stream.h"
#include <iostream>
#include "cuda_runtime_api.h"

static PyObject *dgemmMaxMatrixSize (PyObject *self, PyObject *args, PyObject *kwargs) {
   double mem_size;
   static char *keywords[] = {"memsize", NULL};
   if (!PyArg_ParseTupleAndKeywords(args, kwargs, "d", keywords, &mem_size)) {
      return NULL;
   }

   int nmax = dgemmMaxSize (mem_size);
   return Py_BuildValue("i", nmax); 
}

static PyObject *streamMaxVectorSize (PyObject *self, PyObject *args, PyObject *kwargs) {
   long mem_size;
   static char *keywords[] = {"memsize", NULL};
   if (!PyArg_ParseTupleAndKeywords(args, kwargs, "l", keywords, &mem_size)) {
      return NULL;
   }

   printf ("Input memory: %ld", mem_size);
   double nmax = stream_max_vector_size(mem_size);
   return Py_BuildValue("d", nmax); 
}


static PyObject *performDgemm (PyObject *self, PyObject *args, PyObject *kwargs) {
   int N = 1000;
   const double alpha = 1.0;
   const double beta = 1.0;
   int n_repeats = 1.0;
   
   static char *keywords[] = {"N", "nrepeats", NULL};
   if (!PyArg_ParseTupleAndKeywords(args, kwargs, "i|i", keywords, &N, &n_repeats)) {
     //return PyErr_BadArgument();
     return NULL;
   }
   printf ("Input: %d %d\n", N, n_repeats);

   double gflops_avg, gflops_min, gflops_max, gflops_stddev;
   unsigned int status;
   doDgemm(N, alpha, beta, n_repeats,
           &gflops_avg, &gflops_min, &gflops_max, &gflops_stddev, &status);

   const char *str_status;
   if (status == DGEMM_CUBLAS_SUCCESS) {
      str_status = "OK";
   } else if (status == DGEMM_CUBLAS_ERR_OOM) {
      str_status = "OOM";
   } else {
      str_status = "Unknown";
   }

   return Py_BuildValue("{s:s,s:d,s:d,s:d,s:d}",
                       "Status", str_status,
                       "Avg", gflops_avg,
                       "Min", gflops_min,
                       "Max", gflops_max,
                       "Stddev", gflops_stddev);
}

static PyObject *performStream (PyObject *self, PyObject *args, PyObject *kwargs) {
  static char *keywords[] = {"array_size", "n_times", NULL};
  int array_size = 10000;
  int n_times = 10;
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ii", keywords, &array_size, &n_times)) {
     return NULL;
  }

  int status;
  double bw_copy, bw_scale, bw_add, bw_triad;
  do_stream (array_size, n_times, &bw_copy, &bw_scale, &bw_add, &bw_triad, &status);

  const char *str_status;
  if (status == STREAM_SUCCESS) {
     str_status = "OK";
     return Py_BuildValue("{s:s,s:f,s:f,s:f,s:f}",
                            "Status", str_status,
                            "Copy", bw_copy,
                            "Scale", bw_scale,
                            "Add", bw_add,
                            "Triad", bw_triad);
  } else if (status == STREAM_OOM_HOST) {
     str_status = "OOM_HOST";
     return Py_BuildValue("{s:s}", "Status", str_status);
  } else if (status == STREAM_OOM_HOST) {
     str_status = "OOM_DEVICE";
     return Py_BuildValue("{s:s}", "Status", str_status);
  } else {
     str_status = "INVALID";
     return Py_BuildValue("{s:s}", "Status", str_status);
  }
}

static PyObject *readOut (device_manager_t *self) {
   NVML nvml;
   NVMLDeviceManager device_manager{nvml};
   for (int i = 0; i < self->num_devices; i++) {
      self->temp[i] = device_manager.getTemperature(i); 
      self->freq[i] = device_manager.getFrequency(i); 
      self->pcie_rate[i] = device_manager.getPcieRate(i);
      self->power_usage[i] = device_manager.getPowerUsage(i);
      device_manager.getUtilization(i, &(self->gpu_util[i]), &(self->mem_util[i]));
      device_manager.getMemoryInfo(i, &(self->memory[i].free), &(self->memory[i].total), &(self->memory[i].used));
   }
   Py_RETURN_NONE;
}

static PyObject *getItems (device_manager_t *self, PyObject *args) {
  int device_id;
  if (!PyArg_ParseTuple (args, "i", &device_id)) {
    //return PyErr_BadArgument();
    return NULL;
  }
  return Py_BuildValue("{s:i,s:i,s:i,s:i,s:i,s:i}",
                       "Temperature", self->temp[device_id],
                       "Frequency", self->freq[device_id],
                       "PCIE", self->pcie_rate[device_id],
                       "Power", self->power_usage[device_id],
                       "GPU-Util", self->gpu_util[device_id],
                       "Memory-Util", self->mem_util[device_id]);
}

static PyObject *getUtilization(device_manager_t *self, PyObject *args) {
  int device_id;
  if (!PyArg_ParseTuple (args, "i", &device_id)) {
     //return PyErr_BadArgument();
     return NULL;
  }
  return Py_BuildValue("{s:i,s:i}",
                       "GPU", self->gpu_util[device_id],
                       "Memory", self->mem_util[device_id]);
}

static PyObject *getMemoryInfo(device_manager_t *self, PyObject *args) {
  int device_id;
  if (!PyArg_ParseTuple (args, "i", &device_id)) {
     //return PyErr_BadArgument();
     return NULL;
  }
  long long free_gb = self->memory[device_id].free;
  long long total_gb = self->memory[device_id].total;
  long long used_gb = self->memory[device_id].used;
  
  return Py_BuildValue("{s:L,s:L,s:L}",
                       "Free", free_gb,
                       "Total", total_gb,
                       "Used", used_gb);
}

static PyObject *getDeviceName (device_manager_t *self, PyObject *args) {
  int device_id;
  if (!PyArg_ParseTuple (args, "i", &device_id)) {
     //return PyErr_BadArgument();
     return NULL;
  }
  return Py_BuildValue("s", self->gpu_names[device_id].c_str());
}

static PyObject *getNumCores (device_manager_t *self, PyObject *args) {
  int device_id;
  if (!PyArg_ParseTuple (args, "i", &device_id)) {
     //return PyErr_BadArgument();
     return NULL;
  }
  if (self->num_cores[device_id] != DEVICE_RETURN_INVALID) {
     return Py_BuildValue("{s:i}",
                          "GPUCores", self->num_cores[device_id]);
  } else {
     return Py_BuildValue("{}");
  }
}

static PyObject *getProcessInfo (device_manager_t *self, PyObject *args) {
  int device_id;
  if (!PyArg_ParseTuple (args, "i", &device_id)) {
     //return PyErr_BadArgument();
     return NULL;
  }
  NVML nvml;
  NVMLDeviceManager device_manager{nvml};
  device_manager.getProcessInfo(device_id, &(self->current_processes[device_id]),
                                &(self->max_running_processes[device_id]),
                                &(self->process_ids[device_id]));
  PyObject *ret = PyList_New(self->current_processes[device_id]);
  for (int i = 0; i < self->current_processes[device_id]; i++) {
     PyObject *python_int = Py_BuildValue("i", self->process_ids[device_id][i]);
     PyList_SetItem (ret, i, python_int);
  }
  return ret;
}

static PyObject *getNumGpus (device_manager_t *self, PyObject *args) {
  int n_gpus;
  cudaGetDeviceCount (&n_gpus);
  return Py_BuildValue("i", n_gpus); 
}

static PyMethodDef deviceMethods[] = {
   {"readOut", (PyCFunction)readOut, METH_NOARGS, "TBD"},
   {"getItems", (PyCFunction)getItems, METH_VARARGS, "TBD"},
   {"getUtilization", (PyCFunction)getUtilization, METH_VARARGS, "TBD"},
   {"getDeviceName", (PyCFunction)getDeviceName, METH_VARARGS, "TBD"},
   {"getNumCores", (PyCFunction)getNumCores, METH_VARARGS, "TBD"},
   {"getMemoryInfo", (PyCFunction)getMemoryInfo, METH_VARARGS, "TBD"},
   {"getProcessInfo", (PyCFunction)getProcessInfo, METH_VARARGS, "TBD"},
   {"getNumGpus", (PyCFunction)getNumGpus, METH_NOARGS, "TBD"},
   {NULL}
};

static PyTypeObject deviceManagerType = {
   PyVarObject_HEAD_INIT(NULL, 0)
   .tp_name = "nvml.deviceManager",
   .tp_basicsize = sizeof(device_manager_t),
   .tp_dealloc = (destructor)deviceManager_tp_dealloc,
   .tp_flags = Py_TPFLAGS_DEFAULT,
   .tp_traverse = (traverseproc)deviceManager_tp_traverse,
   .tp_clear = (inquiry)deviceManager_tp_clear, 
   .tp_methods = deviceMethods,
   .tp_init = (initproc)deviceManager_tp_init,
   .tp_new = deviceManager_tp_new
};

static int deviceManager_tp_init (device_manager_t *self, PyObject *args, PyObject *kwargs) {
   NVML nvml;
   NVMLDeviceManager device_manager{nvml};

   self->num_devices = device_manager.num_devices;
   self->memory = (nvml_memory_t *) malloc(self->num_devices * sizeof(nvml_memory_t));
   self->max_running_processes = (unsigned int *) malloc(self->num_devices * sizeof(unsigned int));
   self->current_processes = (unsigned int *) malloc(self->num_devices * sizeof(unsigned int));
   self->process_ids = (int **) malloc(self->num_devices * sizeof(int*));

   for (int i = 0; i < self->num_devices; i++) {
     self->gpu_names.push_back(device_manager.getName(i));
     self->num_cores.push_back(device_manager.getNumCores(i));
     self->temp.push_back(0);
     self->freq.push_back(0);
     self->pcie_rate.push_back(0);
     self->power_usage.push_back(0);
     self->gpu_util.push_back(0);
     self->mem_util.push_back(0);
     self->memory[i].free = 0;
     self->memory[i].used = 0;
     self->memory[i].total = 0;
     self->current_processes[i] = 0;
     self->max_running_processes[i] = 0;
     self->process_ids[i] = NULL;
   }
   return 0;
}

static PyObject *deviceManager_tp_new (PyTypeObject *type, PyObject *args, PyObject *kwargs) {
   device_manager_t *self = (device_manager_t*)type->tp_alloc(type, 0);
   return (PyObject*)self;
}

static int deviceManager_tp_clear (device_manager_t *self) {
   //Py_CLEAR(self->temp);
   return 0;
}

static int deviceManager_tp_traverse (device_manager_t *self, visitproc visit, void *arg) {
   //Py_VISIT(self->temp);
   return 0;
}

static void deviceManager_tp_dealloc (device_manager_t *self) {
   deviceManager_tp_clear(self);
}

static PyMethodDef nvml_methods[] = {
   {
       "performDgemm", (PyCFunction)performDgemm, METH_VARARGS | METH_KEYWORDS,
       "Do DGEMM",
   },
   {
       "dgemmMaxMatrixSize", (PyCFunction)dgemmMaxMatrixSize, METH_VARARGS | METH_KEYWORDS,
       "Maximal size for DGEMM matrices",
   },
   {
       "performStream", (PyCFunction)performStream, METH_VARARGS | METH_KEYWORDS,
       "Do STREAM",
   },
   {
       "streamMaxVectorSize", (PyCFunction)streamMaxVectorSize, METH_VARARGS | METH_KEYWORDS,
       "Maximal size for DGEMM matrices",
   },
   {NULL, NULL, 0, NULL}
};

static struct PyModuleDef nvml_definition = {
   PyModuleDef_HEAD_INIT,
   "nvml",
   "Display GPU hardware counters.", 
   -1,
   nvml_methods
};

PyMODINIT_FUNC PyInit_nvml(void) {
   Py_Initialize();
   PyObject *thisPy = PyModule_Create(&nvml_definition);
   PyModule_AddType(thisPy, &deviceManagerType);
   return thisPy;
}
