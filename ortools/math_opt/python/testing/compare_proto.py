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

"""Assertions to compare protocol buffers.

Two comparisons are provided:

1. assert_protos_equal(): Checks that two messages are exactly the same (roughly
speaking, they should produce the same binary representation).
2. assert_protos_equiv(): Checks that two messages are equivalent in the MathOpt
sense (roughly speaking, empty submessages being set/unset do not matter,
details below).

(2) is reduced to (1) by recursively clearing empty messages via normalize.py.
To implement (1),
in open source we check that the text proto representations are the same (this
is a slightly weaker test).

The exact contract for assert_protos_equiv() is as follows.
Empty sub-messages (recursively) have no effect on equivalence, except for the
Duration message, otherwise the same as proto equality. Oneof messages have
their fields recursively cleared, but the oneof itself is not, to preserve the
selection. This is similar to C++ EquivToProto, but this function cares about:
  * field presence for optional scalar fields,
  * field presence for Duration messages (e.g. Duration unset of time limit
    means +inf),
  * field presence for one_ofs,
and C++ EquivToProto does not.

See normalize.py for details.
"""

import copy
import unittest

from google.protobuf import message

from ortools.math_opt.python import normalize


def assert_protos_equal(
    test: unittest.TestCase,
    actual: message.Message,
    expected: message.Message,
) -> None:
    """Asserts the input protos are equal, see module doc for details."""
    test.assertEqual(str(actual), str(expected))


def assert_protos_equiv(
    test: unittest.TestCase,
    actual: message.Message,
    expected: message.Message,
) -> None:
    """Asserts the input protos are equivalent, see module doc for details."""
    normalized_actual = copy.deepcopy(actual)
    normalize.math_opt_normalize_proto(normalized_actual)
    normalized_expected = copy.deepcopy(expected)
    normalize.math_opt_normalize_proto(normalized_expected)
    test.assertEqual(str(normalized_actual), str(normalized_expected))


class MathOptProtoAssertions(unittest.TestCase):
    """Provides a custom MathOpt proto equivalence assertion for tests."""

    def assert_protos_equal(
        self,
        actual: message.Message,
        expected: message.Message,
    ) -> None:
        """Asserts the input protos are equal, see module doc for details."""
        return assert_protos_equal(self, actual, expected)

    def assert_protos_equiv(
        self,
        actual: message.Message,
        expected: message.Message,
    ) -> None:
        """Asserts the input protos are equivalent, see module doc for details."""
        return assert_protos_equiv(self, actual, expected)
