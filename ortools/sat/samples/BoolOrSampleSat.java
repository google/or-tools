// Copyright 2010-2018 Google LLC
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

import com.google.ortools.sat.CpModel;
import com.google.ortools.sat.IntVar;
import com.google.ortools.sat.Literal;

/** Code sample to demonstrates a simple Boolean constraint. */
public class BoolOrSampleSat {
  static {
    System.loadLibrary("jniortools");
  }

  public static void main(String[] args) throws Exception {
    CpModel model = new CpModel();
    IntVar x = model.newBoolVar("x");
    IntVar y = model.newBoolVar("y");
    model.addBoolOr(new Literal[] {x, y.not()});
  }
}
