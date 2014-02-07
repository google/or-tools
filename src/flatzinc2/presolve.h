// Copyright 2010-2013 Google
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
#ifndef OR_TOOLS_FLATZINC_PRESOLVE_H_
#define OR_TOOLS_FLATZINC_PRESOLVE_H_

#include <string>
#include "base/hash.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/hash.h"
#include "flatzinc2/model.h"

namespace operations_research {

typedef hash_map<const FzIntegerVariable*, FzIntegerVariable*>
    VarSubstitutionMap;

class FzPresolve {
 public:
  enum PresolveStatus {
    NO_CHANGE,
    SOME_PRESOLVE,
    REMOVE_ME,
  };

  typedef PresolveStatus (FzPresolve::*FzPresolveRule)(
      FzConstraint* const input, FzConstraint** output);

  ~FzPresolve();
  void Init();
  bool Run(FzModel* const model);

  PresolveStatus PresolveOneConstraint(FzConstraint* const ct,
                                       FzConstraint** output);

  void ApplyVariableSubstitution(
      const VarSubstitutionMap& var_substitution_map);

  bool AddSubstitution(FzIntegerVariable* const from,
                       FzIntegerVariable* const to);

 private:
  // Substitution support.
  void SubstituteConstraint(FzConstraint* const input);
  void SubstituteAnnotation(FzAnnotation* const annotation);
  void SubstituteOutput(FzOnSolutionOutput* const output);
  void SubstituteArgument(FzArgument* const argument);
  // Presolve rules.
  void Register(const std::string& id, FzPresolveRule const rule);
  PresolveStatus PresolveBool2Int(FzConstraint* const input,
                                  FzConstraint** output);
  PresolveStatus PresolveIntEq(FzConstraint* const input,
                               FzConstraint** output);
  // Fields.
  hash_map<std::string, std::vector<FzPresolveRule>> rules_;
  VarSubstitutionMap var_substitution_map_;
};
}  // namespace operations_research

#endif  // OR_TOOLS_FLATZINC_PRESOLVE_H_
