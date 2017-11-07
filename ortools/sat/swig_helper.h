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

#ifndef OR_TOOLS_SAT_SWIG_HELPER_H_
#define OR_TOOLS_SAT_SWIG_HELPER_H_

#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_parameters.pb.h"

namespace operations_research {
namespace sat {

class SatHelper {
 public:
  // The arguments of the functions defined below must follow these rules
  // to be wrapped by swig correctly:
  // 1) Their types must include the full operations_research::sat::
  //    namespace.
  // 2) Their names must correspond to the ones declared in the .i
  //    file (see the python/ and java/ subdirectories).
  static operations_research::sat::CpSolverResponse Solve(
      const operations_research::sat::CpModelProto& model_proto) {
    Model model;
    return operations_research::sat::SolveCpModel(model_proto, &model);
  }

  static operations_research::sat::CpSolverResponse SolveWithParameters(
      const operations_research::sat::CpModelProto& model_proto,
      const operations_research::sat::SatParameters& parameters) {
    Model model;
    model.Add(NewSatParameters(parameters));
    return SolveCpModel(model_proto, &model);
  }

  static operations_research::sat::CpSolverResponse
  SolveWithParametersAndSolutionObserver(
      const operations_research::sat::CpModelProto& model_proto,
      const operations_research::sat::SatParameters& parameters,
      std::function<
          void(const operations_research::sat::CpSolverResponse& response)>
          observer) {
    Model model;
    model.Add(NewSatParameters(parameters));
    model.Add(NewFeasibleSolutionObserver(observer));
    return SolveCpModel(model_proto, &model);
  }
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SWIG_HELPER_H_
