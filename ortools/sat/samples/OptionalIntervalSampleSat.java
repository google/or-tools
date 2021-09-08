// Copyright 2010-2021 Google LLC
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

package com.google.ortools.sat.samples;

import com.google.ortools.Loader;
import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.IntervalVar;
import com.google.ortools.sat.LinearExpr;
import com.google.ortools.sat.Literal;

/** Code sample to demonstrates how to build an optional interval. */
public class OptionalIntervalSampleSat {
  public static void main(String[] args) throws Exception {
    Loader.loadNativeLibraries();
    CpModel model = new CpModel();
    int horizon = 100;

    // An interval can be created from three affine expressions, and a literal.
    IntVar startVar = model.newIntVar(0, horizon, "start");
    IntVar endVar = model.newIntVar(0, horizon, "end");
    Literal presence = model.newBoolVar("presence");
    IntervalVar intervalVar = model.newOptionalIntervalVar(
        startVar, LinearExpr.constant(10), LinearExpr.affine(endVar, 1, 2), presence, "interval");
    System.out.println(intervalVar);

    // If the size is fixed, a simpler version uses the start expression, the size and the literal.
    IntervalVar fixedSizeIntervalVar =
        model.newOptionalFixedSizeIntervalVar(startVar, 10, presence, "fixed_size_interval_var");
    System.out.println(fixedSizeIntervalVar);

    // A fixed interval can be created using another method.
    IntervalVar fixedInterval = model.newOptionalFixedInterval(5, 10, presence, "fixed_interval");
    System.out.println(fixedInterval);
  }
}
