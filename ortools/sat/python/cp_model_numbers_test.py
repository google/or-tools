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

import sys

from absl.testing import absltest
import numpy as np

from ortools.sat.python import cp_model_numbers as cmn


class CpModelNumbersTest(absltest.TestCase):

    def tearDown(self) -> None:
        super().tearDown()
        sys.stdout.flush()

    def test_is_boolean(self):
        self.assertTrue(cmn.is_boolean(True))
        self.assertTrue(cmn.is_boolean(False))
        self.assertFalse(cmn.is_boolean(1))
        self.assertFalse(cmn.is_boolean(0))
        self.assertTrue(cmn.is_boolean(np.bool_(1)))
        self.assertTrue(cmn.is_boolean(np.bool_(0)))

    def testto_capped_int64(self):
        self.assertEqual(cmn.to_capped_int64(cmn.INT_MAX), cmn.INT_MAX)
        self.assertEqual(cmn.to_capped_int64(cmn.INT_MAX + 1), cmn.INT_MAX)
        self.assertEqual(cmn.to_capped_int64(cmn.INT_MIN), cmn.INT_MIN)
        self.assertEqual(cmn.to_capped_int64(cmn.INT_MIN - 1), cmn.INT_MIN)
        self.assertEqual(cmn.to_capped_int64(15), 15)

    def testcapped_subtraction(self):
        self.assertEqual(cmn.capped_subtraction(10, 5), 5)
        self.assertEqual(cmn.capped_subtraction(cmn.INT_MIN, 5), cmn.INT_MIN)
        self.assertEqual(cmn.capped_subtraction(cmn.INT_MIN, -5), cmn.INT_MIN)
        self.assertEqual(cmn.capped_subtraction(cmn.INT_MAX, 5), cmn.INT_MAX)
        self.assertEqual(cmn.capped_subtraction(cmn.INT_MAX, -5), cmn.INT_MAX)
        self.assertEqual(cmn.capped_subtraction(2, cmn.INT_MIN), cmn.INT_MAX)
        self.assertEqual(cmn.capped_subtraction(2, cmn.INT_MAX), cmn.INT_MIN)
        self.assertRaises(
            OverflowError, cmn.capped_subtraction, cmn.INT_MAX, cmn.INT_MAX
        )
        self.assertRaises(
            OverflowError, cmn.capped_subtraction, cmn.INT_MIN, cmn.INT_MIN
        )
        self.assertRaises(TypeError, cmn.capped_subtraction, 5, "dummy")
        self.assertRaises(TypeError, cmn.capped_subtraction, "dummy", 5)


if __name__ == "__main__":
    absltest.main()
