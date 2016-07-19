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

#ifndef OR_TOOLS_SAT_INTEGER_SUM_H_
#define OR_TOOLS_SAT_INTEGER_SUM_H_

#include "sat/integer.h"
#include "sat/model.h"
#include "sat/sat_base.h"

namespace operations_research {
namespace sat {

// A really basic implementation of a sum of integer variables.
// The complexity is in O(num_variables) at each propagation.
//
// TODO(user): handle negative coefficients.
// TODO(user): Propagate all the bounds.
// TODO(user): If one has many such constraint, it will be more efficient to
// propagate all of them at once rather than doing it one at the time.
class IntegerSum : public PropagatorInterface {
 public:
  IntegerSum(const std::vector<IntegerVariable>& vars,
             const std::vector<int>& coefficients, IntegerVariable sum,
             IntegerTrail* integer_trail);

  // Currently we only propagates the directions:
  // * vars lower-bound -> sum lower-bound.
  // * for all vars i,
  //   vars lower-bound (excluding i) + sum upper_bound -> i upper-bound.
  bool Propagate(Trail* trail) final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  std::vector<IntegerVariable> vars_;
  std::vector<int> coeffs_;
  IntegerVariable sum_;
  IntegerTrail* integer_trail_;

  std::vector<Literal> literal_reason_;
  std::vector<IntegerLiteral> integer_reason_;

  DISALLOW_COPY_AND_ASSIGN(IntegerSum);
};

// Model-based function to create an IntegerVariable that corresponds to the
// given weighted sum of other IntegerVariables.
inline std::function<IntegerVariable(Model*)> NewWeightedSum(
    const std::vector<int>& coefficients, const std::vector<IntegerVariable>& vars) {
  return [=](Model* model) {
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();

    // The trival bounds will be propagated correctly at level zero.
    // TODO(user): pay attention to integer overflow. It currently work but
    // it is not really robust.
    IntegerVariable sum = integer_trail->AddIntegerVariable(
        std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
    IntegerSum* constraint =
        new IntegerSum(vars, coefficients, sum, integer_trail);
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
    return sum;
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_INTEGER_SUM_H_
