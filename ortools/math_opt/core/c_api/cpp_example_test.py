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

"""Tests that cpp_example.cc solves a small mip and prints out the answer.

This is done by running the program in a subprocess and then asserting against
what was printed to standard out.
"""

from absl.testing import absltest
from ortools.math_opt.examples import log_scraping
from ortools.math_opt.testing import binary_testing


class CppExampleTest(
    binary_testing.BinaryAssertions,
    log_scraping.LogScraping,
    absltest.TestCase,
):

    def test_regression(self):
        result = self.assert_binary_succeeds("ortools/math_opt/core/c_api/cpp_example")
        is_optimal = self.assert_has_line_with_prefixed_number(
            "Termination is optimal: ", result.stdout
        )
        self.assertEqual(is_optimal, 1)
        objective_value = self.assert_has_line_with_prefixed_number(
            "Objective value: ", result.stdout
        )
        self.assertAlmostEqual(objective_value, 1.0)


if __name__ == "__main__":
    absltest.main()
