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

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNotNull;
import static org.junit.jupiter.api.Assertions.assertTrue;

import com.google.ortools.Loader;
import java.util.stream.IntStream;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;

public final class RoutingDimensionTest {
  @BeforeEach
  public void setUp() {
    Loader.loadNativeLibraries();
  }

  @Test
  public void testBoundCost_ctor() {
    // Create Routing Index Manager
    BoundCost boundCost = new BoundCost();
    assertNotNull(boundCost);
    assertEquals(0, boundCost.getBound());
    assertEquals(0, boundCost.getCost());

    boundCost = new BoundCost(/* bound= */ 97, /* cost= */ 101);
    assertNotNull(boundCost);
    assertEquals(97, boundCost.getBound());
    assertEquals(101, boundCost.getCost());
  }

  @Test
  public void testRoutingDimension_ctor() {
    final IndexManager manager = new IndexManager(31, 7, 3);
    assertNotNull(manager);
    final Model model = new Model(manager);
    assertNotNull(model);
    final int transitIndex = model.registerTransitCallback((long fromIndex, long toIndex) -> {
      final int fromNode = manager.indexToNode(fromIndex);
      final int toNode = manager.indexToNode(toIndex);
      return (long) Math.abs(toNode - fromNode);
    });
    assertTrue(model.addDimension(transitIndex, 100, 100, true, "Dimension"));
    final Dimension dimension = model.getMutableDimension("Dimension");
    assertNotNull(dimension);
  }

  @Test
  public void testRoutingDimension_softSpanUpperBound() {
    final IndexManager manager = new IndexManager(31, 7, 3);
    assertNotNull(manager);
    final Model model = new Model(manager);
    assertNotNull(model);
    final int transitIndex = model.registerTransitCallback((long fromIndex, long toIndex) -> {
      final int fromNode = manager.indexToNode(fromIndex);
      final int toNode = manager.indexToNode(toIndex);
      return (long) Math.abs(toNode - fromNode);
    });
    assertTrue(model.addDimension(transitIndex, 100, 100, true, "Dimension"));
    final Dimension dimension = model.getMutableDimension("Dimension");

    final BoundCost boundCost = new BoundCost(/* bound= */ 97, /* cost= */ 101);
    assertNotNull(boundCost);
    assertFalse(dimension.hasSoftSpanUpperBounds());
    for (int v : IntStream.range(0, manager.getNumberOfVehicles()).toArray()) {
      dimension.setSoftSpanUpperBoundForVehicle(boundCost, v);
      final BoundCost bc = dimension.getSoftSpanUpperBoundForVehicle(v);
      assertNotNull(bc);
      assertEquals(97, bc.getBound());
      assertEquals(101, bc.getCost());
    }
    assertTrue(dimension.hasSoftSpanUpperBounds());
  }

  @Test
  public void testRoutingDimension_quadraticCostSoftSpanUpperBound() {
    final IndexManager manager = new IndexManager(31, 7, 3);
    assertNotNull(manager);
    final Model model = new Model(manager);
    assertNotNull(model);
    final int transitIndex = model.registerTransitCallback((long fromIndex, long toIndex) -> {
      final int fromNode = manager.indexToNode(fromIndex);
      final int toNode = manager.indexToNode(toIndex);
      return (long) Math.abs(toNode - fromNode);
    });
    assertTrue(model.addDimension(transitIndex, 100, 100, true, "Dimension"));
    final Dimension dimension = model.getMutableDimension("Dimension");

    final BoundCost boundCost = new BoundCost(/* bound= */ 97, /* cost= */ 101);
    assertNotNull(boundCost);
    assertFalse(dimension.hasQuadraticCostSoftSpanUpperBounds());
    for (int v : IntStream.range(0, manager.getNumberOfVehicles()).toArray()) {
      dimension.setQuadraticCostSoftSpanUpperBoundForVehicle(boundCost, v);
      final BoundCost bc = dimension.getQuadraticCostSoftSpanUpperBoundForVehicle(v);
      assertNotNull(bc);
      assertEquals(97, bc.getBound());
      assertEquals(101, bc.getCost());
    }
    assertTrue(dimension.hasQuadraticCostSoftSpanUpperBounds());
  }
}
