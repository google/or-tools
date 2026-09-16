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

package com.google.ortools.util;

import static com.google.common.truth.Truth.assertThat;

import com.google.ortools.Loader;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class SortedIntervalListTest {
  @BeforeEach
  public void loadJni() {
    Loader.loadNativeLibraries();
  }

  @Test
  public void testSortedIntervalList_insert() throws Exception {
    SortedDisjointIntervalList i = new SortedDisjointIntervalList();
    i.insertInterval(10, 20);
    i.insertInterval(30, 40);
    assertThat(i.numIntervals()).isEqualTo(2);
    assertThat(i.toString()).isEqualTo("[10,20][30,40]");
  }

  @Test
  public void testInsertAll() throws Exception {
    SortedDisjointIntervalList i = new SortedDisjointIntervalList();
    int[] starts = new int[] {10, 30};
    int[] ends = new int[] {20, 40};
    i.insertIntervals(starts, ends);
    assertThat(i.numIntervals()).isEqualTo(2);
    assertThat(i.toString()).isEqualTo("[10,20][30,40]");
  }

  @Test
  public void testCtor() throws Exception {
    int[] starts = new int[] {10, 30};
    int[] ends = new int[] {20, 40};
    SortedDisjointIntervalList i = new SortedDisjointIntervalList(starts, ends);
    assertThat(i.numIntervals()).isEqualTo(2);
    assertThat(i.toString()).isEqualTo("[10,20][30,40]");
  }

  @Test
  public void testDomainCtor() throws Exception {
    Domain domain = new Domain(0, 1);
    assertThat(domain.size()).isEqualTo(2);
    assertThat(domain.min()).isEqualTo(0);
    assertThat(domain.max()).isEqualTo(1);
    assertThat(domain.contains(0)).isTrue();
    assertThat(domain.contains(2)).isFalse();
    assertThat(domain.toString()).isEqualTo("[0,1]");
  }

  @Test
  public void testFromValues() throws Exception {
    Domain domain = Domain.fromValues(new long[] {1, 3, -5, 5});
    assertThat(domain.size()).isEqualTo(4);
    assertThat(domain.min()).isEqualTo(-5);
    assertThat(domain.max()).isEqualTo(5);
    assertThat(domain.flattenedIntervals()).isEqualTo(new long[] {-5, -5, 1, 1, 3, 3, 5, 5});
    assertThat(domain.contains(1)).isTrue();
    assertThat(domain.contains(0)).isFalse();
  }

  @Test
  public void testFromIntervals() throws Exception {
    Domain domain = Domain.fromIntervals(new long[][] {{2, 4}, {-2, 0}});
    assertThat(domain.size()).isEqualTo(6);
    assertThat(domain.min()).isEqualTo(-2);
    assertThat(domain.max()).isEqualTo(4);
    assertThat(domain.flattenedIntervals()).isEqualTo(new long[] {-2, 0, 2, 4});
  }

  @Test
  public void testFromFlatIntervals() throws Exception {
    Domain domain = Domain.fromFlatIntervals(new long[] {2, 4, -2, 0});
    assertThat(domain.size()).isEqualTo(6);
    assertThat(domain.min()).isEqualTo(-2);
    assertThat(domain.max()).isEqualTo(4);
    assertThat(domain.flattenedIntervals()).isEqualTo(new long[] {-2, 0, 2, 4});
  }

  @Test
  public void testNegation() throws Exception {
    Domain domain = new Domain(5, 20);
    assertThat(domain.negation().flattenedIntervals()).isEqualTo(new long[] {-20, -5});
  }

  @Test
  public void testUnion() throws Exception {
    Domain d1 = new Domain(0, 5);
    Domain d2 = new Domain(10, 15);
    Domain d3 = d1.unionWith(d2);
    assertThat(d1.flattenedIntervals()).isEqualTo(new long[] {0, 5});
    assertThat(d2.flattenedIntervals()).isEqualTo(new long[] {10, 15});
    assertThat(d3.flattenedIntervals()).isEqualTo(new long[] {0, 5, 10, 15});
  }

  @Test
  public void testIntersection() throws Exception {
    Domain d1 = new Domain(0, 10);
    Domain d2 = new Domain(5, 15);
    Domain d3 = d1.intersectionWith(d2);
    assertThat(d1.flattenedIntervals()).isEqualTo(new long[] {0, 10});
    assertThat(d2.flattenedIntervals()).isEqualTo(new long[] {5, 15});
    assertThat(d3.flattenedIntervals()).isEqualTo(new long[] {5, 10});
  }

  @Test
  public void testAddition() throws Exception {
    Domain d1 = new Domain(0, 5);
    Domain d2 = new Domain(10, 15);
    Domain d3 = d1.additionWith(d2);
    assertThat(d1.flattenedIntervals()).isEqualTo(new long[] {0, 5});
    assertThat(d2.flattenedIntervals()).isEqualTo(new long[] {10, 15});
    assertThat(d3.flattenedIntervals()).isEqualTo(new long[] {10, 20});
  }

  @Test
  public void testComplement() throws Exception {
    Domain d1 = new Domain(Long.MIN_VALUE, 5);
    Domain d2 = d1.complement();
    assertThat(d1.flattenedIntervals()).isEqualTo(new long[] {Long.MIN_VALUE, 5});
    assertThat(d2.flattenedIntervals()).isEqualTo(new long[] {6, Long.MAX_VALUE});
  }
}
