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
  printf ("check if NULL: %d\n", getNVMLDeviceNumCores != NULL);
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

NVMLDevice::NVMLDevice(unsigned int index, const nvmlDevice_t device_handle, const NVML &nvmlAPI):
   index{index},
   device_handle{device_handle},
   nvmlAPI{nvmlAPI}
{
   name = nvmlAPI.getDeviceName(index, device_handle);
}

NVMLDevice::~NVMLDevice() {
}

int NVMLDevice::get_metrics() {
   int temperature = nvmlAPI.getTemperature(index, device_handle);
   return temperature;
}

int NVMLDevice::getFrequency() {
  return nvmlAPI.getFrequency(index, device_handle);
}

std::string NVMLDevice::getName() {
  return nvmlAPI.getDeviceName(index, device_handle);
}

DEVICE_RETURN_T NVMLDevice::getNumCores() {
  return nvmlAPI.getNumCores (index, device_handle);
}

int NVMLDevice::getPcieRate() {
  return nvmlAPI.getPcieRate (index, device_handle);
}

int NVMLDevice::getPowerUsage() {
  return nvmlAPI.getPowerUsage (index, device_handle);
}

void NVMLDevice::getUtilization(unsigned int *gpu, unsigned int *memory) {
  nvmlAPI.getUtilization(index, device_handle, gpu, memory);
}

void NVMLDevice::getMemoryInfo(unsigned long long *free, unsigned long long *total,
                               unsigned long long *used) {
  nvmlAPI.getMemoryInfo(index, device_handle, free, total, used);
}

void NVMLDevice::getProcessInfo(unsigned int *n_procs, unsigned int *max_running_processes, int **proc_ids) {
  nvmlAPI.getProcessInfo (index, device_handle, n_procs, max_running_processes, proc_ids);
}

NVMLDeviceManager::NVMLDeviceManager (const NVML &nvmlAPI):
   nvmlAPI(nvmlAPI)
{
   device_count = nvmlAPI.getDeviceCount();
   if (device_count == 0) {
      std::cout << "No devices found!" << std::endl;
      temps = NULL;
   } else {
      //std::cout << "Initialized device manager with " << device_count << " devices." << std::endl; 
      temps = (int*)malloc(device_count * sizeof(int));
   }
   for (unsigned int device_index{0}; device_index < device_count; device_index++) {
         nvmlDevice_t device_handle = nvmlAPI.getDeviceHandle(device_index);         
         NVMLDevice device{device_index, device_handle, nvmlAPI};
         //device.get_metrics();
         devices.push_back(device);
   }
}

NVMLDeviceManager::~NVMLDeviceManager () {
   devices.clear();
   free(temps);
}

void NVMLDeviceManager::readOutValues() {
  for (int device_index{0}; device_index < device_count; device_index++) {
     auto device = devices[device_index]; 
     temps[device_index] = device.get_metrics(); 
  }
}

int NVMLDeviceManager::getTemp(int index) {
   auto device = devices[index];
   return device.get_metrics();
}

int NVMLDeviceManager::getFrequency(int index) {
  auto device = devices[index];
  return device.getFrequency();
}

std::string NVMLDeviceManager::getName(int index) {
   auto device = devices[index];
   return device.getName();
}

DEVICE_RETURN_T NVMLDeviceManager::getNumCores(int index) {
  auto device = devices[index];
  return device.getNumCores();
}

int NVMLDeviceManager::getPcieRate(int index) {
  auto device = devices[index];
  return device.getPcieRate();
}

int NVMLDeviceManager::getPowerUsage(int index) {
  auto device = devices[index];
  return device.getPowerUsage();
}

void NVMLDeviceManager::getUtilization(int index, unsigned int *gpu, unsigned int *memory) {
  auto device = devices[index];
  device.getUtilization(gpu, memory);
}

void NVMLDeviceManager::getMemoryInfo(int index, unsigned long long *free,
                                      unsigned long long *total, unsigned long long *used) {
  auto device = devices[index];
  device.getMemoryInfo(free, total, used);
}

void NVMLDeviceManager::getProcessInfo(int index, unsigned int *n_procs, unsigned int *max_running_processes, int **proc_ids) {
  auto device = devices[index];
  device.getProcessInfo (n_procs, max_running_processes, proc_ids);
} 

void NVMLDeviceManager::displayValues(int index) {
   if (index >= 0) {
      printf ("Device %d: %d C\n", index, temps[index]);
   } else {
      for (int device_index{0}; device_index < device_count; device_index++) {
         printf ("Device %d: %d C\n", device_index, temps[device_index]);
      }
   }
}
