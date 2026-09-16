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

import static com.google.common.truth.Truth.assertThat;
import static java.util.Comparator.naturalOrder;

import com.google.common.collect.ImmutableList;
import java.util.Comparator;
import org.junit.jupiter.api.Test;

public final class MergeTest {
  @Test
  public void merge_noDuplicatesAndFirstFinishesLast() {
    ImmutableList<Integer> first = ImmutableList.of(2, 4, 6, 12);
    ImmutableList<Integer> second = ImmutableList.of(3, 9);

    assertThat(Merge.mergeSortedSkipDuplicates(first, second, naturalOrder()))
        .containsExactly(2, 3, 4, 6, 9, 12);
  }

  @Test
  public void merge_noDuplicatesAndSecondFinishesLast() {
    ImmutableList<Integer> first = ImmutableList.of(2, 4, 6);
    ImmutableList<Integer> second = ImmutableList.of(3, 9);

    assertThat(Merge.mergeSortedSkipDuplicates(first, second, naturalOrder()))
        .containsExactly(2, 3, 4, 6, 9);
  }

  @Test
  public void merge_hasDuplicatesAndFirstFinishesLast_areRemoved() {
    ImmutableList<Integer> first = ImmutableList.of(2, 4, 6, 12);
    ImmutableList<Integer> second = ImmutableList.of(2, 3, 6, 9);
    assertThat(Merge.mergeSortedSkipDuplicates(first, second, naturalOrder()))
        .containsExactly(2, 3, 4, 6, 9, 12);
  }

  @Test
  public void merge_hasDuplicatesAndSecondFinishesLast_areRemoved() {
    ImmutableList<Integer> first = ImmutableList.of(2, 4, 6);
    ImmutableList<Integer> second = ImmutableList.of(2, 3, 6, 9);
    assertThat(Merge.mergeSortedSkipDuplicates(first, second, naturalOrder()))
        .containsExactly(2, 3, 4, 6, 9);
  }

  @Test
  public void merge_firstEmpty() {
    ImmutableList<Integer> first = ImmutableList.of();
    ImmutableList<Integer> second = ImmutableList.of(3, 9);

    assertThat(Merge.mergeSortedSkipDuplicates(first, second, naturalOrder()))
        .containsExactly(3, 9);
  }

  @Test
  public void merge_secondEmpty() {
    ImmutableList<Integer> first = ImmutableList.of(2, 4, 8);
    ImmutableList<Integer> second = ImmutableList.of();

    assertThat(Merge.mergeSortedSkipDuplicates(first, second, naturalOrder()))
        .containsExactly(2, 4, 8);
  }

  @Test
  public void merge_bothEmpty() {
    assertThat(Merge.mergeSortedSkipDuplicates(
                   ImmutableList.of(), ImmutableList.of(), Comparator.<Integer>naturalOrder()))
        .isEmpty();
  }
}
