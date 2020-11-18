// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_SAT_IMPLIED_BOUNDS_H_
#define OR_TOOLS_SAT_IMPLIED_BOUNDS_H_

#include <algorithm>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/int_type.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/util/bitset.h"

namespace operations_research {
namespace sat {

// For each IntegerVariable, the ImpliedBound class allows to list all such
// entries.
//
// This is meant to be used in the cut generation code when it make sense: if we
// have BoolVar => X >= bound, we can always lower bound the variable X by
// (bound - X_lb) * BoolVar + X_lb, and that can lead to stronger cuts.
struct ImpliedBoundEntry {
  // An integer variable in [0, 1]. When at 1, then the IntegerVariable
  // corresponding to this entry must be greater or equal to the given lower
  // bound.
  IntegerVariable literal_view = kNoIntegerVariable;
  IntegerValue lower_bound = IntegerValue(0);

  // If false, it is when the literal_view is zero that the lower bound is
  // valid.
  bool is_positive = true;

  // These constructors are needed for OR-Tools.
  ImpliedBoundEntry(IntegerVariable lit, IntegerValue lb, bool positive)
      : literal_view(lit), lower_bound(lb), is_positive(positive) {}

  ImpliedBoundEntry()
      : literal_view(kNoIntegerVariable), lower_bound(0), is_positive(true) {}
};

// Maintains all the implications of the form Literal => IntegerLiteral. We
// collect these implication at model loading, during probing and during search.
//
// TODO(user): This can quickly use up too much memory. Add some limit in place.
// In particular, each time we have literal => integer_literal we should avoid
// storing the same integer_literal for all other_literal for which
// other_literal => literal. For this we need to interact with the
// BinaryImplicationGraph.
//
// TODO(user): This is a bit of a duplicate with the Literal <=> IntegerLiteral
// stored in the IntegerEncoder class. However we only need one side here.
//
// TODO(user): Do like in the DomainDeductions class and allow to process
// clauses (or store them) to perform more level zero deductions. Note that this
// is again a slight duplicate with what we do there (except that we work at the
// Domain level in that presolve class).
//
// TODO(user): Add an implied bound cut generator to add these simple
// constraints to the LP when needed.
class ImpliedBounds {
 public:
  explicit ImpliedBounds(Model* model)
      : parameters_(*model->GetOrCreate<SatParameters>()),
        sat_solver_(model->GetOrCreate<SatSolver>()),
        integer_trail_(model->GetOrCreate<IntegerTrail>()),
        integer_encoder_(model->GetOrCreate<IntegerEncoder>()) {}
  ~ImpliedBounds();

  // Adds literal => integer_literal to the repository.
  //
  // Not that it checks right aways if there is another bound on the same
  // variable involving literal.Negated(), in which case we can improve the
  // level zero lower bound of the variable.
  void Add(Literal literal, IntegerLiteral integer_literal);

  // This must be called after first_decision has been enqueued and propagated.
  // It will inspect the trail and add all new implied bounds.
  //
  // Preconditions: The decision level must be one (CHECKed). And the decision
  // must be equal to first decision (we currently do not CHECK that).
  void ProcessIntegerTrail(Literal first_decision);

  // Returns all the implied bounds stored for the given variable.
  // Note that only literal with an IntegerView are considered here.
  const std::vector<ImpliedBoundEntry>& GetImpliedBounds(IntegerVariable var);

  // Returns all the variables for which GetImpliedBounds(var) is not empty. Or
  // at least that was not empty at some point, because we lazily remove bounds
  // that become trivial as the search progress.
  const std::vector<IntegerVariable>& VariablesWithImpliedBounds() const {
    return has_implied_bounds_.PositionsSetAtLeastOnce();
  }

  // Adds to the integer trail all the new level-zero deduction made here.
  // This can only be called at decision level zero. Returns false iff the model
  // is infeasible.
  bool EnqueueNewDeductions();

  // When a literal does not have an integer view, we do not add any
  // ImpliedBoundEntry. This allows to create missing entries for a literal for
  // which a view was just created.
  //
  // TODO(user): Implement and call when we create new views in the linear
  // relaxation.
  void NotifyNewIntegerView(Literal literal) {}

 private:
  const SatParameters& parameters_;
  SatSolver* sat_solver_;
  IntegerTrail* integer_trail_;
  IntegerEncoder* integer_encoder_;

  // TODO(user): Remove the need for this.
  std::vector<IntegerLiteral> tmp_integer_literals_;

  // For each (Literal, IntegerVariable) the best lower bound implied by this
  // literal. Note that there is no need to store any entries that do not
  // improve on the level zero lower bound.
  //
  // TODO(user): we could lazily remove old entries to save a bit of space if
  // many deduction where made at level zero.
  absl::flat_hash_map<std::pair<LiteralIndex, IntegerVariable>, IntegerValue>
      bounds_;

  // Note(user): The plan is to use these during cut generation, so only the
  // Literal with an IntegerView that can be used in the LP relaxation need to
  // be kept here.
  //
  // TODO(user): Use inlined vectors.
  std::vector<ImpliedBoundEntry> empty_implied_bounds_;
  absl::StrongVector<IntegerVariable, std::vector<ImpliedBoundEntry>>
      var_to_bounds_;

  // Track the list of variables with some implied bounds.
  SparseBitset<IntegerVariable> has_implied_bounds_;

  // TODO(user): Ideally, this should go away if we manage to push level-zero
  // fact at a positive level directly.
  absl::StrongVector<IntegerVariable, IntegerValue> level_zero_lower_bounds_;
  SparseBitset<IntegerVariable> new_level_zero_bounds_;

  // Stats.
  int64 num_deductions_ = 0;
  int64 num_enqueued_in_var_to_bounds_ = 0;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_IMPLIED_BOUNDS_H_
