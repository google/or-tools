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

"""Test sat parameters builder."""

from absl.testing import absltest
from ortools.sat.python import sat_parameters_builder


class SatParametersBuilderTest(absltest.TestCase):

    def test_basic_api(self) -> None:
        params = sat_parameters_builder.SatParameters()

        # Test that we can set and get an integer parameter.
        params.num_workers = 10
        self.assertEqual(params.num_workers, 10)

        # Test that we can set and get an enum parameter.
        self.assertEqual(
            params.clause_cleanup_ordering,
            sat_parameters_builder.SatParameters.ClauseOrdering.CLAUSE_ACTIVITY,
        )
        params.clause_cleanup_ordering = (
            sat_parameters_builder.SatParameters.ClauseOrdering.CLAUSE_LBD
        )
        self.assertEqual(
            params.clause_cleanup_ordering,
            sat_parameters_builder.SatParameters.ClauseOrdering.CLAUSE_LBD,
        )

        # Test that we can set and get a repeated string parameter.
        params.subsolvers.append("no_lp")
        self.assertLen(params.subsolvers, 1)
        self.assertEqual(params.subsolvers[0], "no_lp")


if __name__ == "__main__":
    absltest.main()
