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

import static com.google.common.truth.extensions.proto.ProtoTruth.assertThat;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMap;
import com.google.common.collect.ImmutableSet;
import java.util.AbstractMap;
import java.util.Map;
import org.junit.jupiter.api.Test;

public final class SparseContainersTest {
  private static class DataWithId implements ModelElement {
    private final double doubleData;
    private final long id;

    public DataWithId(double doubleData, long id) {
      this.doubleData = doubleData;
      this.id = id;
    }

    double getDoubleData() {
      return doubleData;
    }

    boolean isPositive() {
      return doubleData > 0;
    }

    @Override
    public long getId() {
      return id;
    }
  }

  @Test
  public void toSparseDoubleVectorProto_createsNonEmpty() {
    ImmutableSet<DataWithId> s = ImmutableSet.of(new DataWithId(4.0, 2), new DataWithId(1.5, 5));
    assertThat(SparseContainers.toSparseDoubleVectorProto(s, DataWithId::getDoubleData))
        .isEqualTo(SparseDoubleVectorProto.newBuilder()
                .addIds(2)
                .addValues(4.0)
                .addIds(5)
                .addValues(1.5)
                .build());
  }

  @Test
  public void toSparseDoubleVectorProto_fromSet_createsEmpty() {
    assertThat(
        SparseContainers.toSparseDoubleVectorProto(ImmutableSet.of(), DataWithId::getDoubleData))
        .isEqualToDefaultInstance();
  }

  @Test
  public void toSparseBoolVectorProto_fromSet_createsNonEmpty() {
    ImmutableSet<DataWithId> s = ImmutableSet.of(new DataWithId(-4.0, 2), new DataWithId(1.5, 5));
    assertThat(SparseContainers.toSparseBoolVectorProto(s, DataWithId::isPositive))
        .isEqualTo(SparseBoolVectorProto.newBuilder()
                .addIds(2)
                .addValues(false)
                .addIds(5)
                .addValues(true)
                .build());
  }

  @Test
  public void toSparseDoubleVectorProto_fromMap_createsNonEmpty() {
    ImmutableMap<DataWithId, Double> sparseDoubleData =
        ImmutableMap.of(new DataWithId(0.0, 2), 10.0, new DataWithId(0.0, 5), 11.0);
    assertThat(SparseContainers.toSparseDoubleVectorProto(sparseDoubleData))
        .isEqualTo(SparseDoubleVectorProto.newBuilder()
                .addIds(2)
                .addValues(10.0)
                .addIds(5)
                .addValues(11.0)
                .build());
  }

  @Test
  public void toSparseDoubleVectorProto_fromMap_createsEmpty() {
    assertThat(SparseContainers.toSparseDoubleVectorProto(ImmutableMap.of()))
        .isEqualToDefaultInstance();
  }

  @Test
  public void toSparseDoubleVectorProto_fromMapEntryIterable_createsNonEmpty() {
    ImmutableList<Map.Entry<DataWithId, Double>> sparseDoubleData =
        ImmutableList.of(new AbstractMap.SimpleEntry<>(new DataWithId(0.0, 2), 10.0),
            new AbstractMap.SimpleEntry<>(new DataWithId(0.0, 5), 11.0));
    assertThat(SparseContainers.toSparseDoubleVectorProto(sparseDoubleData))
        .isEqualTo(SparseDoubleVectorProto.newBuilder()
                .addIds(2)
                .addValues(10.0)
                .addIds(5)
                .addValues(11.0)
                .build());
  }

  @Test
  public void toSparseDoubleVectorProto_fromMapEntryIterable_createsEmpty() {
    assertThat(SparseContainers.toSparseDoubleVectorProto(ImmutableList.of()))
        .isEqualToDefaultInstance();
  }

  @Test
  public void toSparseBoolVectorProto_createsEmpty() {
    assertThat(SparseContainers.toSparseBoolVectorProto(ImmutableSet.of(), DataWithId::isPositive))
        .isEqualToDefaultInstance();
  }

  @Test
  public void toSparseIntVectorProto_createsNonEmpty() {
    ImmutableMap<DataWithId, Integer> sparseIntData =
        ImmutableMap.of(new DataWithId(0.0, 2), 10, new DataWithId(0.0, 5), 11);
    assertThat(SparseContainers.toSparseInt32VectorProto(sparseIntData))
        .isEqualTo(SparseInt32VectorProto.newBuilder()
                .addIds(2)
                .addValues(10)
                .addIds(5)
                .addValues(11)
                .build());
  }

  @Test
  public void toSparseIntVectorProto_createsEmpty() {
    assertThat(SparseContainers.toSparseInt32VectorProto(ImmutableMap.of()))
        .isEqualToDefaultInstance();
  }
}
