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

public class LiteralSample {

  static {
    System.loadLibrary("jniortools");
  }

  static void LiteralSample()
  {
    CpModel model = new CpModel();
    IntVar x = model.newBoolVar("x");
    ILiteral not_x = x.not();
    System.out.println(not_x.shortString());
  }

  public static void main(String[] args) throws Exception {
    LiteralSample();
  }
}
