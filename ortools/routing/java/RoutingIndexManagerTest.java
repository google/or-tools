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

package com.google.ortools.routing;

import static org.junit.jupiter.api.Assertions.assertArrayEquals;
import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertNotNull;

import com.google.ortools.Loader;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class RoutingIndexManagerTest {
  @BeforeEach
  public void setUp() {
    Loader.loadNativeLibraries();
  }

  @Test
  public void testSingleDepot() {
    final int numNodes = 10;
    final int numVehicles = 5;
    final int depotIndex = 1;
    final IndexManager manager = new IndexManager(numNodes, numVehicles, depotIndex);
    assertNotNull(manager);
    assertEquals(numNodes, manager.getNumberOfNodes());
    assertEquals(numVehicles, manager.getNumberOfVehicles());
    assertEquals(numNodes + 2 * numVehicles - 1, manager.getNumberOfIndices());
    assertEquals(1, manager.getNumberOfUniqueDepots());

    final long[] expectedStarts = {1, 10, 11, 12, 13};
    final long[] expectedEnds = {14, 15, 16, 17, 18};
    for (int i = 0; i < manager.getNumberOfVehicles(); ++i) {
      assertEquals(expectedStarts[i], manager.getStartIndex(i));
      assertEquals(expectedEnds[i], manager.getEndIndex(i));
    }

    for (int i = 0; i < manager.getNumberOfIndices(); ++i) {
      if (i >= numNodes) { // start or end nodes
        assertEquals(depotIndex, manager.indexToNode(i));
      } else {
        assertEquals(i, manager.indexToNode(i));
      }
    }

    final long[] inputIndices = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18};
    final int[] expectedNodes = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    assertArrayEquals(expectedNodes, manager.indicesToNodes(inputIndices));

    for (int i = 0; i < manager.getNumberOfNodes(); ++i) {
      assertEquals(i, manager.nodeToIndex(i));
    }

    final int[] inputNodes = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    final long[] expectedIndicesFromNodes = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    assertArrayEquals(expectedIndicesFromNodes, manager.nodesToIndices(inputNodes));
  }

  @Test
  public void testMultiDepot() {
    final int numNodes = 10;
    final int numVehicles = 5;
    final int[] starts = {0, 3, 9, 2, 2};
    final int[] ends = {0, 9, 3, 2, 1};
    final IndexManager manager = new IndexManager(numNodes, numVehicles, starts, ends);
    assertNotNull(manager);
    assertEquals(numNodes, manager.getNumberOfNodes());
    assertEquals(numVehicles, manager.getNumberOfVehicles());
    assertEquals(numNodes + 2 * numVehicles - 5, manager.getNumberOfIndices());
    assertEquals(5, manager.getNumberOfUniqueDepots());

    final long[] expectedStarts = {0, 2, 8, 1, 9};
    final long[] expectedEnds = {10, 11, 12, 13, 14};
    for (int i = 0; i < manager.getNumberOfVehicles(); ++i) {
      assertEquals(expectedStarts[i], manager.getStartIndex(i));
      assertEquals(expectedEnds[i], manager.getEndIndex(i));
    }

    final int[] expectedNodes = {0, 2, 3, 4, 5, 6, 7, 8, 9, 2, 0, 9, 3, 2, 1};
    for (int i = 0; i < manager.getNumberOfIndices(); ++i) {
      assertEquals(expectedNodes[i], manager.indexToNode(i));
    }

    final long[] inputIndices = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
    assertArrayEquals(expectedNodes, manager.indicesToNodes(inputIndices));

    final long unassigned = IndexManager.getKUnassigned();
    final long[] expectedIndices = {0, unassigned, 1, 2, 3, 4, 5, 6, 7, 8};
    for (int i = 0; i < manager.getNumberOfNodes(); ++i) {
      assertEquals(expectedIndices[i], manager.nodeToIndex(i));
    }

    final int[] inputNodes = {0, 2, 3, 4, 5, 6, 7, 8, 9};
    final long[] expectedIndicesFromNodes = {0, 1, 2, 3, 4, 5, 6, 7, 8};
    assertArrayEquals(expectedIndicesFromNodes, manager.nodesToIndices(inputNodes));
  }
}
