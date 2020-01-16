from sys import executable
from os.path import join as pjoin
from os.path import dirname

setuptools_import_error_message = """setuptools is not installed for """ + executable + """
Please follow this link for installing instructions :
https://pypi.python.org/pypi/setuptools
make sure you use \"""" + executable + """\" during the installation"""

try:
    from setuptools import setup, Distribution
except ImportError:
    raise ImportError(setuptools_import_error_message)

# Utility function to read the README file.
# Used for the long_description.  It's nice, because now 1) we have a top level
# README file and 2) it's easier to type in the README file than to put a raw
# string in below ...
def read(fname):
    return open(pjoin(dirname(__file__), fname)).read()

class BinaryDistribution(Distribution):
    def is_pure(self):
        return False
    def has_ext_modules(self):
        return True

from setuptools.command.install import install
class InstallPlatlib(install):
    def finalize_options(self):
        install.finalize_options(self)
        self.install_lib = self.install_platlib

setup(
    name='ORTOOLS_PYTHON_VERSION',
    version='VVVV',
    packages=[
        'ortools',
        'ortools.algorithms',
        'ortools.constraint_solver',
        'ortools.data',
        'ortools.graph',
        'ortools.linear_solver',
        'ortools.sat',
        'ortools.sat.python',
        'ortools.util',
    ],
    install_requires=[
        'protobuf >= PROTOBUF_TAG',
        'six >= 1.10',
    ],
    package_data={
        'ortools.constraint_solver' : ['_pywrapcp.dll', '*.pyi'],
        'ortools.data' : ['_pywraprcpsp.dll', '*.pyi'],
        'ortools.linear_solver' : ['_pywraplp.dll', '*.pyi'],
        'ortools.graph' : ['_pywrapgraph.dll'],
        'ortools.algorithms' : ['_pywrapknapsack_solver.dll'],
        'ortools.sat' : ['_pywrapsat.dll', '*.md', '*.pyi'],
        'ortools.util' : ['_sorted_interval_list.dll', '*.pyi'],
        DELETEWIN 'ortools' : ['.libs/*' DDDD]
    },
    include_package_data=True,
    license='Apache 2.0',
    author='Google Inc',
    author_email='lperron@google.com',
    description='Google OR-Tools python libraries and modules',
    keywords=('operations research, constraint programming, ' +
              'linear programming,' + 'flow algorithms,' +
              'python'),
    url='https://developers.google.com/optimization/',
    download_url='https://github.com/google/or-tools/releases',
    classifiers=[
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
        'Programming Language :: Python :: 3.7',
        'Topic :: Office/Business :: Scheduling',
        'Topic :: Scientific/Engineering',
        'Topic :: Scientific/Engineering :: Mathematics',
        'Topic :: Software Development :: Libraries :: Python Modules'],
    distclass=BinaryDistribution,
    cmdclass={'install': InstallPlatlib},
    long_description=read('README.txt'),
)
