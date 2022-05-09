#include "monitor_gpu.h"
#include "dgemm.h"
#include "stream.h"
#include <iostream>

static PyObject *showTemps (PyObject *self, PyObject *args) {
   NVML nvml;
   NVMLDeviceManager device_manager{nvml};
   device_manager.readOutValues();
   device_manager.displayValues();
   Py_RETURN_NONE;
}

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
     printf ("OH NO!\n");
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

static PyObject *readOut (device_info *self) {
   NVML nvml;
   NVMLDeviceManager device_manager{nvml};
   device_manager.readOutValues();
   self->temp = device_manager.getTemp(0); 
   self->freq = device_manager.getFrequency(0); 
   self->pcie_rate = device_manager.getPcieRate(0);
   self->power_usage = device_manager.getPowerUsage(0);
   device_manager.getUtilization(0, &(self->gpu_util), &(self->mem_util));
   device_manager.getMemoryInfo(0, &(self->memory.free), &(self->memory.total), &(self->memory.used));
   Py_RETURN_NONE;
}

static PyObject *getItems (device_info *self) {
  return Py_BuildValue("{s:i,s:i,s:i,s:i}",
                       "Temperature", self->temp,
                       "Frequency", self->freq,
                       "PCIE", self->pcie_rate,
                       "Power", self->power_usage);
}

static PyObject *getUtilization(device_info *self) {
  return Py_BuildValue("{s:i,s:i}",
                       "GPU", self->gpu_util,
                       "Memory", self->mem_util);
}

static PyObject *getMemoryInfo(device_info *self) {
  long long free_gb = self->memory.free;
  long long total_gb = self->memory.total;
  long long used_gb = self->memory.used;
  
  return Py_BuildValue("{s:L,s:L,s:L}",
                       "Free", free_gb,
                       "Total", total_gb,
                       "Used", used_gb);
}

static PyObject *getDeviceName (device_info *self) {
  return Py_BuildValue("s", self->gpu_name);
}

static PyObject *getNumCores (device_info *self) {
  return Py_BuildValue("i", self->num_cores);
}

static PyMethodDef deviceMethods[] = {
   {"readOut", (PyCFunction)readOut, METH_NOARGS, "TBD"},
   {"getItems", (PyCFunction)getItems, METH_NOARGS, "TBD"},
   {"getUtilization", (PyCFunction)getUtilization, METH_NOARGS, "TBD"},
   {"getDeviceName", (PyCFunction)getDeviceName, METH_NOARGS, "TBD"},
   {"getNumCores", (PyCFunction)getNumCores, METH_NOARGS, "TBD"},
   {"getMemoryInfo", (PyCFunction)getMemoryInfo, METH_NOARGS, "TBD"},
   {NULL}
};

static PyMemberDef device_info_members[] = {
   {"temp", T_INT, 0, 0, "Temperature in Celsius"},
   {NULL}
};

static PyTypeObject deviceInfoType = {
   PyVarObject_HEAD_INIT(NULL, 0)
   .tp_name = "nvml.deviceInfo",
   .tp_basicsize = sizeof(device_info),
   .tp_dealloc = (destructor)deviceInfo_tp_dealloc,
   .tp_flags = Py_TPFLAGS_DEFAULT,
   .tp_traverse = (traverseproc)deviceInfo_tp_traverse,
   .tp_clear = (inquiry)deviceInfo_tp_clear, 
   .tp_methods = deviceMethods,
   .tp_members = device_info_members,
   .tp_init = (initproc)deviceInfo_tp_init,
   .tp_new = deviceInfo_tp_new
};

static int deviceInfo_tp_init (device_info *self, PyObject *args, PyObject *kwargs) {
   //char *kwlist[] = {"temp", NULL};
   //
   //printf ("tp_init!\n");
   //if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ss:__init__", kwlist)) {
   //   return -1;
   //}
   //printf ("in tp_init!\n");
   NVML nvml;
   NVMLDeviceManager device_manager{nvml};
   self->gpu_name = device_manager.getName(0);
   self->num_cores = device_manager.getNumCores(0);
   self->temp = 0;
   self->freq = 0;
   self->pcie_rate = 0;
   self->power_usage = 0;
   std::cout << "Profiling: " << self->gpu_name << std::endl;
   std::cout << "   has " << self->num_cores << " cores." << std::endl;
   return 0;
}

static PyObject *deviceInfo_tp_new (PyTypeObject *type, PyObject *args, PyObject *kwargs) {
   device_info *self = (device_info*)type->tp_alloc(type, 0);
   return (PyObject*)self;
}

static int deviceInfo_tp_clear (device_info *self) {
   //Py_CLEAR(self->temp);
   return 0;
}

static int deviceInfo_tp_traverse (device_info *self, visitproc visit, void *arg) {
   //Py_VISIT(self->temp);
   return 0;
}

static void deviceInfo_tp_dealloc (device_info *self) {
   deviceInfo_tp_clear(self);
}

static PyMethodDef nvml_methods[] = {
   {
       "showTemps", showTemps, METH_NOARGS,
       "Show temperatures of all GPUs."
   },
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
   PyModule_AddType(thisPy, &deviceInfoType);
   printf ("HUHU from NVML!\n");
   return thisPy;
}
