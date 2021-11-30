#!/usr/bin/env python3
# Copyright 2010-2021 Google LLC
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
"""Simple unit tests for python/init.swig. Not exhaustive."""

import unittest
from ortools.init import pywrapinit

class PyWrapInit(unittest.TestCase):

    def test_logging(self):
        print('test_logging')
        pywrapinit.CppBridge.InitLogging('pywrapinit_test.py')
        pywrapinit.CppBridge.ShutdownLogging()

    def test_flags(self):
        print('test_cpp_flags')
        cpp_flags = pywrapinit.CppFlags()
        cpp_flags.logtostderr = True
        cpp_flags.log_prefix = True
        cpp_flags.cp_model_dump_prefix = 'pywrapinit_test'
        cpp_flags.cp_model_dump_model = True
        cpp_flags.cp_model_dump_lns = True
        cpp_flags.cp_model_dump_response = True
        pywrapinit.CppBridge.SetFlags(cpp_flags)

    def test_version(self):
        print('test_version')
        major = pywrapinit.OrToolsVersion.MajorNumber()
        self.assertIsInstance(major, int)
        minor = pywrapinit.OrToolsVersion.MinorNumber()
        self.assertIsInstance(minor, int)
        patch = pywrapinit.OrToolsVersion.PatchNumber()
        self.assertIsInstance(patch, int)
        version = pywrapinit.OrToolsVersion.VersionString()
        self.assertIsInstance(version, str)
        string = f'{major}.{minor}.{patch}'
        self.assertEqual(version, string)

if __name__ == '__main__':
    unittest.main()
