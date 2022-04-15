from distutils.core import setup, Extension
import os

try:
    testdir = '/'.join([os.environ['OMNIPLAY_DIR'], "test"])
except KeyError:
    testdir = ''

parseklograw = Extension('parseklograw',
                include_dirs = [testdir],
                libraries = ['util'],
                library_dirs = [testdir],
                sources = ['parseklograwmodule.c'])

setup (name = "ParseklogRaw",
    version = '1.0',
    description = "Extension to parseklib, to parse replay kernel logs from python",
    ext_modules = [parseklograw])


utilcalls = Extension('utilcalls',
                include_dirs = [testdir],
                libraries = ['util'],
                library_dirs = [testdir],
                sources = ['utilcalls.c'])

setup (name = "UtilCalls",
       version = '1.0',
       description = "Extension to arnold, to call replay stuff from python",
       ext_modules = [utilcalls])
