#!/usr/bin/env python

from datetime import datetime

class fileWriter():
  def __init__(self):
    self.handle = None
    self.is_open = False

  def start (self, device_name, host_name, keys, filename=""):

    now = datetime.now()
    start_date = now.strftime("%Y_%m_%d_%H_%M_%S")
    if filename == "":
       auto_filename = device_name + "_" + start_date + ".hwout"
       self.handle = open(auto_filename, "w+")
    else:
       self.handle = open(filename, "w+")
    self.is_open = True
    self.handle.write("Watching %s on %s\n\n" % (device_name, host_name))
    self.handle.write("Start date:      %s" % (start_date))
    for key in keys:
       self.handle.write("%s " % key)
    self.handle.write("\n")

  def stop (self, end_date=None):
    self.handle.write("Finished recording\n")
    if end_date != None: self.handle.write("End date:     %s" % (end_date))
    self.handle.close()
    self.is_open = False

  def add_items(self, timestamps, real_times, all_y):
    for t, rt, y_line in zip(timestamps, real_times, all_y): 
      self.handle.write("%s, %d: " % (rt, t))
      for y in y_line:
        self.handle.write("%f " % y)
      self.handle.write("\n")


