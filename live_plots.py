#!/usr/bin/env python

import dash
from dash import html
from dash import dcc
from dash.dependencies import Input, Output, State
import plotly
import nvml

from datetime import datetime

import time
import multiprocessing

import file_writer

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
      self.last_read_at.value -= 1

  def flush(self):
    ret = [self.content[i] for i in range(self.last_read_at.value, self.n_elements.value)]
    self.last_read_at.value = self.n_elements.value
    return ret

  def has_new_data(self):
    return self.last_read_at.value != self.n_elements.value

  def get_all(self):
    return [self.content[i] for i in range(self.n_elements.value)]

  def reset(self):
    self.n_elements.value = 0

class multiProcQueueCollection():
  def __init__(self, lock, num_plots, buffer_size):
    self.yvalues = [multiProcQueue('i', lock, buffer_size) for i in range(num_plots)]

  def reset (self):
    for y in self.yvalues:
      y.reset()

  def has_new_data(self):
    return any([queue.has_new_data() for queue in self.yvalues])

class multiProcState():
  def __init__(self, keys, buffer_size, num_gpus=1):
    self.num_gpus = num_gpus
    self.lock = multiprocessing.Lock()
    self.count = multiprocessing.Value('i', 0, lock=self.lock)
    self.timestamps = multiProcQueue('i', self.lock, buffer_size)
    self.keys = {}
    for i, key in enumerate(keys):
       self.keys[key] = i
    num_plots = len(keys)
    self.start_time = datetime.now()
    self.queues = [multiProcQueueCollection(self.lock, num_plots, buffer_size) for i in range(num_gpus)]

  def inc_timestamps(self):
    self.count.value += 1
    delta = datetime.now() - self.start_time
    self.timestamps.put(delta.seconds * 1000000 + delta.microseconds)

  def put_observables(self, gpu_id, key, value):
    self.queues[gpu_id].yvalues[self.keys[key]].put(value) 

  def get_observable(self, key, gpu_id):
    return self.queues[gpu_id].yvalues[self.keys[key]].get_all()

  def reset(self):
    self.count.value = 0
    self.timestamps.reset()
    self.start_time = datetime.now()
    for queue in self.queues:
       queue.reset()

  def has_new_data(self, gpu_id):
    return self.queues[gpu_id].has_new_data()
    

class hardwarePlot():
  def __init__(self, key, label, is_visible=True):
    self.key = key
    self.label = label
    self.visible = is_visible

class hardwarePlotCollection ():
  def __init__(self, device, host_reader, keys, labels, init_visible_keys, n_x_values=50):
    ll = len(keys)
    self.n_cols = 1 if ll == 1 else 2
    self.n_rows = (ll + 1) // 2
    self.n_x_values = n_x_values
    self.plots = []
    for key, label in zip(keys, labels):
      self.plots.append(hardwarePlot(key, label))
    self.device = device
    self.set_visible (init_visible_keys)
    self.display_gpus = [0]
    self.fig = None
    self.colors = ["black", "red", "blue", "green"]
    self.update_active = True
    self.host_reader = host_reader

  def set_visible (self, new_keys):
    for plot in self.plots:
       plot.visible = plot.key in new_keys

  def gen_plots (self):
    if not self.update_active: return self.fig
    self.fig = plotly.tools.make_subplots(rows=self.n_rows, cols=self.n_cols, vertical_spacing=0.075)
    i_plot = 0
    for plot in self.plots:
      if not plot.visible: continue
      irow = (i_plot // 2) + 1
      icol = (i_plot % 2) + 1
      y_max = 0
      y_min = 1000
      x = [xx / 1000000 for xx in global_values.timestamps.get_all()]
      for i_gpu in self.display_gpus:
         y = global_values.get_observable(plot.key, i_gpu)
         y_max = max(max(y) * 1.25, y_max)
         y_min = min(min(y) * 0.8, y_min)
         self.fig.append_trace({
            'x': x,
            'y': y,
            'name': "GPU-" + str(i_gpu),
            'marker': {'color': self.colors[i_gpu]}
         }, irow, icol)

      self.fig.update_yaxes(range=[y_min, y_max], row=irow, col=icol, title_text=plot.label)
      self.fig.update_xaxes(range=[x[0] - 1, x[-1] + 2], row=irow, col=icol, title_text="t [s]")
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

global_values = None

def multiProcRead (hwPlots, t_record_s):
  while True:
    #now = datetime.now()
    #print (now)
    hwPlots.device.readOut() 
    for gpu_index in range(global_values.num_gpus):
      for key, value in hwPlots.device.getItems(gpu_index).items():
         global_values.put_observables(gpu_index, key, value)
      for key, value in hwPlots.host_reader.read_out().items():
         global_values.put_observables(gpu_index, key, value)  
    time.sleep(t_record_s)
    

file_writer = file_writer.fileWriter()

tab_style = {'display':'inline'}
def Tab (deviceProps, hwPlots, num_gpus, buffer_size, t_update_s, t_record_s, do_logfile):
  global global_values
  global_values = multiProcState(hwPlots.all_keys(), buffer_size, num_gpus)
  readOutProc = multiprocessing.Process(target=multiProcRead, args=(hwPlots,t_record_s))
  readOutProc.start()
  # Where to join (signal handling)?
  #readOutProc.join()
  if do_logfile:
     file_writer.start(deviceProps.names, hwPlots.host_reader.host_name, hwPlots.all_keys())

  proclist = [html.P ("Active processes: ")]
  for gpu_id in range(num_gpus):
    id_string = "live-update-procids-" + str(gpu_id)
    proclist.append(html.Plaintext (id=id_string, children=deviceProps.procString(hwPlots.device, gpu_id)))

  return dcc.Tab(
           label='Live Plots', children=[
           html.H1('Watching %d GPUs on %s' % (num_gpus, hwPlots.host_reader.host_name)), 
           html.Div(children=proclist), 
           html.H2('Choose plots:'), 
           dcc.Checklist(id='choosePlots', options = hwPlots.all_keys(), value = hwPlots.get_visible_keys()),
           html.Div(["Choose GPU ID: ",
                     dcc.Input(id="choose-gpu", value='0', type='string', debounce=True)
           ]),
           html.Div(id="gpu-out", style={'display': 'none'}),
           html.Button('Stop', id='stopButton', n_clicks=0),
           dcc.Graph(id='live-update-graph'),
           dcc.Interval(id='interval-component',
                        interval = t_update_s * 1000,
                        n_intervals = 0),
           ],
         )

def register_callbacks (app, hwPlots, deviceProps):
  @app.callback(
      Output('choosePlots', 'value'),
      Input('choosePlots', 'value'))
  def checklistAction(values):
    hwPlots.set_visible(values)
    return values

  @app.callback(
      Output('live-update-procids-0', 'children'),
      Input ('interval-component', 'n_intervals'))
  def update_proc_ids(n):
    deviceProps.update_process(hwPlots.device, 0)
    return deviceProps.procString(hwPlots.device, 0)

  @app.callback(
      Output('live-update-procids-1', 'children'),
      Input ('interval-component', 'n_intervals'))
  def update_proc_ids(n):
    deviceProps.update_process(hwPlots.device, 1)
    return deviceProps.procString(hwPlots.device, 1)

  @app.callback(
      Output('live-update-procids-2', 'children'),
      Input ('interval-component', 'n_intervals'))
  def update_proc_ids(n):
    deviceProps.update_process(hwPlots.device, 2)
    return deviceProps.procString(hwPlots.device, 2)

  @app.callback(
      Output('live-update-procids-3', 'children'),
      Input ('interval-component', 'n_intervals'))
  def update_proc_ids(n):
    deviceProps.update_process(hwPlots.device, 3)
    return deviceProps.procString(hwPlots.device, 3)

  @app.callback(
    Output ('gpu-out', 'children'),
    Input ('choose-gpu', 'value'),
    )
  def choose_gpus (gpu_ids):
    if gpu_ids != '':
      if gpu_ids.isdigit():
        hwPlots.display_gpus = [int(gpu_ids)]
      else:
        check_number = gpu_ids.replace('-','').replace(',','')
        if not check_number.isdigit():
          return "Invalid" 
        tmp = gpu_ids.split(',')
        hwPlots.display_gpus = []
        for s in tmp:
          if '-' in s:
            tmp2 = s.split('-')
            for i in range(int(tmp2[0]), int(tmp2[1])+1):
              hwPlots.display_gpus.append(i)
          else:
            hwPlots.display_gpus.append(int(s))
    return gpu_ids
    
  @app.callback(
      Output('live-update-graph', 'figure'),
      Input('interval-component', 'n_intervals'))
  def update_graph_live(n):

    n_retries = 75
    n_sleep = 0
    for i in range(n_retries):
      if global_values.has_new_data(0):
         break
      else:
         n_sleep += 1
         time.sleep(0.01)

    no_new_data = n_sleep == n_retries
    if (no_new_data):
       return hwPlots.fig

    global_values.inc_timestamps()
    if file_writer.is_open:
       t = [tt / 1000000 for tt in global_values.timestamps.flush()]
       if len(t) != 0:
         y = []
         for queue in global_values.queues:
           for yy in queue.yvalues:
              y.append(yy.flush())
         file_writer.add_items (t, list(map(list, zip(*y))))

    return hwPlots.gen_plots ()

  @app.callback(
      Output('stopButton', 'children'),
      Input('stopButton', 'n_clicks')
  )
  def stop_button_click (n_clicks):
    if n_clicks % 2 == 1:
      hwPlots.update_active = False 
      file_writer.stop()
      return "Restart"
    elif n_clicks > 0:
      hwPlots.update_active = True
      global_values.reset()
    return "Stop"
  
