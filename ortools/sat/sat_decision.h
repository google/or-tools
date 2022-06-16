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

#ifndef OR_TOOLS_SAT_SAT_DECISION_H_
#define OR_TOOLS_SAT_SAT_DECISION_H_

#include <cstdint>
#include <utility>
#include <vector>

#include "ortools/base/integral_types.h"
#include "ortools/base/strong_vector.h"
#include "ortools/sat/model.h"
#include "ortools/sat/pb_constraint.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/util.h"
#include "ortools/util/bitset.h"
#include "ortools/util/integer_pq.h"

namespace operations_research {
namespace sat {

// Implement the SAT branching policy responsible for deciding the next Boolean
// variable to branch on, and its polarity (true or false).
class SatDecisionPolicy {
 public:
  explicit SatDecisionPolicy(Model* model);

  // Notifies that more variables are now present. Note that currently this may
  // change the current variable order because the priority queue need to be
  // reconstructed.
  void IncreaseNumVariables(int num_variables);

  // Reinitializes the decision heuristics (which variables to choose with which
  // polarity) according to the current parameters. Note that this also resets
  // the activity of the variables to 0. Note that this function is lazy, and
  // the work will only happen on the first NextBranch() to cover the cases when
  // this policy is not used at all.
  void ResetDecisionHeuristic();

  // Returns next decision to branch upon. This shouldn't be called if all the
  // variables are assigned.
  Literal NextBranch();

  // Updates statistics about literal occurrences in constraints.
  // Input is a canonical linear constraint of the form (terms <= rhs).
  void UpdateWeightedSign(const std::vector<LiteralWithCoeff>& terms,
                          Coefficient rhs);

  // Bumps the activity of all variables appearing in the conflict. All literals
  // must be currently assigned. See VSIDS decision heuristic: Chaff:
  // Engineering an Efficient SAT Solver. M.W. Moskewicz et al. ANNUAL ACM IEEE
  // DESIGN AUTOMATION CONFERENCE 2001.
  void BumpVariableActivities(const std::vector<Literal>& literals);

  // Updates the increment used for activity bumps. This is basically the same
  // as decaying all the variable activities, but it is a lot more efficient.
  void UpdateVariableActivityIncrement();

  // Called on Untrail() so that we can update the set of possible decisions.
  void Untrail(int target_trail_index);

  // Called on a new conflict before Untrail(). The trail before the given index
  // is used in the phase saving heuristic as a partial assignment.
  void BeforeConflict(int trail_index);

  // By default, we alternate between a stable phase (better suited for finding
  // SAT solution) and a more restart heavy phase more suited for proving UNSAT.
  // This changes a bit the polarity heuristics and is controlled from within
  // SatRestartPolicy.
  void SetStablePhase(bool is_stable) { in_stable_phase_ = is_stable; }
  bool InStablePhase() const { return in_stable_phase_; }

  // This is used to temporarily disable phase_saving when we do some probing
  // during search for instance.
  void MaybeEnablePhaseSaving(bool save_phase) {
    maybe_enable_phase_saving_ = save_phase;
  }

  // Gives a hint so the solver tries to find a solution with the given literal
  // set to true. Currently this take precedence over the phase saving heuristic
  // and a variable with a preference will always be branched on according to
  // this preference.
  //
  // The weight is used as a tie-breaker between variable with the same
  // activities. Larger weight will be selected first. A weight of zero is the
  // default value for the other variables.
  //
  // Note(user): Having a lot of different weights may slow down the priority
  // queue operations if there is millions of variables.
  void SetAssignmentPreference(Literal literal, double weight);

  // Returns the vector of the current assignment preferences.
  std::vector<std::pair<Literal, double>> AllPreferences() const;

  // Returns the current activity of a BooleanVariable.
  double Activity(Literal l) const {
    if (l.Variable() < activities_.size()) return activities_[l.Variable()];
    return 0.0;
  }

 private:
  // Computes an initial variable ordering.
  void InitializeVariableOrdering();

  // Rescales activity value of all variables when one of them reached the max.
  void RescaleVariableActivities(double scaling_factor);

  // Reinitializes the initial polarity of all the variables with an index
  // greater than or equal to the given one.
  void ResetInitialPolarity(int from, bool inverted = false);

  // Code used for resetting the initial polarity at the beginning of each
  // phase.
  void RephaseIfNeeded();
  void UseLongestAssignmentAsInitialPolarity();
  void FlipCurrentPolarity();
  void RandomizeCurrentPolarity();

  // Adds the given variable to var_ordering_ or updates its priority if it is
  // already present.
  void PqInsertOrUpdate(BooleanVariable var);

  // Singleton model objects.
  const SatParameters& parameters_;
  const Trail& trail_;
  ModelRandomGenerator* random_;

  // Variable ordering (priority will be adjusted dynamically). queue_elements_
  // holds the elements used by var_ordering_ (it uses pointers).
  //
  // Note that we recover the variable that a WeightedVarQueueElement refers to
  // by its position in the queue_elements_ vector, and we can recover the later
  // using (pointer - &queue_elements_[0]).
  struct WeightedVarQueueElement {
    // Interface for the IntegerPriorityQueue.
    int Index() const { return var.value(); }

    // Priority order. The IntegerPriorityQueue returns the largest element
    // first.
    //
    // Note(user): We used to also break ties using the variable index, however
    // this has two drawbacks:
    // - On problem with many variables, this slow down quite a lot the priority
    //   queue operations (which do as little work as possible and hence benefit
    //   from having the majority of elements with a priority of 0).
    // - It seems to be a bad heuristics. One reason could be that the priority
    //   queue will automatically diversify the choice of the top variables
    //   amongst the ones with the same priority.
    //
    // Note(user): For the same reason as explained above, it is probably a good
    // idea not to have too many different values for the tie_breaker field. I
    // am not even sure we should have such a field...
    bool operator<(const WeightedVarQueueElement& other) const {
      return weight < other.weight ||
             (weight == other.weight && (tie_breaker < other.tie_breaker));
    }

    BooleanVariable var;
    float tie_breaker;

    // TODO(user): Experiment with float. In the rest of the code, we use
    // double, but maybe we don't need that much precision. Using float here may
    // save memory and make the PQ operations faster.
    double weight;
  };
  static_assert(sizeof(WeightedVarQueueElement) == 16,
                "ERROR_WeightedVarQueueElement_is_not_well_compacted");

  bool var_ordering_is_initialized_ = false;
  IntegerPriorityQueue<WeightedVarQueueElement> var_ordering_;

  // This is used for the branching heuristic described in "Learning Rate Based
  // Branching Heuristic for SAT solvers", J.H.Liang, V. Ganesh, P. Poupart,
  // K.Czarnecki, SAT 2016.
  //
  // The entries are sorted by trail index, and one can get the number of
  // conflicts during which a variable at a given trail index i was assigned by
  // summing the entry.count for all entries with a trail index greater than i.
  struct NumConflictsStackEntry {
    int trail_index;
    int64_t count;
  };
  int64_t num_conflicts_ = 0;
  std::vector<NumConflictsStackEntry> num_conflicts_stack_;

  // Whether the priority of the given variable needs to be updated in
  // var_ordering_. Note that this is only accessed for assigned variables and
  // that for efficiency it is indexed by trail indices. If
  // pq_need_update_for_var_at_trail_index_[trail_->Info(var).trail_index] is
  // true when we untrail var, then either var need to be inserted in the queue,
  // or we need to notify that its priority has changed.
  BitQueue64 pq_need_update_for_var_at_trail_index_;

  // Increment used to bump the variable activities.
  double variable_activity_increment_ = 1.0;

  // Stores variable activity and the number of time each variable was "bumped".
  // The later is only used with the ERWA heuristic.
  absl::StrongVector<BooleanVariable, double> activities_;
  absl::StrongVector<BooleanVariable, double> tie_breakers_;
  absl::StrongVector<BooleanVariable, int64_t> num_bumps_;

  // If the polarity if forced (externally) we always use this first.
  absl::StrongVector<BooleanVariable, bool> has_forced_polarity_;
  absl::StrongVector<BooleanVariable, bool> forced_polarity_;

  // If we are in a stable phase, we follow the current target.
  bool in_stable_phase_ = false;
  int target_length_ = 0;
  absl::StrongVector<BooleanVariable, bool> has_target_polarity_;
  absl::StrongVector<BooleanVariable, bool> target_polarity_;

  // Otherwise we follow var_polarity_ which is reset at the beginning of
  // each new polarity phase. This is also overwritten by phase saving.
  // Each phase last for an arithmetically increasing number of conflicts.
  absl::StrongVector<BooleanVariable, bool> var_polarity_;
  bool maybe_enable_phase_saving_ = true;
  int64_t polarity_phase_ = 0;
  int64_t num_conflicts_until_rephase_ = 1000;

  // The longest partial assignment since the last reset.
  std::vector<Literal> best_partial_assignment_;

  // Used in initial polarity computation.
  absl::StrongVector<BooleanVariable, double> weighted_sign_;

  // Used in InitializeVariableOrdering().
  std::vector<BooleanVariable> tmp_variables_;
};

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_SAT_DECISION_H_
