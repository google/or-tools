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

namespace Google.OrTools.Algorithms {
using System;
using System.Collections.Generic;

public partial class KInt64Vector: IDisposable, System.Collections.IEnumerable
#if !SWIG_DOTNET_1
    , System.Collections.Generic.IList<long>
#endif
{
  // cast from C# long array
  public static implicit operator KInt64Vector(long[] inVal) {
    var outVal= new KInt64Vector();
    foreach (long element in inVal) {
      outVal.Add(element);
    }
    return outVal;
  }

  // cast to C# long array
  public static implicit operator long[](KInt64Vector inVal) {
    var outVal= new long[inVal.Count];
    inVal.CopyTo(outVal);
    return outVal;
  }
}

public partial class KIntVector: IDisposable, System.Collections.IEnumerable
#if !SWIG_DOTNET_1
    , System.Collections.Generic.IList<int>
#endif
{
  // cast from C# int array
  public static implicit operator KIntVector(int[] inVal) {
    var outVal= new KIntVector();
    foreach (int element in inVal) {
      outVal.Add(element);
    }
    return outVal;
  }

  // cast to C# int array
  public static implicit operator int[](KIntVector inVal) {
    var outVal= new int[inVal.Count];
    inVal.CopyTo(outVal);
    return outVal;
  }
}
}  // namespace Google.OrTools.Algorithms
