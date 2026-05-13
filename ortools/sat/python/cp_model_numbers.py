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

"""helpers methods for the cp_model module."""

import numbers
from typing import Any
import numpy as np


INT_MIN = -9223372036854775808  # hardcoded to be platform independent.
INT_MAX = 9223372036854775807


def is_boolean(x: Any) -> bool:
    """Checks if the x is a boolean."""
    if isinstance(x, bool):
        return True
    if isinstance(x, np.bool_):
        return True
    return False


def assert_is_zero_or_one(x: Any) -> int:
    """Asserts that x is 0 or 1 and returns it as an int."""
    if not isinstance(x, numbers.Integral):
        raise TypeError(f"Not a boolean: {x} of type {type(x)}")
    x_as_int = int(x)
    if x_as_int < 0 or x_as_int > 1:
        raise TypeError(f"Not a boolean: {x}")
    return x_as_int


def to_capped_int64(v: int) -> int:
    """Restrict v within [INT_MIN..INT_MAX] range."""
    if v > INT_MAX:
        return INT_MAX
    if v < INT_MIN:
        return INT_MIN
    return v


def capped_subtraction(x: int, y: int) -> int:
    """Saturated arithmetics. Returns x - y truncated to the int64_t range."""
    if y == 0:
        return x
    if x == y:
        if x == INT_MAX or x == INT_MIN:
            raise OverflowError("Integer NaN: subtracting INT_MAX or INT_MIN to itself")
        return 0
    if x == INT_MAX or x == INT_MIN:
        return x
    if y == INT_MAX:
        return INT_MIN
    if y == INT_MIN:
        return INT_MAX
    return to_capped_int64(x - y)
