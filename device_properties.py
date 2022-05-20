#!/usr/bin/env python

import nvml

class deviceProperties():
  def __init__(self, nvml_device):
    nvml_device.readOut()
    print ("Readout done")
    self.name = nvml_device.getDeviceName(0)
    print ("Name: ", self.name)
    memory = nvml_device.getMemoryInfo(0)
    print ("Memory done")
    #self.total_mem = memory["Free"]
    #self.total_mem_gib = self.total_mem / 1024 / 1024 / 1024
    #self.processes = nvml_device.getProcessInfo(0)

  def procString(self):
    n = len(self.processes)
    s = "Active processes: " + str(n)
    if n > 0:
      s += "["
      for i, proc in enumerate(self.processes):
        s += str(proc)
        if i < n - 1: 
          s += ", "
      s += "]"
    return s

