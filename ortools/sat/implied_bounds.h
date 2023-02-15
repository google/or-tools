// Copyright 2010-2022 Google LLC
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
#include <array>
#include <bitset>
#include <cstdint>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/linear_constraint.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/util/bitset.h"
#include "ortools/util/strong_integers.h"

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
        integer_encoder_(model->GetOrCreate<IntegerEncoder>()),
        shared_stats_(model->GetOrCreate<SharedStatistics>()) {}
  ~ImpliedBounds();

  // Adds literal => integer_literal to the repository.
  //
  // Not that it checks right aways if there is another bound on the same
  // variable involving literal.Negated(), in which case we can improve the
  // level zero lower bound of the variable.
  bool Add(Literal literal, IntegerLiteral integer_literal);

  // Adds literal => var == value.
  void AddLiteralImpliesVarEqValue(Literal literal, IntegerVariable var,
                                   IntegerValue value);

  // This must be called after first_decision has been enqueued and propagated.
  // It will inspect the trail and add all new implied bounds.
  //
  // Preconditions: The decision level must be one (CHECKed). And the decision
  // must be equal to first decision (we currently do not CHECK that).
  bool ProcessIntegerTrail(Literal first_decision);

  // Returns all the implied bounds stored for the given variable.
  // Note that only literal with an IntegerView are considered here.
  const std::vector<ImpliedBoundEntry>& GetImpliedBounds(IntegerVariable var);

  // Returns all the variables for which GetImpliedBounds(var) is not empty. Or
  // at least that was not empty at some point, because we lazily remove bounds
  // that become trivial as the search progress.
  const std::vector<IntegerVariable>& VariablesWithImpliedBounds() const {
    return has_implied_bounds_.PositionsSetAtLeastOnce();
  }

  // Returns all the implied values stored for a given literal.
  const absl::flat_hash_map<IntegerVariable, IntegerValue>& GetImpliedValues(
      Literal literal) const {
    const auto it = literal_to_var_to_value_.find(literal.Index());
    return it != literal_to_var_to_value_.end() ? it->second
                                                : empty_var_to_value_;
  }

  // Register the fact that var = sum literal * value with sum literal == 1.
  // Note that we call this an "element" encoding because a value can appear
  // more than once.
  void AddElementEncoding(IntegerVariable var,
                          const std::vector<ValueLiteralPair>& encoding,
                          int exactly_one_index);

  // Returns an empty map if there is no such encoding.
  const absl::flat_hash_map<int, std::vector<ValueLiteralPair>>&
  GetElementEncodings(IntegerVariable var);

  // Get an unsorted set of variables appearing in element encodings.
  const std::vector<IntegerVariable>& GetElementEncodedVariables() const;

  // Adds to the integer trail all the new level-zero deduction made here.
  // This can only be called at decision level zero. Returns false iff the model
  // is infeasible.
  bool EnqueueNewDeductions();

 private:
  const SatParameters& parameters_;
  SatSolver* sat_solver_;
  IntegerTrail* integer_trail_;
  IntegerEncoder* integer_encoder_;
  SharedStatistics* shared_stats_;

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

  // Note(user): This is currently only used during cut generation, so only the
  // Literal with an IntegerView that can be used in the LP relaxation need to
  // be kept here.
  //
  // TODO(user): Use inlined vectors. Even better, we actually only process
  // all variables at once, so no need to organize it by IntegerVariable even
  // if that might be more friendly cache-wise.
  std::vector<ImpliedBoundEntry> empty_implied_bounds_;
  absl::StrongVector<IntegerVariable, std::vector<ImpliedBoundEntry>>
      var_to_bounds_;
  SparseBitset<IntegerVariable> has_implied_bounds_;

  // Stores implied values per variable.
  absl::flat_hash_map<LiteralIndex,
                      absl::flat_hash_map<IntegerVariable, IntegerValue>>
      literal_to_var_to_value_;
  const absl::flat_hash_map<IntegerVariable, IntegerValue> empty_var_to_value_;

  absl::flat_hash_map<IntegerVariable,
                      absl::flat_hash_map<int, std::vector<ValueLiteralPair>>>
      var_to_index_to_element_encodings_;
  const absl::flat_hash_map<int, std::vector<ValueLiteralPair>>
      empty_element_encoding_;
  std::vector<IntegerVariable> element_encoded_variables_;

  // Stats.
  int64_t num_deductions_ = 0;
  int64_t num_enqueued_in_var_to_bounds_ = 0;
};

// Tries to decompose a product left * right in a list of constant alternative
// left_value * right_value controlled by literals in an exactly one
// relationship. We construct this by using literals from the full encoding or
// element encodings of the variables of the two affine expressions.
// If it fails, it returns an empty vector.
std::vector<LiteralValueValue> TryToDecomposeProduct(
    const AffineExpression& left, const AffineExpression& right, Model* model);

// Looks at value encodings and detects if the product of two variables can be
// linearized.
//
// In the returned encoding, note that all the literals will be unique and in
// exactly one relation, and that the values can be duplicated. This is what we
// call an "element" encoding.
//
// The expressions will also be canonical.
bool DetectLinearEncodingOfProducts(const AffineExpression& left,
                                    const AffineExpression& right, Model* model,
                                    LinearConstraintBuilder* builder);

// Class used to detect and hold all the information about a variable beeing the
// product of two others. This class is meant to be used by LP relaxation and
// cuts.
class ProductDetector {
 public:
  explicit ProductDetector(Model* model);
  ~ProductDetector();

  // Internally, a Boolean product is encoded in a linear fashion:
  // p = a * b become:
  // 1/ a and b => p, i.e.  a clause (not(a), not(b), p).
  // 2/ p => a and p => b, which is a clause (not(p), a) and (not(p), b).
  //
  // In particular if we have a+b+c==1 then we have a=b*c, b=a*c, and c=a*b !!
  //
  // For the detection to work, we must load all ternary clause first, then the
  // implication.
  void ProcessTernaryClause(absl::Span<const Literal> ternary_clause);
  void ProcessTernaryExactlyOne(absl::Span<const Literal> ternary_exo);
  void ProcessBinaryClause(absl::Span<const Literal> binary_clause);

  // Utility function to process a bunch of implication all at once.
  void ProcessImplicationGraph(BinaryImplicationGraph* graph);
  void ProcessTrailAtLevelOne();

  // We also detect product of Boolean with IntegerVariable.
  // After presolve, a product P = l * X should be encoded with:
  //      l => P = X
  // not(l) => P = 0
  //
  // TODO(user): Generalize to a * X + b = l * (Y + c) since these are also
  // easy to linearize if we see l * Y.
  void ProcessConditionalEquality(Literal l, IntegerVariable x,
                                  IntegerVariable y);
  void ProcessConditionalZero(Literal l, IntegerVariable p);

  // Query function mainly used for testing.
  LiteralIndex GetProduct(Literal a, Literal b) const;
  IntegerVariable GetProduct(Literal a, IntegerVariable b) const;

  // Query Functions. LinearizeProduct() should only be called if
  // ProductIsLinearizable() is true.
  bool ProductIsLinearizable(IntegerVariable a, IntegerVariable b) const;

  // TODO(user): Implement!
  LinearExpression LinearizeProduct(IntegerVariable a, IntegerVariable b);

  // Returns an expression that is always lower or equal to the product a * b.
  // This use the exact LinearizeProduct() if ProductIsLinearizable() otherwise
  // it uses the simple McCormick lower bound.
  //
  // TODO(user): Implement!
  LinearExpression ProductLowerBound(IntegerVariable a, IntegerVariable b);

 private:
  std::array<LiteralIndex, 2> GetKey(LiteralIndex a, LiteralIndex b) const;
  void ProcessNewProduct(LiteralIndex p, LiteralIndex a, LiteralIndex b);
  void ProcessNewProduct(IntegerVariable p, Literal l, IntegerVariable x);

  // Fixed at creation time.
  bool enabled_;
  SatSolver* sat_solver_;
  Trail* trail_;
  IntegerTrail* integer_trail_;
  IntegerEncoder* integer_encoder_;
  SharedStatistics* shared_stats_;

  // No need to process implication a => b if a was never seen.
  absl::StrongVector<LiteralIndex, bool> seen_;

  // For each clause of size 3 (l0, l1, l2) and a permutation of index (i, j, k)
  // we bitset[i] to true if lj => not(lk) and lk => not(lj).
  //
  // The key is sorted.
  absl::flat_hash_map<std::array<LiteralIndex, 3>, std::bitset<3>> detector_;

  // For each (l0, l1) we list all the l2 such that (l0, l1, l2) is a 3 clause.
  absl::flat_hash_map<std::array<LiteralIndex, 2>, std::vector<LiteralIndex>>
      candidates_;

  // Products (a, b) -> p such that p == a * b. They key is sorted.
  absl::flat_hash_map<std::array<LiteralIndex, 2>, LiteralIndex> products_;

  // Same keys has in products_ but canonicalized so we capture all 4 products
  // a * b, (1 - a) * b, a * (1 - b) and (1 - a) * (1 - b) with one query.
  absl::flat_hash_set<std::array<LiteralIndex, 2>> has_product_;

  // For bool * int detection. Note that we only use positive IntegerVariable
  // in the key part.
  absl::flat_hash_set<std::pair<LiteralIndex, IntegerVariable>>
      conditional_zeros_;
  absl::flat_hash_map<std::pair<LiteralIndex, IntegerVariable>,
                      std::vector<IntegerVariable>>
      conditional_equalities_;

  // Stores l * X = P.
  absl::flat_hash_map<std::pair<LiteralIndex, IntegerVariable>, IntegerVariable>
      int_products_;

  // Stats.
  int64_t num_products_ = 0;
  int64_t num_int_products_ = 0;
  int64_t num_trail_updates_ = 0;
  int64_t num_processed_binary_ = 0;
  int64_t num_processed_ternary_ = 0;
  int64_t num_processed_exo_ = 0;
  int64_t num_conditional_zeros_ = 0;
  int64_t num_conditional_equalities_ = 0;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_IMPLIED_BOUNDS_H_
