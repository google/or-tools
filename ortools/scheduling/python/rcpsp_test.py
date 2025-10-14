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

import os
from absl.testing import absltest
from ortools.scheduling.python import rcpsp


class RcpspTest(absltest.TestCase):

    def testParseAndAccess(self):
        test_srcdir = os.environ.get("TEST_SRCDIR")
        data = "ortools/scheduling/testdata/j301_1.sm"
        if test_srcdir:
            filename = f"{test_srcdir}/_main/{data}"
        else:
            filename = f"../../../{data}"
        parser = rcpsp.RcpspParser()
        self.assertTrue(parser.parse_file(filename))
        problem = parser.problem()
        self.assertLen(problem.resources, 4)
        self.assertLen(problem.tasks, 32)


if __name__ == "__main__":
    absltest.main()
