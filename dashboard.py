#!/usr/bin/env python

import dash
from dash import html
from dash import dcc
from dash.dependencies import Input, Output, State
import plotly
import nvml

from collections import deque
import multiprocessing

import device_properties
import dgemm_tab
import stream_tab

device = nvml.deviceInfo()
deviceProps = device_properties.deviceProperties(device)
app = dash.Dash()

n_max_data = 50
t_update = 1000
timestamps = deque([], n_max_data)
temperature = deque([], n_max_data)
frequency = deque([], n_max_data)

y_low = {"Temperature": 1000, "Frequency": 1000, "PCIE": 1000, "Power": 1000}
y_max = {"Temperature": 0, "Frequency": 0, "PCIE": 0, "Power": 0}


dgemm_tab.register_callbacks(app)
stream_tab.register_callbacks(app)


app.layout = html.Div(
   dcc.Tabs([
      dcc.Tab(label='History', children=[
         dcc.Graph(id='live-update-graph'),
         dcc.Interval(id='interval-component',
                      interval = t_update,
                      n_intervals = 0
         )
      ]),
      dgemm_tab.Tab(deviceProps),
      stream_tab.Tab(deviceProps)
   ]),
)


def gen_plots (x, ys, labels):
  fig = plotly.tools.make_subplots(rows=len(labels), cols=1, vertical_spacing=0.2)
  for i in range(len(ys)):
     fig.append_trace({
        'x': x,
        'y': ys[i],
        'name': labels[i],
     }, i + 1, 1)
  fig.update_yaxes(range=[y_low['Temperature'], y_max['Temperature']], row=1, col=1)
  fig.update_yaxes(range=[y_low['Frequency'], y_max['Frequency']], row=2, col=1)
  return fig

def rescale_axes (values, scale_min=0.8, scale_max=1.2):
  for key, value in values.items():
    if value * scale_min < y_low[key]: y_low[key] = value * scale_min
    if value * scale_max > y_max[key]: y_max[key] = value * scale_max
  
@app.callback(Output('live-update-graph', 'figure'),
               Input('interval-component', 'n_intervals'))
def update_graph_live(n):
  t = t_update * n / 1000
  device.readOut()
  items = device.getItems()
  timestamps.appendleft(t)
  temperature.appendleft(items['Temperature']) 
  frequency.appendleft(items['Frequency'])
  rescale_axes (items)
  x = list(timestamps)
  x.reverse()
  all_y = []
  y = list(temperature)
  y.reverse()
  all_y.append(y)
  y = list(frequency)
  y.reverse()
  all_y.append(y)
  labels = ['Temperature', 'Frequency']
  return gen_plots (x, all_y, labels)

if __name__ == '__main__':
   app.run_server()
