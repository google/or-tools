// Copyright 2010-2014 Google
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

#ifndef OR_TOOLS_FLATZINC_SOLVER_UTIL_H_
#define OR_TOOLS_FLATZINC_SOLVER_UTIL_H_

#include <unordered_set>

#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/flatzinc/model.h"

namespace operations_research {
namespace fz {

// The Flatzinc SearchLog is just like a regular SearchLog, except
// that it uses stdout with a "%% " prefix instead of LOG(INFO).
class Log : public SearchLog {
 public:
  Log(operations_research::Solver* s, OptimizeVar* obj, int period)
      : SearchLog(s, obj, nullptr, nullptr, period) {}
  ~Log() override {}

 protected:
  void OutputLine(const std::string& line) override {
    std::cout << "%% " << line << std::endl;
  }
};

// A custom search limit that checks the Control-C call.
class Interrupt : public SearchLimit {
 public:
  explicit Interrupt(operations_research::Solver* const solver);

  ~Interrupt() override;

  bool Check() override;

  void Init() override;

  void Copy(const SearchLimit* const limit) override;

  SearchLimit* MakeClone() const override;

  // Sets Interrupt::control_c_ to true
  static void ControlCHandler(int s);

  static bool Interrupted() { return control_c_; }

 private:
  static bool control_c_;
};

// Helper to sort variable for the automatic search.
// First it groups them in size bucket if use_size is true.
//    (size < 10, < 1000, < 100000 and >= 100000).
// Then in each bucket, it sorts them by decreasing number of occurences.
void SortVariableByDegree(const std::vector<int>& occurrences, bool use_size,
                          std::vector<IntVar*>* int_vars);

// Report memory usage in a nice way.
std::string MemoryUsage();

// Helper method to flatten Search annotations.
void FlattenAnnotations(const Annotation& ann, std::vector<Annotation>* out);

// This method tries to reduce the list of active variables when defining a
// search procedure with search annotations. In order to do so, it looks at
// constraints which semantics clearly defines output variables (x = sum(yi)
// for instances will mark x as computed).
// If this create cycles, they will be broken later during extraction.
void MarkComputedVariables(Constraint* ct,
                           std::unordered_set<IntegerVariable*>* marked);

}  // namespace fz
}  // namespace operations_research

#endif  // OR_TOOLS_FLATZINC_SOLVER_UTIL_H_
