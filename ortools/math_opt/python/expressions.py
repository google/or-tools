# Copyright 2010-2024 Google LLC
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

"""Utilities for working with linear and quadratic expressions."""

import typing
from typing import Iterable, Mapping, Union

from ortools.math_opt.python import model


@typing.overload
def fast_sum(summands: Iterable[model.LinearTypes]) -> model.LinearSum: ...


@typing.overload
def fast_sum(
    summands: Iterable[model.QuadraticTypes],
) -> Union[model.LinearSum, model.QuadraticSum]: ...


# TODO(b/312200030): There is a pytype bug so that for the code:
#   m = mathopt.Model()
#   x = m.Variable()
#   s = expressions.fast_sum([x*x, 4.0])
# pytype picks the wrong overload and thinks s has type model.LinearSum, rather
# than Union[model.LinearSum, model.QuadraticSum]. Once the bug is fixed,
# confirm that the overloads actually work.
def fast_sum(summands):
    """Sums the elements of summand into a linear or quadratic expression.

    Similar to Python's sum function, but faster for input that not just integers
    and floats.

    Unlike sum(), the function returns a linear expression when all inputs are
    floats and/or integers. Importantly, the code:
      model.add_linear_constraint(fast_sum(maybe_empty_list) <= 1.0)
    is safe to call, while:
      model.add_linear_constraint(sum(maybe_empty_list) <= 1.0)
    fails at runtime when the list is empty.

    Args:
      summands: The elements to add up.

    Returns:
      A linear or quadratic expression with the sum of the elements of summand.
    """
    summands_tuple = tuple(summands)
    for s in summands_tuple:
        if isinstance(s, model.QuadraticBase):
            return model.QuadraticSum(summands_tuple)
    return model.LinearSum(summands_tuple)


def evaluate_expression(
    expression: model.QuadraticTypes,
    variable_values: Mapping[model.Variable, float],
) -> float:
    """Evaluates a linear or quadratic expression for given variable values.

    E.g. if expression  = 3 * x + 4 and variable_values = {x: 2.0}, then
    evaluate_expression(expression, variable_values) equals 10.0.

    Args:
      expression: The expression to evaluate.
      variable_values: Must contain a value for every variable in expression.

    Returns:
      The value of the expression when replacing variables by their value.
    """
    if isinstance(expression, model.QuadraticBase):
        return model.as_flat_quadratic_expression(expression).evaluate(variable_values)
    return model.as_flat_linear_expression(expression).evaluate(variable_values)
