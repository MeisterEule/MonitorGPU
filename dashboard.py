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

labels = ["Temperature", "Frequency"]
hwPlots = live_plots.hardwarePlotCollection(device, labels)
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
