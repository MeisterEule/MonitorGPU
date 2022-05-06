#!/usr/bin/env python

import nvml
import time

device = nvml.deviceInfo()
time_start = time.time()
elapsed_time = 0
sleep_time = 0.5

while (True):
   device.readOut()
   elapsed_time = time.time() - time_start
   print ("Elapsed: ", elapsed_time)
   items = device.getItems()
   #print ("Temp: ", items['Temperature'])
   #print ("Freq: ", items['Frequency'])
   #print ("PCIE: ", items['PCIE'])
   #print ("Power: ", items['Power'] / 1000)
   utilization = device.getUtilization()
   memory = device.getMemoryInfo()
   print ("Memory: %d %d %d\n" % (memory["Free"], memory["Total"], memory["Used"]))
   #print ("GPU: %d %%, MEM: %d %%" % (utilization['GPU'], utilization['Memory']))
   #print ("Temp: ", device.getTemp());
   time.sleep(sleep_time)
