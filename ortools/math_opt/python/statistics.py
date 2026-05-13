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

"""Statistics about MIP/LP models."""

import dataclasses
import io
import math
from typing import Iterable, Optional

from ortools.math_opt.python import model


@dataclasses.dataclass(frozen=True)
class Range:
    """A close range of values [min, max].

    Attributes:
      minimum: The minimum value.
      maximum: The maximum value.
    """

    minimum: float
    maximum: float


def merge_optional_ranges(
    lhs: Optional[Range], rhs: Optional[Range]
) -> Optional[Range]:
    """Merges the two optional ranges.

    Args:
      lhs: The left hand side range.
      rhs: The right hand side range.

    Returns:
      A merged range (None if both lhs and rhs are None).
    """
    if lhs is None:
        return rhs
    if rhs is None:
        return lhs
    return Range(
        minimum=min(lhs.minimum, rhs.minimum),
        maximum=max(lhs.maximum, rhs.maximum),
    )


def absolute_finite_non_zeros_range(values: Iterable[float]) -> Optional[Range]:
    """Returns the range of the absolute values of the finite non-zeros.

    Args:
      values: An iterable object of float values.

    Returns:
      The range of the absolute values of the finite non-zeros, None if no such
      value is found.
    """
    minimum: Optional[float] = None
    maximum: Optional[float] = None
    for v in values:
        v = abs(v)
        if math.isinf(v) or v == 0.0:
            continue
        if minimum is None:
            minimum = v
            maximum = v
        else:
            minimum = min(minimum, v)
            maximum = max(maximum, v)

    assert (maximum is None) == (minimum is None), (minimum, maximum)

    if minimum is None:
        return None
    return Range(minimum=minimum, maximum=maximum)


@dataclasses.dataclass(frozen=True)
class ModelRanges:
    """The ranges of the absolute values of the finite non-zero values in the model.

    Each range is optional since there may be no finite non-zero values
    (e.g. empty model, empty objective, all variables unbounded, ...).

    Attributes:
      objective_terms: The linear and quadratic objective terms (not including the
        offset).
      variable_bounds: The variables' lower and upper bounds.
      linear_constraint_bounds: The linear constraints' lower and upper bounds.
      linear_constraint_coefficients: The coefficients of the variables in linear
        constraints.
    """

    objective_terms: Optional[Range]
    variable_bounds: Optional[Range]
    linear_constraint_bounds: Optional[Range]
    linear_constraint_coefficients: Optional[Range]

    def __str__(self) -> str:
        """Prints the ranges in scientific format with 2 digits (i.e.

        f'{x:.2e}').

        It returns a multi-line table list of ranges. The last line does NOT end
        with a new line.

        Returns:
          The ranges in multiline string.
        """
        buf = io.StringIO()

        def print_range(prefix: str, value: Optional[Range]) -> None:
            buf.write(prefix)
            if value is None:
                buf.write("no finite values")
                return
            # Numbers are printed in scientific notation with a precision of 2. Since
            # they are expected to be positive we can ignore the optional leading
            # minus sign. We thus expects `d.dde[+-]dd(d)?` (the exponent is at least
            # 2 digits but double can require 3 digits, with max +308 and min
            # -308). Thus we can use a width of 9 to align the ranges properly.
            buf.write(f"[{value.minimum:<9.2e}, {value.maximum:<9.2e}]")

        print_range("Objective terms           : ", self.objective_terms)
        print_range("\nVariable bounds           : ", self.variable_bounds)
        print_range("\nLinear constraints bounds : ", self.linear_constraint_bounds)
        print_range(
            "\nLinear constraints coeffs : ", self.linear_constraint_coefficients
        )
        return buf.getvalue()


def compute_model_ranges(mdl: model.Model) -> ModelRanges:
    """Returns the ranges of the finite non-zero values in the given model.

    Args:
      mdl: The input model.

    Returns:
      The ranges of the finite non-zero values in the model.
    """
    return ModelRanges(
        objective_terms=absolute_finite_non_zeros_range(
            term.coefficient for term in mdl.objective.linear_terms()
        ),
        variable_bounds=merge_optional_ranges(
            absolute_finite_non_zeros_range(v.lower_bound for v in mdl.variables()),
            absolute_finite_non_zeros_range(v.upper_bound for v in mdl.variables()),
        ),
        linear_constraint_bounds=merge_optional_ranges(
            absolute_finite_non_zeros_range(
                c.lower_bound for c in mdl.linear_constraints()
            ),
            absolute_finite_non_zeros_range(
                c.upper_bound for c in mdl.linear_constraints()
            ),
        ),
        linear_constraint_coefficients=absolute_finite_non_zeros_range(
            e.coefficient for e in mdl.linear_constraint_matrix_entries()
        ),
    )
