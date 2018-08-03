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

package com.google.ortools.sat;


/** The negation of a boolean variable. */
public class NotBooleanVariable implements ILiteral {
  public NotBooleanVariable(IntVar boolvar) {
    boolvar_ = boolvar;
  }

  public int getIndex() {
    return -boolvar_.getIndex() - 1;
  }

  public ILiteral not() {
    return boolvar_;
  }

  public String shortString() {
    return "not(" + boolvar_.shortString() + ")";
  }

  private IntVar boolvar_;
}
