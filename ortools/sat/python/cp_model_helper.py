# Copyright 2010-2021 Google LLC
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

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import numbers

INT_MIN = -9223372036854775808  # hardcoded to be platform independent.
INT_MAX = 9223372036854775807
INT32_MIN = -2147483648
INT32_MAX = 2147483647


def AssertIsInt64(x):
    """Asserts that x is integer and x is in [min_int_64, max_int_64]."""
    if not isinstance(x, numbers.Integral):
        raise TypeError('Not an integer: %s' % x)
    if x < INT_MIN or x > INT_MAX:
        raise OverflowError('Does not fit in an int64_t: %s' % x)


def AssertIsInt32(x):
    """Asserts that x is integer and x is in [min_int_32, max_int_32]."""
    if not isinstance(x, numbers.Integral):
        raise TypeError('Not an integer: %s' % x)
    if x < INT32_MIN or x > INT32_MAX:
        raise OverflowError('Does not fit in an int32_t: %s' % x)


def AssertIsBoolean(x):
    """Asserts that x is 0 or 1."""
    if not isinstance(x, numbers.Integral) or x < 0 or x > 1:
        raise TypeError('Not an boolean: %s' % x)


def CapInt64(v):
    """Restrict v within [INT_MIN..INT_MAX] range."""
    if v > INT_MAX:
        return INT_MAX
    if v < INT_MIN:
        return INT_MIN
    return v


def CapSub(x, y):
    """Saturated arithmetics. Returns x - y truncated to the int64_t range."""
    AssertIsInt64(x)
    AssertIsInt64(y)
    if y == 0:
        return x
    if x == y:
        if x == INT_MAX or x == INT_MIN:
            raise OverflowError(
                'Integer NaN: subtracting INT_MAX or INT_MIN to itself')
        return 0
    if x == INT_MAX or x == INT_MIN:
        return x
    if y == INT_MAX:
        return INT_MIN
    if y == INT_MIN:
        return INT_MAX
    return CapInt64(x - y)
