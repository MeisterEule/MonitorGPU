#!/usr/bin/env python

import dash
from dash import html
from dash import dcc
from dash.dependencies import Input, Output
import nvml

from collections import deque

device = nvml.deviceInfo()
app = dash.Dash()

n_max_data = 50
t_update = 1000
timestamps = deque([], n_max_data)
temperature = deque([], n_max_data)


app.layout = html.Div(
   html.Div([
      dcc.Graph(id='live-update-graph'),
      dcc.Interval(id='interval-component',
                   interval = t_update,
                   n_intervals = 0
      )
   ])
)

@app.callback(Output('live-update-graph', 'figure'),
               Input('interval-component', 'n_intervals'))
def update_graph_live(n):
  t = t_update * n / 1000
  device.readOut()
  items = device.getItems()
  timestamps.appendleft(t)
  temperature.appendleft(items['Temperature']) 
  x = list(timestamps)
  x.reverse()
  y = list(temperature)
  y.reverse()
  return dict(data=[dict(x=x, y=y)])

if __name__ == '__main__':
   app.run_server()
