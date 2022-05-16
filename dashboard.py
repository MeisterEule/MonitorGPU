#!/usr/bin/env python

import dash
from dash import html
from dash import dcc
import nvml

import device_properties
import live_plots
import dgemm_tab
import stream_tab

device = nvml.deviceInfo()
deviceProps = device_properties.deviceProperties(device)
app = dash.Dash()

# Pay attention that the order of the keys corresponds to the one
# in nvml.getItems. (get keys from C++?)
device_keys = ["Temperature", "Frequency", "PCIE", "Power", "GPU-Util", "Memory-Util"]
device_labels = ["T [C]", "f [MHz]", "", "P [mW]", "GPU-Util[%]", "Memory-Util[%]"]
host_keys = ["CPU"]
host_labels = ["CPU [%]"]
init_keys = ["GPU-Util", "Memory-Util"]
hwPlots = live_plots.hardwarePlotCollection(device, device_keys, host_keys,
                                            device_labels, host_labels, init_keys)
live_plots.register_callbacks(app, hwPlots, deviceProps)

dgemm_tab.register_callbacks(app)
stream_tab.register_callbacks(app)

app.layout = html.Div(
   dcc.Tabs([
      live_plots.Tab(deviceProps, hwPlots),
      dgemm_tab.Tab(deviceProps),
      stream_tab.Tab(deviceProps)
   ]),
)

if __name__ == '__main__':
   app.run_server()
