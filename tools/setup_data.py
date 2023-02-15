#!/usr/bin/env python3
# Copyright 2010-2022 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Setup.py for data package."""
from os import path
import sys

setuptools_import_error_message = """setuptools is not installed for """ + sys.executable + """
Please follow this link for installing instructions :
https://pypi.python.org/pypi/setuptools
make sure you use \"""" + sys.executable + """\" during the installation"""

try:
  from setuptools import setup  # pylint: disable=g-import-not-at-top
except ImportError:
  raise ImportError(setuptools_import_error_message)


# Utility function to read the README file.
# Used for the long_description.  It's nice, because now 1) we have a top level
# README file and 2) it's easier to type in the README file than to put a raw
# string in below ...
def read(fname):
  return open(path.join(path.dirname(__file__), fname)).read()

install_requires = ['ortoolsXXXX == VVVV']

setup(
    name='ortools_examples',
    version='VVVV',
    install_requires=install_requires,
    license='Apache 2.0',
    author='Google LLC',
    author_email='or-tools@google.com',
    description='Google OR-Tools python libraries and modules',
    keywords=('operations research, constraint programming, ' +
              'linear programming,' + 'flow algorithms,' +
              'python'),
    url='https://developers.google.com/optimization/',
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
        'Topic :: Office/Business :: Scheduling',
        'Topic :: Scientific/Engineering',
        'Topic :: Scientific/Engineering :: Mathematics',
        'Topic :: Software Development :: Libraries :: Python Modules'],
    long_description=read('README.txt'),
)
