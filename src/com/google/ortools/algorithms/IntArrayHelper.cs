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

public partial class KInt64VectorVector : IDisposable, System.Collections.IEnumerable
#if !SWIG_DOTNET_1
    , System.Collections.Generic.IEnumerable<KInt64Vector>
#endif
 {
  // cast from C# long matrix
  public static implicit operator KInt64VectorVector(long[,] inVal) {
    int x_size = inVal.GetLength(0);
    int y_size = inVal.GetLength(1);
    KInt64VectorVector outVal = new KInt64VectorVector();
    for (int i = 0; i < x_size; ++i)
    {
      outVal.Add(new KInt64Vector());
      for (int j = 0; j < y_size; ++j)
      {
        outVal[i].Add(inVal[i, j]);
      }
    }
    return outVal;
  }

  // cast to C# long matrix
  public static implicit operator long[,](KInt64VectorVector inVal) {
    int x_size = inVal.Count;
    int y_size = inVal.Count == 0  ? 0 : inVal[0].Count;
    var outVal= new long[x_size, y_size];
    for (int i = 0; i < x_size; ++i)
    {
      for (int j = 0; j < y_size; ++j)
      {
        outVal[i, j] = inVal[i][j];
      }
    }
    return outVal;
  }
}
}  // namespace Google.OrTools.Algorithms
