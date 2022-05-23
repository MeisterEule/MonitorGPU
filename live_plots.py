#!/usr/bin/env python

import dash
from dash import html
from dash import dcc
from dash.dependencies import Input, Output, State
import plotly
import nvml

from collections import deque
from datetime import datetime
from platform import node

import time
import re
import multiprocessing

class multiProcQueue():
  def __init__(self, element_type, lock, queue_size=50):
    self.content = multiprocessing.Array(element_type, queue_size, lock=lock)
    self.size = queue_size
    self.n_elements = multiprocessing.Value('i', 0, lock=lock)
    self.last_read_at = multiprocessing.Value('i', 0, lock=lock)

  def put(self, value):
    if self.n_elements.value < self.size:
      self.content[self.n_elements.value] = value
      self.n_elements.value += 1
    else:
      for i in range(self.size - 1):
        self.content[i] = self.content[i+1]
      self.content[self.size-1] = value
      if self.last_read_at.value > 0:
         self.last_read_at.value -= 1

  def flush(self):
    ret = [self.content[i] for i in range(self.last_read_at.value, self.n_elements.value)]
    self.last_read_at.value = self.n_elements.value
    return ret

  def reset(self):
    self.last_read_at.value = 0

class multiProcValues():
  def __init__(self, buffer_size=None):
    self.lock = multiprocessing.Lock()
    self.time = multiprocessing.Value('i', 0, lock=self.lock)
    if buffer_size != None:
      self.timestamps = multiProcQueue('i', self.lock, buffer_size)
    else:
      self.timestamps = None
    self.yvalues = []
    self.keys = {}
    self.n_total = multiprocessing.Value('i', 0, lock=self.lock)

  def setup (self, keys, buffer_size=50):
     self.timestamps = multiProcQueue ('i', self.lock, buffer_size)
     for i, key in enumerate(keys):
        self.keys[key] = i
        self.yvalues.append(multiProcQueue('d', self.lock, buffer_size))


class hardwarePlot():
  def __init__(self, key, label, n_x_values, is_visible=True):
    self.key = key
    self.label = label
    self.y_values = deque([], n_x_values)
    self.y_low = 1000
    self.y_max = 0
    self.visible = is_visible

  def rescale_yaxis (self, value, scale_min=0.8, scale_max=1.2):
    if value * scale_min < self.y_low: self.y_low = value * scale_min
    if value * scale_max > self.y_max: self.y_max = value * scale_max

class hardwarePlotCollection ():
  def __init__(self, device, keys, labels, init_visible_keys, n_x_values=50):
    ll = len(keys)
    self.n_cols = 1 if ll == 1 else 2
    self.n_rows = (ll + 1) // 2
    self.n_x_values = n_x_values
    self.timestamps = deque([], self.n_x_values)
    self.plots = []
    for key, label in zip(keys, labels):
      self.plots.append(hardwarePlot(key, label, self.n_x_values))
    self.device = device
    self.set_visible (init_visible_keys)
    self.display_gpu = [0]
    self.redraw = False
    self.fig = None

  def set_visible (self, new_keys):
    for plot in self.plots:
       plot.visible = plot.key in new_keys

  def gen_plots (self):
    self.fig = plotly.tools.make_subplots(rows=self.n_rows, cols=self.n_cols, vertical_spacing=0.2)
    x = list(self.timestamps)
    x.reverse()
    i_plot = 0
    for plot in self.plots:
      if not plot.visible: continue
      irow = (i_plot // 2) + 1
      icol = (i_plot % 2) + 1
      y = list(plot.y_values)
      y.reverse()
      self.fig.append_trace({
         'x': x,
         'y': y,
         'name': plot.key,
         'marker': {'color': 'black'}
      }, irow, icol)
      self.fig.update_yaxes(range=[plot.y_low, plot.y_max], row=irow, col=icol,
                            title_text=plot.label)
      self.fig.update_xaxes(range=[self.timestamps[-1], self.timestamps[0] + 1], row=irow, col=icol)
      i_plot += 1
    self.fig.update_layout(height=self.n_rows * 500, width = self.n_cols * 600,
                           showlegend = False,
                          )
    return self.fig

  def getData (self):
    t = list(self.timestamps)
    t.reverse()
    all_y = []
    all_keys = []
    for plot in self.plots:
      y = list(plot.y_values)
      y.reverse()
      all_y.append(y)
      all_keys.append(plot.key)
    # Transpose the lists of y values
    return t, all_keys, list(map(list, zip(*all_y)))
   

  def all_keys (self):
    return [plot.key for plot in self.plots]

  def num_keys (self):
    return len(self.plots)

  def get_visible_keys (self):
    keys = []
    for plot in self.plots:
      if plot.visible:
        keys.append(plot.key)
    return keys

class hostReader():
  def __init__(self):
    self.handle = open("/proc/stat")
    self.prev_jiffies = [0 for i in range(10)]
    self.current_cpu_usage = 0
    self.host_name = node()
    
  def __del__(self):
    self.handle.close()
    self.handle = None

  def get_cpu_usage(self):
    for line in self.handle.readlines():
      if re.search("cpu0 ", line):
        jiffies = [int(l) for l in line.split()[1:]]
        if any ([j_new > j_old for j_new, j_old in zip(jiffies[0:3], self.prev_jiffies[0:3])]):
          total = sum(jiffies[0:-1])
          work = sum(jiffies[0:3])
          prev_total = sum(self.prev_jiffies[0:-1])
          prev_work = sum(self.prev_jiffies[0:3])
          self.current_cpu_usage = (work - prev_work) / (total - prev_total) * 100
          self.prev_jiffies = jiffies.copy()

    self.handle.seek(0) 
    return self.current_cpu_usage

  def read_out(self):
    return {'CPU': self.get_cpu_usage()}

class fileWriter():
  def __init__(self):
    self.handle = None
    self.is_open = False

  def start (self, filename, device_name, host_name, keys, start_date=None):
    self.handle = open(filename, "w+")
    self.is_open = True
    self.handle.write("Watching %s on %s\n\n" % (device_name, host_name))
    if start_date != None: self.handle.write("Start date:      %s" % (start_date))
    for key in keys:
       self.handle.write("%s " % key)
    self.handle.write("\n")

  def stop (self, end_date=None):
    self.handle.write("Finished recording\n")
    if end_date != None: self.handle.write("End date:     %s" % (end_date))
    self.handle.close()
    self.is_open = False

  def add_items(self, timestamps, all_y):
    for t, y_line in zip(timestamps, all_y): 
      self.handle.write("%d: " % t)
      for y in y_line:
        self.handle.write("%f " % y)
      self.handle.write("\n")

global_values = [multiProcValues() for i in range(2)]

def multiProcRead (hwPlots, t_record_s):
  while True:
    for gpu_index, gpu_element in enumerate(global_values):
      gpu_element.timestamps.put(gpu_element.time.value)
      hwPlots.device.readOut() 
      for key, value in hwPlots.device.getItems(gpu_index).items():
        i = gpu_element.keys[key]
        gpu_element.yvalues[i].put(value)
      for key, value in host_reader.read_out().items():
        i = gpu_element.keys[key]
        gpu_element.yvalues[i].put(value)

      gpu_element.time.value += 1
      gpu_element.n_total.value += 1
    time.sleep(t_record_s)
    

file_writer = fileWriter()
host_reader = hostReader()

tab_style = {'display':'inline'}
def Tab (deviceProps, hwPlots, buffer_size, t_update_s, t_record_s):
  for gpu_element in global_values:
     gpu_element.setup(hwPlots.all_keys(), buffer_size)
  readOutProc = multiprocessing.Process(target=multiProcRead, args=(hwPlots,t_record_s))
  readOutProc.start()
  # Where to join (signal handling)?
  #readOutProc.join()
  return dcc.Tab(
           label='History', children=[
           html.H1('Watching %s on %s' % (deviceProps.name, host_reader.host_name)), 
           html.P (id='live-update-procids', children=deviceProps.procString()),
           html.H2('Choose plots:'), 
           dcc.Checklist(id='choosePlots', options = hwPlots.all_keys(), value = hwPlots.get_visible_keys()),
           html.Div(["Choose GPU ID: ",
                     dcc.Input(id="choose-gpu", value='0', type='string')
           ]),
           html.Div(id="gpu-out", style={'display': 'none'}),
           html.Button('Start recording', id='saveFile', n_clicks=0),
           dcc.Graph(id='live-update-graph'),
           dcc.Interval(id='interval-component',
                        interval = t_update_s * 1000,
                        n_intervals = 0
           )],
         )

def register_callbacks (app, hwPlots, deviceProps):
  @app.callback(
      Output('choosePlots', 'value'),
      Input('choosePlots', 'value'))
  def checklistAction(values):
    hwPlots.set_visible(values)
    return values

  @app.callback(
      Output('live-update-procids', 'children'),
      Input ('interval-component', 'n_intervals'))
  def update_proc_ids(n):
    deviceProps.processes = hwPlots.device.getProcessInfo(0)

    return deviceProps.procString()

  @app.callback(
    Output ('gpu-out', 'children'),
    Input ('choose-gpu', 'value'),
    )
  def choose_gpus (gpu_ids):
    if gpu_ids != '':
      prev_gpu_ids = hwPlots.display_gpu[0]
      hwPlots.redraw = prev_gpu_ids != int(gpu_ids)
      if hwPlots.redraw:
        global_values[prev_gpu_ids].timestamps.reset() 
        for obs in global_values[prev_gpu_ids].yvalues:
          obs.reset()
        hwPlots.display_gpu = [int(gpu_ids)]
    return gpu_ids
    
  @app.callback(
      Output('live-update-graph', 'figure'),
      Input('interval-component', 'n_intervals'))
  def update_graph_live(n):
    this_gpu = global_values[hwPlots.display_gpu[0]]
    if not hwPlots.redraw:
      n_draw = this_gpu.timestamps.n_elements.value
    else:
      hwPlots.timestamps.clear()
      for plot in hwPlots.plots:
        plot.y_values.clear()
      n_draw = min(this_gpu.n_total.value, hwPlots.n_x_values)
      hwPlots.redraw = False

    ts = this_gpu.timestamps.flush()
    for t in ts:
      hwPlots.timestamps.appendleft(t)
    for plot in hwPlots.plots:
      i = this_gpu.keys[plot.key]
      ys = this_gpu.yvalues[i].flush() 
      for y in ys:
        plot.y_values.appendleft(y)
        plot.rescale_yaxis(y)

    if file_writer.is_open:
       t, _, y = hwPlots.getData()
       file_writer.add_items (t, y)

    return hwPlots.gen_plots ()

  @app.callback(
      Output('saveFile', 'children'),
      Input('saveFile', 'n_clicks')
  )
  def do_button_click (n_clicks):
    if n_clicks % 2 == 1: 
       now = datetime.now()
       date_str = now.strftime("%Y_%m_%d_%H_%M_%S")
       filename = deviceProps.name + "_" + date_str + ".hwout"
       file_writer.start(filename, deviceProps.name, host_reader.host_name, hwPlots.all_keys(), date_str)
       return "Recording..."
    elif n_clicks > 0:
       now = datetime.now()
       date_str = now.strftime("%Y_%m_%d_%H_%M_%S")
       file_writer.stop(date_str)
       return "Start recording"
    else:
       return "Start recording"
  
