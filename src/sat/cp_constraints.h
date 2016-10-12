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

#ifndef OR_TOOLS_SAT_CP_CONSTRAINTS_H_
#define OR_TOOLS_SAT_CP_CONSTRAINTS_H_

#include "sat/integer.h"
#include "sat/model.h"

namespace operations_research {
namespace sat {

// Propagate the fact that a XOR of literals is equal to the given value.
// The complexity is in O(n).
//
// TODO(user): By using a two watcher mechanism, we can propagate this a lot
// faster.
class BooleanXorPropagator : public PropagatorInterface {
 public:
  BooleanXorPropagator(const std::vector<Literal>& literals, bool value,
                       IntegerTrail* integer_trail)
      : literals_(literals), value_(value), integer_trail_(integer_trail) {}

  bool Propagate(Trail* trail) final;
  void RegisterWith(GenericLiteralWatcher* watcher);

 private:
  const std::vector<Literal> literals_;
  const bool value_;
  IntegerTrail* integer_trail_;

  DISALLOW_COPY_AND_ASSIGN(BooleanXorPropagator);
};

// ============================================================================
// Model based functions.
// ============================================================================

// Enforces that the given tuple of variables takes different values.
std::function<void(Model*)> AllDifferent(const std::vector<IntegerVariable>& vars);

// Enforces the XOR of a set of literals to be equal to the given value.
inline std::function<void(Model*)> LiteralXorIs(const std::vector<Literal>& literals,
                                                bool value) {
  return [=](Model* model) {
    IntegerTrail* integer_trail = model->GetOrCreate<IntegerTrail>();
    BooleanXorPropagator* constraint =
        new BooleanXorPropagator(literals, value, integer_trail);
    constraint->RegisterWith(model->GetOrCreate<GenericLiteralWatcher>());
    model->TakeOwnership(constraint);
  };
}

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_CONSTRAINTS_H_
