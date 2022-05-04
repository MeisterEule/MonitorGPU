from distutils.core import setup, Extension

nvml_module = Extension('nvml', sources = ['monitor_gpu.cpp', 'nvml_interface.cpp'])

setup(name = 'nvml',
      version='0.10.0',
      description='Display GPU Hardware Counters',
      ext_modules=[nvml_module]) 
