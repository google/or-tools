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

// This file contains implementations of several resource constraints.
// The implemented constraints are:
// * Disjunctive: forces a set of intervals to be non-overlapping
// * Cumulative: forces a set of intervals with associated demands to be such
//     that the sum of demands of the intervals containing any given integer
//     does not exceed a capacity.
// In addition, it implements the SequenceVar that allows ranking decisions
// on a set of interval variables.

#include <algorithm>
#include "base/hash.h"
#include <string>
#include <vector>

#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stringprintf.h"
#include "base/join.h"
#include "base/stl_util.h"
#include "base/mathutil.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/bitset.h"
#include "util/monoid_operation_tree.h"
#include "util/saturated_arithmetic.h"
#include "util/string_array.h"

// TODO(user) Should these remains flags, or should they move to
// SolverParameters?
DEFINE_bool(cp_use_cumulative_edge_finder, true,
            "Use the O(n log n) cumulative edge finding algorithm described "
            "in 'Edge Finding Filtering Algorithm for Discrete  Cumulative "
            "Resources in O(kn log n)' by Petr Vilim, CP 2009.");
DEFINE_bool(cp_use_cumulative_time_table, true,
            "Use a O(n^2) cumulative time table propagation algorithm.");
DEFINE_bool(cp_use_sequence_high_demand_tasks, true,
            "Use a sequence constraints for cumulative tasks that have a "
            "demand greater than half of the capacity of the resource.");
DEFINE_bool(cp_use_all_possible_disjunctions, true,
            "Post temporal disjunctions for all pairs of tasks sharing a "
            "cumulative resource and that cannot overlap because the sum of "
            "their demand exceeds the capacity.");

namespace operations_research {
namespace {
// ----- Comparison functions -----

// TODO(user): Tie breaking.

// Comparison methods, used by the STL sort.
template <class Task>
bool StartMinLessThan(Task* const w1, Task* const w2) {
  return (w1->interval->StartMin() < w2->interval->StartMin());
}

template <class Task>
bool StartMaxLessThan(Task* const w1, Task* const w2) {
  return (w1->interval->StartMax() < w2->interval->StartMax());
}

template <class Task>
bool EndMinLessThan(Task* const w1, Task* const w2) {
  return (w1->interval->EndMin() < w2->interval->EndMin());
}

template <class Task>
bool EndMaxLessThan(Task* const w1, Task* const w2) {
  return (w1->interval->EndMax() < w2->interval->EndMax());
}

// ----- Wrappers around intervals -----

// A DisjunctiveTask is a non-preemptive task sharing a disjunctive resource.
// That is, it corresponds to an interval, and this interval cannot overlap with
// any other interval of a DisjunctiveTask sharing the same resource.
// It is indexed, that is it is aware of its position in a reference array.
struct DisjunctiveTask {
  explicit DisjunctiveTask(IntervalVar* const interval_)
      : interval(interval_), index(-1) {}

  std::string DebugString() const { return interval->DebugString(); }

  IntervalVar* interval;
  int index;
};

// A CumulativeTask is a non-preemptive task sharing a cumulative resource.
// That is, it corresponds to an interval and a demand. The sum of demands of
// all cumulative tasks CumulativeTasks sharing a resource of capacity c those
// intervals contain any integer t cannot exceed c.
// It is indexed, that is it is aware of its position in a reference array.
struct CumulativeTask {
  CumulativeTask(IntervalVar* const interval_, int64 demand_)
      : interval(interval_), demand(demand_), index(-1) {}

  int64 EnergyMin() const { return interval->DurationMin() * demand; }

  int64 DemandMin() const { return demand; }

  void WhenAnything(Demon* const demon) {
    interval->WhenAnything(demon);
  }

  std::string DebugString() const {
    return StringPrintf("Task{ %s, demand: %" GG_LL_FORMAT "d }",
                        interval->DebugString().c_str(), demand);
  }

  IntervalVar* interval;
  int64 demand;
  int index;
};

// A VariableCumulativeTask is a non-preemptive task sharing a
// cumulative resource.  That is, it corresponds to an interval and a
// demand. The sum of demands of all cumulative tasks
// VariableCumulativeTasks sharing a resource of capacity c those
// intervals contain any integer t cannot exceed c.  It is indexed,
// that is it is aware of its position in a reference array.
struct VariableCumulativeTask {
  VariableCumulativeTask(IntervalVar* const interval_, IntVar* demand_)
      : interval(interval_), demand(demand_), index(-1) {}

  int64 EnergyMin() const { return interval->DurationMin() * demand->Min(); }

  int64 DemandMin() const { return demand->Min(); }

  void WhenAnything(Demon* const demon) {
    interval->WhenAnything(demon);
    demand->WhenRange(demon);
  }

  std::string DebugString() const {
    return StringPrintf("Task{ %s, demand: %" GG_LL_FORMAT "d }",
                        interval->DebugString().c_str(), demand);
  }

  IntervalVar* interval;
  IntVar* demand;
  int index;
};

// ---------- Theta-Trees ----------

// This is based on Petr Vilim (public) PhD work.
// All names comes from his work. See http://vilim.eu/petr.

// Node of a Theta-tree
struct ThetaNode {
  // Identity element
  ThetaNode() : total_processing(0), total_ect(kint64min) {}

  // Single interval element
  explicit ThetaNode(const IntervalVar* const interval)
      : total_processing(interval->DurationMin()),
        total_ect(interval->EndMin()) {}

  void Compute(const ThetaNode& left, const ThetaNode& right) {
    total_processing = left.total_processing + right.total_processing;
    total_ect =
        std::max(left.total_ect + right.total_processing, right.total_ect);
  }

  bool IsIdentity() const {
    return total_processing == 0LL && total_ect == kint64min;
  }

  std::string DebugString() const {
    return StringPrintf("ThetaNode{ p = %" GG_LL_FORMAT "d, e = %" GG_LL_FORMAT
                        "d }",
                        total_processing, total_ect < 0LL ? -1LL : total_ect);
  }

  int64 total_processing;
  int64 total_ect;
};

// A theta-tree is a container for a set of intervals supporting the following
// operations:
// * Insertions and deletion in O(log size_), with size_ the maximal number of
//     tasks the tree may contain;
// * Querying the following quantity in O(1):
//     Max_{subset S of the set of contained intervals} (
//             Min_{i in S}(i.StartMin) + Sum_{i in S}(i.DurationMin) )
class ThetaTree : public MonoidOperationTree<ThetaNode> {
 public:
  explicit ThetaTree(int size) : MonoidOperationTree<ThetaNode>(size) {}

  int64 Ect() const { return result().total_ect; }

  void Insert(const DisjunctiveTask* const task) {
    Set(task->index, ThetaNode(task->interval));
  }

  void Remove(const DisjunctiveTask* const task) { Reset(task->index); }

  bool IsInserted(const DisjunctiveTask* const task) const {
    return !GetOperand(task->index).IsIdentity();
  }
};

// ----------------- Lambda Theta Tree -----------------------

// Lambda-theta-node
// These nodes are cumulative lambda theta-node. This is reflected in the
// terminology. They can also be used in the disjunctive case, and this incurs
// no performance penalty.
struct LambdaThetaNode {
  // Special value for task indices meaning 'no such task'.
  static const int kNone;

  // Identity constructor
  LambdaThetaNode()
      : energy(0LL),
        energetic_end_min(kint64min),
        energy_opt(0LL),
        argmax_energy_opt(kNone),
        energetic_end_min_opt(kint64min),
        argmax_energetic_end_min_opt(kNone) {}

  // Constructor for a single cumulative task in the Theta set
  LambdaThetaNode(int64 capacity, const CumulativeTask& task)
      : energy(task.EnergyMin()),
        energetic_end_min(capacity * task.interval->StartMin() + energy),
        energy_opt(energy),
        argmax_energy_opt(kNone),
        energetic_end_min_opt(energetic_end_min),
        argmax_energetic_end_min_opt(kNone) {}

  // Constructor for a single cumulative task in the Lambda set
  LambdaThetaNode(int64 capacity, const CumulativeTask& task, int index)
      : energy(0LL),
        energetic_end_min(kint64min),
        energy_opt(task.EnergyMin()),
        argmax_energy_opt(index),
        energetic_end_min_opt(capacity * task.interval->StartMin() +
                              energy_opt),
        argmax_energetic_end_min_opt(index) {
    DCHECK_GE(index, 0);
  }

  // Constructor for a single cumulative task in the Theta set
  LambdaThetaNode(int64 capacity, const VariableCumulativeTask& task)
      : energy(task.EnergyMin()),
        energetic_end_min(capacity * task.interval->StartMin() + energy),
        energy_opt(energy),
        argmax_energy_opt(kNone),
        energetic_end_min_opt(energetic_end_min),
        argmax_energetic_end_min_opt(kNone) {}

  // Constructor for a single cumulative task in the Lambda set
  LambdaThetaNode(int64 capacity, const VariableCumulativeTask& task, int index)
      : energy(0LL),
        energetic_end_min(kint64min),
        energy_opt(task.EnergyMin()),
        argmax_energy_opt(index),
        energetic_end_min_opt(capacity * task.interval->StartMin() +
                              energy_opt),
        argmax_energetic_end_min_opt(index) {
    DCHECK_GE(index, 0);
  }

  // Constructor for a single interval in the Theta set
  explicit LambdaThetaNode(const IntervalVar* const interval)
      : energy(interval->DurationMin()),
        energetic_end_min(interval->EndMin()),
        energy_opt(interval->DurationMin()),
        argmax_energy_opt(kNone),
        energetic_end_min_opt(interval->EndMin()),
        argmax_energetic_end_min_opt(kNone) {}

  // Constructor for a single interval in the Lambda set
  // 'index' is the index of the given interval in the est vector
  LambdaThetaNode(const IntervalVar* const interval, int index)
      : energy(0LL),
        energetic_end_min(kint64min),
        energy_opt(interval->DurationMin()),
        argmax_energy_opt(index),
        energetic_end_min_opt(interval->EndMin()),
        argmax_energetic_end_min_opt(index) {
    DCHECK_GE(index, 0);
  }

  // Sets this LambdaThetaNode to the result of the natural binary operations
  // over the two given operands, corresponding to the following set operations:
  // Theta = left.Theta union right.Theta
  // Lambda = left.Lambda union right.Lambda
  //
  // No set operation actually occur: we only maintain the relevant quantities
  // associated with such sets.
  void Compute(const LambdaThetaNode& left, const LambdaThetaNode& right) {
    energy = left.energy + right.energy;
    energetic_end_min = std::max(right.energetic_end_min,
                                 left.energetic_end_min + right.energy);
    const int64 energy_left_opt = left.energy_opt + right.energy;
    const int64 energy_right_opt = left.energy + right.energy_opt;
    if (energy_left_opt > energy_right_opt) {
      energy_opt = energy_left_opt;
      argmax_energy_opt = left.argmax_energy_opt;
    } else {
      energy_opt = energy_right_opt;
      argmax_energy_opt = right.argmax_energy_opt;
    }
    const int64 ect1 = right.energetic_end_min_opt;
    const int64 ect2 = left.energetic_end_min + right.energy_opt;
    const int64 ect3 = left.energetic_end_min_opt + right.energy;
    if (ect1 >= ect2 && ect1 >= ect3) {  // ect1 max
      energetic_end_min_opt = ect1;
      argmax_energetic_end_min_opt = right.argmax_energetic_end_min_opt;
    } else if (ect2 >= ect1 && ect2 >= ect3) {  // ect2 max
      energetic_end_min_opt = ect2;
      argmax_energetic_end_min_opt = right.argmax_energy_opt;
    } else {  // ect3 max
      energetic_end_min_opt = ect3;
      argmax_energetic_end_min_opt = left.argmax_energetic_end_min_opt;
    }
    // The processing time, with one grey interval, should be no less than
    // without any grey interval.
    DCHECK(energy_opt >= energy);
    // If there is no responsible grey interval for the processing time,
    // the processing time with a grey interval should equal the one
    // without.
    DCHECK((argmax_energy_opt != kNone) || (energy_opt == energy));
  }

  // Amount of resource consumed by the Theta set, in units of demand X time.
  // This is energy(Theta).
  int64 energy;

  // Max_{subset S of Theta} (capacity * start_min(S) + energy(S))
  int64 energetic_end_min;

  // Max_{i in Lambda} (energy(Theta union {i}))
  int64 energy_opt;

  // The argmax in energy_opt_. It is the index of the chosen task in the Lambda
  // set, if any, or kNone if none.
  int argmax_energy_opt;

  // Max_{subset S of Theta, i in Lambda}
  //     (capacity * start_min(S union {i}) + energy(S union {i}))
  int64 energetic_end_min_opt;

  // The argmax in energetic_end_min_opt_. It is the index of the chosen task in
  // the Lambda set, if any, or kNone if none.
  int argmax_energetic_end_min_opt;
};

const int LambdaThetaNode::kNone = -1;

// Disjunctive Lambda-Theta tree
class DisjunctiveLambdaThetaTree : public MonoidOperationTree<LambdaThetaNode> {
 public:
  explicit DisjunctiveLambdaThetaTree(int size)
      : MonoidOperationTree<LambdaThetaNode>(size) {}

  void Insert(const DisjunctiveTask& task) {
    Set(task.index, LambdaThetaNode(task.interval));
  }

  void Grey(const DisjunctiveTask& task) {
    const int index = task.index;
    Set(index, LambdaThetaNode(task.interval, index));
  }

  int64 Ect() const { return result().energetic_end_min; }
  int64 EctOpt() const { return result().energetic_end_min_opt; }
  int ResponsibleOpt() const { return result().argmax_energetic_end_min_opt; }
};

// A cumulative lambda-theta tree
class CumulativeLambdaThetaTree : public MonoidOperationTree<LambdaThetaNode> {
 public:
  CumulativeLambdaThetaTree(int size, int64 capacity)
      : MonoidOperationTree<LambdaThetaNode>(size), capacity_(capacity) {}

  void Insert(const CumulativeTask& task) {
    Set(task.index, LambdaThetaNode(capacity_, task));
  }

  void Grey(const CumulativeTask& task) {
    const int index = task.index;
    Set(index, LambdaThetaNode(capacity_, task, index));
  }

  void Insert(const VariableCumulativeTask& task) {
    Set(task.index, LambdaThetaNode(capacity_, task));
  }

  void Grey(const VariableCumulativeTask& task) {
    const int index = task.index;
    Set(index, LambdaThetaNode(capacity_, task, index));
  }

  int64 energetic_end_min() const { return result().energetic_end_min; }
  int64 energetic_end_min_opt() const { return result().energetic_end_min_opt; }
  int64 Ect() const {
    return MathUtil::CeilOfRatio(energetic_end_min(), capacity_);
  }
  int64 EctOpt() const {
    return MathUtil::CeilOfRatio(result().energetic_end_min_opt, capacity_);
  }
  int argmax_energetic_end_min_opt() const {
    return result().argmax_energetic_end_min_opt;
  }

 private:
  const int64 capacity_;
};

// -------------- Not Last -----------------------------------------

// A class that implements the 'Not-Last' propagation algorithm for the unary
// resource constraint.
class NotLast {
 public:
  NotLast(Solver* const solver, const std::vector<IntervalVar*>& intervals,
          bool mirror);

  ~NotLast() { STLDeleteElements(&by_start_min_); }

  bool Propagate();

 private:
  ThetaTree theta_tree_;
  std::vector<DisjunctiveTask*> by_start_min_;
  std::vector<DisjunctiveTask*> by_end_max_;
  std::vector<DisjunctiveTask*> by_start_max_;
  std::vector<int64> new_lct_;
};

NotLast::NotLast(Solver* const solver,
                 const std::vector<IntervalVar*>& intervals, bool mirror)
    : theta_tree_(intervals.size()),
      by_start_min_(intervals.size()),
      by_end_max_(intervals.size()),
      by_start_max_(intervals.size()),
      new_lct_(intervals.size(), -1LL) {
  // Populate the different vectors.
  for (int i = 0; i < intervals.size(); ++i) {
    IntervalVar* const underlying =
        mirror ? solver->MakeMirrorInterval(intervals[i]) : intervals[i];
    IntervalVar* const relaxed = solver->MakeIntervalRelaxedMin(underlying);
    by_start_min_[i] = new DisjunctiveTask(relaxed);
    by_end_max_[i] = by_start_min_[i];
    by_start_max_[i] = by_start_min_[i];
  }
}

bool NotLast::Propagate() {
  // ---- Init ----
  std::sort(by_start_max_.begin(), by_start_max_.end(),
            StartMaxLessThan<DisjunctiveTask>);
  std::sort(by_end_max_.begin(), by_end_max_.end(),
            EndMaxLessThan<DisjunctiveTask>);
  // Update start min positions
  std::sort(by_start_min_.begin(), by_start_min_.end(),
            StartMinLessThan<DisjunctiveTask>);
  for (int i = 0; i < by_start_min_.size(); ++i) {
    by_start_min_[i]->index = i;
  }
  theta_tree_.Clear();
  for (int i = 0; i < by_start_min_.size(); ++i) {
    new_lct_[i] = by_start_min_[i]->interval->EndMax();
  }

  // --- Execute ----
  int j = 0;
  for (int i = 0; i < by_start_max_.size(); ++i) {
    DisjunctiveTask* const twi = by_end_max_[i];
    while (j < by_start_max_.size() &&
           twi->interval->EndMax() > by_start_max_[j]->interval->StartMax()) {
      if (j > 0 && theta_tree_.Ect() > by_start_max_[j]->interval->StartMax()) {
        const int64 new_end_max = by_start_max_[j - 1]->interval->StartMax();
        new_lct_[by_start_max_[j]->index] = new_end_max;
      }
      theta_tree_.Insert(by_start_max_[j]);
      j++;
    }
    const bool inserted = theta_tree_.IsInserted(twi);
    if (inserted) {
      theta_tree_.Remove(twi);
    }
    const int64 ect_theta_less_i = theta_tree_.Ect();
    if (inserted) {
      theta_tree_.Insert(twi);
    }
    if (ect_theta_less_i > twi->interval->EndMax() && j > 0) {
      const int64 new_end_max = by_start_max_[j - 1]->interval->EndMax();
      if (new_end_max > new_lct_[twi->index]) {
        new_lct_[twi->index] = new_end_max;
      }
    }
  }

  // Apply modifications
  bool modified = false;
  for (int i = 0; i < by_start_min_.size(); ++i) {
    if (by_start_min_[i]->interval->EndMax() > new_lct_[i]) {
      modified = true;
      by_start_min_[i]->interval->SetEndMax(new_lct_[i]);
    }
  }
  return modified;
}

// ------ Edge finder + detectable precedences -------------

// A class that implements two propagation algorithms: edge finding and
// detectable precedences. These algorithms both push intervals to the right,
// which is why they are grouped together.
class EdgeFinderAndDetectablePrecedences {
 public:
  EdgeFinderAndDetectablePrecedences(Solver* const solver,
                                     const std::vector<IntervalVar*>& intervals,
                                     bool mirror);
  ~EdgeFinderAndDetectablePrecedences() { STLDeleteElements(&by_start_min_); }
  int64 size() const { return by_start_min_.size(); }
  IntervalVar* interval(int index) { return by_start_min_[index]->interval; }
  void UpdateEst();
  void OverloadChecking();
  bool DetectablePrecedences();
  bool EdgeFinder();

 private:
  Solver* const solver_;

  // --- All the following member variables are essentially used as local ones:
  // no invariant is maintained about them, except for the fact that the vectors
  // always contains all the considered intervals, so any function that wants to
  // use them must first sort them in the right order.

  // All of these vectors store the same set of objects. Therefore, at
  // destruction time, STLDeleteElements should be called on only one of them.
  // It does not matter which one.

  ThetaTree theta_tree_;
  std::vector<DisjunctiveTask*> by_end_min_;
  std::vector<DisjunctiveTask*> by_start_min_;
  std::vector<DisjunctiveTask*> by_end_max_;
  std::vector<DisjunctiveTask*> by_start_max_;
  // new_est_[i] is the new start min for interval est_[i]->interval.
  std::vector<int64> new_est_;
  // new_lct_[i] is the new end max for interval est_[i]->interval.
  std::vector<int64> new_lct_;
  DisjunctiveLambdaThetaTree lt_tree_;
};

EdgeFinderAndDetectablePrecedences::EdgeFinderAndDetectablePrecedences(
    Solver* const solver, const std::vector<IntervalVar*>& intervals,
    bool mirror)
    : solver_(solver),
      theta_tree_(intervals.size()),
      lt_tree_(intervals.size()) {
  // Populate of the array of intervals
  for (int i = 0; i < intervals.size(); ++i) {
    IntervalVar* const underlying =
        mirror ? solver->MakeMirrorInterval(intervals[i]) : intervals[i];
    IntervalVar* const relaxed = solver->MakeIntervalRelaxedMax(underlying);
    DisjunctiveTask* const w = new DisjunctiveTask(relaxed);
    by_end_min_.push_back(w);
    by_start_min_.push_back(w);
    by_end_max_.push_back(w);
    by_start_max_.push_back(w);
    new_est_.push_back(kint64min);
  }
}

void EdgeFinderAndDetectablePrecedences::UpdateEst() {
  std::sort(by_start_min_.begin(), by_start_min_.end(),
            StartMinLessThan<DisjunctiveTask>);
  for (int i = 0; i < size(); ++i) {
    by_start_min_[i]->index = i;
  }
}

void EdgeFinderAndDetectablePrecedences::OverloadChecking() {
  // Initialization.
  UpdateEst();
  std::sort(by_end_max_.begin(), by_end_max_.end(),
            EndMaxLessThan<DisjunctiveTask>);
  theta_tree_.Clear();

  for (int i = 0; i < size(); ++i) {
    DisjunctiveTask* const task = by_end_max_[i];
    theta_tree_.Insert(task);
    if (theta_tree_.Ect() > task->interval->EndMax()) {
      solver_->Fail();
    }
  }
}

bool EdgeFinderAndDetectablePrecedences::DetectablePrecedences() {
  // Initialization.
  UpdateEst();
  for (int i = 0; i < size(); ++i) {
    new_est_[i] = kint64min;
  }

  // Propagate in one direction
  std::sort(by_end_min_.begin(), by_end_min_.end(),
            EndMinLessThan<DisjunctiveTask>);
  std::sort(by_start_max_.begin(), by_start_max_.end(),
            StartMaxLessThan<DisjunctiveTask>);
  theta_tree_.Clear();
  int j = 0;
  for (int i = 0; i < size(); ++i) {
    DisjunctiveTask* const task_i = by_end_min_[i];
    if (j < size()) {
      DisjunctiveTask* task_j = by_start_max_[j];
      while (task_i->interval->EndMin() > task_j->interval->StartMax()) {
        theta_tree_.Insert(task_j);
        j++;
        if (j == size()) break;
        task_j = by_start_max_[j];
      }
    }
    const int64 esti = task_i->interval->StartMin();
    bool inserted = theta_tree_.IsInserted(task_i);
    if (inserted) {
      theta_tree_.Remove(task_i);
    }
    const int64 oesti = theta_tree_.Ect();
    if (inserted) {
      theta_tree_.Insert(task_i);
    }
    if (oesti > esti) {
      new_est_[task_i->index] = oesti;
    } else {
      new_est_[task_i->index] = kint64min;
    }
  }

  // Apply modifications
  bool modified = false;
  for (int i = 0; i < size(); ++i) {
    if (new_est_[i] != kint64min) {
      modified = true;
      by_start_min_[i]->interval->SetStartMin(new_est_[i]);
    }
  }
  return modified;
}

bool EdgeFinderAndDetectablePrecedences::EdgeFinder() {
  // Initialization.
  UpdateEst();
  for (int i = 0; i < size(); ++i) {
    new_est_[i] = by_start_min_[i]->interval->StartMin();
  }

  // Push in one direction.
  std::sort(by_end_max_.begin(), by_end_max_.end(),
            EndMaxLessThan<DisjunctiveTask>);
  lt_tree_.Clear();
  for (int i = 0; i < size(); ++i) {
    lt_tree_.Insert(*by_start_min_[i]);
    DCHECK_EQ(i, by_start_min_[i]->index);
  }
  for (int j = size() - 2; j >= 0; --j) {
    lt_tree_.Grey(*by_end_max_[j + 1]);
    DisjunctiveTask* const twj = by_end_max_[j];
    // We should have checked for overloading earlier.
    DCHECK_LE(lt_tree_.Ect(), twj->interval->EndMax());
    while (lt_tree_.EctOpt() > twj->interval->EndMax()) {
      const int i = lt_tree_.ResponsibleOpt();
      DCHECK_GE(i, 0);
      if (lt_tree_.Ect() > new_est_[i]) {
        new_est_[i] = lt_tree_.Ect();
      }
      lt_tree_.Reset(i);
    }
  }

  // Apply modifications.
  bool modified = false;
  for (int i = 0; i < size(); ++i) {
    if (by_start_min_[i]->interval->StartMin() < new_est_[i]) {
      modified = true;
      by_start_min_[i]->interval->SetStartMin(new_est_[i]);
    }
  }
  return modified;
}

// --------- Disjunctive Constraint ----------

// ----- Propagation on ranked activities -----

class RankedPropagator : public Constraint {
 public:
  RankedPropagator(Solver* const solver, const std::vector<IntVar*>& nexts,
                   const std::vector<IntervalVar*>& intervals,
                   const std::vector<IntVar*>& slacks,
                   DisjunctiveConstraint* const disjunctive)
      : Constraint(solver),
        nexts_(nexts),
        intervals_(intervals),
        slacks_(slacks),
        disjunctive_(disjunctive),
        partial_sequence_(intervals.size()),
        previous_(intervals.size() + 2, 0) {}

  virtual ~RankedPropagator() {}

  virtual void Post() {
    Demon* const delayed =
        solver()->MakeDelayedConstraintInitialPropagateCallback(this);
    for (int i = 0; i < intervals_.size(); ++i) {
      nexts_[i]->WhenBound(delayed);
      intervals_[i]->WhenAnything(delayed);
      slacks_[i]->WhenRange(delayed);
    }
    nexts_.back()->WhenBound(delayed);
  }

  virtual void InitialPropagate() {
    PropagateNexts();
    PropagateSequence();
  }

  void PropagateNexts() {
    Solver* const s = solver();
    const int ranked_first = partial_sequence_.NumFirstRanked();
    const int ranked_last = partial_sequence_.NumLastRanked();
    const int sentinel =
        ranked_last == 0
            ? nexts_.size()
            : partial_sequence_[intervals_.size() - ranked_last] + 1;
    int first = 0;
    int counter = 0;
    while (nexts_[first]->Bound()) {
      DCHECK_NE(first, nexts_[first]->Min());
      first = nexts_[first]->Min();
      if (first == sentinel) {
        return;
      }
      if (++counter > ranked_first) {
        DCHECK(intervals_[first - 1]->MayBePerformed());
        partial_sequence_.RankFirst(s, first - 1);
        VLOG(1) << "RankFirst " << first - 1 << " -> "
                << partial_sequence_.DebugString();
      }
    }
    for (int i = 0; i < previous_.size(); ++i) {
      previous_[i] = -1;
    }
    for (int i = 0; i < nexts_.size(); ++i) {
      if (nexts_[i]->Bound()) {
        previous_[nexts_[i]->Min()] = i;
      }
    }
    int last = previous_.size() - 1;
    counter = 0;
    while (previous_[last] != -1) {
      last = previous_[last];
      if (++counter > ranked_last) {
        partial_sequence_.RankLast(s, last - 1);
        VLOG(1) << "RankLast " << last - 1 << " -> "
                << partial_sequence_.DebugString();
      }
    }
  }

  void PropagateSequence() {
    const int last_position = intervals_.size() - 1;
    const int first_sentinel = partial_sequence_.NumFirstRanked();
    const int last_sentinel = last_position - partial_sequence_.NumLastRanked();
    // Propagates on ranked first from left to right.
    for (int i = 0; i < first_sentinel - 1; ++i) {
      IntervalVar* const interval = RankedInterval(i);
      IntervalVar* const next_interval = RankedInterval(i + 1);
      IntVar* const slack = RankedSlack(i);
      const int64 transition_time = RankedTransitionTime(i, i + 1);
      next_interval->SetStartRange(
          CapAdd(interval->StartMin(), slack->Min() + transition_time),
          CapAdd(interval->StartMax(), slack->Max() + transition_time));
    }
    // Propagates on ranked last from right to left.
    for (int i = last_position; i > last_sentinel + 1; --i) {
      IntervalVar* const interval = RankedInterval(i - 1);
      IntervalVar* const next_interval = RankedInterval(i);
      IntVar* const slack = RankedSlack(i - 1);
      const int64 transition_time = RankedTransitionTime(i - 1, i);
      interval->SetStartRange(
          CapSub(next_interval->StartMin(), slack->Max() + transition_time),
          CapSub(next_interval->StartMax(), slack->Min() + transition_time));
    }
    // Propagate across.
    IntervalVar* const first_interval =
        first_sentinel > 0 ? RankedInterval(first_sentinel - 1) : nullptr;
    IntVar* const first_slack =
        first_sentinel > 0 ? RankedSlack(first_sentinel - 1) : nullptr;
    IntervalVar* const last_interval = last_sentinel < last_position
                                           ? RankedInterval(last_sentinel + 1)
                                           : nullptr;

    // Nothing to do afterwards, exiting.
    if (first_interval == nullptr && last_interval == nullptr) {
      return;
    }
    // Propagates to the middle part.
    // This assumes triangular inequality in the transition times.
    for (int i = first_sentinel; i <= last_sentinel; ++i) {
      IntervalVar* const interval = RankedInterval(i);
      IntVar* const slack = RankedSlack(i);
      if (interval->MayBePerformed()) {
        const bool performed = interval->MustBePerformed();
        if (first_interval != nullptr) {
          const int64 transition_time =
              RankedTransitionTime(first_sentinel - 1, i);
          interval->SetStartRange(CapAdd(first_interval->StartMin(),
                                         first_slack->Min() + transition_time),
                                  CapAdd(first_interval->StartMax(),
                                         first_slack->Max() + transition_time));
          if (performed) {
            first_interval->SetStartRange(
                CapSub(interval->StartMin(),
                       first_slack->Max() + transition_time),
                CapSub(interval->StartMax(),
                       first_slack->Min() + transition_time));
          }
        }
        if (last_interval != nullptr) {
          const int64 transition_time =
              RankedTransitionTime(i, last_sentinel + 1);
          interval->SetStartRange(
              CapSub(last_interval->StartMin(), slack->Max() + transition_time),
              CapSub(last_interval->StartMax(),
                     slack->Min() + transition_time));
          if (performed) {
            last_interval->SetStartRange(
                CapAdd(interval->StartMin(), slack->Min() + transition_time),
                CapAdd(interval->StartMax(), slack->Max() + transition_time));
          }
        }
      }
    }
    // TODO(user): cache transition on ranked intervals in a vector.
    // Propagates on ranked first from right to left.
    for (int i = std::min(first_sentinel - 2, last_position - 1); i >= 0; --i) {
      IntervalVar* const interval = RankedInterval(i);
      IntervalVar* const next_interval = RankedInterval(i + 1);
      IntVar* const slack = RankedSlack(i);
      const int64 transition_time = RankedTransitionTime(i, i + 1);
      interval->SetStartRange(
          CapSub(next_interval->StartMin(), slack->Max() + transition_time),
          CapSub(next_interval->StartMax(), slack->Min() + transition_time));
    }
    // Propagates on ranked last from left to right.
    for (int i = last_sentinel + 1; i < last_position - 1; ++i) {
      IntervalVar* const interval = RankedInterval(i);
      IntervalVar* const next_interval = RankedInterval(i + 1);
      IntVar* const slack = RankedSlack(i);
      const int64 transition_time = RankedTransitionTime(i, i + 1);
      next_interval->SetStartRange(
          CapAdd(interval->StartMin(), slack->Min() + transition_time),
          CapAdd(interval->StartMax(), slack->Max() + transition_time));
    }
    // TODO(user) : Propagate on slacks.
  }

  IntervalVar* RankedInterval(int i) const {
    const int index = partial_sequence_[i];
    return intervals_[index];
  }

  IntVar* RankedSlack(int i) const {
    const int index = partial_sequence_[i];
    return slacks_[index];
  }

  int64 RankedTransitionTime(int before, int after) const {
    const int before_index = partial_sequence_[before];
    const int after_index = partial_sequence_[after];

    return disjunctive_->TransitionTime(before_index, after_index);
  }

  virtual std::string DebugString() const {
    return StringPrintf(
        "RankedPropagator([%s], nexts = [%s], intervals = [%s])",
        partial_sequence_.DebugString().c_str(),
        JoinDebugStringPtr(nexts_, ", ").c_str(),
        JoinDebugStringPtr(intervals_, ", ").c_str());
  }

  void Accept(ModelVisitor* const visitor) const {
    LOG(FATAL) << "Not yet implemented";
    // TODO(user): IMPLEMENT ME.
  }

 private:
  std::vector<IntVar*> nexts_;
  std::vector<IntervalVar*> intervals_;
  std::vector<IntVar*> slacks_;
  DisjunctiveConstraint* const disjunctive_;
  RevPartialSequence partial_sequence_;
  std::vector<int> previous_;
};

// A class that stores several propagators for the sequence constraint, and
// calls them until a fixpoint is reached.

class FullDisjunctiveConstraint : public DisjunctiveConstraint {
 public:
  FullDisjunctiveConstraint(Solver* const s,
                            const std::vector<IntervalVar*>& intervals,
                            const std::string& name)
      : DisjunctiveConstraint(s, intervals, name),
        sequence_var_(nullptr),
        straight_(s, intervals, false),
        mirror_(s, intervals, true),
        straight_not_last_(s, intervals, false),
        mirror_not_last_(s, intervals, true) {}

  virtual ~FullDisjunctiveConstraint() {}

  virtual void Post() {
    Demon* const d = MakeDelayedConstraintDemon0(
        solver(), this, &FullDisjunctiveConstraint::InitialPropagate,
        "InitialPropagate");
    for (int32 i = 0; i < straight_.size(); ++i) {
      straight_.interval(i)->WhenAnything(d);
    }
  }

  virtual void InitialPropagate() {
    do {
      do {
        do {
          // OverloadChecking is symmetrical. It has the same effect on the
          // straight and the mirrored version.
          straight_.OverloadChecking();
        } while (straight_.DetectablePrecedences() ||
                 mirror_.DetectablePrecedences());
      } while (straight_not_last_.Propagate() || mirror_not_last_.Propagate());
    } while (straight_.EdgeFinder() || mirror_.EdgeFinder());
  }

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitConstraint(ModelVisitor::kDisjunctive, this);
    visitor->VisitIntervalArrayArgument(ModelVisitor::kIntervalsArgument,
                                        intervals_);
    if (sequence_var_ != nullptr) {
      visitor->VisitSequenceArgument(ModelVisitor::kSequenceArgument,
                                     sequence_var_);
    }
    visitor->EndVisitConstraint(ModelVisitor::kDisjunctive, this);
  }

  virtual SequenceVar* MakeSequenceVar() {
    BuildNextModelIfNeeded();
    if (sequence_var_ == nullptr) {
      solver()->SaveValue(reinterpret_cast<void**>(&sequence_var_));
      sequence_var_ = solver()->RevAlloc(
          new SequenceVar(solver(), intervals_, nexts_, name()));
    }
    return sequence_var_;
  }

  virtual std::string DebugString() const {
    return StringPrintf("FullDisjunctiveConstraint([%s])",
                        JoinDebugStringPtr(intervals_, ",").c_str());
  }

  virtual const std::vector<IntVar*>& nexts() const { return nexts_; }

  virtual const std::vector<IntVar*>& actives() const { return actives_; }

  virtual const std::vector<IntVar*>& time_cumuls() const {
    return time_cumuls_;
  }

  virtual const std::vector<IntVar*>& time_slacks() const {
    return time_slacks_;
  }

 private:
  int64 Distance(int64 activity_plus_one, int64 next_activity_plus_one) {
    return (transition_time_.get() == nullptr || activity_plus_one == 0 ||
            next_activity_plus_one > intervals_.size())
               ? 0
               : transition_time_->Run(activity_plus_one - 1,
                                       next_activity_plus_one - 1);
  }

  void BuildNextModelIfNeeded() {
    if (!nexts_.empty()) {
      return;
    }
    Solver* const s = solver();
    const std::string& ct_name = name();
    const int num_intervals = intervals_.size();
    const int num_nodes = intervals_.size() + 1;
    int64 horizon = 0;
    for (int i = 0; i < intervals_.size(); ++i) {
      if (intervals_[i]->MayBePerformed()) {
        horizon = std::max(horizon, intervals_[i]->EndMax());
      }
    }

    // Create the next model.
    s->MakeIntVarArray(num_nodes, 1, num_nodes, ct_name + "_nexts", &nexts_);
    // Alldifferent on the nexts variable (the equivalent problem is a tsp).
    s->AddConstraint(s->MakeAllDifferent(nexts_));

    actives_.resize(num_nodes);
    for (int i = 0; i < num_intervals; ++i) {
      actives_[i + 1] = intervals_[i]->PerformedExpr()->Var();
      s->AddConstraint(
          s->MakeIsDifferentCstCt(nexts_[i + 1], i + 1, actives_[i + 1]));
    }
    std::vector<IntVar*> short_actives(actives_.begin() + 1, actives_.end());
    actives_[0] = s->MakeMax(short_actives)->Var();

    // No Cycle on the corresponding tsp.
    s->AddConstraint(s->MakeNoCycle(nexts_, actives_));

    // Cumul on time.
    time_cumuls_.resize(num_nodes + 1);
    // Slacks between activities.
    time_slacks_.resize(num_nodes);

    time_slacks_[0] = s->MakeIntVar(0, horizon, "initial_slack");
    // TODO(user): check this.
    time_cumuls_[0] = s->MakeIntConst(0);

    for (int64 i = 0; i < num_intervals; ++i) {
      IntervalVar* const var = intervals_[i];
      if (var->MayBePerformed()) {
        const int64 duration_min = var->DurationMin();
        time_slacks_[i + 1] = s->MakeIntVar(
            duration_min, horizon,
            StringPrintf("time_slacks(%" GG_LL_FORMAT "d)", i + 1));
        // TODO(user): Check SafeStartExpr();
        time_cumuls_[i + 1] = var->SafeStartExpr(var->StartMin())->Var();
        if (var->DurationMax() != duration_min) {
          s->AddConstraint(s->MakeGreaterOrEqual(
              time_slacks_[i + 1], var->SafeDurationExpr(duration_min)));
        }
      } else {
        time_slacks_[i + 1] = s->MakeIntVar(
            0, horizon, StringPrintf("time_slacks(%" GG_LL_FORMAT "d)", i + 1));
        time_cumuls_[i + 1] = s->MakeIntConst(horizon);
      }
    }
    // TODO(user): Find a better UB for the last time cumul.
    time_cumuls_[num_nodes] = s->MakeIntVar(0, 2 * horizon, ct_name + "_ect");
    s->AddConstraint(s->MakePathCumul(
        nexts_, actives_, time_cumuls_, time_slacks_,
        NewPermanentCallback(this, &FullDisjunctiveConstraint::Distance)));

    std::vector<IntVar*> short_slacks(time_slacks_.begin() + 1,
                                      time_slacks_.end());
    s->AddConstraint(s->RevAlloc(
        new RankedPropagator(s, nexts_, intervals_, short_slacks, this)));
  }

  SequenceVar* sequence_var_;
  EdgeFinderAndDetectablePrecedences straight_;
  EdgeFinderAndDetectablePrecedences mirror_;
  NotLast straight_not_last_;
  NotLast mirror_not_last_;
  std::vector<IntVar*> nexts_;
  std::vector<IntVar*> actives_;
  std::vector<IntVar*> time_cumuls_;
  std::vector<IntVar*> time_slacks_;
  DISALLOW_COPY_AND_ASSIGN(FullDisjunctiveConstraint);
};

// =====================================================================
//  Cumulative
// =====================================================================

// A cumulative Theta node, where two energies, corresponding to 2 capacities,
// are stored.
struct DualCapacityThetaNode {
  // Special value for task indices meaning 'no such task'.
  static const int kNone;

  // Identity constructor
  DualCapacityThetaNode()
      : energy(0LL),
        energetic_end_min(kint64min),
        residual_energetic_end_min(kint64min) {}

  // Constructor for a single cumulative task in the Theta set
  DualCapacityThetaNode(int64 capacity, int64 residual_capacity,
                        const CumulativeTask& task)
      : energy(task.EnergyMin()),
        energetic_end_min(capacity * task.interval->StartMin() + energy),
        residual_energetic_end_min(
            residual_capacity * task.interval->StartMin() + energy) {}

  // Constructor for a single cumulative task in the Theta set
  DualCapacityThetaNode(int64 capacity, int64 residual_capacity,
                        const VariableCumulativeTask& task)
      : energy(task.EnergyMin()),
        energetic_end_min(capacity * task.interval->StartMin() + energy),
        residual_energetic_end_min(
            residual_capacity * task.interval->StartMin() + energy) {}

  // Sets this DualCapacityThetaNode to the result of the natural binary
  // operation over the two given operands, corresponding to the following set
  // operation: Theta = left.Theta union right.Theta
  //
  // No set operation actually occur: we only maintain the relevant quantities
  // associated with such sets.
  void Compute(const DualCapacityThetaNode& left,
               const DualCapacityThetaNode& right) {
    energy = left.energy + right.energy;
    energetic_end_min = std::max(left.energetic_end_min + right.energy,
                                 right.energetic_end_min);
    residual_energetic_end_min =
        std::max(left.residual_energetic_end_min + right.energy,
                 right.residual_energetic_end_min);
  }

  // Amount of resource consumed by the Theta set, in units of demand X time.
  // This is energy(Theta).
  int64 energy;

  // Max_{subset S of Theta} (capacity * start_min(S) + energy(S))
  int64 energetic_end_min;

  // Max_{subset S of Theta} (residual_capacity * start_min(S) + energy(S))
  int64 residual_energetic_end_min;
};

const int DualCapacityThetaNode::kNone = -1;

// A tree for dual capacity theta nodes
class DualCapacityThetaTree
    : public MonoidOperationTree<DualCapacityThetaNode> {
 public:
  static const int64 kNotInitialized;

  DualCapacityThetaTree(int size, int64 capacity)
      : MonoidOperationTree<DualCapacityThetaNode>(size),
        capacity_(capacity),
        residual_capacity_(-1) {}

  virtual ~DualCapacityThetaTree() {}

  void Insert(const CumulativeTask* task) {
    Set(task->index,
        DualCapacityThetaNode(capacity_, residual_capacity_, *task));
  }

  void Insert(const VariableCumulativeTask* task) {
    Set(task->index,
        DualCapacityThetaNode(capacity_, residual_capacity_, *task));
  }

  void SetResidualCapacity(int residual_capacity) {
    Clear();
    DCHECK_LE(0, residual_capacity);
    DCHECK_LE(residual_capacity, capacity_);
    residual_capacity_ = residual_capacity;
  }

 private:
  const int64 capacity_;
  int64 residual_capacity_;
  DISALLOW_COPY_AND_ASSIGN(DualCapacityThetaTree);
};

const int64 DualCapacityThetaTree::kNotInitialized = -1LL;

// An object that can dive down a branch of a DualCapacityThetaTree to compute
// Env(j, c) in Petr Vilim's notations.
//
// In 'Edge finding filtering algorithm for discrete cumulative resources in
// O(kn log n)' by Petr Vilim, this corresponds to line 6--8 in algorithm 1.3,
// plus all of algorithm 1.2.
//
// http://vilim.eu/petr/cp2009.pdf
// Note: use the version pointed to by this pointer, not the version from the
// conference proceedings, which has a few errors.
class EnvJCComputeDiver {
 public:
  static const int64 kNotAvailable;
  explicit EnvJCComputeDiver(int energy_threshold)
      : energy_threshold_(energy_threshold),
        energy_alpha_(kNotAvailable),
        energetic_end_min_alpha_(kNotAvailable) {}
  void OnArgumentReached(int index, const DualCapacityThetaNode& argument) {
    energy_alpha_ = argument.energy;
    energetic_end_min_alpha_ = argument.energetic_end_min;
    // We should reach a leaf that is not the identity
    // DCHECK_GT(energetic_end_min_alpha_, kint64min);
    // TODO(user): Check me.
  }
  bool ChooseGoLeft(const DualCapacityThetaNode& current,
                    const DualCapacityThetaNode& left_child,
                    const DualCapacityThetaNode& right_child) {
    if (right_child.residual_energetic_end_min > energy_threshold_) {
      return false;  // enough energy on right
    } else {
      energy_threshold_ -= right_child.energy;
      return true;
    }
  }
  void OnComeBackFromLeft(const DualCapacityThetaNode& current,
                          const DualCapacityThetaNode& left_child,
                          const DualCapacityThetaNode& right_child) {
    // The left subtree intersects the alpha set.
    // The right subtree does not intersect the alpha set.
    // The energy_alpha_ and energetic_end_min_alpha_ previously
    // computed are valid for this node too: there's nothing to do.
  }
  void OnComeBackFromRight(const DualCapacityThetaNode& current,
                           const DualCapacityThetaNode& left_child,
                           const DualCapacityThetaNode& right_child) {
    // The left subtree is included in the alpha set.
    // The right subtree intersects the alpha set.
    energetic_end_min_alpha_ = std::max(
        energetic_end_min_alpha_, left_child.energetic_end_min + energy_alpha_);
    energy_alpha_ += left_child.energy;
  }
  int64 GetEnvJC(const DualCapacityThetaNode& root) const {
    const int64 energy = root.energy;
    const int64 energy_beta = energy - energy_alpha_;
    return energetic_end_min_alpha_ + energy_beta;
  }

 private:
  // Energy threshold such that if a set has an energetic_end_min greater than
  // the threshold, then it can push tasks that must end at or after the
  // currently considered end max.
  //
  // Used when diving down only.
  int64 energy_threshold_;

  // Energy of the alpha set, that is, the set of tasks whose start min does not
  // exceed the max start min of a set with excess residual energy.
  //
  // Used when swimming up only.
  int64 energy_alpha_;

  // Energetic end min of the alpha set.
  //
  // Used when swimming up only.
  int64 energetic_end_min_alpha_;
};

const int64 EnvJCComputeDiver::kNotAvailable = -1LL;

// A closure-like object that updates an interval.
class StartMinUpdater {
 public:
  StartMinUpdater(IntervalVar* interval, int64 new_start_min)
      : interval_(interval), new_start_min_(new_start_min) {}
  void Run() { interval_->SetStartMin(new_start_min_); }

 private:
  IntervalVar* interval_;
  int64 new_start_min_;
};

// In all the following, the term 'update' means 'a potential new start min for
// a task'. The edge-finding algorithm is in two phase: one compute potential
// new start mins, the other detects whether they are applicable or not for each
// task.

// Collection of all updates (i.e., potential new start mins) for a given value
// of the demand.
class UpdatesForADemand {
 public:
  explicit UpdatesForADemand(int size)
      : updates_(size, 0), up_to_date_(false) {}
  const std::vector<int64>& updates() { return updates_; }
  bool up_to_date() const { return up_to_date_; }
  void Reset() { up_to_date_ = false; }
  void SetUpdate(int index, int64 update) {
    DCHECK(!up_to_date_);
    updates_[index] = update;
  }
  void SetUpToDate() { up_to_date_ = true; }

 private:
  std::vector<int64> updates_;
  bool up_to_date_;
  DISALLOW_COPY_AND_ASSIGN(UpdatesForADemand);
};

// Returns std::min(a * b, kint64max). a is positive.
int64 SafeProduct(int64 a, int64 b) {
  DCHECK_GE(a, 0);

  const bool is_positive = b >= 0;
  b = std::max(b, -b);  // abs(b) for int64.
  // Note std::max(kint64min, -kint64min) = kint64min, so when b == kint64min,
  // the following DCHECK fails.
  DCHECK_GE(b, 0);
  const int kint64SurelyOverflow = 63;
  const bool surely_overflow =
      MostSignificantBitPosition64(a) + MostSignificantBitPosition64(b) >
      kint64SurelyOverflow;

  // When the sum of MostSignificantBitPosition64 for a and b is smaller
  // than 61 no overflow can appear and the product result is positive (as a
  // and b are positive). However when the sum is between 61 and 63 then it
  // is not possible to say in advance if an overflow will occur. Nevertheless
  // the product sign is a valid test for overflow: if an overflow occurs,
  // then the product is surely negative, and if the product is negative
  // then an overflow occurred.
  const int64 product = a * b;
  return (!surely_overflow && product >= 0)
             ? ((is_positive) ? product : -product)
             : ((is_positive) ? kint64max : -kint64max);
}

// One-sided cumulative edge finder.
template <class Task>
class EdgeFinder : public Constraint {
 public:
  EdgeFinder(Solver* const solver, const std::vector<Task*>& tasks,
             int64 capacity)
      : Constraint(solver),
        capacity_(capacity),
        by_start_min_(tasks.size()),
        by_end_max_(tasks.size()),
        by_end_min_(tasks.size()),
        lt_tree_(tasks.size(), capacity_) {
    // Populate
    for (int i = 0; i < tasks.size(); ++i) {
      by_start_min_[i] = tasks[i];
      by_end_max_[i] = tasks[i];
      by_end_min_[i] = tasks[i];
      const int64 demand = tasks[i]->DemandMin();
      // Create the UpdateForADemand if needed (the [] operator calls the
      // constructor). May rehash.
      if (update_map_[demand] == nullptr) {
        update_map_[demand] = new UpdatesForADemand(tasks.size());
      }
    }
  }

  virtual ~EdgeFinder() {
    STLDeleteElements(&by_start_min_);
    STLDeleteValues(&update_map_);
  }

  virtual void Post() {
    // Add the demons
    for (int i = 0; i < by_start_min_.size(); ++i) {
      // Delay propagation, as this constraint is not incremental: we pay
      // O(n log n) each time the constraint is awakened.
      Demon* const demon = MakeDelayedConstraintDemon0(
          solver(), this, &EdgeFinder::InitialPropagate, "RangeChanged");
      by_start_min_[i]->WhenAnything(demon);
    }
  }

  // The propagation algorithms: checks for overloading, computes new start mins
  // according to the edge-finding rules, and applies them.
  virtual void InitialPropagate() {
    InitPropagation();
    PropagateBasedOnEndMinGreaterThanEndMax();
    FillInTree();
    PropagateBasedOnEnergy();
    ApplyNewBounds();
  }

  void Accept(ModelVisitor* const visitor) const {
    LOG(FATAL) << "Should Not Be Visited";
  }

  virtual std::string DebugString() const { return "EdgeFinder"; }

 private:
  // Sets the fields in a proper state to run the propagation algorithm.
  void InitPropagation() {
    // Clear the update stack
    new_start_min_.clear();
    // sort y start min.
    std::sort(by_start_min_.begin(), by_start_min_.end(),
              StartMinLessThan<Task>);
    for (int i = 0; i < by_start_min_.size(); ++i) {
      by_start_min_[i]->index = i;
    }
    // Sort by end max.
    std::sort(by_end_max_.begin(), by_end_max_.end(), EndMaxLessThan<Task>);
    // Sort by end min.
    std::sort(by_end_min_.begin(), by_end_min_.end(), EndMinLessThan<Task>);
    // Clear tree.
    lt_tree_.Clear();
    // Clear updates
    for (const auto& entry : update_map_) {
      entry.second->Reset();
    }
  }

  // Computes all possible update values for tasks of given demand, and stores
  // these values in update_map_[demand].
  // Runs in O(n log n).
  // This corresponds to lines 2--13 in algorithm 1.3 in Petr Vilim's paper.
  void ComputeConditionalStartMins(int64 demand) {
    DCHECK_GT(demand, 0);
    UpdatesForADemand* updates = FindPtrOrNull(update_map_, demand);
    if (updates == nullptr) {  // Create on demand.
      updates = new UpdatesForADemand(by_start_min_.size());
      update_map_[demand] = updates;
    }
    DCHECK(!updates->up_to_date());
    DualCapacityThetaTree dual_capa_tree(by_start_min_.size(), capacity_);
    const int64 residual_capacity = capacity_ - demand;
    dual_capa_tree.SetResidualCapacity(residual_capacity);
    // It's important to initialize the update at IntervalVar::kMinValidValue
    // rather than at kInt64min, because its opposite may be used if it's a
    // mirror variable, and
    // -kInt64min = -(-kInt64max - 1) = kInt64max + 1 = -kInt64min
    int64 update = IntervalVar::kMinValidValue;
    for (int i = 0; i < by_end_max_.size(); ++i) {
      const int64 current_end_max = by_end_max_[i]->interval->EndMax();
      dual_capa_tree.Insert(by_end_max_[i]);
      const int64 energy_threshold = residual_capacity * current_end_max;
      const DualCapacityThetaNode& root = dual_capa_tree.result();
      const int64 res_energetic_end_min = root.residual_energetic_end_min;
      if (res_energetic_end_min > energy_threshold) {
        EnvJCComputeDiver diver(energy_threshold);
        dual_capa_tree.DiveInTree(&diver);
        const int64 enjv = diver.GetEnvJC(dual_capa_tree.result());
        const int64 numerator = enjv - energy_threshold;
        const int64 diff = MathUtil::CeilOfRatio(numerator, demand);
        update = std::max(update, diff);
      }
      updates->SetUpdate(i, update);
    }
    updates->SetUpToDate();
  }

  // Returns the new start min that can be inferred for task_to_push if it is
  // proved that it cannot end before by_end_max[end_max_index] does.
  int64 ConditionalStartMin(const Task& task_to_push, int end_max_index) {
    const int64 demand = task_to_push.DemandMin();
    if (!update_map_[demand]->up_to_date()) {
      ComputeConditionalStartMins(demand);
    }
    DCHECK(update_map_[demand]->up_to_date());
    return update_map_[demand]->updates().at(end_max_index);
  }

  // Propagates by discovering all end-after-end relationships purely based on
  // comparisons between end mins and end maxes: there is no energetic reasoning
  // here, but this allow updates that the standard edge-finding detection rule
  // misses.
  // See paragraph 6.2 in http://vilim.eu/petr/cp2009.pdf.
  void PropagateBasedOnEndMinGreaterThanEndMax() {
    int end_max_index = 0;
    int64 max_start_min = kint64min;
    for (int i = 0; i < by_start_min_.size(); ++i) {
      Task* const task = by_end_min_[i];
      const int64 end_min = task->interval->EndMin();
      while (end_max_index < by_start_min_.size() &&
             by_end_max_[end_max_index]->interval->EndMax() <= end_min) {
        max_start_min = std::max(
            max_start_min, by_end_max_[end_max_index]->interval->StartMin());
        ++end_max_index;
      }
      if (end_max_index > 0 && task->interval->StartMin() <= max_start_min &&
          task->interval->EndMax() > task->interval->EndMin()) {
        DCHECK_LE(by_end_max_[end_max_index - 1]->interval->EndMax(), end_min);
        // The update is valid and may be interesting:
        // * If task->StartMin() > max_start_min, then all tasks whose end_max
        //     is less than or equal to end_min have a start min that is less
        //     than task->StartMin(). In this case, any update we could
        //     compute would also be computed by the standard edge-finding
        //     rule. It's better not to compute it, then: it may not be
        //     needed.
        // * If task->EndMax() <= task->EndMin(), that means the end max is
        //     bound. In that case, 'task' itself belong to the set of tasks
        //     that must end before end_min, which may cause the result of
        //     ConditionalStartMin(task, end_max_index - 1) not to be a valid
        //     update.
        const int64 update = ConditionalStartMin(*task, end_max_index - 1);
        StartMinUpdater startMinUpdater(task->interval, update);
        new_start_min_.push_back(startMinUpdater);
      }
    }
  }

  // Fill the theta-lambda-tree, and check for overloading.
  void FillInTree() {
    for (int i = 0; i < by_start_min_.size(); ++i) {
      Task* const task = by_end_max_[i];
      lt_tree_.Insert(*task);
      // Maximum energetic end min without overload.
      const int64 max_feasible =
          SafeProduct(capacity_, task->interval->EndMax());
      if (lt_tree_.energetic_end_min() > max_feasible) {
        solver()->Fail();
      }
    }
  }

  // The heart of the propagation algorithm. Should be called with all tasks
  // being in the Theta set. It detects tasks that need to be pushed.
  void PropagateBasedOnEnergy() {
    for (int j = by_start_min_.size() - 2; j >= 0; --j) {
      lt_tree_.Grey(*by_end_max_[j + 1]);
      Task* const twj = by_end_max_[j];
      // We should have checked for overload earlier.
      const int64 max_feasible =
          SafeProduct(capacity_, twj->interval->EndMax());
      DCHECK_LE(lt_tree_.energetic_end_min(), max_feasible);
      while (lt_tree_.energetic_end_min_opt() > max_feasible) {
        const int i = lt_tree_.argmax_energetic_end_min_opt();
        DCHECK_GE(i, 0);
        PropagateTaskCannotEndBefore(i, j);
        lt_tree_.Reset(i);
      }
    }
  }

  // Takes into account the fact that the task of given index cannot end before
  // the given new end min.
  void PropagateTaskCannotEndBefore(int index, int end_max_index) {
    Task* const task_to_push = by_start_min_[index];
    const int64 update = ConditionalStartMin(*task_to_push, end_max_index);
    StartMinUpdater startMinUpdater(task_to_push->interval, update);
    new_start_min_.push_back(startMinUpdater);
  }

  // Applies the previously computed updates.
  void ApplyNewBounds() {
    for (int i = 0; i < new_start_min_.size(); ++i) {
      new_start_min_[i].Run();
    }
  }

  // Capacity of the cumulative resource.
  const int64 capacity_;

  // Cumulative tasks, ordered by non-decreasing start min.
  std::vector<Task*> by_start_min_;

  // Cumulative tasks, ordered by non-decreasing end max.
  std::vector<Task*> by_end_max_;

  // Cumulative tasks, ordered by non-decreasing end min.
  std::vector<Task*> by_end_min_;

  // Cumulative theta-lamba tree.
  CumulativeLambdaThetaTree lt_tree_;

  // Stack of updates to the new start min to do.
  std::vector<StartMinUpdater> new_start_min_;

  typedef hash_map<int64, UpdatesForADemand*> UpdateMap;

  // update_map_[d][i] is an integer such that if a task
  // whose demand is d cannot end before by_end_max_[i], then it cannot start
  // before update_map_[d][i].
  UpdateMap update_map_;

  DISALLOW_COPY_AND_ASSIGN(EdgeFinder);
};

// A point in time where the usage profile changes.
// Starting from time (included), the usage is what it was immediately before
// time, plus the delta.
//
// Example:
// Consider the following vector of ProfileDelta's:
// { t=1, d=+3}, { t=4, d=+1 }, { t=5, d=-2}, { t=8, d=-1}
// This represents the following usage profile:
//
// usage
// 4 |                   ****.
// 3 |       ************.   .
// 2 |       .           .   ************.
// 1 |       .           .   .           .
// 0 |*******----------------------------*******************-> time
//       0   1   2   3   4   5   6   7   8   9
//
// Note that the usage profile is right-continuous (see
// http://en.wikipedia.org/wiki/Left-continuous#Directional_continuity).
// This is because intervals for tasks are always closed on the start side
// and open on the end side.
struct ProfileDelta {
  ProfileDelta(int64 _time, int64 _delta) : time(_time), delta(_delta) {}
  int64 time;
  int64 delta;
};

bool TimeLessThan(const ProfileDelta& delta1, const ProfileDelta& delta2) {
  return delta1.time < delta2.time;
}

// Cumulative time-table.
//
// This class implements a propagator for the CumulativeConstraint which is not
// incremental, and where a call to InitialPropagate() takes time which is
// O(n^2) and Omega(n log n) with n the number of cumulative tasks.
//
// Despite the high complexity, this propagator is needed, because of those
// implemented, it is the only one that satisfy that if all instantiated, no
// contradiction will be detected if and only if the constraint is satisfied.
//
// The implementation is quite naive, and could certainly be improved, for
// example by maintaining the profile incrementally.
template <class Task>
class CumulativeTimeTable : public Constraint {
 public:
  CumulativeTimeTable(Solver* const solver, const std::vector<Task*>& tasks,
                      int64 capacity)
      : Constraint(solver), by_start_min_(tasks), capacity_(capacity) {
    // There may be up to 2 delta's per interval (one on each side),
    // plus two sentinels
    const int profile_max_size = 2 * NumTasks() + 2;
    profile_non_unique_time_.reserve(profile_max_size);
    profile_unique_time_.reserve(profile_max_size);
  }

  virtual ~CumulativeTimeTable() { STLDeleteElements(&by_start_min_); }

  virtual void InitialPropagate() {
    BuildProfile();
    PushTasks();
  }

  virtual void Post() {
    Demon* d = MakeDelayedConstraintDemon0(
        solver(), this, &CumulativeTimeTable::InitialPropagate,
        "InitialPropagate");
    for (int i = 0; i < NumTasks(); ++i) {
      by_start_min_[i]->WhenAnything(d);
    }
  }

  int NumTasks() const { return by_start_min_.size(); }

  void Accept(ModelVisitor* const visitor) const {
    LOG(FATAL) << "Should not be visited";
  }

  virtual std::string DebugString() const { return "CumulativeTimeTable"; }

 private:
  // Build the usage profile. Runs in O(n log n).
  void BuildProfile() {
    // Build profile with non unique time
    profile_non_unique_time_.clear();
    for (int i = 0; i < NumTasks(); ++i) {
      const Task* const task = by_start_min_[i];
      const IntervalVar* const interval = task->interval;
      const int64 start_max = interval->StartMax();
      const int64 end_min = interval->EndMin();
      if (interval->MustBePerformed() && start_max < end_min) {
        const int64 demand = task->DemandMin();
        if (demand > 0) {
          profile_non_unique_time_.push_back(ProfileDelta(start_max, +demand));
          profile_non_unique_time_.push_back(ProfileDelta(end_min, -demand));
        }
      }
    }
    // Sort
    std::sort(profile_non_unique_time_.begin(), profile_non_unique_time_.end(),
              TimeLessThan);
    // Build profile with unique times
    profile_unique_time_.clear();
    profile_unique_time_.push_back(ProfileDelta(kint64min, 0));
    for (int i = 0; i < profile_non_unique_time_.size(); ++i) {
      const ProfileDelta& profile_delta = profile_non_unique_time_[i];
      if (profile_delta.time == profile_unique_time_.back().time) {
        profile_unique_time_.back().delta += profile_delta.delta;
      } else {
        profile_unique_time_.push_back(profile_delta);
      }
    }
    // Re-scan profile to check max usage w.r.t. capacity, and check
    // final usage to be 0.
    int64 usage = 0;
    for (int i = 0; i < profile_unique_time_.size(); ++i) {
      const ProfileDelta& profile_delta = profile_unique_time_[i];
      usage += profile_delta.delta;
      if (usage > capacity_) {
        solver()->Fail();
      }
    }
    DCHECK_EQ(0, usage);
    profile_unique_time_.push_back(ProfileDelta(kint64max, 0));
  }

  // Update the start min for all tasks. Runs in O(n^2) and Omega(n).
  void PushTasks() {
    std::sort(by_start_min_.begin(), by_start_min_.end(),
              StartMinLessThan<Task>);
    int64 usage = 0;
    int profile_index = 0;
    for (int task_index = 0; task_index < NumTasks(); ++task_index) {
      const Task* const task = by_start_min_[task_index];
      while (task->interval->StartMin() >
             profile_unique_time_[profile_index].time) {
        DCHECK(profile_index < profile_unique_time_.size());
        ++profile_index;
        usage += profile_unique_time_[profile_index].delta;
      }
      PushTask(task, profile_index, usage);
    }
  }

  // Push the given task to new_start_min, defined as the smallest integer such
  // that the profile usage for all tasks, excluding the current one, does not
  // exceed capacity_ - task->demand on the interval
  // [new_start_min, new_start_min + task->interval->DurationMin() ).
  void PushTask(const Task* const task, int profile_index, int64 usage) {
    // Init
    const IntervalVar* const interval = task->interval;
    const int64 demand = task->DemandMin();
    if (demand == 0) {  // Demand can be null, nothing to propagate.
      return;
    }
    const int64 residual_capacity = capacity_ - demand;
    const int64 duration = task->interval->DurationMin();
    const ProfileDelta& first_prof_delta = profile_unique_time_[profile_index];

    int64 new_start_min = interval->StartMin();

    DCHECK_GE(first_prof_delta.time, interval->StartMin());
    // The check above is with a '>='. Let's first treat the '>' case
    if (first_prof_delta.time > interval->StartMin()) {
      // There was no profile delta at a time between interval->StartMin()
      // (included) and the current one.
      // As we don't delete delta's of 0 value, this means the current task
      // does not contribute to the usage before:
      DCHECK((interval->StartMax() >= first_prof_delta.time) ||
             (interval->StartMax() >= interval->EndMin()));
      // The 'usage' given in argument is valid at first_prof_delta.time. To
      // compute the usage at the start min, we need to remove the last delta.
      const int64 usage_at_start_min = usage - first_prof_delta.delta;
      if (usage_at_start_min > residual_capacity) {
        new_start_min = profile_unique_time_[profile_index].time;
      }
    }

    // Influence of current task
    const int64 start_max = interval->StartMax();
    const int64 end_min = interval->EndMin();
    ProfileDelta delta_start(start_max, 0);
    ProfileDelta delta_end(end_min, 0);
    if (interval->MustBePerformed() && start_max < end_min) {
      delta_start.delta = +demand;
      delta_end.delta = -demand;
    }
    while (profile_unique_time_[profile_index].time <
           duration + new_start_min) {
      const ProfileDelta& profile_delta = profile_unique_time_[profile_index];
      DCHECK(profile_index < profile_unique_time_.size());
      // Compensate for current task
      if (profile_delta.time == delta_start.time) {
        usage -= delta_start.delta;
      }
      if (profile_delta.time == delta_end.time) {
        usage -= delta_end.delta;
      }
      // Increment time
      ++profile_index;
      DCHECK(profile_index < profile_unique_time_.size());
      // Does it fit?
      if (usage > residual_capacity) {
        new_start_min = profile_unique_time_[profile_index].time;
      }
      usage += profile_unique_time_[profile_index].delta;
    }
    task->interval->SetStartMin(new_start_min);
  }

  typedef std::vector<ProfileDelta> Profile;

  Profile profile_unique_time_;
  Profile profile_non_unique_time_;
  std::vector<Task*> by_start_min_;
  const int64 capacity_;

  DISALLOW_COPY_AND_ASSIGN(CumulativeTimeTable);
};

class CumulativeConstraint : public Constraint {
 public:
  CumulativeConstraint(Solver* const s,
                       const std::vector<IntervalVar*>& intervals,
                       const std::vector<int64>& demands, int64 capacity,
                       const std::string& name)
      : Constraint(s),
        capacity_(capacity),
        intervals_(intervals),
        demands_(demands) {
    tasks_.reserve(intervals.size());
    for (int i = 0; i < intervals.size(); ++i) {
      tasks_.push_back(CumulativeTask(intervals[i], demands[i]));
    }
  }

  virtual void Post() {
    // For the cumulative constraint, there are many propagators, and they
    // don't dominate each other. So the strongest propagation is obtained
    // by posting a bunch of different propagators.
    if (FLAGS_cp_use_cumulative_time_table) {
      PostOneSidedConstraint(false, false);
      PostOneSidedConstraint(true, false);
    }
    if (FLAGS_cp_use_cumulative_edge_finder) {
      PostOneSidedConstraint(false, true);
      PostOneSidedConstraint(true, true);
    }
    if (FLAGS_cp_use_sequence_high_demand_tasks) {
      PostHighDemandSequenceConstraint();
    }
    if (FLAGS_cp_use_all_possible_disjunctions) {
      PostAllDisjunctions();
    }
  }

  virtual void InitialPropagate() {
    // Nothing to do: this constraint delegates all the work to other classes
  }

  void Accept(ModelVisitor* const visitor) const {
    // TODO(user): Build arrays on demand?
    visitor->BeginVisitConstraint(ModelVisitor::kCumulative, this);
    visitor->VisitIntervalArrayArgument(ModelVisitor::kIntervalsArgument,
                                        intervals_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kDemandsArgument,
                                       demands_);
    visitor->VisitIntegerArgument(ModelVisitor::kCapacityArgument, capacity_);
    visitor->EndVisitConstraint(ModelVisitor::kCumulative, this);
  }

  virtual std::string DebugString() const { return "CumulativeConstraint"; }

 private:
  // Post temporal disjunctions for tasks that cannot overlap.
  void PostAllDisjunctions() {
    for (int i = 0; i < intervals_.size(); ++i) {
      IntervalVar* const interval_i = intervals_[i];
      if (interval_i->MayBePerformed()) {
        for (int j = i + 1; j < intervals_.size(); ++j) {
          IntervalVar* const interval_j = intervals_[j];
          if (interval_j->MayBePerformed()) {
            if (tasks_[i].demand + tasks_[j].demand > capacity_) {
              Constraint* const constraint =
                  solver()->MakeTemporalDisjunction(interval_i, interval_j);
              solver()->AddConstraint(constraint);
            }
          }
        }
      }
    }
  }

  // Post a Sequence constraint for tasks that requires strictly more than half
  // of the resource
  void PostHighDemandSequenceConstraint() {
    Constraint* constraint = nullptr;
    {  // Need a block to avoid memory leaks in case the AddConstraint fails
      std::vector<IntervalVar*> high_demand_intervals;
      high_demand_intervals.reserve(intervals_.size());
      for (int i = 0; i < demands_.size(); ++i) {
        const int64 demand = tasks_[i].demand;
        // Consider two tasks with demand d1 and d2 such that
        // d1 * 2 > capacity_ and d2 * 2 > capacity_.
        // Then d1 + d2 = 1/2 (d1 * 2 + d2 * 2)
        //              > 1/2 (capacity_ + capacity_)
        //              > capacity_.
        // Therefore these two tasks cannot overlap.
        if (demand * 2 > capacity_ && tasks_[i].interval->MayBePerformed()) {
          high_demand_intervals.push_back(tasks_[i].interval);
        }
      }
      if (high_demand_intervals.size() >= 2) {
        // If there are less than 2 such intervals, the constraint would do
        // nothing
        std::string seq_name = StrCat(name(), "-HighDemandSequence");
        constraint = solver()->MakeDisjunctiveConstraint(high_demand_intervals,
                                                         seq_name);
      }
    }
    if (constraint != nullptr) {
      solver()->AddConstraint(constraint);
    }
  }

  // Populate the given vector with useful tasks, meaning the ones on which
  // some propagation can be done
  void PopulateVectorUsefulTasks(
      bool mirror, std::vector<CumulativeTask*>* const useful_tasks) {
    DCHECK(useful_tasks->empty());
    for (int i = 0; i < tasks_.size(); ++i) {
      const CumulativeTask& original_task = tasks_[i];
      IntervalVar* const interval = original_task.interval;
      // Check if exceed capacity
      if (original_task.demand > capacity_) {
        interval->SetPerformed(false);
      }
      // Add to the useful_task vector if it may be performed and that it
      // actually consumes some of the resource.
      if (interval->MayBePerformed() && original_task.demand > 0) {
        Solver* const s = solver();
        IntervalVar* const original_interval = original_task.interval;
        IntervalVar* const interval =
            mirror ? s->MakeMirrorInterval(original_interval)
                   : original_interval;
        IntervalVar* const relaxed_max = s->MakeIntervalRelaxedMax(interval);
        useful_tasks->push_back(
            new CumulativeTask(relaxed_max, original_task.demand));
      }
    }
  }

  // Makes and return an edge-finder or a time table, or nullptr if it is not
  // necessary.
  Constraint* MakeOneSidedConstraint(bool mirror, bool edge_finder) {
    std::vector<CumulativeTask*> useful_tasks;
    PopulateVectorUsefulTasks(mirror, &useful_tasks);
    if (useful_tasks.empty()) {
      return nullptr;
    } else {
      Solver* const s = solver();
      if (edge_finder) {
        return s->RevAlloc(
            new EdgeFinder<CumulativeTask>(s, useful_tasks, capacity_));
      } else {
        return s->RevAlloc(new CumulativeTimeTable<CumulativeTask>(
            s, useful_tasks, capacity_));
      }
    }
  }

  // Post a straight or mirrored edge-finder, if needed
  void PostOneSidedConstraint(bool mirror, bool edge_finder) {
    Constraint* const constraint = MakeOneSidedConstraint(mirror, edge_finder);
    if (constraint != nullptr) {
      solver()->AddConstraint(constraint);
    }
  }

  // Capacity of the cumulative resource
  const int64 capacity_;

  // The tasks that share the cumulative resource
  std::vector<CumulativeTask> tasks_;

  // Array of intervals for the visitor.
  const std::vector<IntervalVar*> intervals_;
  // Array of demands for the visitor.
  const std::vector<int64> demands_;

  DISALLOW_COPY_AND_ASSIGN(CumulativeConstraint);
};

class VariableDemandCumulativeConstraint : public Constraint {
 public:
  VariableDemandCumulativeConstraint(Solver* const s,
                                     const std::vector<IntervalVar*>& intervals,
                                     const std::vector<IntVar*>& demands,
                                     int64 capacity, const std::string& name)
      : Constraint(s),
        capacity_(capacity),
        intervals_(intervals),
        demands_(demands) {
    tasks_.reserve(intervals.size());
    for (int i = 0; i < intervals.size(); ++i) {
      tasks_.push_back(VariableCumulativeTask(intervals[i], demands[i]));
    }
  }

  virtual void Post() {
    // For the cumulative constraint, there are many propagators, and they
    // don't dominate each other. So the strongest propagation is obtained
    // by posting a bunch of different propagators.
    if (FLAGS_cp_use_cumulative_time_table) {
      PostOneSidedConstraint(false, false);
      PostOneSidedConstraint(true, false);
    }
    if (FLAGS_cp_use_cumulative_edge_finder) {
      PostOneSidedConstraint(false, true);
      PostOneSidedConstraint(true, true);
    }
    if (FLAGS_cp_use_sequence_high_demand_tasks) {
      PostHighDemandSequenceConstraint();
    }
    if (FLAGS_cp_use_all_possible_disjunctions) {
      PostAllDisjunctions();
    }
  }

  virtual void InitialPropagate() {
    // Nothing to do: this constraint delegates all the work to other classes
  }

  void Accept(ModelVisitor* const visitor) const {
    // TODO(user): Build arrays on demand?
    visitor->BeginVisitConstraint(ModelVisitor::kCumulative, this);
    visitor->VisitIntervalArrayArgument(ModelVisitor::kIntervalsArgument,
                                        intervals_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kDemandsArgument,
                                               demands_);
    visitor->VisitIntegerArgument(ModelVisitor::kCapacityArgument, capacity_);
    visitor->EndVisitConstraint(ModelVisitor::kCumulative, this);
  }

  virtual std::string DebugString() const {
    return "VariableDemandCumulativeConstraint";
  }

 private:
  // Post temporal disjunctions for tasks that cannot overlap.
  void PostAllDisjunctions() {
    for (int i = 0; i < intervals_.size(); ++i) {
      IntervalVar* const interval_i = intervals_[i];
      if (interval_i->MayBePerformed()) {
        for (int j = i + 1; j < intervals_.size(); ++j) {
          IntervalVar* const interval_j = intervals_[j];
          if (interval_j->MayBePerformed()) {
            if (tasks_[i].demand->Min() + tasks_[j].demand->Min() > capacity_) {
              Constraint* const constraint =
                  solver()->MakeTemporalDisjunction(interval_i, interval_j);
              solver()->AddConstraint(constraint);
            }
          }
        }
      }
    }
  }

  // Post a Sequence constraint for tasks that requires strictly more than half
  // of the resource
  void PostHighDemandSequenceConstraint() {
    Constraint* constraint = nullptr;
    {  // Need a block to avoid memory leaks in case the AddConstraint fails
      std::vector<IntervalVar*> high_demand_intervals;
      high_demand_intervals.reserve(intervals_.size());
      for (int i = 0; i < demands_.size(); ++i) {
        const int64 demand = tasks_[i].demand->Min();
        // Consider two tasks with demand d1 and d2 such that
        // d1 * 2 > capacity_ and d2 * 2 > capacity_.
        // Then d1 + d2 = 1/2 (d1 * 2 + d2 * 2)
        //              > 1/2 (capacity_ + capacity_)
        //              > capacity_.
        // Therefore these two tasks cannot overlap.
        if (demand * 2 > capacity_ && tasks_[i].interval->MayBePerformed()) {
          high_demand_intervals.push_back(tasks_[i].interval);
        }
      }
      if (high_demand_intervals.size() >= 2) {
        // If there are less than 2 such intervals, the constraint would do
        // nothing
        std::string seq_name = StrCat(name(), "-HighDemandSequence");
        constraint = solver()->MakeDisjunctiveConstraint(high_demand_intervals,
                                                         seq_name);
      }
    }
    if (constraint != nullptr) {
      solver()->AddConstraint(constraint);
    }
  }

  // Populate the given vector with useful tasks, meaning the ones on which
  // some propagation can be done
  void PopulateVectorUsefulTasks(
      bool mirror, std::vector<VariableCumulativeTask*>* const useful_tasks) {
    DCHECK(useful_tasks->empty());
    for (int i = 0; i < tasks_.size(); ++i) {
      const VariableCumulativeTask& original_task = tasks_[i];
      IntervalVar* const interval = original_task.interval;
      // Check if exceed capacity
      if (original_task.demand->Min() > capacity_) {
        interval->SetPerformed(false);
      }
      // Add to the useful_task vector if it may be performed and that it
      // actually consumes some of the resource.
      if (interval->MayBePerformed() && original_task.demand > 0) {
        Solver* const s = solver();
        IntervalVar* const original_interval = original_task.interval;
        IntervalVar* const interval =
            mirror ? s->MakeMirrorInterval(original_interval)
                   : original_interval;
        IntervalVar* const relaxed_max = s->MakeIntervalRelaxedMax(interval);
        useful_tasks->push_back(
            new VariableCumulativeTask(relaxed_max, original_task.demand));
      }
    }
  }

  // Makes and return an edge-finder or a time table, or nullptr if it is not
  // necessary.
  Constraint* MakeOneSidedConstraint(bool mirror, bool edge_finder) {
    std::vector<VariableCumulativeTask*> useful_tasks;
    PopulateVectorUsefulTasks(mirror, &useful_tasks);
    if (useful_tasks.empty()) {
      return nullptr;
    } else {
      Solver* const s = solver();
      if (edge_finder) {
        return s->RevAlloc(
            new EdgeFinder<VariableCumulativeTask>(s, useful_tasks, capacity_));
      } else {
        return s->RevAlloc(new CumulativeTimeTable<VariableCumulativeTask>(
            s, useful_tasks, capacity_));
      }
    }
  }

  // Post a straight or mirrored edge-finder, if needed
  void PostOneSidedConstraint(bool mirror, bool edge_finder) {
    Constraint* const constraint = MakeOneSidedConstraint(mirror, edge_finder);
    if (constraint != nullptr) {
      solver()->AddConstraint(constraint);
    }
  }

  // Capacity of the cumulative resource
  const int64 capacity_;

  // The tasks that share the cumulative resource
  std::vector<VariableCumulativeTask> tasks_;

  // Array of intervals for the visitor.
  const std::vector<IntervalVar*> intervals_;
  // Array of demands for the visitor.
  const std::vector<IntVar*> demands_;

  DISALLOW_COPY_AND_ASSIGN(VariableDemandCumulativeConstraint);
};
}  // namespace

// Sequence Constraint

// ----- Public class -----

DisjunctiveConstraint::DisjunctiveConstraint(
    Solver* const s, const std::vector<IntervalVar*>& intervals,
    const std::string& name)
    : Constraint(s), intervals_(intervals) {
  if (!name.empty()) {
    set_name(name);
  }
}

DisjunctiveConstraint::~DisjunctiveConstraint() {}

// ---------- Factory methods ----------

DisjunctiveConstraint* Solver::MakeDisjunctiveConstraint(
    const std::vector<IntervalVar*>& intervals, const std::string& name) {
  return RevAlloc(new FullDisjunctiveConstraint(this, intervals, name));
}

// Demands are constant

Constraint* Solver::MakeCumulative(const std::vector<IntervalVar*>& intervals,
                                   const std::vector<int64>& demands,
                                   int64 capacity, const std::string& name) {
  CHECK_EQ(intervals.size(), demands.size());
  for (int i = 0; i < intervals.size(); ++i) {
    CHECK_GE(demands[i], 0);
  }
  if (capacity == 1 && AreAllOnes(demands)) {
    return MakeDisjunctiveConstraint(intervals, name);
  }
  return RevAlloc(
      new CumulativeConstraint(this, intervals, demands, capacity, name));
}

Constraint* Solver::MakeCumulative(const std::vector<IntervalVar*>& intervals,
                                   const std::vector<int>& demands,
                                   int64 capacity, const std::string& name) {
  return MakeCumulative(intervals, ToInt64Vector(demands), capacity, name);
}

Constraint* Solver::MakeCumulative(const std::vector<IntervalVar*>& intervals,
                                   const std::vector<int64>& demands,
                                   IntVar* const capacity,
                                   const std::string& name) {
  CHECK_EQ(intervals.size(), demands.size());
  for (int i = 0; i < intervals.size(); ++i) {
    CHECK_GE(demands[i], 0);
  }
  if (capacity->Bound()) {
    return MakeCumulative(intervals, demands, capacity->Min(), name);
  }
  std::vector<IntervalVar*> a_intervals;
  std::vector<int64> a_demands;
  int64 horizon_min = kint64max;
  int64 horizon_max = kint64min;
  for (int i = 0; i < demands.size(); ++i) {
    if (demands[i] > 0) {
      a_intervals.push_back(intervals[i]);
      a_demands.push_back(demands[i]);
      horizon_max = std::max(horizon_max, intervals[i]->EndMax());
      horizon_min = std::min(horizon_min, intervals[i]->StartMin());
    }
  }
  const int64 total_duration = horizon_max = horizon_max + 1;
  const int64 capacity_min = std::max(capacity->Min(), 0LL);
  const int64 capacity_max = capacity->Max();
  const int64 delta_capacity = capacity_max - capacity_min;
  std::vector<IntVar*> o_vars;  // Optional variables.
  std::vector<int64> o_coefs;
  for (int64 mult = 1; mult <= delta_capacity; mult *= 2) {
    const std::string name =
        StringPrintf("VariableCapacity<%" GG_LL_FORMAT "d>", mult);
    IntervalVar* const var = MakeFixedDurationIntervalVar(
        horizon_min, horizon_min, total_duration, true, name);
    a_intervals.push_back(var);
    a_demands.push_back(mult);
    o_vars.push_back(var->PerformedExpr()->Var());
    o_coefs.push_back(mult);
  }
  o_vars.push_back(capacity);
  o_coefs.push_back(1);
  AddConstraint(MakeScalProdEquality(o_vars, o_coefs, capacity_max));
  return RevAlloc(new CumulativeConstraint(this, a_intervals, a_demands,
                                           capacity_max, name));
}

Constraint* Solver::MakeCumulative(const std::vector<IntervalVar*>& intervals,
                                   const std::vector<int>& demands,
                                   IntVar* const capacity,
                                   const std::string& name) {
  return MakeCumulative(intervals, ToInt64Vector(demands), capacity, name);
}

// Demands are variable

Constraint* Solver::MakeCumulative(const std::vector<IntervalVar*>& intervals,
                                   const std::vector<IntVar*>& demands,
                                   int64 capacity, const std::string& name) {
  CHECK_EQ(intervals.size(), demands.size());
  for (int i = 0; i < intervals.size(); ++i) {
    CHECK_GE(demands[i]->Min(), 0);
  }
  if (AreAllBound(demands)) {
    std::vector<int64> fixed_demands(demands.size());
    for (int i = 0; i < demands.size(); ++i) {
      fixed_demands[i] = demands[i]->Value();
    }
    return MakeCumulative(intervals, fixed_demands, capacity, name);
  }
  return RevAlloc(new VariableDemandCumulativeConstraint(
      this, intervals, demands, capacity, name));
}

Constraint* Solver::MakeCumulative(const std::vector<IntervalVar*>& intervals,
                                   const std::vector<IntVar*>& demands,
                                   IntVar* const capacity,
                                   const std::string& name) {
  CHECK_EQ(intervals.size(), demands.size());
  for (int i = 0; i < intervals.size(); ++i) {
    CHECK_GE(demands[i], 0);
  }
  if (capacity->Bound()) {
    return MakeCumulative(intervals, demands, capacity->Min(), name);
  }
  if (AreAllBound(demands)) {
    std::vector<int64> fixed_demands(demands.size());
    for (int i = 0; i < demands.size(); ++i) {
      fixed_demands[i] = demands[i]->Value();
    }
    return MakeCumulative(intervals, fixed_demands, capacity, name);
  }
  std::vector<IntervalVar*> a_intervals;
  std::vector<IntVar*> a_demands;
  int64 horizon_min = kint64max;
  int64 horizon_max = kint64min;
  for (int i = 0; i < demands.size(); ++i) {
    if (demands[i]->Max() > 0) {
      a_intervals.push_back(intervals[i]);
      a_demands.push_back(demands[i]);
      horizon_max = std::max(horizon_max, intervals[i]->EndMax());
      horizon_min = std::min(horizon_min, intervals[i]->StartMin());
    }
  }
  const int64 total_duration = horizon_max = horizon_max + 1;
  const int64 capacity_max = capacity->Max();
  IntVar* const residual_capacity =
      MakeDifference(capacity_max, capacity)->Var();
  const std::string residual_name =
      StringPrintf("VariableCapacity<%s>", name.c_str());
  IntervalVar* const var = MakeFixedDurationIntervalVar(
      horizon_min, horizon_min, total_duration, true, residual_name);
  a_intervals.push_back(var);
  a_demands.push_back(residual_capacity);
  return RevAlloc(new VariableDemandCumulativeConstraint(
      this, a_intervals, a_demands, capacity_max, name));
}
}  // namespace operations_research
