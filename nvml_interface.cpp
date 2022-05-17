#include <dlfcn.h>
#include <iostream>

#include "nvml_interface.h"

NVML::NVML(std::string_view lib_name) {
  nvml_solib = dlopen (lib_name.data(), RTLD_LAZY); 
  bind_functions();
  auto nv_status = nvmlInit();
  if (nv_status != nvmlReturn_t::NVML_SUCCESS) {
     printf ("Could not init NVML!\n");
  } else {
     char driver_version[NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE];
     char nvml_version[NVML_SYSTEM_NVML_VERSION_BUFFER_SIZE];
     auto nv_status_1 = getDriverVersion(driver_version, NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE);
     auto nv_status_2 = getNVMLVersion(nvml_version, NVML_SYSTEM_NVML_VERSION_BUFFER_SIZE);
  }
}

sofunc_handle_t load_func_or_halt (solib_handle_t lib, std::string_view func_name) {
   sofunc_handle_t func_handle = dlsym (lib, func_name.data());
   return func_handle;
}

void NVML::bind_functions() {
  nvmlInit = reinterpret_cast<nvmlInit_t>(dlsym(nvml_solib, "nvmlInit"));
  getNVMLDeviceCount = reinterpret_cast<nvmlDeviceGetCount_t>(dlsym(nvml_solib, "nvmlDeviceGetCount"));
  getDriverVersion = reinterpret_cast<nvmlSystemGetDriverVersion_t>(dlsym(nvml_solib, "nvmlSystemGetDriverVersion"));
  getNVMLVersion = reinterpret_cast<nvmlSystemGetNVMLVersion_t>(dlsym(nvml_solib, "nvmlSystemGetNVMLVersion"));
  getNVMLDeviceHandleByIndex = reinterpret_cast<nvmlDeviceGetHandleByIndex_t>(dlsym(nvml_solib, "nvmlDeviceGetHandleByIndex"));
  getNVMLDeviceName = reinterpret_cast<nvmlDeviceGetName_t>(dlsym(nvml_solib, "nvmlDeviceGetName")); 
  getNVMLTemperature = reinterpret_cast<nvmlDeviceGetTemperature_t>(dlsym(nvml_solib, "nvmlDeviceGetTemperature"));
  getNVMLFrequency = reinterpret_cast<nvmlDeviceGetClock_t>(dlsym(nvml_solib, "nvmlDeviceGetClock"));
  getNVMLDeviceNumCores = reinterpret_cast<nvmlDeviceGetNumCores_t>(dlsym(nvml_solib, "nvmlDeviceGetNumGpuCores"));
  getNVMLDevicePcieThroughput = reinterpret_cast<nvmlDeviceGetPcieThroughput_t>(dlsym(nvml_solib, "nvmlDeviceGetPcieThroughput"));
  getNVMLDevicePowerUsage = reinterpret_cast<nvmlDeviceGetPowerUsage_t>(dlsym(nvml_solib, "nvmlDeviceGetPowerUsage"));
  getNVMLDeviceUtilization = reinterpret_cast<nvmlDeviceGetUtilizationRates_t>(dlsym(nvml_solib, "nvmlDeviceGetUtilizationRates"));
  getNVMLMemoryInfo = reinterpret_cast<nvmlDeviceGetMemoryInfo_t>(dlsym(nvml_solib, "nvmlDeviceGetMemoryInfo"));

  getNVMLProcInfo = reinterpret_cast<nvmlDeviceGetProcInfo_t>(dlsym(nvml_solib, "nvmlDeviceGetComputeRunningProcesses_v3"));
  if (getNVMLProcInfo == NULL) {
     getNVMLProcInfo = reinterpret_cast<nvmlDeviceGetProcInfo_t>(dlsym(nvml_solib, "nvmlDeviceGetComputeRunningProcesses_v2"));
  }
  if (getNVMLProcInfo == NULL) {
     getNVMLProcInfo = reinterpret_cast<nvmlDeviceGetProcInfo_t>(dlsym(nvml_solib, "nvmlDeviceGetComputeRunningProcesses"));
  }
}

NVML::~NVML() {}

unsigned int NVML::getDeviceCount() const {
   unsigned int device_count{0};
   auto nv_status = getNVMLDeviceCount(&device_count);
   return device_count;
}

nvmlDevice_t NVML::getDeviceHandle(int index) const {
   nvmlDevice_t device_handle;
   auto nv_status = getNVMLDeviceHandleByIndex(index, &device_handle);
   return device_handle;
}

std::string NVML::getDeviceName (const unsigned int index, const nvmlDevice_t &device_handle) const {
   char value[NVML_DEVICE_NAME_BUFFER_SIZE];
   auto nv_status = getNVMLDeviceName(device_handle, value, NVML_DEVICE_NAME_BUFFER_SIZE);
   return std::string(value);
}

// This does not compile without an additional const????
unsigned int NVML::getTemperature (const unsigned int index, const nvmlDevice_t &device_handle) const {
   unsigned int value;
   auto nv_status = getNVMLTemperature(device_handle, NVML_TEMPERATURE_GPU, &value);
   return value;
}

unsigned int NVML::getFrequency (const unsigned int index, const nvmlDevice_t &device_handle) const {
   unsigned int value;
   auto nv_status = getNVMLFrequency(device_handle, NVML_CLOCK_TYPE_GRAPHICS,
                                     NVML_CLOCK_ID_CURRENT, &value);
   return value;
}

DEVICE_RETURN_T NVML::getNumCores (const unsigned int index, const nvmlDevice_t &device_handle) const {
   unsigned int value;
   if (getNVMLDeviceNumCores != NULL) {
     auto nv_status = getNVMLDeviceNumCores(device_handle, &value);
     return (DEVICE_RETURN_T)value;
   } else {
     return DEVICE_RETURN_INVALID;
   }
}

unsigned int NVML::getPcieRate (const unsigned int index, const nvmlDevice_t &device_handle) const {
   unsigned int value;
   auto nv_status = getNVMLDevicePcieThroughput(device_handle, NVML_PCIE_UTIL_TX_BYTES, &value);
   return value;
}

unsigned int NVML::getPowerUsage (const unsigned int index, const nvmlDevice_t &device_handle) const {
   unsigned int value;
   auto nv_status = getNVMLDevicePowerUsage(device_handle, &value);
   return value;
}

void NVML::getUtilization(const unsigned int index, const nvmlDevice_t &device_handle,
                          unsigned int *gpu, unsigned int *memory) const {
  nvml_utilization_t util;
  auto nv_status = getNVMLDeviceUtilization(device_handle, &util); 
  *gpu = util.gpu;
  *memory = util.memory;
}

void NVML::getMemoryInfo(const unsigned int index, const nvmlDevice_t &device_handle,
                         unsigned long long *free, unsigned long long *total, unsigned long long *used) const {
   nvml_memory_t memory;
   auto nv_status = getNVMLMemoryInfo(device_handle, &memory);
   *free = memory.free;
   *total = memory.total;
   *used = memory.used;
}

void NVML::getProcessInfo (const unsigned int index, const nvmlDevice_t &device_handle, unsigned int *max_running_processes, unsigned int *info_count, int **proc_ids) const {
   auto nv_status = getNVMLProcInfo (device_handle, info_count, NULL);
   if (*info_count > 0) {
     if (*max_running_processes == 0) {
       *proc_ids = (int*) malloc (*info_count * sizeof(int*));
       *max_running_processes = *info_count;
     } else if (*info_count > *max_running_processes) {
       *proc_ids = (int*) realloc (*proc_ids, *info_count * sizeof(int*));
       *max_running_processes = *info_count;
     } 
     nvml_proc_info_t *proc_info = (nvml_proc_info_t*) malloc (*info_count * sizeof(nvml_proc_info_t));
     nv_status = getNVMLProcInfo (device_handle, info_count, proc_info);
     for (int i = 0; i < *info_count; i++) {
        *proc_ids[i] = proc_info[i].computeInstanceId;
     }
     free (proc_info);
   }
}

NVMLDeviceManager::NVMLDeviceManager (const NVML &nvmlAPI):
   nvmlAPI(nvmlAPI)
{
   device_count = nvmlAPI.getDeviceCount();
   for (unsigned int device_index{0}; device_index < device_count; device_index++) {
         nvmlDevice_t device_handle = nvmlAPI.getDeviceHandle(device_index);         
         device_handles.push_back(device_handle);
   }
}

NVMLDeviceManager::~NVMLDeviceManager () {
   device_handles.clear();
}

int NVMLDeviceManager::getTemp(int index) {
   return nvmlAPI.getTemperature(index, device_handles[index]);
}

int NVMLDeviceManager::getFrequency(int index) {
  return nvmlAPI.getFrequency(index, device_handles[index]);
}

std::string NVMLDeviceManager::getName(int index) {
   return nvmlAPI.getDeviceName(index, device_handles[index]);
}

DEVICE_RETURN_T NVMLDeviceManager::getNumCores(int index) {
  return nvmlAPI.getNumCores(index, device_handles[index]);
}

int NVMLDeviceManager::getPcieRate(int index) {
  return nvmlAPI.getPcieRate(index, device_handles[index]);
}

int NVMLDeviceManager::getPowerUsage(int index) {
  return nvmlAPI.getPowerUsage(index, device_handles[index]);
}

void NVMLDeviceManager::getUtilization(int index, unsigned int *gpu, unsigned int *memory) {
  nvmlAPI.getUtilization(index, device_handles[index], gpu, memory);
}

void NVMLDeviceManager::getMemoryInfo(int index, unsigned long long *free,
                                      unsigned long long *total, unsigned long long *used) {
  nvmlAPI.getMemoryInfo(index, device_handles[index], free, total, used);
}

void NVMLDeviceManager::getProcessInfo(int index, unsigned int *n_procs, unsigned int *max_running_processes, int **proc_ids) {
  nvmlAPI.getProcessInfo (index, device_handles[index], n_procs, max_running_processes, proc_ids);
} 
