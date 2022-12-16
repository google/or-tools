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

import math

from absl.testing import absltest
from ortools.math_opt.python import model
from ortools.math_opt.python import statistics


class RangeTest(absltest.TestCase):

    def test_merge_optional_ranges(self) -> None:
        self.assertIsNone(statistics.merge_optional_ranges(None, None))
        r = statistics.Range(1.0, 3.0)
        self.assertEqual(statistics.merge_optional_ranges(r, None), r)
        self.assertEqual(statistics.merge_optional_ranges(None, r), r)
        # We also test that, since Range is a frozen class, we return the non-None
        # input when only one input is not None.
        self.assertIs(statistics.merge_optional_ranges(r, None), r)
        self.assertIs(statistics.merge_optional_ranges(None, r), r)
        self.assertEqual(
            statistics.merge_optional_ranges(
                statistics.Range(1.0, 3.0), statistics.Range(-2.0, 2.0)
            ),
            statistics.Range(-2.0, 3.0),
        )

    def test_absolute_finite_non_zeros_range(self) -> None:
        self.assertIsNone(statistics.absolute_finite_non_zeros_range(()))
        self.assertIsNone(
            statistics.absolute_finite_non_zeros_range((math.inf, 0.0, -0.0, -math.inf))
        )
        self.assertEqual(
            statistics.absolute_finite_non_zeros_range(
                (math.inf, -5.0e2, 0.0, 1.5e-3, -0.0, -math.inf, 1.25e-6, 3.0e2)
            ),
            statistics.Range(minimum=1.25e-6, maximum=5.0e2),
        )


class ModelRangesTest(absltest.TestCase):

    def test_printing(self) -> None:
        self.assertMultiLineEqual(
            str(
                statistics.ModelRanges(
                    objective_terms=None,
                    variable_bounds=None,
                    linear_constraint_bounds=None,
                    linear_constraint_coefficients=None,
                )
            ),
            "Objective terms           : no finite values\n"
            "Variable bounds           : no finite values\n"
            "Linear constraints bounds : no finite values\n"
            "Linear constraints coeffs : no finite values",
        )

        self.assertMultiLineEqual(
            str(
                statistics.ModelRanges(
                    objective_terms=statistics.Range(2.12345e-99, 1.12345e3),
                    variable_bounds=statistics.Range(9.12345e-2, 1.12345e2),
                    linear_constraint_bounds=statistics.Range(2.12345e6, 5.12345e99),
                    linear_constraint_coefficients=statistics.Range(0.0, 0.0),
                )
            ),
            "Objective terms           : [2.12e-99 , 1.12e+03 ]\n"
            "Variable bounds           : [9.12e-02 , 1.12e+02 ]\n"
            "Linear constraints bounds : [2.12e+06 , 5.12e+99 ]\n"
            "Linear constraints coeffs : [0.00e+00 , 0.00e+00 ]",
        )

        self.assertMultiLineEqual(
            str(
                statistics.ModelRanges(
                    objective_terms=statistics.Range(2.12345e-1, 1.12345e3),
                    variable_bounds=statistics.Range(9.12345e-2, 1.12345e2),
                    linear_constraint_bounds=statistics.Range(2.12345e6, 5.12345e99),
                    linear_constraint_coefficients=statistics.Range(0.0, 1.0e100),
                )
            ),
            "Objective terms           : [2.12e-01 , 1.12e+03 ]\n"
            "Variable bounds           : [9.12e-02 , 1.12e+02 ]\n"
            "Linear constraints bounds : [2.12e+06 , 5.12e+99 ]\n"
            "Linear constraints coeffs : [0.00e+00 , 1.00e+100]",
        )

        self.assertMultiLineEqual(
            str(
                statistics.ModelRanges(
                    objective_terms=statistics.Range(2.12345e-100, 1.12345e3),
                    variable_bounds=statistics.Range(9.12345e-2, 1.12345e2),
                    linear_constraint_bounds=statistics.Range(2.12345e6, 5.12345e99),
                    linear_constraint_coefficients=statistics.Range(0.0, 0.0),
                )
            ),
            "Objective terms           : [2.12e-100, 1.12e+03 ]\n"
            "Variable bounds           : [9.12e-02 , 1.12e+02 ]\n"
            "Linear constraints bounds : [2.12e+06 , 5.12e+99 ]\n"
            "Linear constraints coeffs : [0.00e+00 , 0.00e+00 ]",
        )

        self.assertMultiLineEqual(
            str(
                statistics.ModelRanges(
                    objective_terms=statistics.Range(2.12345e-100, 1.12345e3),
                    variable_bounds=statistics.Range(9.12345e-2, 1.12345e2),
                    linear_constraint_bounds=statistics.Range(2.12345e6, 5.12345e99),
                    linear_constraint_coefficients=statistics.Range(0.0, 1.0e100),
                )
            ),
            "Objective terms           : [2.12e-100, 1.12e+03 ]\n"
            "Variable bounds           : [9.12e-02 , 1.12e+02 ]\n"
            "Linear constraints bounds : [2.12e+06 , 5.12e+99 ]\n"
            "Linear constraints coeffs : [0.00e+00 , 1.00e+100]",
        )


class ComputeModelRangesTest(absltest.TestCase):

    def test_empty(self) -> None:
        mdl = model.Model(name="model")
        self.assertEqual(
            statistics.compute_model_ranges(mdl),
            statistics.ModelRanges(
                objective_terms=None,
                variable_bounds=None,
                linear_constraint_bounds=None,
                linear_constraint_coefficients=None,
            ),
        )

    def test_only_zero_and_infinite_values(self) -> None:
        mdl = model.Model(name="model")
        mdl.add_variable(lb=0.0, ub=math.inf)
        mdl.add_variable(lb=-math.inf, ub=0.0)
        mdl.add_variable(lb=-math.inf, ub=math.inf)
        mdl.add_linear_constraint(lb=0.0, ub=math.inf)
        mdl.add_linear_constraint(lb=-math.inf, ub=0.0)
        mdl.add_linear_constraint(lb=-math.inf, ub=math.inf)

        self.assertEqual(
            statistics.compute_model_ranges(mdl),
            statistics.ModelRanges(
                objective_terms=None,
                variable_bounds=None,
                linear_constraint_bounds=None,
                linear_constraint_coefficients=None,
            ),
        )

    def test_mixed_values(self) -> None:
        mdl = model.Model(name="model")
        x = mdl.add_variable(lb=0.0, ub=0.0, name="x")
        y = mdl.add_variable(lb=-math.inf, ub=1e-3, name="y")
        mdl.add_variable(lb=-3e2, ub=math.inf, name="z")

        mdl.objective.is_maximize = False
        mdl.objective.set_linear_coefficient(x, -5.0e4)
        # TODO(b/225219234): add the quadratic term `1.0e-6 * z * x` when the
        # support of quadratic objective is added to the Python API.
        mdl.objective.set_linear_coefficient(y, 3.0)

        c = mdl.add_linear_constraint(lb=0.0, name="c")
        c.set_coefficient(y, 1.25e-3)
        c.set_coefficient(x, -4.5e3)
        mdl.add_linear_constraint(lb=-math.inf, ub=3e4)
        d = mdl.add_linear_constraint(lb=-1e-5, ub=0.0, name="d")
        d.set_coefficient(y, 2.5e-3)

        self.assertEqual(
            statistics.compute_model_ranges(mdl),
            statistics.ModelRanges(
                # TODO(b/225219234): update this to Range(1.0e-6, 5.0e4) once the
                # quadratic term is added.
                objective_terms=statistics.Range(3.0, 5.0e4),
                variable_bounds=statistics.Range(1e-3, 3e2),
                linear_constraint_bounds=statistics.Range(1e-5, 3e4),
                linear_constraint_coefficients=statistics.Range(1.25e-3, 4.5e3),
            ),
        )


if __name__ == "__main__":
    absltest.main()
