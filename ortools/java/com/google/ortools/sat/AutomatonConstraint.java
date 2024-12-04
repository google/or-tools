// Copyright 2010-2024 Google LLC
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

import com.google.ortools.sat.CpModelProto;

/**
 * Specialized automaton constraint.
 *
 * <p>This constraint allows adding transitions to the automaton constraint incrementally.
 */
public class AutomatonConstraint extends Constraint {
  public AutomatonConstraint(CpModelProto.Builder builder) {
    super(builder);
  }

  /**
   * Adds a transitions to the automaton.
   *
   * @param tail the tail of the transition
   * @param head the head of the transition
   * @param label the label of the transition
   * @return this constraint
   */
  public AutomatonConstraint addTransition(int tail, int head, long label) {
    getBuilder()
        .getAutomatonBuilder()
        .addTransitionTail(tail)
        .addTransitionLabel(label)
        .addTransitionHead(head);
    return this;
  }
}
