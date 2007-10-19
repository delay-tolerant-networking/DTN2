from distutils.core import setup, Extension
from os import getenv

INCDIR  = getenv('INCDIR')
LIBDIR  = getenv('LIBDIR')
VERSION = getenv('VERSION')

if INCDIR  == None: raise ValueError('must set INCDIR')
if LIBDIR  == None: raise ValueError('must set LIBDIR')
if VERSION == None: raise ValueError('must set VERSION')

setup(name="dtnapi", 
      version=VERSION,
      description="DTN API Python Bindings",
      author="Michael Demmer",
      author_email="demmer@cs.berkeley.edu",
      url="http://www.dtnrg.org",
      py_modules=["dtnapi"],
      ext_modules=[Extension("_dtnapi", ["dtn_api_wrap_python.cc"],
                             include_dirs=[INCDIR],
			     library_dirs=[LIBDIR],
                             libraries=["".join(['dtnapi-', VERSION])])]
      )
