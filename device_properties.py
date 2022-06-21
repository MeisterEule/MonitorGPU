#!/usr/bin/env python

import nvml

class deviceProperties():
  def __init__(self, nvml_device, num_gpus):
    nvml_device.readOut()
    self.names = [nvml_device.getDeviceName(gpu_id) for gpu_id in range(num_gpus)]
    memories = [nvml_device.getMemoryInfo(gpu_id) for gpu_id in range(num_gpus)]
    self.total_mem = [memory["Free"] for memory in memories]
    self.total_mem_gib = [total_mem / 1024 / 1024 / 1024 for total_mem in self.total_mem]
    self.processes = [nvml_device.getProcessInfo(gpu_id) for gpu_id in range(num_gpus)]
    self.persistence_enabled = [nvml_device.getPersistenceMode(gpu_id) for gpu_id in range(num_gpus)]


  def update_process(self, nvml_device, gpu_id):
    self.processes[gpu_id] = nvml_device.getProcessInfo(gpu_id)

  def procString(self, nvml_device, gpu_id):
    gpu_name = self.names[gpu_id]
    processes = self.processes[gpu_id]
    n = len(processes)
    s = "%s: %d" % (gpu_name, n)
    if n > 0:
      s += "["
      for i, proc in enumerate(processes):
        s += str(proc) + "(" + nvml_device.getProcessName(proc) + ")"
        if i < n - 1: 
          s += ", "
      s += "]"
    return s

