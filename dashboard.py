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

keys = ["GPU-Util", "Memory-Util", "Temperature", "Frequency"]
labels = ["GPU-Util [%]", "Memory-Util [%]", "T [C]", "f [MHz]"]
init_keys = ["GPU-Util", "Memory-Util"]
hwPlots = live_plots.hardwarePlotCollection(device, keys, labels, init_keys)
live_plots.register_callbacks(app, hwPlots)

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
