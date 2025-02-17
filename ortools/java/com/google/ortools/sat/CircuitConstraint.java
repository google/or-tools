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

package com.google.ortools.sat;

import com.google.ortools.sat.CircuitConstraintProto;
import com.google.ortools.sat.CpModelProto;

/**
 * Specialized circuit constraint.
 *
 * <p>This constraint allows adding arcs to the circuit constraint incrementally.
 */
public class CircuitConstraint extends Constraint {
  public CircuitConstraint(CpModelProto.Builder builder) {
    super(builder);
  }

  /**
   * Add an arc to the graph of the circuit constraint.
   *
   * @param tail the index of the tail node.
   * @param head the index of the head node.
   * @param literal it will be set to true if the arc is selected.
   */
  public CircuitConstraint addArc(int tail, int head, Literal literal) {
    CircuitConstraintProto.Builder circuit = getBuilder().getCircuitBuilder();
    circuit.addTails(tail);
    circuit.addHeads(head);
    circuit.addLiterals(literal.getIndex());
    return this;
  }
}
