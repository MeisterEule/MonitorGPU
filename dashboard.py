#!/usr/bin/env python

import dash
from dash import html
from dash import dcc
from dash.dependencies import Input, Output, State
import plotly
import nvml

from collections import deque
import multiprocessing

DGEMM_RESULT_BUFFER_SIZE = 1024
DGEMM_STATE_IDLE = 0
DGEMM_STATE_BUSY = 1
DGEMM_STATE_DONE = 2

device = nvml.deviceInfo()
app = dash.Dash()

n_max_data = 50
t_update = 1000
timestamps = deque([], n_max_data)
temperature = deque([], n_max_data)
frequency = deque([], n_max_data)

dgemm_matrix_size = 1000
dgemm_n_repeat = 30
device.readOut()
memory = device.getMemoryInfo()
free_gpu_mem = memory["Free"] / 1024 / 1024 / 1024
max_dgemm_size = nvml.dgemmMaxMatrixSize(memory["Free"])
max_stream_size = nvml.streamMaxVectorSize(memory["Free"])

dgemm_result = multiprocessing.Array('u', 1024)
dgemm_busy_flag = multiprocessing.Value('i', 0)

def dgemmTab ():
  return dcc.Tab(label='Dgemm', children=[
         html.H1('This is a new tab!'),
         html.P(children="Available GPU Memory: " + str(free_gpu_mem) + " GiB"),
         html.P(children="Maximal DGEMM matrix size: " + str(max_dgemm_size)),
         html.Div(["Matrix size: ",
                   dcc.Input(id='input-dgemm-matrix-size', value=1000, type='number')
         ]),
         html.Div(["N Repeats: ",
                   dcc.Input(id='input-dgemm-nrepeat', value=10, type='number')
         ]),
         html.Button('DGEMM', id='start-dgemm', n_clicks=0),
         html.P(id='button-out', children='This is the button output'),
         html.Div(children= [
            html.P(id='live-update-dgemm', children='Nothing happened'),
            dcc.Interval(id='dgemm-interval-component', interval = 1000, n_intervals = 0)
         ]) 
    ])


def multiprocDgemm (matrix_size, n_repeats, result_string, busy_flag):
  busy_flag.value = DGEMM_STATE_BUSY
  gflops = nvml.performDgemm(matrix_size, n_repeats)
  status = gflops["Status"]
  if status == "OK":
     s = "DGEMM result for N = %d" % matrix_size + "\n" \
         "  Avg: %6.1f GF/s" % gflops["Avg"] + "\n" \
         "  Min: %6.1f GF/s" % gflops["Min"] + "\n" \
         "  Max: %6.1f GF/s" % gflops["Max"] + "\n" \
         "  Stddev: %6.1f GF/s" % gflops["Stddev"]
  else:
     s = "DGEMM failed: Error %s" % status

  for i in range(len(s)):
     result_string[i] = s[i]

  busy_flag.value = DGEMM_STATE_DONE


def streamTab ():
  return dcc.Tab(label='Stream', children=[
         html.H1('Start a stream run'),
         html.P(children="Available GPU Memory: " + str(free_gpu_mem) + " GiB"),
         html.P(children="Maximal Stream vector size: " + str(max_stream_size)),
         #html.Div(["Matrix size: ",
         #          dcc.Input(id='input-dgemm-matrix-size', value=1000, type='number')
         #]),
         #html.Div(["N Repeats: ",
         #          dcc.Input(id='input-dgemm-nrepeat', value=10, type='number')
         #]),
         #html.Button('DGEMM', id='start-dgemm', n_clicks=0),
         #html.P(id='button-out', children='This is the button output'),
         #html.Div(children= [
         #   html.P(id='live-update-dgemm', children='Nothing happened'),
         #   dcc.Interval(id='dgemm-interval-component', interval = 1000, n_intervals = 0)
         #]) 
    ])


app.layout = html.Div(
   dcc.Tabs([
      dcc.Tab(label='History', children=[
         dcc.Graph(id='live-update-graph'),
         dcc.Interval(id='interval-component',
                      interval = t_update,
                      n_intervals = 0
         )
      ]),
      dgemmTab(),
      streamTab()
   ]),
)

@app.callback(
   Output('button-out', 'children'),
   Input('start-dgemm', 'n_clicks'),
   State('input-dgemm-matrix-size', 'value'),
   State('input-dgemm-nrepeat', 'value')
)
def do_dgemm_button_click (n_clicks, matrix_size, n_repeats):
  if n_clicks > 0:
     dgemm_proc = multiprocessing.Process(target=multiprocDgemm, args=(matrix_size, n_repeats, dgemm_result, dgemm_busy_flag))
     dgemm_proc.start()
     dgemm_proc.join()
  #return 'The input was "{}" and "{}"'.format(matrix_size, n_repeats)

@app.callback(
   Output('live-update-dgemm', 'children'),
   Input('live-update-dgemm', 'children'),
   Input('dgemm-interval-component', 'n_intervals'))
def update_dgemm_result(original_name, n_intervals):
  if dgemm_busy_flag.value == DGEMM_STATE_IDLE:
     ret = "Waiting"
  elif dgemm_busy_flag.value == DGEMM_STATE_BUSY:
     ret = "Busy"
  elif dgemm_busy_flag.value == DGEMM_STATE_DONE: 
     ret = []
     tmp = ""
     for s in dgemm_result:
       if s == '\0':
         break
       elif s == '\n':
         ret.append (tmp)
         ret.append (html.Br())
         tmp = ""
       else:
         tmp += s
     ret.append(tmp)
  return ret
  

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
