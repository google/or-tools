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

#ifndef ORTOOLS_SAT_PYTHON_PYBIND_SOLVER_H_
#define ORTOOLS_SAT_PYTHON_PYBIND_SOLVER_H_

#include "pybind11/pybind11.h"

namespace operations_research::sat::python {

void DefinePybindWrapperForSolver(pybind11::module& m);

}  // namespace operations_research::sat::python

#endif  // ORTOOLS_SAT_PYTHON_PYBIND_SOLVER_H_
