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
#include "flatzinc2/flatzinc_constraints.h"

#include "base/commandlineflags.h"
#include "base/join.h"
#include "base/stl_util.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/monoid_operation_tree.h"
#include "util/saturated_arithmetic.h"
#include "util/string_array.h"

DECLARE_bool(cp_trace_search);
DECLARE_bool(cp_trace_propagation);
DECLARE_bool(use_sat);
DECLARE_bool(fz_verbose);

namespace operations_research {
namespace {
// ----- Comparison functions -----

// TODO(user): Tie breaking.

// Comparison methods, used by the STL sort.
template <class Task> bool StartMinLessThan(Task* const w1, Task* const w2) {
  return (w1->StartMin() < w2->StartMin());
}

template <class Task> bool StartMaxLessThan(Task* const w1, Task* const w2) {
  return (w1->StartMax() < w2->StartMax());
}

template <class Task> bool EndMinLessThan(Task* const w1, Task* const w2) {
  return (w1->EndMin() < w2->EndMin());
}

template <class Task> bool EndMaxLessThan(Task* const w1, Task* const w2) {
  return (w1->EndMax() < w2->EndMax());
}

// ----- Wrappers around intervals -----

// A DisjunctiveTask is a non-preemptive task sharing a disjunctive resource.
// That is, it corresponds to an interval, and this interval cannot overlap with
// any other interval of a DisjunctiveTask sharing the same resource.
// It is indexed, that is it is aware of its position in a reference array.
struct DisjunctiveTask {
  explicit DisjunctiveTask(IntVar* start_, int64 duration_, IntVar* performed_)
      : start(start_), duration(duration_), performed(performed_), index(-1) {}

  std::string DebugString() const {
    return StringPrintf("Task(%s, %" GG_LL_FORMAT "d)",
                        start->DebugString().c_str(), duration);
  }

  int64 StartMin() const {
    return MayBePerformed() == 1 ? start->Min() : kint32min;
  }

  int64 StartMax() const {
    return MayBePerformed() == 1 ? start->Max() : kint32max;
  }

  int64 EndMin() const {
    return MayBePerformed() ? start->Min() + duration : kint32min;
  }
  int64 EndMax() const {
    return MayBePerformed() ? start->Max() + duration : kint32max;
  }
  void SetEndMax(int64 value) {}
  void SetStartMin(int64 value) {}

  bool MayBePerformed() const { return performed->Max() == 1; }

  IntVar* start;
  int64 duration;
  IntVar* performed;
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
  explicit ThetaNode(const IntVar* const start, int64 duration)
      : total_processing(duration), total_ect(start->Min() + duration) {}

  void Compute(const ThetaNode& left, const ThetaNode& right) {
    total_processing = left.total_processing + right.total_processing;
    total_ect =
        std::max(left.total_ect + right.total_processing, right.total_ect);
  }

  bool IsIdentity() const {
    return total_processing == 0LL && total_ect == kint64min;
  }

  std::string DebugString() const {
    return StringPrintf(
        "ThetaNode{ p = %" GG_LL_FORMAT "d, e = %" GG_LL_FORMAT "d }",
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
    Set(task->index, ThetaNode(task->start, task->duration));
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

  // Constructor for a single interval in the Theta set
  explicit LambdaThetaNode(const IntVar* const start, const int64 duration)
      : energy(duration),
        energetic_end_min(start->Min() + duration),
        energy_opt(duration),
        argmax_energy_opt(kNone),
        energetic_end_min_opt(start->Min() + duration),
        argmax_energetic_end_min_opt(kNone) {}

  // Constructor for a single interval in the Lambda set
  // 'index' is the index of the given interval in the est vector
  LambdaThetaNode(const IntVar* const start, const int64 duration, int index)
      : energy(0LL),
        energetic_end_min(kint64min),
        energy_opt(duration),
        argmax_energy_opt(index),
        energetic_end_min_opt(start->Min() + duration),
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
    Set(task.index, LambdaThetaNode(task.start, task.duration));
  }

  void Grey(const DisjunctiveTask& task) {
    const int index = task.index;
    Set(index, LambdaThetaNode(task.start, task.duration, index));
  }

  int64 Ect() const { return result().energetic_end_min; }
  int64 EctOpt() const { return result().energetic_end_min_opt; }
  int ResponsibleOpt() const { return result().argmax_energetic_end_min_opt; }
};

// -------------- Not Last -----------------------------------------

// A class that implements the 'Not-Last' propagation algorithm for the unary
// resource constraint.
class NotLast {
 public:
  NotLast(Solver* const solver, const std::vector<IntVar*>& intervals,
          const std::vector<int64>& durations,
          const std::vector<IntVar*>& performed, bool mirror);

  ~NotLast() { STLDeleteElements(&by_start_min_); }

  bool Propagate();

 private:
  ThetaTree theta_tree_;
  std::vector<DisjunctiveTask*> by_start_min_;
  std::vector<DisjunctiveTask*> by_end_max_;
  std::vector<DisjunctiveTask*> by_start_max_;
  std::vector<int64> new_lct_;
};

NotLast::NotLast(Solver* const solver, const std::vector<IntVar*>& starts,
                 const std::vector<int64>& durations,
                 const std::vector<IntVar*>& performed, bool mirror)
    : theta_tree_(starts.size()),
      by_start_min_(starts.size()),
      by_end_max_(starts.size()),
      by_start_max_(starts.size()),
      new_lct_(starts.size(), -1LL) {
  // Populate the different vectors.
  for (int i = 0; i < starts.size(); ++i) {
    IntVar* const underlying =
        mirror ? solver->MakeOpposite(starts[i])->Var() : starts[i];
    by_start_min_[i] =
        new DisjunctiveTask(underlying, durations[i], performed[i]);
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
    new_lct_[i] = by_start_min_[i]->EndMax();
  }

  // --- Execute ----
  int j = 0;
  for (int i = 0; i < by_start_max_.size(); ++i) {
    DisjunctiveTask* const twi = by_end_max_[i];
    while (j < by_start_max_.size() &&
           twi->EndMax() > by_start_max_[j]->StartMax()) {
      if (j > 0 && theta_tree_.Ect() > by_start_max_[j]->StartMax()) {
        const int64 new_end_max = by_start_max_[j - 1]->StartMax();
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
    if (ect_theta_less_i > twi->EndMax() && j > 0) {
      const int64 new_end_max = by_start_max_[j - 1]->EndMax();
      if (new_end_max > new_lct_[twi->index]) {
        new_lct_[twi->index] = new_end_max;
      }
    }
  }

  // Apply modifications
  bool modified = false;
  for (int i = 0; i < by_start_min_.size(); ++i) {
    if (by_start_min_[i]->EndMax() > new_lct_[i]) {
      modified = true;
      by_start_min_[i]->SetEndMax(new_lct_[i]);
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
                                     const std::vector<IntVar*>& intervals,
                                     const std::vector<int64>& durations,
                                     const std::vector<IntVar*>& performed,
                                     bool mirror);
  ~EdgeFinderAndDetectablePrecedences() { STLDeleteElements(&by_start_min_); }
  int64 size() const { return by_start_min_.size(); }
  IntVar* start(int index) { return by_start_min_[index]->start; }
  IntVar* performed(int index) { return by_start_min_[index]->performed; }
  int64 duration(int index) { return by_start_min_[index]->duration; }
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
    Solver* const solver, const std::vector<IntVar*>& starts,
    const std::vector<int64>& durations, const std::vector<IntVar*>& performed,
    bool mirror)
    : solver_(solver), theta_tree_(starts.size()), lt_tree_(starts.size()) {
  // Populate of the array of intervals
  for (int i = 0; i < starts.size(); ++i) {
    IntVar* const underlying =
        mirror ? solver->MakeOpposite(starts[i])->Var() : starts[i];
    DisjunctiveTask* const w =
        new DisjunctiveTask(underlying, durations[i], performed[i]);
    // Relaxed max.
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
    if (theta_tree_.Ect() > task->EndMax()) {
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
      while (task_i->EndMin() > task_j->StartMax()) {
        theta_tree_.Insert(task_j);
        j++;
        if (j == size()) break;
        task_j = by_start_max_[j];
      }
    }
    const int64 esti = task_i->StartMin();
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
      by_start_min_[i]->SetStartMin(new_est_[i]);
    }
  }
  return modified;
}

bool EdgeFinderAndDetectablePrecedences::EdgeFinder() {
  // Initialization.
  UpdateEst();
  for (int i = 0; i < size(); ++i) {
    new_est_[i] = by_start_min_[i]->StartMin();
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
    DCHECK_LE(lt_tree_.Ect(), twj->EndMax());
    while (lt_tree_.EctOpt() > twj->EndMax()) {
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
    if (by_start_min_[i]->StartMin() < new_est_[i]) {
      modified = true;
      by_start_min_[i]->SetStartMin(new_est_[i]);
    }
  }
  return modified;
}

// --------- Disjunctive Constraint ----------

// A class that stores several propagators for the disjunctive constraint, and
// calls them until a fixpoint is reached.

class FzDisjunctiveConstraint : public Constraint {
 public:
  FzDisjunctiveConstraint(Solver* const s, const std::vector<IntVar*>& starts,
                          const std::vector<int64>& durations,
                          const std::vector<IntVar*>& performed)
      : Constraint(s),
        starts_(starts),
        durations_(durations),
        performed_(performed),
        straight_(s, starts, durations, performed, false),
        mirror_(s, starts, durations, performed, true),
        straight_not_last_(s, starts, durations, performed, false),
        mirror_not_last_(s, starts, durations, performed, true) {}

  virtual ~FzDisjunctiveConstraint() {}

  virtual void Post() {
    Demon* const d = MakeDelayedConstraintDemon0(
        solver(), this, &FzDisjunctiveConstraint::InitialPropagate,
        "InitialPropagate");
    for (int32 i = 0; i < straight_.size(); ++i) {
      starts_[i]->WhenRange(d);
      performed_[i]->WhenBound(d);
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
    visitor->EndVisitConstraint(ModelVisitor::kDisjunctive, this);
  }

  virtual std::string DebugString() const {
    return StringPrintf("FzDisjunctiveConstraint([%s, %s, %s])",
                        JoinDebugStringPtr(starts_, ",").c_str(),
                        strings::Join(durations_, ",").c_str(),
                        JoinDebugStringPtr(performed_, ",").c_str());
  }

 private:
  const std::vector<IntVar*> starts_;
  const std::vector<int64> durations_;
  const std::vector<IntVar*> performed_;
  EdgeFinderAndDetectablePrecedences straight_;
  EdgeFinderAndDetectablePrecedences mirror_;
  NotLast straight_not_last_;
  NotLast mirror_not_last_;
  DISALLOW_COPY_AND_ASSIGN(FzDisjunctiveConstraint);
};

}  // namespace

Constraint* MakeDisjunctiveConstraint(Solver* const solver,
                                      const std::vector<IntVar*>& starts,
                                      const std::vector<int64>& durations,
                                      const std::vector<IntVar*>& performed) {
  return solver->RevAlloc(
      new FzDisjunctiveConstraint(solver, starts, durations, performed));
}

// Constraint* MakeVariableCumulative(Solver* const solver,
//                                    const std::vector<IntVar*>& starts,
//                                    const std::vector<IntVar*>& durations,
//                                    const std::vector<IntVar*>& usages,
//                                    IntVar* const capacity) {
//   std::vector<VariableCumulativeTask*> tasks;
//   for (int i = 0; i < starts.size(); ++i) {
//     if (usages[i]->Max() > 0) {
//       tasks.push_back(solver->RevAlloc(
//           new VariableCumulativeTask(starts[i], durations[i], usages[i])));
//     }
//   }
//   return solver->RevAlloc(
//       new VariableCumulativeTimeTable(solver, tasks, capacity));
// }
}  // namespace operations_research
