#!/usr/bin/env python

import dash
from dash import html
from dash import dcc
from dash.dependencies import Input, Output, State
import plotly
import nvml

import multiprocessing

STREAM_STATE_IDLE = 0
STREAM_STATE_BUSY = 1
STREAM_STATE_DONE = 2

stream_result = multiprocessing.Array('u', 1024)
stream_busy_flag = multiprocessing.Value('i', 0)

def Tab (deviceProps):
  max_stream_size = nvml.streamMaxVectorSize(deviceProps.total_mem[0])
  return dcc.Tab(label='Stream', children=[
         html.H1('Start a stream run'),
         html.P(children="Available GPU Memory: " + str(deviceProps.total_mem_gib[0]) + " GiB"),
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

def register_callbacks(app):
  @app.callback(
     Output('stream-button-out', 'children'),
     Input('start-stream', 'n_clicks'),
     State('input-stream-matrix-size', 'value'),
     State('input-stream-nrepeat', 'value'))
  def do_stream_button_click (n_clicks, vector_size, n_repeats):
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

