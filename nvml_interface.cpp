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
     //if (nv_status_1 == nvmlReturn_t::NVML_SUCCESS && nv_status_2 == nvmlReturn_t::NVML_SUCCESS) {
     //   std::cout << "Successfully loaded NVML: " << std::endl;
     //   std::cout << "  Cuda version: " << driver_version << std::endl;
     //   std::cout << "  NVML version: " << nvml_version << std::endl;
     //}
  }
}

sofunc_handle_t load_func_or_halt (solib_handle_t lib, std::string_view func_name) {
   sofunc_handle_t handle = dlsym (lib, func_name.data());
   if (handle == NULL) {
     //printf ("Failed to get %s " + std::string(func_name) + " from library."); 
     std::cout << "Failed to get " << std::string(func_name) << " from library." << std::endl;
   }
   return handle;
}

void NVML::bind_functions() {
  nvmlInit = reinterpret_cast<nvmlInit_t>(load_func_or_halt(nvml_solib, "nvmlInit"));
  getNVMLDeviceCount = reinterpret_cast<nvmlDeviceGetCount_t>(load_func_or_halt(nvml_solib, "nvmlDeviceGetCount"));
  getDriverVersion = reinterpret_cast<nvmlSystemGetDriverVersion_t>(load_func_or_halt(nvml_solib, "nvmlSystemGetDriverVersion"));
  getNVMLVersion = reinterpret_cast<nvmlSystemGetNVMLVersion_t>(load_func_or_halt(nvml_solib, "nvmlSystemGetNVMLVersion"));
  getNVMLDeviceHandleByIndex = reinterpret_cast<nvmlDeviceGetHandleByIndex_t>(load_func_or_halt(nvml_solib, "nvmlDeviceGetHandleByIndex"));
  getNVMLDeviceName = reinterpret_cast<nvmlDeviceGetName_t>(load_func_or_halt(nvml_solib, "nvmlDeviceGetName")); 
  getNVMLTemperature = reinterpret_cast<nvmlDeviceGetTemperature_t>(load_func_or_halt(nvml_solib, "nvmlDeviceGetTemperature"));
}

NVML::~NVML() {
}

unsigned int NVML::getDeviceCount() const {
   unsigned int device_count{0};
   auto nv_status = getNVMLDeviceCount(&device_count);
   return device_count;
}

nvmlDevice_t NVML::getDeviceHandle(int index) const {
   nvmlDevice_t handle;
   auto nv_status = getNVMLDeviceHandleByIndex(index, &handle);
   return handle;
}

std::string NVML::getDeviceName (const unsigned int index, const nvmlDevice_t &handle) const {
   char value[NVML_DEVICE_NAME_BUFFER_SIZE];
   auto nv_status = getNVMLDeviceName(handle, value, NVML_DEVICE_NAME_BUFFER_SIZE);
   return std::string(value);
}

// This does not compile without an additional const????
unsigned int NVML::getTemperature (const unsigned int index, const nvmlDevice_t &handle) const {
   unsigned int value;
   auto nv_status = getNVMLTemperature(handle, nvmlTemperatureSensors_t::NVML_TEMPERATURE_GPU, &value);
   return value;
}

NVMLDevice::NVMLDevice(unsigned int index, const nvmlDevice_t handle, const NVML &nvmlAPI):
   index{index},
   handle{handle},
   nvmlAPI{nvmlAPI}
{
   name = nvmlAPI.getDeviceName(index, handle);
}

NVMLDevice::~NVMLDevice() {
}

int NVMLDevice::get_metrics() {
   int temperature = nvmlAPI.getTemperature(index, handle);
   return temperature;
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
         nvmlDevice_t handle = nvmlAPI.getDeviceHandle(device_index);         
         NVMLDevice device{device_index, handle, nvmlAPI};
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

void NVMLDeviceManager::displayValues(int index) {
   if (index >= 0) {
      printf ("Device %d: %d C\n", index, temps[index]);
   } else {
      for (int device_index{0}; device_index < device_count; device_index++) {
         printf ("Device %d: %d C\n", device_index, temps[device_index]);
      }
   }
}
