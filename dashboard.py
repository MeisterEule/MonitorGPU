#!/usr/bin/env python

import dash
from dash import html
from dash import dcc
from dash.dependencies import Input, Output, State
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

dgemm_matrix_size = 1000
dgemm_n_repeat = 30


app.layout = html.Div(
   dcc.Tabs([
      dcc.Tab(label='History', children=[
         dcc.Graph(id='live-update-graph'),
         dcc.Interval(id='interval-component',
                      interval = t_update,
                      n_intervals = 0
         )
      ]),
      dcc.Tab(label='Dgemm', children=[
         html.H1('This is a new tab!'),
         #html.Div(id='dummy1', style={'display':'none'}),
         #html.Div(id='dummy2', style={'display':'none'}),
         html.Div(["Matrix size: ",
                   dcc.Input(id='input-dgemm-matrix-size', value=1000, type='number')
         ]),
         html.Div(["N Repeats: ",
                   dcc.Input(id='input-dgemm-nrepeat', value=10, type='number')
         ]),
         html.Button('DGEMM', id='start-dgemm', n_clicks=0),
         html.P(id='button-out', children='This is the button output')
      ])
   ]),
)

@app.callback(
   Output('button-out', 'children'),
   Input('start-dgemm', 'n_clicks'),
   State('input-dgemm-matrix-size', 'value'),
   State('input-dgemm-nrepeat', 'value')
)
def do_button_click (n_clicks, matrix_size, n_repeats):
  gflops = nvml.performDgemm(matrix_size, n_repeats)
  #s = 'DGEMM results: \n\t Avg: "{}" GF/s\n\t Stddev: "{}" GF/s\n\t Min: "{}" GF/s\n\t Max: "{}" GF/s'.format(
  #s = 'DGEMM results: \n\t Avg: %6.1f GF/s\n\t Stddev: %6.1f GF/s\n\t Min: %6.1f GF/s\n\t Max: %6.1f GF/s'%
  #   (gflops["Avg"], gflops["Stddev"], gflops["Min"], gflops["Max"])
  #s = "Avg: %6.1f GF/s <br> Min: %6.1f GF/s" % (gflops["Avg"], gflops["Min"])
  s = ["Avg: %6.1f GF/s" % gflops["Avg"], html.Br(),
       "Min: %6.1f GF/s" % gflops["Min"], html.Br(),
       "Max: %6.1f GF/s" % gflops["Max"], html.Br(),
       "Stddev: %6.1f GF/s" % gflops["Stddev"]]
  return s
  #return 'The input was "{}" and "{}"'.format(matrix_size, n_repeats)

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
