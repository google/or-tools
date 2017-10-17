// Copyright 2010-2017 Google
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


package com.google.ortools.constraintsolver;

/**
 * This class acts as a intermediate step between a c++ decision builder
 * and a java one. Its main purpose is to catch the java exception launched
 * when a failure occurs during the Next() call, and to return silently
 * a FailDecision that will propagate the failure back to the C++ code.
 *
 */
public class JavaDecisionBuilder extends DecisionBuilder {
  /**
   * This methods wraps the calls to next() and catches fail exceptions.
   */
  public final Decision nextWrap(Solver solver) {
    try {
      return next(solver);
    } catch (Solver.FailException e) {
      return solver.makeFailDecision();
    }
  }
  /**
   * This is the new method to subclass when defining a java decision builder.
   */
  public Decision next(Solver solver) throws Solver.FailException {
    return null;
  }
}
