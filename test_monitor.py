#!/usr/bin/env python

import nvml
import time

device = nvml.deviceInfo()

while (True):
   device.readOut()
   items = device.getItems()
   print ("Temp: ", items['Temperature'])
   #print ("Temp: ", device.getTemp());
   time.sleep(1)
