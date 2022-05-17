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

global_time = multiprocessing.Value('i', 0)
global_timestamps = multiprocessing.Array('i', 50)
global_yvalues = []
n_waiting_timestamps = multiprocessing.Value('i', 0)

class hardwarePlot():
  def __init__(self, key, label, n_x_values, is_host, is_visible=True):
    self.key = key
    self.label = label
    self.y_values = deque([], n_x_values)
    self.y_low = 1000
    self.y_max = 0
    self.is_host = is_host
    self.visible = is_visible

  def rescale_yaxis (self, value, scale_min=0.8, scale_max=1.2):
    if value * scale_min < self.y_low: self.y_low = value * scale_min
    if value * scale_max > self.y_max: self.y_max = value * scale_max

class hardwarePlotCollection ():
  def __init__(self, device, device_keys, host_keys, 
               device_labels, host_labels, init_visible_keys, t_update=1000, n_x_values=50):
    ll = len(device_keys) + len(host_keys)
    self.n_cols = 1 if ll == 1 else 2
    self.n_rows = (ll + 1) // 2
    self.t_update = t_update
    self.n_x_values = n_x_values
    self.timestamps = deque([], self.n_x_values)
    self.plots = []
    for key, label in zip(device_keys, device_labels):
      self.plots.append(hardwarePlot(key, label, self.n_x_values, False))
    for key, label in zip(host_keys, host_labels):
      self.plots.append(hardwarePlot(key, label, self.n_x_values, True))
    self.device = device
    self.set_visible (init_visible_keys)
    self.fig = None
    self.update_counter = 0

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

  def all_keys (self):
    return [plot.key for plot in self.plots]

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
          #self.prev_cpu_total = total
          #self.prev_cpu_work = work
          self.prev_jiffies = jiffies.copy()

    self.handle.seek(0) 
    return self.current_cpu_usage

  def read_out(self):
    return {'CPU': self.get_cpu_usage()}

class fileWriter():
  def __init__(self):
    self.handle = None
    self.is_open = False

  def start (self, filename, device_name, host_name, keys):
    self.handle = open(filename, "w+")
    self.is_open = True
    self.handle.write("Watching %s on %s\n\n" % (device_name, host_name))
    self.handle.write("Date       ")
    for key in keys:
       self.handle.write("%s " % key)
    self.handle.write("\n")

  def stop (self):
    self.handle.write("STOP\n")
    self.handle.close()
    self.is_open = False

  def add_items(self, items):
    now = datetime.now()
    self.handle.write("%s: " % now.strftime("%H:%M:%S"))
    for key, item in items.items():
       print ("key: ", key)
       self.handle.write("%d " % item)
    self.handle.write("\n")

def multiprocRead (hwPlots):
  indices = {}
  for i, key in enumerate(hwPlots.all_keys()):
     indices[key] = i 
     global_yvalues.append(multiprocessing.Array('i', 50)) 
  while True:
    global_timestamps[n_waiting_timestamps.value] = global_time.value
    hwPlots.device.readOut() 
    for key, value in hwPlots.device.getItems().items():
      i = indices[key]
      global_yvalues[i][n_waiting_timestamps.value] = value
    print (hwPlots.device.getItems())
    time.sleep(1)
    n_waiting_timestamps.value += 1
    global_time.value += 1
    

file_writer = fileWriter()
host_reader = hostReader()

tab_style = {'display':'inline'}
def Tab (deviceProps, hwPlots):
  readOutProc = multiprocessing.Process(target=multiprocRead, args=(hwPlots,))
  readOutProc.start()
  #readOutProc.join()
  return dcc.Tab(
           label='History', children=[
           html.H1('Watching %s on %s' % (deviceProps.name, host_reader.host_name)), 
           html.P (id='live-update-procids', children=deviceProps.procString()),
           html.H2('Choose plots:'), 
           dcc.Checklist(id='choosePlots', options = hwPlots.all_keys(), value = ['Temperature', 'Frequency']),
           html.Button('Start recording', id='saveFile', n_clicks=0),
           dcc.Graph(id='live-update-graph'),
           dcc.Interval(id='interval-component',
                        interval = hwPlots.t_update,
                        n_intervals = 0
           )],
         )

def register_callbacks (app, hwPlots, deviceProps):
  @app.callback(
      Output('choosePlots', 'value'),
      Input('choosePlots', 'value'))
  def checklistAction(values):
    print ("Clicked: ", values)
    hwPlots.set_visible(values)
    return values

  @app.callback(
      Output('live-update-procids', 'children'),
      Input ('interval-component', 'n_intervals'))
  def update_proc_ids(n):
    if hwPlots.update_counter != n:
       #hwPlots.device.readOut()
       hwPlots.update_counter = n
    deviceProps.processes = hwPlots.device.getProcessInfo()

    return deviceProps.procString()
    
  @app.callback(
      Output('live-update-graph', 'figure'),
      Input('interval-component', 'n_intervals'))
  def update_graph_live(n):
    t = hwPlots.t_update * n / 1000
    if hwPlots.update_counter != n:
       #hwPlots.device.readOut()
       if n_waiting_timestamps.value > 0:
          print ("Nr. of timestamps: ", n_waiting_timestamps.value)
          n_waiting_timestamps.value = 0
       hwPlots.update_counter = n

    device_usage = hwPlots.device.getItems()
    host_usage = host_reader.read_out()

    if file_writer.is_open:
       file_writer.add_items (device_usage)

    #cpu_reader.get_cpu_usage()

    #active_processes = hwPlots.device.getProcessInfo()
    #print ("active: ", active_processes)

    hwPlots.timestamps.appendleft(t)
    for plot in hwPlots.plots:
      if plot.is_host:
        y = host_usage[plot.key]
      else:
        y = device_usage[plot.key]
      plot.y_values.appendleft(y)
      plot.rescale_yaxis (y)
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
       file_writer.start(filename, deviceProps.name, hostReader.host_name, hwPlots.all_keys())
       return "Recording..."
    elif n_clicks > 0:
       file_writer.stop()
       return "Start recording"
  
