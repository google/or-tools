# Copyright 2010-2018 Google LLC
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
"""Tests for ortools.util.sorted_interval_list."""

from __future__ import absolute_import
from __future__ import division
from __future__ import print_function

import unittest
from ortools.util import sorted_interval_list


class SortedIntervalListTest(unittest.TestCase):

    def testCtorAndGetter(self):
        bool_domain = sorted_interval_list.Domain(0, 1)
        self.assertEqual(2, bool_domain.Size())
        self.assertEqual(0, bool_domain.Min())
        self.assertEqual(1, bool_domain.Max())
        self.assertFalse(bool_domain.IsEmpty())
        self.assertEqual(str(bool_domain), '[0,1]')

    def testFromValues(self):
        domain = sorted_interval_list.Domain.FromValues([1, 3, -5, 5])
        self.assertEqual(4, domain.Size())
        self.assertEqual(-5, domain.Min())
        self.assertEqual(5, domain.Max())
        self.assertEqual([-5, -5, 1, 1, 3, 3, 5, 5],
                         domain.FlattenedIntervals())
        self.assertTrue(domain.Contains(1))
        self.assertFalse(domain.Contains(0))

    def testFromIntervals(self):
        domain = sorted_interval_list.Domain.FromIntervals([[2, 4], [-2, 0]])
        self.assertEqual(6, domain.Size())
        self.assertEqual(-2, domain.Min())
        self.assertEqual(4, domain.Max())
        self.assertEqual([-2, 0, 2, 4], domain.FlattenedIntervals())

    def testFromFlatIntervals(self):
        domain = sorted_interval_list.Domain.FromFlatIntervals([2, 4, -2, 0])
        self.assertEqual(6, domain.Size())
        self.assertEqual(-2, domain.Min())
        self.assertEqual(4, domain.Max())
        self.assertEqual([-2, 0, 2, 4], domain.FlattenedIntervals())

    def testNegation(self):
        domain = sorted_interval_list.Domain(5, 20)
        self.assertEqual([-20, -5], domain.Negation().FlattenedIntervals())


if __name__ == '__main__':
    unittest.main()
