#!/usr/bin/env python

import dash
from dash import html
from dash import dcc
from dash.dependencies import Input, Output, State
import plotly
import nvml

from collections import deque
import multiprocessing

import dgemm_tab

STREAM_STATE_IDLE = 0
STREAM_STATE_BUSY = 1
STREAM_STATE_DONE = 2

device = nvml.deviceInfo()
app = dash.Dash()

n_max_data = 50
t_update = 1000
timestamps = deque([], n_max_data)
temperature = deque([], n_max_data)
frequency = deque([], n_max_data)

y_low = {"Temperature": 1000, "Frequency": 1000, "PCIE": 1000, "Power": 1000}
y_max = {"Temperature": 0, "Frequency": 0, "PCIE": 0, "Power": 0}

device.readOut()
memory = device.getMemoryInfo()
free_gpu_mem = memory["Free"] / 1024 / 1024 / 1024
max_dgemm_size = nvml.dgemmMaxMatrixSize(memory["Free"])
max_stream_size = nvml.streamMaxVectorSize(memory["Free"])

stream_result = multiprocessing.Array('u', 1024)
stream_busy_flag = multiprocessing.Value('i', 0)

dgemm_tab.register_dgemm_callbacks(app)

def streamTab ():
  return dcc.Tab(label='Stream', children=[
         html.H1('Start a stream run'),
         html.P(children="Available GPU Memory: " + str(free_gpu_mem) + " GiB"),
         html.P(children="Maximal Stream vector size: " + str(max_stream_size)),
         html.Div(["Vector size: ",
                   dcc.Input(id='input-stream-matrix-size', value=10000, type='number')
         ]),
         html.Div(["N Repeats: ",
                   dcc.Input(id='input-stream-nrepeat', value=10, type='number')
         ]),
         html.Button('STREAM', id='start-stream', n_clicks=0),
         html.P(id='stream-button-out', children='This is the button output'),
         html.Div(children= [
            html.P(id='live-update-stream', children='Nothing happened'),
            dcc.Interval(id='stream-interval-component', interval = 1000, n_intervals = 0)
         ]) 
    ])

def multiprocStream (vector_size, n_repeats, result_string, busy_flag):
  busy_flag.value = STREAM_STATE_BUSY
  bw = nvml.performStream(vector_size, n_repeats)
  status = bw["Status"]
  if status == "OK":
    s = "STREAM result for N = %d" % vector_size + "\n" \
        "   Copy: %6.1f GiB/s " % bw["Copy"] + "\n" \
        "  Scale: %6.1f GiB/s " % bw["Scale"] + "\n" \
        "    Add: %6.1f GiB/s " % bw["Add"] + "\n" \
        "  Triad: %6.1f GiB/s " % bw["Triad"] 
  else:
    s = "STREAM failed: Error %s" % status
  
  for i in range(len(s)):
     result_string[i] = s[i]

  busy_flag.value = STREAM_STATE_DONE

@app.callback(
   Output('stream-button-out', 'children'),
   Input('start-stream', 'n_clicks'),
   State('input-stream-matrix-size', 'value'),
   State('input-stream-nrepeat', 'value'))
def do_stream_button_click (n_clicks, vector_size, n_repeats):
  print ("THE BUTTON HAS BEEN CLICKED: ", n_clicks)
  if n_clicks > 0:
     stream_proc = multiprocessing.Process(target=multiprocStream, args=(vector_size, n_repeats, stream_result, stream_busy_flag))
     stream_proc.start()
     stream_proc.join()

@app.callback(
   Output('live-update-stream', 'children'),
   Input('live-update-stream', 'children'),
   Input('stream-interval-component', 'n_intervals'))
def update_stream_result(original_name, n_intervals):
  if stream_busy_flag.value == STREAM_STATE_IDLE:
     ret = "Waiting"
  elif stream_busy_flag.value == STREAM_STATE_BUSY:
     ret = "Busy"
  elif stream_busy_flag.value == STREAM_STATE_DONE:
     ret = []
     tmp = ""
     for s in stream_result:
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

app.layout = html.Div(
   dcc.Tabs([
      dcc.Tab(label='History', children=[
         dcc.Graph(id='live-update-graph'),
         dcc.Interval(id='interval-component',
                      interval = t_update,
                      n_intervals = 0
         )
      ]),
      dgemm_tab.Tab(),
      streamTab()
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
