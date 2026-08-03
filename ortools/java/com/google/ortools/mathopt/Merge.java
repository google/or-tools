// Copyright 2010-2025 Google LLC
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package com.google.ortools.mathopt;

import com.google.common.collect.ImmutableList;
import java.util.Comparator;
import java.util.List;

final class Merge {
  private Merge() {}

  /**
   * Given two sorted lists, each individually containing no duplicates, returns a single merged
   * sorted list with duplicates removed.
   *
   * <p>The inputs {@code first} and {@code second} are not modified.
   *
   * <p>The caller must ensure that first and second are sorted and individually without duplicates,
   * otherwise no exception will be thrown and the returned list is arbitrary.
   */
  static <T> ImmutableList<T> mergeSortedSkipDuplicates(
      List<T> first, List<T> second, Comparator<T> comparator) {
    ImmutableList.Builder<T> result = ImmutableList.builder();
    int i = 0;
    int j = 0;
    while (i < first.size() && j < second.size()) {
      int cmp = comparator.compare(first.get(i), second.get(j));
      if (cmp < 0) {
        result.add(first.get(i));
        i++;
      } else if (cmp > 0) {
        result.add(second.get(j));
        j++;
      } else {
        result.add(first.get(i));
        i++;
        j++;
      }
    }
    for (; i < first.size(); i++) {
      result.add(first.get(i));
    }
    for (; j < second.size(); j++) {
      result.add(second.get(j));
    }
    return result.build();
  }
}
