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

# pylint: disable=line-too-long
from absl.testing import absltest, parameterized
from python.runfiles import Runfiles

from ortools.copts import generate_copts

get_file_content = generate_copts.get_file_content
get_file_name = generate_copts.get_file_name
BuildSystem = generate_copts.BuildSystem


class GenerateCoptsTest(parameterized.TestCase):

    def setUp(self):
        super().setUp()
        self.runfiles = Runfiles.Create()

    @parameterized.parameters(BuildSystem.BAZEL, BuildSystem.CMAKE)
    def test_valid_content(self, build_system: BuildSystem):
        file_name = get_file_name(build_system)
        expected_content = get_file_content(build_system)
        prefix = "or-tools/"
        path = f"{prefix}ortools/copts/{file_name}"
        with open(self.runfiles.Rlocation(path), "r") as f:
            actual_content = f.read()
        self.assertEqual(actual_content, expected_content)


if __name__ == "__main__":
    absltest.main()
