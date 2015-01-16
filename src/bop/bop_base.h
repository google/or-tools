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

#ifndef OR_TOOLS_BOP_BOP_BASE_H_
#define OR_TOOLS_BOP_BOP_BASE_H_

#include <string>

#include "base/basictypes.h"
#include "base/mutex.h"
#include "bop/bop_parameters.pb.h"
#include "bop/bop_solution.h"
#include "lp_data/lp_types.h"
#include "sat/boolean_problem.pb.h"
#include "sat/sat_base.h"
#include "sat/clause.h"
#include "util/stats.h"
#include "util/time_limit.h"

namespace operations_research {
namespace bop {

// Forward declaration.
struct LearnedInfo;

// This class represents the current state of the problem with all the
// information that the solver learned about it at a given time.
class ProblemState {
 public:
  explicit ProblemState(const LinearBooleanProblem& problem);

  // Sets parameters, used for instance to get the tolerance, the gap limit...
  void SetParameters(const BopParameters& parameters) {
    parameters_ = parameters;
  }

  // Merges the learned information with the current problem state. For
  // instance, if variables x, and y are fixed in the current state, and z is
  // learned to be fixed, the result of the merge will be x, y, and z being
  // fixed in the problem state.
  // Note that the LP values contained in the learned information (if any)
  // will replace the LP values of the problem state, whatever the cost is.
  // Returns true when the merge has changed the problem state.
  bool MergeLearnedInfo(const LearnedInfo& learned_info);

  // Returns all the information learned so far.
  // TODO(user): In the current implementation the learned information only
  //              contains binary clauses added since the last call to
  //              SynchronizationDone().
  //              Add an iterator on the sat::BinaryClauseManager.
  LearnedInfo GetLearnedInfo() const;

  // Marks the problem state as optimal.
  void MarkAsOptimal();

  // Marks the problem state as infeasible.
  void MarkAsInfeasible();

  // Returns true when the current state is proved to be optimal. In such a case
  // solution() returns the optimal solution.
  bool IsOptimal() const {
    return solution_.IsFeasible() && solution_.GetCost() == lower_bound();
  }

  // Returns true when the problem is proved to be infeasible.
  bool IsInfeasible() const { return lower_bound() > upper_bound(); }

  // Returns true when the variable var is fixed in the current problem state.
  // The value of the fixed variable is returned by GetVariableFixedValue(var).
  bool IsVariableFixed(VariableIndex var) const { return is_fixed_[var]; }
  const ITIVector<VariableIndex, bool>& is_fixed() const { return is_fixed_; }

  // Returns the value of the fixed variable var. Should be only called on fixed
  // variables (CHECKed).
  bool GetVariableFixedValue(VariableIndex var) const {
    return fixed_values_[var];
  }
  const ITIVector<VariableIndex, bool>& fixed_values() const {
    return fixed_values_;
  }

  // Returns the values of the LP relaxation of the problem. Returns an empty
  // vector when the LP has not been populated.
  const glop::DenseRow& lp_values() const { return lp_values_; }

  // Returns the solution to the current state problem.
  // Note that the solution might not be feasible because until we find one, it
  // will just be the all-false assignment.
  const BopSolution& solution() const { return solution_; }

  // Returns the original problem. Note that the current problem might be
  // different, e.g. fixed variables, but equivalent, i.e. a solution to one
  // should be a solution to the other too.
  const LinearBooleanProblem& original_problem() const {
    return original_problem_;
  }

  // Returns the current lower (resp. upper) bound of the objective cost.
  // For internal use only: this is the unscaled version of the lower (resp.
  // upper) bound, and so should be compared only to the unscaled cost given by
  // solution.GetCost().
  int64 lower_bound() const { return lower_bound_; }
  int64 upper_bound() const { return upper_bound_; }

  // Returns the scaled lower bound of the original problem.
  double GetScaledLowerBound() const {
    return lower_bound() * original_problem_.objective().scaling_factor() +
           original_problem_.objective().offset();
  }

  // Returns the newly added binary clause since the last SynchronizationDone().
  const std::vector<sat::BinaryClause>& NewlyAddedBinaryClauses() const;

  // Resets what is considered "new" information. This is meant to be called
  // once all the optimize have been synchronized.
  void SynchronizationDone();

 private:
  const LinearBooleanProblem& original_problem_;
  BopParameters parameters_;
  ITIVector<VariableIndex, bool> is_fixed_;
  ITIVector<VariableIndex, bool> fixed_values_;
  glop::DenseRow lp_values_;
  BopSolution solution_;

  int64 lower_bound_;
  int64 upper_bound_;

  // Manage the set of the problem binary clauses (including the learned ones).
  sat::BinaryClauseManager binary_clause_manager_;

  DISALLOW_COPY_AND_ASSIGN(ProblemState);
};

// This struct represents what has been learned on the problem state by
// running an optimizer. The goal is then to merge the learned information
// with the problem state in order to get a more constrained problem to be used
// by the next called optimizer.
struct LearnedInfo {
  explicit LearnedInfo(const LinearBooleanProblem& problem)
      : fixed_literals(),
        solution(problem, "AllZero"),
        lower_bound(kint64min),
        lp_values(),
        binary_clauses() {}

  // Clears all just as if the object were a brand new one. This can be used
  // to reduce the number of creation / deletion of objects.
  void Clear() {
    fixed_literals.clear();
    lower_bound = kint64min;
    lp_values.clear();
    binary_clauses.clear();
  }

  // Vector of all literals that have been fixed.
  std::vector<sat::Literal> fixed_literals;

  // New solution. Note that the solution might be infeasible.
  BopSolution solution;

  // A lower bound (for multi-threading purpose).
  int64 lower_bound;

  // An assignment for the relaxed linear programming problem (can be empty).
  // This is meant to be the optimal LP solution, but can just be a feasible
  // solution or any floating point assignment if the LP solver didn't solve
  // the relaxed problem optimally.
  glop::DenseRow lp_values;

  // New binary clauses.
  std::vector<sat::BinaryClause> binary_clauses;
};

// Thread-safe storage of LearnedInfo for each stamp. This class is used in
// a multi-threading context to synchronize learned information between solvers.
class StampedLearnedInfo {
 public:
  StampedLearnedInfo();

  // Adds the learned_info at position stamp.
  // Requires the learned_infos to be added sequentially, i.e. the stamp should
  // not already exist, and should be +1 of the current hightest stored stamp.
  void AddLearnedInfo(SolverTimeStamp stamp, const LearnedInfo& learned_info)
      LOCKS_EXCLUDED(mutex_);

  // Gets the learned_info corresponding to the given stamp.
  // Returns false when the info doesn't and won't exist because the search
  // was stopped.
  // Note that when the search is not marked as stopped, this method will wait
  // for the given stamp to be added.
  bool GetLearnedInfo(SolverTimeStamp stamp, LearnedInfo* learned_info)
      LOCKS_EXCLUDED(mutex_);

  // Marks the search as stopped, the last stamp is reached.
  void MarkLastStampReached() LOCKS_EXCLUDED(mutex_);

 private:
  // Returns true when the last stamp has been reached, i.e. no more stamps
  // will be stored. This might be due to a reached time limit.
  // The caller must hold the mutex_ (Asserted).
  bool LastStampReached() const EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  // The caller must hold the mutex_ (Asserted).
  bool ContainsStamp(SolverTimeStamp stamp) const
      EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  struct MutexConditionInfo {
    MutexConditionInfo(StampedLearnedInfo* i, SolverTimeStamp s)
        : learned_infos(i), stamp(s) {}
    const StampedLearnedInfo* learned_infos;
    SolverTimeStamp stamp;
  };

  // Static function used as a Condition to wait for available info at a given
  // stamp.
  // Returns true when the info is available.
  static bool SatisfiesStampCondition(MutexConditionInfo* const m)
      NO_THREAD_SAFETY_ANALYSIS;

  ITIVector<SolverTimeStamp, LearnedInfo> learned_infos_ GUARDED_BY(mutex_);
  bool last_stamp_reached_ GUARDED_BY(mutex_);
  // TODO(user): Use ReadMutex and WriteMutex?
  mutable Mutex mutex_;
};

// Base class used to optimize a ProblemState.
// Optimizers implementing this class are used in a sort of portfolio and
// are run sequentially or concurrently. See for instance BopRandomLNSOptimizer.
class BopOptimizerBase {
 public:
  explicit BopOptimizerBase(const std::string& name);
  virtual ~BopOptimizerBase();

  // Returns true if it's useless to run the optimizer several times on the
  // same solution.
  virtual bool RunOncePerSolution() const = 0;

  // Returns true if the optimizer needs a feasible solution to run.
  virtual bool NeedAFeasibleSolution() const = 0;

  // TODO(user): To redesign, some are not needed anymore thanks to the
  //              problem state, e.g. IsOptimal().
  enum Status {
    OPTIMAL_SOLUTION_FOUND,
    SOLUTION_FOUND,
    INFEASIBLE,
    LIMIT_REACHED,
    CONTINUE,
    ABORT
  };

  // Resets all internal structures to start a new round of Optimize() calls
  // with a new problem state.
  // Returns the status of the optimizer for the current problem state, e.g.
  // returns INFEASIBLE when the problem is proved UNSAT, ABORT when the
  // optimizer already knows it can't optimize the problem state...
  // Note that the underlying problem state object might change during search;
  // If possible copy needed objects, keep a pointer on them only with great
  // caution.
  virtual Status Synchronize(const ProblemState& problem_state) = 0;

  // Tries to infer more information about the problem state, i.e. reduces the
  // gap by increasing the lower bound or finding a better solution.
  // Returns SOLUTION_FOUND when a new solution with a better objective cost is
  // found before a time limit.
  // The learned information is cleared and the filled with any new information
  // about the problem, e.g. a new lower bound.
  virtual Status Optimize(const BopParameters& parameters,
                          LearnedInfo* learned_info, TimeLimit* time_limit) = 0;

  std::string name() const { return name_; }

  // Returns a std::string describing the status.
  static std::string GetStatusString(Status status);

 protected:
  const std::string name_;

  mutable StatsGroup stats_;
};

inline std::ostream& operator<<(std::ostream& os,
                                BopOptimizerBase::Status status) {
  os << BopOptimizerBase::GetStatusString(status);
  return os;
}

}  // namespace bop
}  // namespace operations_research
#endif  // OR_TOOLS_BOP_BOP_BASE_H_
