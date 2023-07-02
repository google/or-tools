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

"""Simple unit tests for python/init.i. Not exhaustive."""

from ortools.init.python import pywrapinit
from absl.testing import absltest


class PyWrapInit(absltest.TestCase):
    def test_logging(self):
        print("test_logging")
        pywrapinit.CppBridge.init_logging("pywrapinit_test.py")
        pywrapinit.CppBridge.shutdown_logging()

    def test_flags(self):
        print("test_cpp_flags")
        cpp_flags = pywrapinit.CppFlags()
        assert hasattr(cpp_flags, "stderrthreshold")
        assert hasattr(cpp_flags, "log_prefix")
        assert hasattr(cpp_flags, "cp_model_dump_prefix")
        assert hasattr(cpp_flags, "cp_model_dump_models")
        assert hasattr(cpp_flags, "cp_model_dump_lns")
        assert hasattr(cpp_flags, "cp_model_dump_response")
        pywrapinit.CppBridge.set_flags(cpp_flags)

    def test_version(self):
        print("test_version")
        major = pywrapinit.OrToolsVersion.major_number()
        self.assertIsInstance(major, int)
        minor = pywrapinit.OrToolsVersion.minor_number()
        self.assertIsInstance(minor, int)
        patch = pywrapinit.OrToolsVersion.patch_number()
        self.assertIsInstance(patch, int)
        version = pywrapinit.OrToolsVersion.version_string()
        self.assertIsInstance(version, str)
        string = f"{major}.{minor}.{patch}"
        self.assertEqual(version, string)


if __name__ == "__main__":
    absltest.main()
