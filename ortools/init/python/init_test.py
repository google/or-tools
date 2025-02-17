#!/usr/bin/env python3
# Copyright 2010-2025 Google LLC
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

from absl.testing import absltest
from ortools.init.python import init


class InitTest(absltest.TestCase):

    def test_logging(self):
        print("test_logging")
        init.CppBridge.init_logging("pywrapinit_test.py")
        init.CppBridge.shutdown_logging()

    def test_flags(self):
        print("test_cpp_flags")
        cpp_flags = init.CppFlags()
        assert hasattr(cpp_flags, "stderrthreshold")
        assert hasattr(cpp_flags, "log_prefix")
        assert hasattr(cpp_flags, "cp_model_dump_prefix")
        assert hasattr(cpp_flags, "cp_model_dump_models")
        assert hasattr(cpp_flags, "cp_model_dump_submodels")
        assert hasattr(cpp_flags, "cp_model_dump_response")
        init.CppBridge.set_flags(cpp_flags)

    def test_version(self):
        print("test_version")
        major = init.OrToolsVersion.major_number()
        self.assertIsInstance(major, int)
        minor = init.OrToolsVersion.minor_number()
        self.assertIsInstance(minor, int)
        patch = init.OrToolsVersion.patch_number()
        self.assertIsInstance(patch, int)
        version = init.OrToolsVersion.version_string()
        self.assertIsInstance(version, str)
        string = f"{major}.{minor}.{patch}"
        self.assertEqual(version, string)


if __name__ == "__main__":
    absltest.main()
