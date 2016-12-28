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

namespace Google.OrTools {

using System;
using System.Collections.Generic;

public static class NestedArrayHelper
{
  public static T[] GetFlatArray<T>(T[][] arr)
  {
    int flatLength = 0;
    for (var i = 0; i < arr.GetLength(0); i++)
      flatLength += arr[i].GetLength(0);

    int idx = 0;
    T[] flat = new T[flatLength];

    for (int i = 0; i < arr.GetLength(0); i++)
    {
      for (int j = 0; j < arr[i].GetLength(0); j++)
        flat[idx++] = arr[i][j];
    }

    return flat;
  }
  public static int[] GetArraySecondSize<T>(T[][]arr)
  {
    var result = new int[arr.GetLength(0)];
    for (var i=0; i<arr.GetLength(0); i++)
    {
      if (arr[i] != null)
        result[i] = arr[i].Length;
    }
    return result;
  }
}
}  // namespace Google.OrTools