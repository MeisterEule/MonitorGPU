#!/usr/bin/env python

import dash
from dash import html
from dash import dcc
from dash.dependencies import Input, Output
import plotly
import nvml

from collections import deque

device = nvml.deviceInfo()
app = dash.Dash()

n_max_data = 50
t_update = 1000
timestamps = deque([], n_max_data)
temperature = deque([], n_max_data)
frequency = deque([], n_max_data)


app.layout = html.Div(
   html.Div([
      dcc.Graph(id='live-update-graph'),
      dcc.Interval(id='interval-component',
                   interval = t_update,
                   n_intervals = 0
      )
   ])
)

def gen_plots (x, ys, labels):
  fig = plotly.tools.make_subplots(rows=len(labels), cols=1, vertical_spacing=0.2)
  for i in range(len(ys)):
     fig.append_trace({
        'x': x,
        'y': ys[i],
        'name': labels[i],
        #'yaxis': dict(range=[35, 66])
     }, i + 1, 1)
  return fig

@app.callback(Output('live-update-graph', 'figure'),
               Input('interval-component', 'n_intervals'))
def update_graph_live(n):
  t = t_update * n / 1000
  device.readOut()
  items = device.getItems()
  timestamps.appendleft(t)
  temperature.appendleft(items['Temperature']) 
  frequency.appendleft(items['Frequency'])
  labels = ['Temperature', 'Frequency']
  x = list(timestamps)
  x.reverse()
  all_y = []
  y = list(temperature)
  y.reverse()
  all_y.append(y)
  y = list(frequency)
  y.reverse()
  all_y.append(y)
  #return dict(data=[dict(x=x, y=y)])
  return gen_plots (x, all_y, labels)

if __name__ == '__main__':
   app.run_server()
