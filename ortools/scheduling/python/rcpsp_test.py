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

"""Tests for ortools.scheduling.python.rcpsp."""

from absl import app
from absl import flags
from absl.testing import absltest
from ortools.scheduling.python import rcpsp

FLAGS = flags.FLAGS


class RcpspTest(absltest.TestCase):

    def testParseAndAccess(self):
        parser = rcpsp.RcpspParser()
        data = "ortools/scheduling/testdata/j301_1.sm"
        try:
            filename = f"{FLAGS.test_srcdir}/_main/{data}"
        except flags._exceptions.UnparsedFlagAccessError:
            filename = f"../../../{data}"
        self.assertTrue(parser.parse_file(filename))
        problem = parser.problem()
        self.assertLen(problem.resources, 4)
        self.assertLen(problem.tasks, 32)


def main(unused_argv):
    absltest.main()


if __name__ == "__main__":
    app.run(main)
