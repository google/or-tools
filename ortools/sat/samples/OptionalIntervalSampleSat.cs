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

public class OptionalIntervalSampleSat
{
    static void Main()
    {
        CpModel model = new CpModel();
        int horizon = 100;

        // C# code supports constant of affine expressions.
        IntVar start_var = model.NewIntVar(0, horizon, "start");
        IntVar end_var = model.NewIntVar(0, horizon, "end");
        BoolVar presence_var = model.NewBoolVar("presence");
        IntervalVar interval = model.NewOptionalIntervalVar(start_var, 10, end_var + 2, presence_var, "interval");
        Console.WriteLine(interval);

        // If the size is fixed, a simpler version uses the start expression, the size and the
        // literal.
        IntervalVar fixedSizeIntervalVar =
            model.NewOptionalFixedSizeIntervalVar(start_var, 10, presence_var, "fixed_size_interval_var");
        Console.WriteLine(fixedSizeIntervalVar);

        // A fixed interval can be created using the same API.
        IntervalVar fixedInterval = model.NewOptionalFixedSizeIntervalVar(5, 10, presence_var, "fixed_interval");
        Console.WriteLine(fixedInterval);
    }
}
