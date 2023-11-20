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

_BIN_PATH = "ortools/math_opt/examples/python/time_indexed_scheduling"


class TimeIndexedSchedulingTest(
    binary_testing.BinaryAssertions,
    log_scraping.LogScraping,
    unittest.TestCase,
):
    def test_simple_jobs_schedule(self) -> None:
        output = self.assert_binary_succeeds(_BIN_PATH, ("--use_test_data",))
        sum_completion_time = self.assert_has_line_with_prefixed_number(
            "Sum of completion times:", output.stdout
        )
        self.assertEqual(sum_completion_time, 26)

    def test_random_jobs_schedule(self) -> None:
        output = self.assert_binary_succeeds(_BIN_PATH, ("--num_jobs=10",))
        sum_completion_time = self.assert_has_line_with_prefixed_number(
            "Sum of completion times:", output.stdout
        )
        self.assertGreaterEqual(sum_completion_time, 10)


if __name__ == "__main__":
    unittest.main()
