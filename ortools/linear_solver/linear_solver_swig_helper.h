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

#ifndef OR_TOOLS_LINEAR_SOLVER_SWIG_HELPER_H_
#define OR_TOOLS_LINEAR_SOLVER_SWIG_HELPER_H_

#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver_callback.h"

namespace operations_research {
class LinearSolutionCallback : public MPCallback {
 public:
  LinearSolutionCallback() : MPCallback(false, false) {
      context_ = NULL;
  }

  virtual ~LinearSolutionCallback() {}

  virtual void OnSolutionCallback() {}
  
  double VariableValue(const MPVariable* variable) { return context_->VariableValue(variable); }
  
  bool CanQueryVariableValues(){ return context_->CanQueryVariableValues(); }
  
  MPCallbackEvent Event() { return context_->Event(); }
  
  int64_t NumExploredNodes() { return context_->NumExploredNodes(); }

  double GetRelativeMipGap() { return context_->GetRelativeMipGap(); }

  bool HasValidMipSolution() { return context_->HasValidMipSolution(); }

  bool IsNewSolution() { return context_->IsNewSolution(); }
  
  void RunCallback(MPCallbackContext* callback_context) override{
      context_ = callback_context;

      OnSolutionCallback();
  }
  
 private:
  MPCallbackContext* context_;
};
}  // namespace operations_research

#endif  // OR_TOOLS_LINEAR_SOLVER_LINEAR_SOLVER_CALLBACK_H_
