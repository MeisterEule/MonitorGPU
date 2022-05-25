#!/usr/bin/env python

from platform import node
import re

class hostReader():
  def __init__(self):
    self.handle = open("/proc/stat")
    self.prev_jiffies = [0 for i in range(10)]
    self.current_cpu_usage = 0
    self.host_name = node()
    
  def __del__(self):
    self.handle.close()
    self.handle = None

  def get_cpu_usage(self):
    for line in self.handle.readlines():
      if re.search("cpu0 ", line):
        jiffies = [int(l) for l in line.split()[1:]]
        if any ([j_new > j_old for j_new, j_old in zip(jiffies[0:3], self.prev_jiffies[0:3])]):
          total = sum(jiffies[0:-1])
          work = sum(jiffies[0:3])
          prev_total = sum(self.prev_jiffies[0:-1])
          prev_work = sum(self.prev_jiffies[0:3])
          self.current_cpu_usage = (work - prev_work) / (total - prev_total) * 100
          self.prev_jiffies = jiffies.copy()

    self.handle.seek(0) 
    return int(round(self.current_cpu_usage,0))

  def read_out(self):
    return {'CPU': self.get_cpu_usage()}

