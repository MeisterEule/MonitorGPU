#!/usr/bin/env python

import nvml

class deviceProperties():
  def __init__(self, nvml_device):
    nvml_device.readOut()
    self.name = nvml_device.getDeviceName()
    memory = nvml_device.getMemoryInfo()
    self.total_mem = memory["Free"]
    self.total_mem_gib = self.total_mem / 1024 / 1024 / 1024

