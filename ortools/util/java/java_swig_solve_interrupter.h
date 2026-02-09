// Copyright 2010-2025 Google LLC
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

#ifndef ORTOOLS_UTIL_JAVA_JAVA_SWIG_SOLVE_INTERRUPTER_H_
#define ORTOOLS_UTIL_JAVA_JAVA_SWIG_SOLVE_INTERRUPTER_H_

#include "ortools/util/solve_interrupter.h"

namespace operations_research {

// C++ class wrapped by Swig for Java.
class JavaSwigSolveInterrupter {
 public:
  JavaSwigSolveInterrupter() = default;
  void interrupt() { interrupter_.Interrupt(); }
  bool isInterrupted() { return interrupter_.IsInterrupted(); }
  // Not SWIGed.
  operations_research::SolveInterrupter& interrupter() { return interrupter_; }

 private:
  operations_research::SolveInterrupter interrupter_;
};

}  // namespace operations_research

#endif  // ORTOOLS_UTIL_JAVA_JAVA_SWIG_SOLVE_INTERRUPTER_H_
