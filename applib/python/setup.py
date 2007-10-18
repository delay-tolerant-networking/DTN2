from distutils.core import setup, Extension
from os import getenv

INCDIR   = getenv('INCDIR')
LIBDIR = getenv('LIBDIR')

if INCDIR == None: raise ValueError('must set INCDIR')
if LIBDIR == None: raise ValueError('must set LIBDIR')

setup(name="dtnapi", 
      version="2.4.1", 
      description="DTN API Python Bindings",
      author="Michael Demmer",
      author_email="demmer@cs.berkeley.edu",
      url="http://www.dtnrg.org",
      py_modules=["dtnapi"],
      ext_modules=[Extension("_dtnapi", ["dtn_api_wrap_python.cc"],
                             include_dirs=[INCDIR],
			     library_dirs=[LIBDIR],
                             libraries=["dtnapi-2.4.1"])]
      )
