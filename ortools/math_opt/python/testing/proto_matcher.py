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

"""A matcher that tests if protos meet MathOpts definition of equivalence.

This is designed to be used with unittest.mock, which is canonical for mocking
in Google Python (e.g., see stubby codelabs).

See normalize.py for technical definitions.

The matcher can be used as a replacement for
google3.net.proto2.contrib.pyutil.matcher.Proto2Matcher
but supports a much smaller set of features.
"""

import copy

from google.protobuf import message
from ortools.math_opt.python import normalize


class MathOptProtoEquivMatcher:
    """Matcher that checks if protos are equivalent in the MathOpt sense.

    See normalize.py for technical definitions.
    """

    def __init__(self, expected: message.Message):
        self._expected = copy.deepcopy(expected)
        normalize.math_opt_normalize_proto(self._expected)

    def __eq__(self, actual: message.Message) -> bool:
        actual = copy.deepcopy(actual)
        normalize.math_opt_normalize_proto(actual)
        return str(actual) == str(self._expected)

    def __ne__(self, other: message.Message) -> bool:
        return not self == other
