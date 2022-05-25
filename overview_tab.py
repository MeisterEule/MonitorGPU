#!/usr/bin/env python

import dash
from dash import html
from dash import dcc

def persistence_string (enabled):
  if enabled:
     return " [Persistence enabled] "
  else:
     return " [Persistence disabled] "

def Tab (deviceProps, num_gpus, host_name):

  gpu_list = [html.H3 ("Avaibable GPUs: ")]
  for gpu_id, name in enumerate(deviceProps.names):
     gpu_list.append(html.Span(children="GPU %d: %s" % (gpu_id, name)))
     this_color = "Green" if deviceProps.persistence_enabled else "Red"
     gpu_list.append(html.Span(children=persistence_string(deviceProps.persistence_enabled),
                               style={"color": this_color}))
     gpu_list.append(html.Br())

  return dcc.Tab(
            label="Overview", children=[
            html.H1("Watching %d GPUs on %s" % (num_gpus, host_name)),
            html.Div(gpu_list),
         ]) 
