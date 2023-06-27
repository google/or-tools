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

"""helpers methods for the cp_model module."""

import numbers
import numpy as np

INT_MIN = -9223372036854775808  # hardcoded to be platform independent.
INT_MAX = 9223372036854775807
INT32_MIN = -2147483648
INT32_MAX = 2147483647


def is_integral(x):
    """Checks if x has either a number.Integral or a np.integer type."""
    return isinstance(x, numbers.Integral) or isinstance(x, np.integer)


def is_a_number(x):
    """Checks if x has either a number.Number or a np.double type."""
    return (
        isinstance(x, numbers.Number)
        or isinstance(x, np.double)
        or isinstance(x, np.integer)
    )


def is_zero(x):
    """Checks if the x is 0 or 0.0."""
    return (is_integral(x) and int(x) == 0) or (is_a_number(x) and float(x) == 0.0)


def is_one(x):
    """Checks if x is 1 or 1.0."""
    return (is_integral(x) and int(x) == 1) or (is_a_number(x) and float(x) == 1.0)


def is_minus_one(x):
    """Checks if x is -1 or -1.0."""
    return (is_integral(x) and int(x) == -1) or (is_a_number(x) and float(x) == -1.0)


def assert_is_int64(x):
    """Asserts that x is integer and x is in [min_int_64, max_int_64]."""
    if not is_integral(x):
        raise TypeError("Not an integer: %s" % x)
    if x < INT_MIN or x > INT_MAX:
        raise OverflowError("Does not fit in an int64_t: %s" % x)
    return int(x)


def assert_is_int32(x):
    """Asserts that x is integer and x is in [min_int_32, max_int_32]."""
    if not is_integral(x):
        raise TypeError("Not an integer: %s" % x)
    if x < INT32_MIN or x > INT32_MAX:
        raise OverflowError("Does not fit in an int32_t: %s" % x)
    return int(x)


def assert_is_boolean(x):
    """Asserts that x is 0 or 1."""
    if not is_integral(x) or x < 0 or x > 1:
        raise TypeError("Not a boolean: %s" % x)


def assert_is_a_number(x):
    """Asserts that x is a number and returns it."""
    if not is_a_number(x):
        raise TypeError("Not a number: %s" % x)
    elif is_integral(x):
        return int(x)
    else:
        return float(x)


def to_capped_int64(v):
    """Restrict v within [INT_MIN..INT_MAX] range."""
    if v > INT_MAX:
        return INT_MAX
    if v < INT_MIN:
        return INT_MIN
    return v


def capped_subtraction(x, y):
    """Saturated arithmetics. Returns x - y truncated to the int64_t range."""
    assert_is_int64(x)
    assert_is_int64(y)
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
