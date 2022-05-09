from distutils.core import setup, Extension
from distutils.command.build_ext import build_ext

import os

def customize_compiler_for_nvcc(self):
  self.src_extensions.append('.cu')
  default_compiler_so = self.compiler_so
  default_compiler_cxx = self.compiler_cxx
  default_linker_so = self.linker_so
  super = self._compile

  def _compile(obj, src, ext, cc_args, extra_postargs, pp_opts):
     if os.path.splitext(src)[1] == '.cu':
        self.set_executable('compiler_so', "/usr/local/cuda-11.2/bin/nvcc")
        self.set_executable('compiler_cxx', "/usr/local/cuda-11.2/bin/nvcc")
        self.set_executable('linker_so', "/usr/local/cuda-11.2/bin/nvcc")
        postargs = ['--compiler-options', '-fPIC']
     else:
        postargs = extra_postargs

     super(obj, src, ext, cc_args, postargs, pp_opts)
     self.compiler_so = default_compiler_so
     self.compiler_cxx = default_compiler_cxx
     self.linker_so = default_linker_so

  self._compile = _compile


class cuda_build_ext(build_ext):
   def build_extensions(self):
      customize_compiler_for_nvcc(self.compiler)
      build_ext.build_extensions(self)


nvml_ext = Extension('nvml',
                      sources = ['monitor_gpu.cpp', 'nvml_interface.cpp', 'common.cpp', 'dgemm.cpp', 'stream.cu'],
                      extra_compile_args=['-std=c++17', '-I/usr/local/cuda-11.2/include'],
                      extra_objects=['-L/usr/local/cuda-11.2/lib64', '-lcudart', '-lcublas'])

                       

setup(name = 'nvml',
      cmdclass={'build_ext': cuda_build_ext,},
      extra_link_args=["-fPIC"],
      version='0.10.0',
      description='Display GPU Hardware Counters',
      #ext_modules=[nvml_ext, stream_ext]) 
      ext_modules=[nvml_ext]) 
