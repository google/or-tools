// Copyright 2010-2014 Google
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

namespace Google.OrTools.LinearSolver {
using System;
using System.Collections.Generic;

// double[] helper class.
public partial class MpDoubleVector: IDisposable, System.Collections.IEnumerable
#if !SWIG_DOTNET_1
    , System.Collections.Generic.IList<double>
#endif
{
  // cast from C# double array
  public static implicit operator MpDoubleVector(double[] inVal) {
    var outVal= new MpDoubleVector();
    foreach (double element in inVal) {
      outVal.Add(element);
    }
    return outVal;
  }

  // cast to C# double array
  public static implicit operator double[](MpDoubleVector inVal) {
    var outVal= new double[inVal.Count];
    inVal.CopyTo(outVal);
    return outVal;
  }
}
}  // namespace Google.OrTools.LinearSolver
