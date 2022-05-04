#!/usr/bin/env python

import nvml
import time

device = nvml.deviceInfo()

while (True):
   device.readOut()
   items = device.getItems()
   print ("Temp: ", items['Temperature'])
   print ("Freq: ", items['Frequency'])
   print ("PCIE: ", items['PCIE'])
   print ("Power: ", items['Power'] / 1000)
   utilization = device.getUtilization()
   print ("GPU: %d %%, MEM: %d %%" % (utilization['GPU'], utilization['Memory']))
   #print ("Temp: ", device.getTemp());
   time.sleep(1)
