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

"""Tests for hash_model_storage that cannot be covered by model_storage_(update)_test."""

from absl.testing import absltest
from ortools.math_opt.python import hash_model_storage


class HashModelStorageTest(absltest.TestCase):

  def test_quadratic_term_storage(self):
    storage = hash_model_storage._QuadraticTermStorage()
    storage.set_coefficient(0, 1, 1.0)
    storage.delete_variable(0)
    self.assertEmpty(list(storage.get_adjacent_variables(0)))


if __name__ == "__main__":
  absltest.main()
