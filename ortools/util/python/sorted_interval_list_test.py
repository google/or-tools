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

from absl.testing import absltest
from ortools.util.python import sorted_interval_list


class SortedIntervalListTest(absltest.TestCase):

  def testCtorAndGetter(self):
    bool_domain = sorted_interval_list.Domain(0, 1)
    self.assertEqual(2, bool_domain.size())
    self.assertEqual(0, bool_domain.min())
    self.assertEqual(1, bool_domain.max())
    self.assertFalse(bool_domain.is_empty())
    self.assertEqual(str(bool_domain), "[0,1]")

  def testFromValues(self):
    domain = sorted_interval_list.Domain.FromValues([1, 3, -5, 5])
    self.assertEqual(4, domain.size())
    self.assertEqual(-5, domain.min())
    self.assertEqual(5, domain.max())
    self.assertEqual([-5, -5, 1, 1, 3, 3, 5, 5], domain.flattened_intervals())
    self.assertTrue(domain.contains(1))
    self.assertFalse(domain.contains(0))

  def testFromIntervals(self):
    domain = sorted_interval_list.Domain.from_intervals([[2, 4], [-2, 0]])
    self.assertEqual(6, domain.size())
    self.assertEqual(-2, domain.min())
    self.assertEqual(4, domain.max())
    self.assertEqual([-2, 0, 2, 4], domain.flattened_intervals())

  def testFromFlatIntervals(self):
    domain = sorted_interval_list.Domain.from_flat_intervals([2, 4, -2, 0])
    self.assertEqual(6, domain.size())
    self.assertEqual(-2, domain.min())
    self.assertEqual(4, domain.max())
    self.assertEqual([-2, 0, 2, 4], domain.flattened_intervals())

  def testNegation(self):
    domain = sorted_interval_list.Domain(5, 20)
    self.assertEqual([-20, -5], domain.negation().flattened_intervals())

  def testUnion(self):
    d1 = sorted_interval_list.Domain(0, 5)
    d2 = sorted_interval_list.Domain(10, 15)
    d3 = d1.union_with(d2)
    self.assertEqual([0, 5], d1.flattened_intervals())
    self.assertEqual([10, 15], d2.flattened_intervals())
    self.assertEqual([0, 5, 10, 15], d3.flattened_intervals())

  def testIntersection(self):
    d1 = sorted_interval_list.Domain(0, 10)
    d2 = sorted_interval_list.Domain(5, 15)
    d3 = d1.intersection_with(d2)
    self.assertEqual([0, 10], d1.flattened_intervals())
    self.assertEqual([5, 15], d2.flattened_intervals())
    self.assertEqual([5, 10], d3.flattened_intervals())

  def testAddition(self):
    d1 = sorted_interval_list.Domain(0, 5)
    d2 = sorted_interval_list.Domain(10, 15)
    d3 = d1.addition_with(d2)
    self.assertEqual([0, 5], d1.flattened_intervals())
    self.assertEqual([10, 15], d2.flattened_intervals())
    self.assertEqual([10, 20], d3.flattened_intervals())

  def testComplement(self):
    d1 = sorted_interval_list.Domain(-9223372036854775808, 5)
    d2 = d1.complement()
    self.assertEqual([-9223372036854775808, 5], d1.flattened_intervals())
    self.assertEqual([6, 9223372036854775807], d2.flattened_intervals())

  def testStr(self):
    d1 = sorted_interval_list.Domain(0, 5)
    self.assertEqual(str(d1), "[0,5]")
    self.assertEqual(repr(d1), "Domain([0,5])")


if __name__ == "__main__":
  absltest.main()
