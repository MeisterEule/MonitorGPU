#!/usr/bin/env python

import dash
from dash import html
from dash import dcc
from dash.dependencies import Input, Output, State
import plotly
import nvml

import multiprocessing

import device_properties

DGEMM_RESULT_BUFFER_SIZE = 1024
DGEMM_STATE_IDLE = 0
DGEMM_STATE_BUSY = 1
DGEMM_STATE_DONE = 2

matrix_size = 1000
n_repeat = 30

dgemm_result = multiprocessing.Array('u', 1024)
dgemm_busy_flag = multiprocessing.Value('i', 0)

def Tab (deviceProps):
  max_dgemm_size = nvml.dgemmMaxMatrixSize(deviceProps.total_mem)
  return dcc.Tab(label='Dgemm', children=[
         html.H1('This is a new tab!'),
         html.P(children="Available GPU Memory: " + str(deviceProps.total_mem_gib) + " GiB"),
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

def register_callbacks(app):
  @app.callback(
     Output('button-out', 'children'),
     Input('start-dgemm', 'n_clicks'),
     State('input-dgemm-matrix-size', 'value'),
     State('input-dgemm-nrepeat', 'value')
  )
  def do_button_click (n_clicks, matrix_size, n_repeats):
    if n_clicks > 0:
       dgemm_proc = multiprocessing.Process(target=multiprocDgemm, args=(matrix_size, n_repeats, dgemm_result, dgemm_busy_flag))
       dgemm_proc.start()
       dgemm_proc.join()
    #return 'The input was "{}" and "{}"'.format(matrix_size, n_repeats)
  
  @app.callback(
     Output('live-update-dgemm', 'children'),
     Input('live-update-dgemm', 'children'),
     Input('dgemm-interval-component', 'n_intervals'))
  def update_result(original_name, n_intervals):
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


