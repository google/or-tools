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

import static com.google.common.collect.Streams.stream;
import static java.util.Comparator.comparingLong;
import static java.util.Map.Entry.comparingByKey;

import java.util.Comparator;
import java.util.Map;
import java.util.Set;
import java.util.function.Function;

/** Static methods for generating messages from sparse_containers.proto from java collections. */
final class SparseContainers {
  private SparseContainers() {}

  /** Applies {@code f} to each element of {@code elements} and sorts the result by element id. */
  public static <T extends ModelElement> SparseDoubleVectorProto toSparseDoubleVectorProto(
      Set<T> elements, Function<T, Double> function) {
    var result = SparseDoubleVectorProto.newBuilder();
    elements.stream().sorted(Comparator.comparingLong(T::getId)).forEachOrdered(element -> {
      result.addIds(element.getId());
      result.addValues(function.apply(element));
    });
    return result.build();
  }

  /**
   * Returns an equivalent protocol buffer where the {@link ModelElement} keys are replaced by their
   * id.
   */
  public static <T extends ModelElement> SparseDoubleVectorProto toSparseDoubleVectorProto(
      Map<T, Double> values) {
    return toSparseDoubleVectorProto(values.entrySet());
  }

  /**
   * Returns an equivalent protocol buffer where the {@link ModelElement} keys are replaced by their
   * id.
   */
  static <T extends ModelElement> SparseDoubleVectorProto toSparseDoubleVectorProto(
      Iterable<Map.Entry<T, Double>> values) {
    var result = SparseDoubleVectorProto.newBuilder();
    stream(values).sorted(comparingByKey(comparingLong(T::getId))).forEachOrdered((entry) -> {
      result.addIds(entry.getKey().getId());
      result.addValues(entry.getValue().doubleValue());
    });
    return result.build();
  }

  /**
   * Returns an equivalent protocol buffer where the {@link ModelElement} keys are replaced by their
   * id.
   */
  public static <T extends ModelElement> SparseInt32VectorProto toSparseInt32VectorProto(
      Map<T, Integer> values) {
    var result = SparseInt32VectorProto.newBuilder();
    values.entrySet()
        .stream()
        .sorted(comparingByKey(comparingLong(T::getId)))
        .forEachOrdered((entry) -> {
          result.addIds(entry.getKey().getId());
          result.addValues(entry.getValue().intValue());
        });
    return result.build();
  }

  /** Applies {@code f} to each element of {@code elements} and sorts the result by element id. */
  public static <T extends ModelElement> SparseBoolVectorProto toSparseBoolVectorProto(
      Set<T> elements, Function<T, Boolean> function) {
    var result = SparseBoolVectorProto.newBuilder();
    elements.stream().sorted(Comparator.comparingLong(T::getId)).forEachOrdered(element -> {
      result.addIds(element.getId());
      result.addValues(function.apply(element));
    });
    return result.build();
  }
}
