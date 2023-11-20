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

import unittest
from ortools.math_opt.examples import log_scraping
from ortools.math_opt.testing import binary_testing


class LinearProgrammingTest(
    binary_testing.BinaryAssertions,
    log_scraping.LogScraping,
    unittest.TestCase,
):
    def test_linear_programming_example(self) -> None:
        result = self.assert_binary_succeeds(
            "ortools/math_opt/examples/python/linear_programming"
        )
        objective_value = self.assert_has_line_with_prefixed_number(
            "Objective value:", result.stdout
        )
        self.assertAlmostEqual(objective_value, 733 + 1 / 3, delta=1e-2)


if __name__ == "__main__":
    unittest.main()
