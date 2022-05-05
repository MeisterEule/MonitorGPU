from distutils.core import setup, Extension

nvml_module = Extension('nvml',
                        sources = ['monitor_gpu.cpp', 'nvml_interface.cpp', 'dgemm.cpp'],
                        extra_compile_args=['-std=c++17', '-I/usr/local/cuda-11.2/include'],
                        extra_objects=['-L/usr/local/cuda-11.2/lib64', '-lcudart', '-lcublas'])

setup(name = 'nvml',
      version='0.10.0',
      description='Display GPU Hardware Counters',
      ext_modules=[nvml_module]) 
