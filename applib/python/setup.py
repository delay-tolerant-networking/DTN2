from distutils.core import setup, Extension

setup(name="dtnapi", 
      version="2.4.1", 
      description="DTN API Python Bindings",
      author="Michael Demmer",
      author_email="demmer@cs.berkeley.edu",
      url="http://www.dtnrg.org",
      py_modules=["dtnapi"],
      ext_modules=[Extension("_dtnapi", ["dtn_api_wrap_python.cc"],
			     library_dirs=[".."], libraries=["dtnapi-2.4.1"])]
      )
