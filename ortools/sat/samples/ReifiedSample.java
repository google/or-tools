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

import com.google.ortools.sat.*;

public class ReifiedSample {

  static {
    System.loadLibrary("jniortools");
  }

  static void ReifiedSample()
  {
    CpModel model = new CpModel();

    IntVar x = model.newBoolVar("x");
    IntVar y = model.newBoolVar("y");
    IntVar b = model.newBoolVar("b");

    //  First version using a half-reified bool and.
    model.addBoolAnd(new ILiteral[] {x, y.not()}).onlyEnforceIf(b);

    // Second version using implications.
    model.addImplication(b, x);
    model.addImplication(b, y.not());

    // Third version using bool or.
    model.addBoolOr(new ILiteral[] {b.not(), x});
    model.addBoolOr(new ILiteral[] {b.not(), y.not()});
  }

  public static void main(String[] args) throws Exception {
    ReifiedSample();
  }
}
