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

public class IntervalSample {

  static {
    System.loadLibrary("jniortools");
  }

  public static void IntervalSample() {
    CpModel model = new CpModel();
    int horizon = 100;
    IntVar start_var = model.newIntVar(0, horizon, "start");
    // Java code supports IntVar or integer constants in intervals.
    int duration = 10;
    IntVar end_var = model.newIntVar(0, horizon, "end");
    IntervalVar interval =
        model.newIntervalVar(start_var, duration, end_var, "interval");

    System.out.println(interval);
  }

  public static void main(String[] args) throws Exception {
    IntervalSample();
  }
}
