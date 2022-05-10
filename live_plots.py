#!/usr/bin/env python

import dash
from dash import html
from dash import dcc
from dash.dependencies import Input, Output, State
import plotly
import nvml

from collections import deque

class hardwarePlot():
  def __init__(self, label, n_x_values):
    self.label = label
    self.y_values = deque([], n_x_values)
    self.y_low = 1000
    self.y_max = 0

  def rescale_yaxis (self, value, scale_min=0.8, scale_max=1.2):
    if value * scale_min < self.y_low: self.y_low = value * scale_min
    if value * scale_max > self.y_max: self.y_max = value * scale_max

class hardwarePlotCollection ():
  def __init__(self, device, labels, t_update=1000, n_x_values=50):
    self.t_update = t_update
    self.n_x_values = n_x_values
    self.timestamps = deque([], self.n_x_values)
    self.plots = [hardwarePlot(label, self.n_x_values) for label in labels]
    self.device = device
    self.fig = None

  def gen_plots (self):
    self.fig = plotly.tools.make_subplots(rows=len(self.plots), cols=1, vertical_spacing=0.2)
    x = list(self.timestamps)
    x.reverse()
    for i, plot in enumerate(self.plots):
      y = list(plot.y_values)
      y.reverse()
      self.fig.append_trace({
         'x': x,
         'y': y,
         'name': plot.label
      }, i + 1, 1)
      self.fig.update_yaxes(range=[plot.y_low, plot.y_max], row=i+1, col=1)
    return self.fig

def Tab (deviceProps, hwPlots):
  return dcc.Tab(label='History', children=[
           dcc.Graph(id='live-update-graph'),
           dcc.Interval(id='interval-component',
                        interval = hwPlots.t_update,
                        n_intervals = 0
           )
         ])

def register_callbacks (app, hwPlots):
  @app.callback(Output('live-update-graph', 'figure'),
                 Input('interval-component', 'n_intervals'))
  def update_graph_live(n):
    t = hwPlots.t_update * n / 1000
    hwPlots.device.readOut()
    items = hwPlots.device.getItems()
    hwPlots.timestamps.appendleft(t)
    for plot in hwPlots.plots:
      plot.y_values.appendleft(items[plot.label])
      plot.rescale_yaxis (items[plot.label])
      #this_y = list(plot.y_values)
      #this_y.reverse()
      #y.append(this_y)
      
    #temperature.appendleft(items['Temperature']) 
    #frequency.appendleft(items['Frequency'])
    #rescale_axes (items)
    #all_y = []
    #y = list(temperature)
    #y.reverse()
    #all_y.append(y)
    #y = list(frequency)
    #y.reverse()
    #all_y.append(y)
    #labels = ['Temperature', 'Frequency']
    return hwPlots.gen_plots ()
  
