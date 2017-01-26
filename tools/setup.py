from sys import executable

setuptools_import_error_message = """setuptools is not installed for """ + executable + """
Please follow this link for installing instructions :
https://pypi.python.org/pypi/setuptools
make sure you use \"""" + executable + """\" during the installation"""

try:
    from setuptools import setup, Extension
except ImportError:
    raise ImportError(setuptools_import_error_message)

from os.path import join as pjoin
from os.path import dirname

# Utility function to read the README file.
# Used for the long_description.  It's nice, because now 1) we have a top level
# README file and 2) it's easier to type in the README file than to put a raw
# string in below ...
def read(fname):
    return open(pjoin(dirname(__file__), fname)).read()

dummy_module = Extension('dummy_ortools_dependency',
                         sources = ['dummy/dummy_ortools_dependency.cc'],
DELETEUNIX               extra_link_args=['/MANIFEST'],
                        )

setup(
    name='ORTOOLS_PYTHON_VERSION',
    version='VVVV',
    packages=[
        'ortools',
        'ortools.algorithms',
        'ortools.constraint_solver',
        'ortools.graph',
        'ortools.linear_solver',],
    ext_modules = [dummy_module],
    install_requires = [
        'protobuf == PROTOBUF_TAG'],
    package_data = {
        'ortools.constraint_solver' : ['_pywrapcp.dll'],
        'ortools.linear_solver' : ['_pywraplp.dll'],
        'ortools.graph' : ['_pywrapgraph.dll'],
        'ortools.algorithms' : ['_pywrapknapsack_solver.dll'],
        DELETEWIN 'ortools' : ['libortools.DLL']
    },
    license='Apache 2.0',
    author = 'Google Inc',
    author_email = 'lperron@google.com',
    description = 'Google OR-Tools python libraries and modules',
    keywords = ('operations research, constraint programming, ' +
                'linear programming,' + 'flow algorithms,' +
                'python'),
    url = 'https://developers.google.com/optimization/',
    download_url = 'https://github.com/google/or-tools/releases',
    classifiers = [
        'Development Status :: 5 - Production/Stable',
        'Intended Audience :: Developers',
        'License :: OSI Approved :: Apache Software License',
        'Operating System :: MacOS :: MacOS X',
        'Operating System :: Microsoft :: Windows',
        'Operating System :: POSIX :: Linux',
        'Programming Language :: Python :: 2',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.5',
        'Programming Language :: Python :: 3.6',
        'Topic :: Office/Business :: Scheduling',
        'Topic :: Scientific/Engineering',
        'Topic :: Scientific/Engineering :: Mathematics',
        'Topic :: Software Development :: Libraries :: Python Modules'],
    long_description = read('README.txt'),
)
