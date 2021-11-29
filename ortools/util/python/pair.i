// Copyright 2010-2021 Google LLC
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

// SWIG Macros to use std::pair<T, U> OUTPUT in Python,
//
// Normally we'd simply use:
// ```swig
// %include "std_pair.i"
// %template(IntBoolPair) std::pair<int, bool>;
// ```
// see http://swig.org/Doc4.0/Library.html#Library_std_vector), but
// here we can't, because exceptions are forbidden and std_pair.i will rely on
// on python/pystdcommon.swg which use few throw in type traits...

%include "ortools/base/base.i"

%typemap(out) std::pair<int, bool> {
  $result = Py_BuildValue("(ib)", $1.first, $1.second);
}
