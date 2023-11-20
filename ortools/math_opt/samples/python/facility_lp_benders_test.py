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
from google3.testing.pybase import parameterized
from ortools.math_opt.testing import binary_testing

_BINARY_NAME = "ortools/math_opt/examples/python/facility_lp_benders"


class FacilityLpBendersTest(binary_testing.BinaryAssertions, parameterized.TestCase):
    @parameterized.named_parameters(
        ("_glop", "glop"), ("_glpk", "glpk"), ("_pdlp", "pdlp")
    )
    def test_facility_lp_benders(self, solver: str) -> None:
        result = self.assert_binary_succeeds(
            _BINARY_NAME, ("--num_facilities", "10", "--solver_type", f"{solver}")
        )
        self.assertIn("Benders solve time:", result.stdout)


if __name__ == "__main__":
    unittest.main()
