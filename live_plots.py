#!/usr/bin/env python

import dash
from dash import html
from dash import dcc
from dash.dependencies import Input, Output, State
import plotly
import nvml

from collections import deque

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
         'name': plot.key
      }, irow, icol)
      self.fig.update_yaxes(range=[plot.y_low, plot.y_max], row=irow, col=icol,
                            title_text=plot.label)
      i_plot += 1
    self.fig.update_layout(height=self.n_rows * 500, width = self.n_cols * 600,
                           showlegend = False,
                           #title_text="Side By Side Subplots")
                          )
    return self.fig

  def all_keys (self):
    return [plot.key for plot in self.plots]

tab_style = {'display':'inline'}
def Tab (deviceProps, hwPlots):
  return dcc.Tab(label='History', children=[
           dcc.Checklist(id='choosePlots', options = hwPlots.all_keys(), value = ['Temperature', 'Frequency']),
           dcc.Graph(id='live-update-graph'),
           dcc.Interval(id='interval-component',
                        interval = hwPlots.t_update,
                        n_intervals = 0
           )],
         )

def register_callbacks (app, hwPlots):
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
    hwPlots.timestamps.appendleft(t)
    for plot in hwPlots.plots:
      plot.y_values.appendleft(items[plot.key])
      plot.rescale_yaxis (items[plot.key])
    return hwPlots.gen_plots ()
  
