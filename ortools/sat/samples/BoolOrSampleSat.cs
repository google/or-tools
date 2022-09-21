// Copyright 2010-2022 Google LLC
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

using System;
using Google.OrTools.Sat;

public class BoolOrSampleSat
{
    static void Main()
    {
        CpModel model = new CpModel();

        BoolVar x = model.NewBoolVar("x");
        BoolVar y = model.NewBoolVar("y");

        model.AddBoolOr(new ILiteral[] { x, y.Not() });
    }
}
