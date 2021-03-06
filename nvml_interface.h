#ifndef NVML_INTERFACE_H
#define NVML_INTERFACE_H

#include <string_view>
#include <vector>
  
constexpr auto NVML_LIB_NAME{"libnvidia-ml.so"};

constexpr unsigned int NVML_DEVICE_NAME_BUFFER_SIZE{64};
constexpr unsigned int NVML_DEVICE_SERIAL_BUFFER_SIZE{30};

constexpr unsigned int NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE{80};
constexpr unsigned int NVML_SYSTEM_NVML_VERSION_BUFFER_SIZE{80};

constexpr unsigned int NVML_PERSISTENCE_DISABLED{0};
constexpr unsigned int NVML_PERSISTENCE_ENABLED{1};

typedef void* solib_handle_t;
typedef void* sofunc_handle_t;

typedef long long DEVICE_RETURN_T; 
#define DEVICE_RETURN_INVALID -99999

enum class nvmlReturn_t {
  NVML_SUCCESS                       = 0,
  NVML_ERROR_UNINITIALIZED           = 1,
  NVML_ERROR_INVALID_ARGUMENT        = 2,
  NVML_ERROR_NOT_SUPPORTED           = 3,
  NVML_ERROR_NO_PERMISSION           = 4,
  NVML_ERROR_ALREADY_INITIALIZED     = 5,
  NVML_ERROR_NOT_FOUND               = 6,
  NVML_ERROR_INSUFFICIENT_SIZE       = 7,
  NVML_ERROR_INSUFFICIENT_POWER      = 8,
  NVML_ERROR_DRIVER_NOT_LOADED       = 9,
  NVML_ERROR_TIMEOUT                 = 10,
  NVML_ERROR_IRQ_ISSUE               = 11,
  NVML_ERROR_LIBRARY_NOT_FOUND       = 12,
  NVML_ERROR_FUNCTION_NOT_FOUND      = 13,
  NVML_ERROR_CORRUPTED_INFOROM       = 14,
  NVML_ERROR_GPU_IS_LOST             = 15,
  NVML_ERROR_RESET_REQUIRED          = 16,
  NVML_ERROR_OPERATING_SYSTEM        = 17,
  NVML_ERROR_LIB_RM_VERSION_MISMATCH = 18,
  NVML_ERROR_IN_USE                  = 19,
  NVML_ERROR_MEMORY                  = 20,
  NVML_ERROR_NO_DATA                 = 21,
  NVML_ERROR_VGPU_ECC_NOT_SUPPORTED  = 22,
  NVML_ERROR_UNKNOWN                 = 999,
};

constexpr unsigned int NVML_TEMPERATURE_GPU{0};
constexpr unsigned int NVML_CLOCK_TYPE_GRAPHICS{0};
constexpr unsigned int NVML_CLOCK_ID_CURRENT{0};
constexpr unsigned int NVML_PCIE_UTIL_TX_BYTES{0};

typedef struct nvmlDevice_st* nvmlDevice_t;

typedef struct {
  unsigned int gpu;
  unsigned int memory;
} nvml_utilization_t; 

typedef struct {
  unsigned long long free;
  unsigned long long total;
  unsigned long long used;
} nvml_memory_t;

typedef struct {
  unsigned int computeInstanceId;
  unsigned int gpuInstanceId;
  unsigned int pid;
  unsigned long long usedGpuMemory;
} nvml_proc_info_t;

typedef nvmlReturn_t (*nvmlInit_t)(void);
typedef nvmlReturn_t (*nvmlSystemGetDriverVersion_t)(char *version, unsigned int length);
typedef nvmlReturn_t (*nvmlSystemGetNVMLVersion_t)(char *version, unsigned int length);
typedef nvmlReturn_t (*nvmlSystemGetProcessName_t) (unsigned int pid, char *name, unsigned int length);
typedef nvmlReturn_t (*nvmlDeviceGetCount_t)(unsigned int *deviceCount);
typedef nvmlReturn_t (*nvmlDeviceGetHandleByIndex_t)(unsigned int index, nvmlDevice_t* device);
typedef nvmlReturn_t (*nvmlDeviceGetName_t)(nvmlDevice_t device, char *name, unsigned int length);  
typedef nvmlReturn_t (*nvmlDeviceGetTemperature_t)(nvmlDevice_t device, unsigned int sensorType, unsigned int* temp);
typedef nvmlReturn_t (*nvmlDeviceGetClock_t)(nvmlDevice_t device,  unsigned int clockType, unsigned int clockId, unsigned int *clockMHz);
typedef nvmlReturn_t (*nvmlDeviceGetNumCores_t)(nvmlDevice_t device, unsigned int *numCores);
typedef nvmlReturn_t (*nvmlDeviceGetPcieThroughput_t)(nvmlDevice_t device, unsigned int counter, unsigned int *rate);
typedef nvmlReturn_t (*nvmlDeviceGetPowerUsage_t)(nvmlDevice_t device, unsigned int *power);
typedef nvmlReturn_t (*nvmlDeviceGetUtilizationRates_t)(nvmlDevice_t device, nvml_utilization_t *utilization);
typedef nvmlReturn_t (*nvmlDeviceGetMemoryInfo_t)(nvmlDevice_t device, nvml_memory_t *memory);
typedef nvmlReturn_t (*nvmlDeviceGetProcInfo_t)(nvmlDevice_t device, unsigned int *info_count, nvml_proc_info_t *infos);
typedef nvmlReturn_t (*nvmlDeviceGetPersistenceMode_t)(nvmlDevice_t device, unsigned int *mode);

class NVML {
   public:
      NVML(std::string_view lib_name = NVML_LIB_NAME);
      ~NVML();
      
      unsigned int getTemperature(const unsigned int index, const nvmlDevice_t &device_handle) const;
      unsigned int getFrequency(const unsigned int index, const nvmlDevice_t &device_handle) const;
      unsigned int getDeviceCount() const;
      DEVICE_RETURN_T getNumCores(const unsigned int index, const nvmlDevice_t &device_handle) const;
      unsigned int getPcieRate(const unsigned int index, const nvmlDevice_t &device_handle) const;
      unsigned int getPowerUsage(const unsigned int index, const nvmlDevice_t &device_handle) const;
      nvmlDevice_t getDeviceHandle(int index) const;
      std::string getDeviceName(int index, const nvmlDevice_t &device_handle) const;
      void getUtilization(const unsigned int index, const nvmlDevice_t &device_handle,
                          unsigned int *gpu, unsigned int *memory) const;
      void getMemoryInfo(const unsigned int index, const nvmlDevice_t &device_handle,
                         unsigned long long *free, unsigned long long *total, unsigned long long *used) const;
      void getProcessInfo (const unsigned int index, const nvmlDevice_t &device_handle, unsigned int *n_procs, unsigned int *max_running_processes, int **proc_infos) const;
      void getProcessName (unsigned int pid, char *name);
      void getPersistenceMode (const unsigned int index, const nvmlDevice_t &device_handle, unsigned int *mode) const;
   private:
      solib_handle_t nvml_solib;
      
      nvmlInit_t nvmlInit{NULL};
      nvmlSystemGetDriverVersion_t getDriverVersion{NULL};
      nvmlSystemGetNVMLVersion_t getNVMLVersion{NULL};
      nvmlSystemGetProcessName_t getNVMLProcName{NULL};
      nvmlDeviceGetCount_t getNVMLDeviceCount{NULL};
      nvmlDeviceGetName_t getNVMLDeviceName{NULL};
      nvmlDeviceGetTemperature_t getNVMLTemperature{NULL}; 
      nvmlDeviceGetClock_t getNVMLFrequency{NULL};
      nvmlDeviceGetHandleByIndex_t getNVMLDeviceHandleByIndex{NULL};
      nvmlDeviceGetNumCores_t getNVMLDeviceNumCores{NULL};
      nvmlDeviceGetPcieThroughput_t getNVMLDevicePcieThroughput{NULL};
      nvmlDeviceGetPowerUsage_t getNVMLDevicePowerUsage{NULL};
      nvmlDeviceGetUtilizationRates_t getNVMLDeviceUtilization{NULL};
      nvmlDeviceGetMemoryInfo_t getNVMLMemoryInfo{NULL};
      nvmlDeviceGetProcInfo_t getNVMLProcInfo{NULL};
      nvmlDeviceGetPersistenceMode_t getNVMLPersistenceMode{NULL};
      void bind_functions();
      
};

typedef struct nvmlDevice_st* nvmlDevice_t;

class NVMLDeviceManager {
   public:
      NVMLDeviceManager(const NVML &nvmlAPI);
      ~NVMLDeviceManager();
      int num_devices;
      int getTemperature(int index = 0);
      int getFrequency(int index = 0);
      std::string getName(int index = 0);
      DEVICE_RETURN_T getNumCores(int index = 0);
      int getPcieRate(int index = 0);
      int getPowerUsage(int index = 0);
      void getUtilization(int index, unsigned int *gpu, unsigned int *memory);
      void getMemoryInfo(int index, unsigned long long *free,
                         unsigned long long *total, unsigned long long *used);
      void getProcessInfo(int index, unsigned int *n_procs, unsigned int *max_running_processes, int **proc_ids);
      void getPersistenceMode(int index, unsigned int *mode);
   private: 
      const NVML &nvmlAPI;
      std::vector<nvmlDevice_t> device_handles;
};

#endif
