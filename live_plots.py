#!/usr/bin/env python

import dash
from dash import html
from dash import dcc
from dash.dependencies import Input, Output, State
import plotly
import nvml

from collections import deque
from datetime import datetime

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
  def __init__(self, device, keys, labels, init_visible_keys, t_update=1000, n_x_values=50):
    ll = len(keys)
    self.n_cols = 1 if ll == 1 else 2
    self.n_rows = (ll + 1) // 2
    self.t_update = t_update
    self.n_x_values = n_x_values
    self.timestamps = deque([], self.n_x_values)
    self.plots = [hardwarePlot(key, label, self.n_x_values) for key, label in zip(keys, labels)]
    self.device = device
    self.set_visible (init_visible_keys)
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

  def all_keys (self):
    return [plot.key for plot in self.plots]

class fileWriter():
  def __init__(self):
    self.handle = None
    self.is_open = False

  def start (self, filename, device_name, keys):
    self.handle = open(filename, "w+")
    self.is_open = True
    self.handle.write("Watching %s\n\n" % device_name)
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
    for item in items.values():
       self.handle.write("%d " % item)
    self.handle.write("\n")
    

file_writer = fileWriter()

tab_style = {'display':'inline'}
def Tab (deviceProps, hwPlots):
  return dcc.Tab(
           label='History', children=[
           html.H1('Watching ' + deviceProps.name), 
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
      Output('live-update-graph', 'figure'),
      Input('interval-component', 'n_intervals'))
  def update_graph_live(n):
    t = hwPlots.t_update * n / 1000
    hwPlots.device.readOut()
    items = hwPlots.device.getItems()

    if file_writer.is_open:
       file_writer.add_items (items)

    hwPlots.timestamps.appendleft(t)
    for plot in hwPlots.plots:
      plot.y_values.appendleft(items[plot.key])
      plot.rescale_yaxis (items[plot.key])
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
       file_writer.start(filename, deviceProps.name, hwPlots.all_keys())
       return "Recording..."
    else:
       file_writer.stop()
       return "Start recording"
  
