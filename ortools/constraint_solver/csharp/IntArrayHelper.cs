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

namespace Google.OrTools.ConstraintSolver {
using System;
using System.Collections.Generic;

// int[] and long[] helper class.
public static class IntArrayHelper  {
  public static IntExpr Element(this int[] array, IntExpr index) {
    return index.solver().MakeElement(array, index.Var());
  }
  public static IntExpr Element(this long[] array, IntExpr index) {
    return index.solver().MakeElement(array, index.Var());
  }
}

}  // namespace Google.OrTools.ConstraintSolver
