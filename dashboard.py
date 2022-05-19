#!/usr/bin/env python
import argparse

import dash
from dash import html
from dash import dcc
import nvml

import device_properties
import live_plots
import dgemm_tab
import stream_tab


# Pay attention that the order of the keys corresponds to the one
# in nvml.getItems. (get keys from C++?)
keys = ["Temperature", "Frequency", "PCIE", "Power", "GPU-Util", "Memory-Util", "CPU"]
labels = ["T [C]", "f [MHz]", "", "P [mW]", "GPU-Util[%]", "Memory-Util[%]", "CPU [%]"]
init_keys = ["GPU-Util", "Memory-Util"]

if __name__ == '__main__':
   parser = argparse.ArgumentParser(description="Launch the GPU dashboard.")
   parser.add_argument("--buffer-size", dest="buffer_size", type=int, default=50,
                       help="Nr. of data points to be stored and displayed.")
   parser.add_argument("--update-time", dest="t_update", type=float, default=1.0,
                       help="Time interval in seconds in which the live-view windows update themselves.") 
   parser.add_argument("--record-time", dest="t_record", type=float, default=1.0,
                       help="Time interval in seconds in which the hardware recorder takes data.")
   args = parser.parse_args()

   device = nvml.deviceInfo()
   deviceProps = device_properties.deviceProperties(device)
   app = dash.Dash()

   hwPlots = live_plots.hardwarePlotCollection(device, keys, labels, init_keys)
   live_plots.register_callbacks(app, hwPlots, deviceProps)
   
   dgemm_tab.register_callbacks(app)
   stream_tab.register_callbacks(app)
   
   app.layout = html.Div(
      dcc.Tabs([
         live_plots.Tab(deviceProps, hwPlots, args.buffer_size, args.t_update, args.t_record),
         dgemm_tab.Tab(deviceProps),
         stream_tab.Tab(deviceProps)
      ]),
   )
   app.run_server()
