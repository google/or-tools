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

_TSP_PATH = "ortools/math_opt/examples/python/tsp"


class TspTest(
    binary_testing.BinaryAssertions,
    log_scraping.LogScraping,
    unittest.TestCase,
):
    def test_tsp_simple(self) -> None:
        output = self.assert_binary_succeeds(
            _TSP_PATH, ("--test_instance", "--solve_logs")
        )
        lazy = self.assert_has_line_with_prefixed_number(
            "  Lazy constraints: ", output.stdout
        )
        self.assertEqual(lazy, 1)
        route_length = self.assert_has_line_with_prefixed_number(
            "Route length: ", output.stdout
        )
        self.assertAlmostEqual(route_length, 2.2)

    def test_tsp_large_no_crash(self) -> None:
        output = self.assert_binary_succeeds(_TSP_PATH)
        self.assertIn("Route length:", output.stdout)


if __name__ == "__main__":
    unittest.main()
