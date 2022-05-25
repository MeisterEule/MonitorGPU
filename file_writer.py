#!/usr/bin/env python
from datetime import datetime

class fileWriter():
  def __init__(self):
    self.handle = None
    self.is_open = False
    self.n_columns_per_gpu = 0

  def start (self, device_names, host_name, keys, filename=""):

    now = datetime.now()
    start_date = now.strftime("%Y_%m_%d_%H_%M_%S")
    if filename == "":
       auto_filename = host_name + "_" + start_date + ".hwout"
       self.handle = open(auto_filename, "w+")
    else:
       self.handle = open(filename, "w+")
    self.is_open = True
    self.handle.write("Watching %d GPUs on %s\n\n" % (len(device_names), host_name))
    for i, name in enumerate(device_names):
      self.handle.write ("%d: %s\n" % (i, name))
    self.handle.write("Start date:      %s\n" % (start_date))
    self.handle.write("Registered keys: ")
    for key in keys:
       self.handle.write("%s " % key)
    self.handle.write("\n\n")
    self.n_columns_per_gpu = len(keys)

  def stop (self, end_date=None):
    self.handle.write("Finished recording\n")
    if end_date != None: self.handle.write("End date:     %s" % (end_date))
    self.handle.close()
    self.is_open = False

  def add_items(self, timestamps, all_y):
    for t, y_line in zip(timestamps, all_y): 
      self.handle.write("%.2f: " % t)
      for i, y in enumerate(y_line):
        if i > 0 and i % self.n_columns_per_gpu == 0: self.handle.write ("|| ")
        self.handle.write("%d " % y)
      self.handle.write("\n")


