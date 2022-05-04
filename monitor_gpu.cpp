#include "monitor_gpu.h"

static PyObject *showTemps (PyObject *self, PyObject *args) {
   NVML nvml;
   NVMLDeviceManager device_manager{nvml};
   device_manager.readOutValues();
   device_manager.displayValues();
   Py_RETURN_NONE;
}

static PyObject *readOut (device_info *self) {
   NVML nvml;
   NVMLDeviceManager device_manager{nvml};
   device_manager.readOutValues();
   self->temp = device_manager.getTemp(0); 
   Py_RETURN_NONE;
}

static PyObject *getTemp (device_info *self) {
  return Py_BuildValue("i", self->temp);
}

static PyObject *getItems (device_info *self) {
  return Py_BuildValue("{s:i}", "Temperature", self->temp);
}

static PyMethodDef deviceMethods[] = {
   {"readOut", (PyCFunction)readOut, METH_NOARGS, "TBD"},
   {"getTemp", (PyCFunction)getTemp, METH_NOARGS, "TBD"},
   {"getItems", (PyCFunction)getItems, METH_NOARGS, "TBD"},
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

   self->temp = 0;
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