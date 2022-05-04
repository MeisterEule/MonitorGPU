#ifndef NVML_INTERFACE_H
#define NVML_INTERFACE_H

#include <string_view>
#include <vector>
  
constexpr auto NVML_LIB_NAME{"libnvidia-ml.so"};

constexpr unsigned int NVML_DEVICE_NAME_BUFFER_SIZE{64};
constexpr unsigned int NVML_DEVICE_SERIAL_BUFFER_SIZE{30};

constexpr unsigned int NVML_SYSTEM_DRIVER_VERSION_BUFFER_SIZE{80};
constexpr unsigned int NVML_SYSTEM_NVML_VERSION_BUFFER_SIZE{80};

typedef void* solib_handle_t;
typedef void* sofunc_handle_t;

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

typedef nvmlReturn_t (*nvmlInit_t)(void);
typedef nvmlReturn_t (*nvmlSystemGetDriverVersion_t)(char *version, unsigned int length);
typedef nvmlReturn_t (*nvmlSystemGetNVMLVersion_t)(char *version, unsigned int length);
typedef nvmlReturn_t (*nvmlDeviceGetCount_t)(unsigned int *deviceCount);
typedef nvmlReturn_t (*nvmlDeviceGetHandleByIndex_t)(unsigned int index, nvmlDevice_t* device);
typedef nvmlReturn_t (*nvmlDeviceGetName_t)(nvmlDevice_t device, char *name, unsigned int length);  
typedef nvmlReturn_t (*nvmlDeviceGetTemperature_t)(nvmlDevice_t device, unsigned int sensorType, unsigned int* temp);
typedef nvmlReturn_t (*nvmlDeviceGetClock_t)(nvmlDevice_t device,  unsigned int clockType, unsigned int clockId, unsigned int *clockMHz);
typedef nvmlReturn_t (*nvmlDeviceGetNumCores_t)(nvmlDevice_t device, unsigned int *numCores);
typedef nvmlReturn_t (*nvmlDeviceGetPcieThroughput_t)(nvmlDevice_t device, unsigned int counter, unsigned int *rate);

class NVML {
   public:
      NVML(std::string_view lib_name = NVML_LIB_NAME);
      ~NVML();
      
      unsigned int getTemperature(const unsigned int index, const nvmlDevice_t &handle) const;
      unsigned int getFrequency(const unsigned int index, const nvmlDevice_t &handle) const;
      unsigned int getDeviceCount() const;
      unsigned int getNumCores(const unsigned int index, const nvmlDevice_t &handle) const;
      unsigned int getPcieRate(const unsigned int index, const nvmlDevice_t &handle) const;
      nvmlDevice_t getDeviceHandle(int index) const;
      std::string getDeviceName(const unsigned int index, const nvmlDevice_t &handle) const;
   private:
      solib_handle_t nvml_solib;
      
      nvmlInit_t nvmlInit{NULL};
      nvmlSystemGetDriverVersion_t getDriverVersion{NULL};
      nvmlSystemGetNVMLVersion_t getNVMLVersion{NULL};
      nvmlDeviceGetCount_t getNVMLDeviceCount{NULL};
      nvmlDeviceGetName_t getNVMLDeviceName{NULL};
      nvmlDeviceGetTemperature_t getNVMLTemperature{NULL}; 
      nvmlDeviceGetClock_t getNVMLFrequency{NULL};
      nvmlDeviceGetHandleByIndex_t getNVMLDeviceHandleByIndex{NULL};
      nvmlDeviceGetNumCores_t getNVMLDeviceNumCores{NULL};
      nvmlDeviceGetPcieThroughput_t getNVMLDevicePcieThroughput{NULL};
      void bind_functions();
      
};

typedef struct nvmlDevice_st* nvmlDevice_t;

class NVMLDevice {
   public:
      std::string_view name;
      const unsigned int index;

      NVMLDevice (unsigned int index, const nvmlDevice_t handle, const NVML &api);
      ~NVMLDevice();
      int  get_metrics();
      int getFrequency();
      std::string getName();
      int getNumCores();
      int getPcieRate();
   private:
      const nvmlDevice_t handle;
      const NVML &nvmlAPI;
};

class NVMLDeviceManager {
   public:
      NVMLDeviceManager(const NVML &nvmlAPI);
      ~NVMLDeviceManager();
      std::vector<NVMLDevice> devices;
      int *temps;
      void readOutValues();
      void displayValues(int index = -1);
      int getTemp(int index = 0);
      int getFrequency(int index = 0);
      std::string getName(int index = 0);
      int getNumCores(int index = 0);
      int getPcieRate(int index = 0);
   private: 
      const NVML &nvmlAPI;
      int device_count;
};

#endif
