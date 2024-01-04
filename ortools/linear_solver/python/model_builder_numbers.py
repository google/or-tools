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

"""helpers methods for the cp_model_builder module on numbers."""

import numbers
from typing import Any, Sequence, Union
import numpy as np
import numpy.typing as npt

# Custom types.
NumberT = Union[numbers.Number, np.number]


def is_integral(x: Any) -> bool:
    """Checks if x has either a number.Integral or a np.integer type."""
    return isinstance(x, numbers.Integral) or isinstance(x, np.integer)


def is_a_number(x: Any) -> bool:
    """Checks if x has either a number.Number or a np.double type."""
    return (
        isinstance(x, numbers.Number)
        or isinstance(x, np.double)
        or isinstance(x, np.integer)
    )


def is_zero(x: Any) -> bool:
    """Checks if the x is 0 or 0.0."""
    return (is_integral(x) and int(x) == 0) or (is_a_number(x) and float(x) == 0.0)


def is_one(x: Any) -> bool:
    """Checks if x is 1 or 1.0."""
    return (is_integral(x) and int(x) == 1) or (is_a_number(x) and float(x) == 1.0)


def is_minus_one(x: Any) -> bool:
    """Checks if x is -1 or -1.0."""
    return (is_integral(x) and int(x) == -1) or (is_a_number(x) and float(x) == -1.0)


def assert_is_a_number(x: NumberT) -> np.double:
    """Asserts that x is a number and converts to a np.double."""
    if not is_a_number(x):
        raise TypeError("Not a number: %s" % x)
    return np.double(x)


def assert_is_a_number_array(x: Sequence[NumberT]) -> npt.NDArray[np.double]:
    """Asserts x is a list of numbers and converts it to np.array(np.double)."""
    result = np.empty(len(x), dtype=np.double)
    pos = 0
    for c in x:
        result[pos] = assert_is_a_number(c)
        pos += 1
    assert pos == len(x)
    return result
