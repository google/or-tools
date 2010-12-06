// Copyright 2010 Google
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
// * Sequence: forces a set of intervals to be non-overlapping
// * Cumulative: forces a set of intervals with associated demands to be such
//     that the sum of demands of the intervals containing any given integer
//     does not exceed a capacity.

#include <cmath>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
//#include "strings/join.h"
#include "base/stl_util-inl.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/monoid_operation_tree.h"


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

// ----- Sequence -----

Constraint* MakeDecomposedSequenceConstraint(
    Solver* const s, IntervalVar* const * intervals, int size);

Sequence::Sequence(Solver* const s,
                   const IntervalVar* const * intervals,
                   int size,
                   const string& name)
    : Constraint(s), intervals_(new IntervalVar*[size]),
      size_(size),  ranks_(new int[size]), current_rank_(0) {
  memcpy(intervals_.get(), intervals, size_ * sizeof(*intervals));
  states_.resize(size);
  for (int i = 0; i < size_; ++i) {
    ranks_[i] = 0;
    states_[i].resize(size, UNDECIDED);
  }
}

Sequence::~Sequence() {}

IntervalVar* Sequence::Interval(int index) const {
  CHECK_GE(index, 0);
  CHECK_LT(index, size_);
  return intervals_[index];
}

void Sequence::Post() {
  for (int i = 0; i < size_; ++i) {
    IntervalVar* t = intervals_[i];
    Demon* d = MakeConstraintDemon1(solver(),
                                    this,
                                    &Sequence::RangeChanged,
                                    "RangeChanged",
                                    i);
    t->WhenAnything(d);
  }
  Constraint* ct =
      MakeDecomposedSequenceConstraint(solver(), intervals_.get(), size_);
  solver()->AddConstraint(ct);
}

void Sequence::InitialPropagate() {
  for (int i = 0; i < size_; ++i) {
    RangeChanged(i);
  }
}

void Sequence::RangeChanged(int index) {
  for (int i = 0; i < index; ++i) {
    Apply(i, index);
  }
  for (int i = index + 1; i < size_; ++i) {
    Apply(index, i);
  }
}

void Sequence::Apply(int i, int j) {
  DCHECK_LT(i, j);
  IntervalVar* t1 = intervals_[i];
  IntervalVar* t2 = intervals_[j];
  State s = states_[i][j];
  if (s == UNDECIDED) {
    TryToDecide(i, j);
  }
  if (s == ONE_BEFORE_TWO) {
    if (t1->MustBePerformed() && t2->MayBePerformed()) {
      t2->SetStartMin(t1->EndMin());
    }
    if (t2->MustBePerformed() && t1->MayBePerformed()) {
      t1->SetEndMax(t2->StartMax());
    }
  } else if (s == TWO_BEFORE_ONE) {
    if (t1->MustBePerformed() && t2->MayBePerformed()) {
      t2->SetEndMax(t1->StartMax());
    }
    if (t2->MustBePerformed() && t1->MayBePerformed()) {
      t1->SetStartMin(t2->EndMin());
    }
  }
}

void Sequence::TryToDecide(int i, int j) {
  DCHECK_LT(i, j);
  DCHECK_EQ(UNDECIDED, states_[i][j]);
  IntervalVar* t1 = intervals_[i];
  IntervalVar* t2 = intervals_[j];
  if (t1->MayBePerformed() && t2->MayBePerformed() &&
      (t1->MustBePerformed() || t2->MustBePerformed())) {
    if (t1->EndMin() > t2->StartMax()) {
      Decide(TWO_BEFORE_ONE, i, j);
    } else if (t2->EndMin() > t1->StartMax()) {
      Decide(ONE_BEFORE_TWO, i, j);
    }
  }
}

void Sequence::Decide(State s, int i, int j) {
  DCHECK_LT(i, j);
  // Should Decide on a fixed state?
  DCHECK_NE(s, UNDECIDED);
  if (states_[i][j] != UNDECIDED && states_[i][j] != s) {
    solver()->Fail();
  }
  solver()->SaveValue(reinterpret_cast<int*>(&states_[i][j]));
  states_[i][j] = s;
  Apply(i, j);
}

string Sequence::DebugString() const {
  int64 hmin, hmax, dmin, dmax;
  HorizonRange(&hmin, &hmax);
  DurationRange(&dmin, &dmax);
  return StringPrintf("%s(horizon = %" GG_LL_FORMAT
                      "d..%" GG_LL_FORMAT
                      "d, duration = %" GG_LL_FORMAT
                      "d..%" GG_LL_FORMAT
                      "d, not ranked = %d, fixed = %d, ranked = %d)",
                      name().c_str(), hmin, hmax, dmin, dmax, NotRanked(),
                      Fixed(), Ranked());
}

void Sequence::DurationRange(int64* dmin, int64* dmax) const {
  int64 dur_min = 0;
  int64 dur_max = 0;
  for (int i = 0; i < size_; ++i) {
    IntervalVar* t = intervals_[i];
    if (t->MayBePerformed()) {
      if (t->MustBePerformed()) {
        dur_min += t->DurationMin();
      }
      dur_max += t->DurationMax();
    }
  }
  *dmin = dur_min;
  *dmax = dur_max;
}

void Sequence::HorizonRange(int64* hmin, int64* hmax) const {
  int64 hor_min = kint64max;
  int64 hor_max = kint64min;
  for (int i = 0; i < size_; ++i) {
    IntervalVar* t = intervals_[i];
    if (t->MayBePerformed()) {
      const int64 tmin = t->StartMin();
      const int64 tmax = t->EndMax();
      if (tmin < hor_min) {
        hor_min = tmin;
      }
      if (tmax > hor_max) {
        hor_max = tmax;
      }
    }
  }
  *hmin = hor_min;
  *hmax = hor_max;
}

void Sequence::ActiveHorizonRange(int64* hmin, int64* hmax) const {
  int64 hor_min = kint64max;
  int64 hor_max = kint64min;
  for (int i = 0; i < size_; ++i) {
    IntervalVar* t = intervals_[i];
    if (t->MayBePerformed() && ranks_[i] >= current_rank_) {
      const int64 tmin = t->StartMin();
      const int64 tmax = t->EndMax();
      if (tmin < hor_min) {
        hor_min = tmin;
      }
      if (tmax > hor_max) {
        hor_max = tmax;
      }
    }
  }
  *hmin = hor_min;
  *hmax = hor_max;
}

int Sequence::Ranked() const {
  int count = 0;
  for (int i = 0; i < size_; ++i) {
    if (ranks_[i] < current_rank_ && intervals_[i]->MayBePerformed()) {
      count++;
    }
  }
  return count;
}

int Sequence::NotRanked() const {
  int count = 0;
  for (int i = 0; i < size_; ++i) {
    if (ranks_[i] >= current_rank_ && intervals_[i]->MayBePerformed()) {
      count++;
    }
  }
  return count;
}

int Sequence::Active() const {
  int count = 0;
  for (int i = 0; i < size_; ++i) {
    if (intervals_[i]->MayBePerformed()) {
      count++;
    }
  }
  return count;
}

int Sequence::Fixed() const {
  int count = 0;
  for (int i = 0; i < size_; ++i) {
    if (intervals_[i]->MustBePerformed() &&
        intervals_[i]->StartMin() == intervals_[i]->StartMax()) {
      count++;
    }
  }
  return count;
}

void Sequence::ComputePossibleRanks() {
  for (int i = 0; i < size_; ++i) {
    if (ranks_[i] == current_rank_) {
      int before = 0;
      int after = 0;
      for (int j = 0; j < i; ++j) {
        if (intervals_[j]->MustBePerformed()) {
          State s = states_[j][i];
          if (s == ONE_BEFORE_TWO) {
            before++;
          } else if (s == TWO_BEFORE_ONE) {
            after++;
          }
        }
      }
      for (int j = i + 1; j < size_; ++j) {
        if (intervals_[j]->MustBePerformed()) {
          State s = states_[i][j];
          if (s == ONE_BEFORE_TWO) {
            after++;
          } else if (s == TWO_BEFORE_ONE) {
            before++;
          }
        }
      }
      if (before > current_rank_) {
        solver()->SaveAndSetValue(&ranks_[i], before);
      }
    }
  }
}

bool Sequence::PossibleFirst(int index) {
  return (ranks_[index] == current_rank_);
}

void Sequence::RankFirst(int index) {
  IntervalVar* t = intervals_[index];
  t->SetPerformed(true);
  Solver* const s = solver();
  for (int i = 0; i < size_; ++i) {
    if (i != index &&
        ranks_[i] >= current_rank_ &&
        intervals_[i]->MayBePerformed()) {
      s->SaveAndSetValue(&ranks_[i], current_rank_ + 1);
      if (i < index) {
        Decide(TWO_BEFORE_ONE, i, index);
      } else {
        Decide(ONE_BEFORE_TWO, index, i);
      }
    }
  }
  s->SaveAndSetValue(&ranks_[index], current_rank_);
  s->SaveAndAdd(&current_rank_, 1);
}

void Sequence::RankNotFirst(int index) {
  solver()->SaveAndSetValue(&ranks_[index], current_rank_ + 1);
  int count = 0;
  int support = -1;
  for (int i = 0; i < size_; ++i) {
    if (ranks_[i] == current_rank_ && intervals_[i]->MayBePerformed()) {
      count++;
      support = i;
    }
  }
  if (count == 0) {
    solver()->Fail();
  }
  if (count == 1 && intervals_[support]->MustBePerformed()) {
    RankFirst(support);
  }
}

Sequence* Solver::MakeSequence(const vector<IntervalVar*>& intervals,
                               const string& name) {
  return RevAlloc(new Sequence(this,
                               intervals.data(), intervals.size(), name));
}

Sequence* Solver::MakeSequence(const IntervalVar* const * intervals, int size,
                               const string& name) {
  return RevAlloc(new Sequence(this, intervals, size, name));
}

// ----- Additional constraint on Sequence -----

namespace {

// Returns the ceil of the ratio of two integers.
// numerator may be any integer: positive, negative, or zero.
// denominator must be non-zero, positive or negative.
int64 CeilOfRatio(int64 numerator, int64 denominator) {
  DCHECK_NE(denominator, 0);
  const int64 rounded_toward_zero = numerator / denominator;
  const bool needs_one_more = numerator > (rounded_toward_zero * denominator);
  const int64 one_if_needed = static_cast<int64>(needs_one_more);
  const int64 ceil_of_ratio = rounded_toward_zero + one_if_needed;
  DCHECK_GE(denominator * ceil_of_ratio, numerator);
  return ceil_of_ratio;
}

// A DisjunctiveTask is a non-preemptive task sharing a disjunctive resource.
// That is, it corresponds to an interval, and this interval cannot overlap with
// any other interval of a DisjunctiveTask sharing the same resource.
//
// As this is just a pointer, copying is allowed. Assignment is not, since
// the pointer is const.
class DisjunctiveTask : public BaseObject {
 public:
  explicit DisjunctiveTask(IntervalVar* const interval) : interval_(interval) {}
  // Copy constructor. Cannot be explicit, because we want to pass instances of
  // DisjunctiveTask by copy.
  DisjunctiveTask(const DisjunctiveTask& task) : interval_(task.interval_) {}
  const IntervalVar* const interval() const { return interval_; }
  IntervalVar* const mutable_interval() { return interval_; }
  virtual string DebugString() const {
    return interval()->DebugString();
  }
 private:
  IntervalVar* const interval_;
};

// A CumulativeTask is a non-preemptive task sharing a cumulative resource.
// That is, it corresponds to an interval and a demand. The sum of demands of
// all cumulative tasks CumulativeTasks sharing a resource of capacity c those
// intervals contain any integer t cannot exceed c.
//
// As this is a very small object, copying is allowed. Assignment is not, since
// the interval pointer is const.
class CumulativeTask : public BaseObject {
 public:
  CumulativeTask(IntervalVar* const interval, int64 demand)
  : interval_(interval),
    demand_(demand) {}
  // Copy constructor. Cannot be explicit, because we want to pass instances of
  // CumulativeTask by copy.
  CumulativeTask(const CumulativeTask& task)
  : interval_(task.interval_), demand_(task.demand_) {}

  const IntervalVar* const interval() const { return interval_; }
  IntervalVar* const mutable_interval() { return interval_; }
  int64 demand() const { return demand_; }
  int64 EnergyMin() const { return interval_->DurationMin() * demand_; }
  virtual string DebugString() const {
    return StringPrintf("Task{ %s, demand: %" GG_LL_FORMAT "d }",
                        interval_->DebugString().c_str(), demand_);
  }
 private:
  IntervalVar* const interval_;
  const int64 demand_;
};

// An indexed task is a task that is aware of its position in an array of
// indexed tasks sorted by non-decreasing start min.
template <class Task>
class IndexedTask {
 public:
  static const int kUnknown;
  explicit IndexedTask(Task task) : task_(task), start_min_index_(kUnknown) {}

  IntervalVar* const mutable_interval() { return task_.mutable_interval(); }
  const IntervalVar* const interval() const { return task_.interval(); }
  const Task& task() const { return task_; }
  int start_min_index() const { return start_min_index_; }
  void set_start_min_index(int pos) { start_min_index_ = pos; }

  // Convenience methods: give access to some characteristics of the interval
  int64 StartMin() const { return interval()->StartMin(); }
  int64 StartMax() const { return interval()->StartMax(); }
  int64 EndMin() const { return interval()->EndMin(); }
  int64 EndMax() const { return interval()->EndMax(); }

  string DebugString() const {
    return StringPrintf("Wrapper(%s, start_min_index = %d)",
                        task_->DebugString().c_str(), start_min_index_);
  }
 private:
  Task task_;
  int start_min_index_;
  DISALLOW_COPY_AND_ASSIGN(IndexedTask<Task>);
};

template <class Task> const int IndexedTask<Task>::kUnknown = -1;

typedef IndexedTask<DisjunctiveTask> DisjunctiveIndexedTask;
typedef IndexedTask<CumulativeTask> CumulativeIndexedTask;

// Comparison methods, used by the STL sort.
template<class Task>
bool StartMinLessThan(const IndexedTask<Task>* const w1,
                      const IndexedTask<Task>* const w2) {
  return (w1->StartMin() < w2->StartMin());
}

template<class Task>
bool EndMaxLessThan(const IndexedTask<Task>* const w1,
                    const IndexedTask<Task>* const w2) {
  return (w1->EndMax() < w2->EndMax());
}

template<class Task>
bool StartMaxLessThan(const IndexedTask<Task>* const w1,
                      const IndexedTask<Task>* const w2) {
  return (w1->StartMax() < w2->StartMax());
}

template<class Task>
bool EndMinLessThan(const IndexedTask<Task>* const w1,
                    const IndexedTask<Task>* const w2) {
  return (w1->EndMin() < w2->EndMin());
}

template<typename T, typename Compare>
void Sort(vector<T>* vector, Compare compare) {
  std::sort(vector->begin(), vector->end(), compare);
}

bool TaskStartMinLessThan(const CumulativeTask* const task1,
                          const CumulativeTask* const task2) {
  return (task1->interval()->StartMin() < task2->interval()->StartMin());
}


// ----------------- Theta-Trees --------------------------------

// Node of a Theta-tree
class ThetaNode {
 public:
  // Identity element
  ThetaNode() : total_processing_(0), total_ect_(kint64min) {}
  // Single interval element
  explicit ThetaNode(const IntervalVar* const interval)
  : total_processing_(interval->DurationMin()),
    total_ect_(interval->EndMin()) {}
  void Set(const ThetaNode& node) {
    total_ect_ = node.total_ect_;
    total_processing_ = node.total_processing_;
  }
  void Compute(const ThetaNode& left, const ThetaNode& right) {
    total_processing_ = left.total_processing_ + right.total_processing_;
    total_ect_ = max(left.total_ect_ + right.total_processing_,
                     right.total_ect_);
  }
  int64 total_ect() const { return total_ect_; }
  bool IsIdentity() const {
    return total_processing_ == 0LL && total_ect_ == kint64min;
  }
  string DebugString() const {
    return StringPrintf(
        "ThetaNode{ p = %" GG_LL_FORMAT "d, e = %" GG_LL_FORMAT "d }",
        total_processing_, total_ect_ < 0LL ? -1LL : total_ect_);
  }
 private:
  int64 total_processing_;
  int64 total_ect_;
  DISALLOW_COPY_AND_ASSIGN(ThetaNode);
};

// This is based on Petr Vilim (public) PhD work. All names comes from his work.
// See http://vilim.eu/petr.
// A theta-tree is a container for a set of intervals supporting the following
// operations:
// * Insertions and deletion in O(log size_), with size_ the maximal number of
//     tasks the tree may contain;
// * Querying the following quantity in O(1):
//     Max_{subset S of the set of contained intervals} (
//             Min_{i in S}(i.StartMin) + Sum_{i in S}(i.DurationMin) )
class ThetaTree : public MonoidOperationTree<ThetaNode> {
 public:
  explicit ThetaTree(int size)
  : MonoidOperationTree<ThetaNode>(size) {}
  virtual ~ThetaTree() {}
  int64 ECT() const { return result().total_ect(); }
  void Insert(const DisjunctiveIndexedTask* const indexed_task) {
    ThetaNode thetaNode(indexed_task->interval());
    Set(indexed_task->start_min_index(), thetaNode);
  }
  void Remove(const DisjunctiveIndexedTask* indexed_task) {
    Reset(indexed_task->start_min_index());
  }
  bool IsInserted(const DisjunctiveIndexedTask* const indexed_task) const {
    return !GetOperand(indexed_task->start_min_index()).IsIdentity();
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(ThetaTree);
};

// ----------------- Lambda Theta Tree -----------------------

// Lambda-theta-node
// These nodes are cumulative lambda theta-node. This is reflected in the
// terminology. They can also be used in the disjunctive case, and this incurs
// no performance penalty.
class LambdaThetaNode {
 public:
  // Special value for task indices meaning 'no such task'.
  static const int kNone;

  // Identity constructor
  LambdaThetaNode()
  : energy_(0LL),
    energetic_end_min_(kint64min),
    energy_opt_(0LL),
    argmax_energy_opt_(kNone),
    energetic_end_min_opt_(kint64min),
    argmax_energetic_end_min_opt_(kNone) {}

  // Constructor for a single cumulative task in the Theta set
  LambdaThetaNode(int64 capacity, const CumulativeTask& task)
  : energy_(task.EnergyMin()),
    energetic_end_min_(capacity * task.interval()->StartMin() + energy_),
    energy_opt_(energy_),
    argmax_energy_opt_(kNone),
    energetic_end_min_opt_(energetic_end_min_),
    argmax_energetic_end_min_opt_(kNone) {}

  // Constructor for a single cumulative task in the Lambda set
  LambdaThetaNode(int64 capacity, const CumulativeTask& task, int index)
  : energy_(0LL),
    energetic_end_min_(kint64min),
    energy_opt_(task.EnergyMin()),
    argmax_energy_opt_(index),
    energetic_end_min_opt_(capacity * task.interval()->StartMin() +
                           energy_opt_),
    argmax_energetic_end_min_opt_(index) {
    DCHECK_GE(index, 0);
  }

  // Constructor for a single disjunctive task in the Theta set
  explicit LambdaThetaNode(const IntervalVar* const interval)
  : energy_(interval->DurationMin()),
    energetic_end_min_(interval->EndMin()),
    energy_opt_(interval->DurationMin()),
    argmax_energy_opt_(kNone),
    energetic_end_min_opt_(interval->EndMin()),
    argmax_energetic_end_min_opt_(kNone) {}

  // Constructor for a single interval in the Lambda set
  // 'index' is the index of the given interval in the est vector
  explicit LambdaThetaNode(const IntervalVar* const interval, int index)
  : energy_(0LL),
    energetic_end_min_(kint64min),
    energy_opt_(interval->DurationMin()),
    argmax_energy_opt_(index),
    energetic_end_min_opt_(interval->EndMin()),
    argmax_energetic_end_min_opt_(index) {
    DCHECK_GE(index, 0);
  }

  // Getters
  int64 energetic_end_min() const { return energetic_end_min_; }
  int64 energetic_end_min_opt() const { return energetic_end_min_opt_; }
  int argmax_energetic_end_min_opt() const {
    return argmax_energetic_end_min_opt_;
  }

  // Copy from the given node
  void Set(const LambdaThetaNode& node) {
    energy_ = node.energy_;
    energetic_end_min_ = node.energetic_end_min_;
    energy_opt_ = node.energy_opt_;
    argmax_energy_opt_ = node.argmax_energy_opt_;
    energetic_end_min_opt_ = node.energetic_end_min_opt_;
    argmax_energetic_end_min_opt_ =
        node.argmax_energetic_end_min_opt_;
  }

  // Sets this LambdaThetaNode to the result of the natural binary operations
  // over the two given operands, corresponding to the following set operations:
  // Theta = left.Theta union right.Theta
  // Lambda = left.Lambda union right.Lambda
  //
  // No set operation actually occur: we only maintain the relevant quantities
  // associated with such sets.
  void Compute(const LambdaThetaNode& left, const LambdaThetaNode& right) {
    energy_ = left.energy_ + right.energy_;
    energetic_end_min_ = max(right.energetic_end_min_,
                             left.energetic_end_min_ + right.energy_);
    const int64 energy_left_opt = left.energy_opt_ + right.energy_;
    const int64 energy_right_opt = left.energy_ + right.energy_opt_;
    if (energy_left_opt > energy_right_opt) {
      energy_opt_ = energy_left_opt;
      argmax_energy_opt_ = left.argmax_energy_opt_;
    } else {
      energy_opt_ = energy_right_opt;
      argmax_energy_opt_ = right.argmax_energy_opt_;
    }
    const int64 ect1 = right.energetic_end_min_opt_;
    const int64 ect2 = left.energetic_end_min_ + right.energy_opt_;
    const int64 ect3 = left.energetic_end_min_opt_ + right.energy_;
    if (ect1 >= ect2 && ect1 >= ect3) {  // ect1 max
      energetic_end_min_opt_ = ect1;
      argmax_energetic_end_min_opt_ = right.argmax_energetic_end_min_opt_;
    } else if (ect2 >= ect1 && ect2 >= ect3) {  // ect2 max
      energetic_end_min_opt_ = ect2;
      argmax_energetic_end_min_opt_ = right.argmax_energy_opt_;
    } else {  // ect3 max
      energetic_end_min_opt_ = ect3;
      argmax_energetic_end_min_opt_ = left.argmax_energetic_end_min_opt_;
    }
    // The processing time, with one grey interval, should be no less than
    // without any grey interval.
    DCHECK(energy_opt_ >= energy_);
    // If there is no responsible grey interval for the processing time,
    // the processing time with a grey interval should equal the one
    // without.
    DCHECK((argmax_energy_opt_ != kNone) || (energy_opt_ == energy_));
  }

 private:
  // Amount of resource consumed by the Theta set, in units of demand X time.
  // This is energy(Theta).
  int64 energy_;

  // Max_{subset S of Theta} (capacity * start_min(S) + energy(S))
  int64 energetic_end_min_;

  // Max_{i in Lambda} (energy(Theta union {i}))
  int64 energy_opt_;

  // The argmax in energy_opt_. It is the index of the chosen task in the Lambda
  // set, if any, or kNone if none.
  int argmax_energy_opt_;

  // Max_{subset S of Theta, i in Lambda}
  //     (capacity * start_min(S union {i}) + energy(S union {i}))
  int64 energetic_end_min_opt_;

  // The argmax in energetic_end_min_opt_. It is the index of the chosen task in
  // the Lambda set, if any, or kNone if none.
  int argmax_energetic_end_min_opt_;
};

const int LambdaThetaNode::kNone = -1;

// Disjunctive Lambda-Theta tree
class DisjunctiveLambdaThetaTree : public MonoidOperationTree<LambdaThetaNode> {
 public:
  explicit DisjunctiveLambdaThetaTree(int size)
  : MonoidOperationTree<LambdaThetaNode>(size) {}
  virtual ~DisjunctiveLambdaThetaTree() {}
  void Insert(const DisjunctiveIndexedTask* indexed_task) {
    LambdaThetaNode lambdaThetaNode(indexed_task->interval());
    Set(indexed_task->start_min_index(), lambdaThetaNode);
  }
  void Grey(const DisjunctiveIndexedTask* indexed_task) {
    const IntervalVar* const interval = indexed_task->interval();
    const int start_min_index = indexed_task->start_min_index();
    LambdaThetaNode greyNode(interval, start_min_index);
    Set(indexed_task->start_min_index(), greyNode);
  }
  int64 ECT() const { return result().energetic_end_min(); }
  int64 ECT_opt() const { return result().energetic_end_min_opt(); }
  int Responsible_opt() const {
    return result().argmax_energetic_end_min_opt();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DisjunctiveLambdaThetaTree);
};

// A cumulative lambda-theta tree
class CumulativeLambdaThetaTree : public MonoidOperationTree<LambdaThetaNode> {
 public:
  CumulativeLambdaThetaTree(int size, int64 capacity)
  : MonoidOperationTree<LambdaThetaNode>(size),
    capacity_(capacity) {}
  virtual ~CumulativeLambdaThetaTree() {}
  void Insert(const CumulativeIndexedTask* indexed_task) {
    LambdaThetaNode lambdaThetaNode(capacity_, indexed_task->task());
    Set(indexed_task->start_min_index(), lambdaThetaNode);
  }
  void Grey(const CumulativeIndexedTask* indexed_task) {
    const CumulativeTask& task = indexed_task->task();
    const int start_min_index = indexed_task->start_min_index();
    LambdaThetaNode greyNode(capacity_, task, start_min_index);
    Set(indexed_task->start_min_index(), greyNode);
  }
  int64 energetic_end_min() const { return result().energetic_end_min(); }
  int64 energetic_end_min_opt() const {
    return result().energetic_end_min_opt();
  }
  int64 ECT() const {
    return CeilOfRatio(energetic_end_min(), capacity_);
  }
  int64 ECT_opt() const {
    return CeilOfRatio(result().energetic_end_min_opt(), capacity_);
  }
  int argmax_energetic_end_min_opt() const {
    return result().argmax_energetic_end_min_opt();
  }

 private:
  const int64 capacity_;
  DISALLOW_COPY_AND_ASSIGN(CumulativeLambdaThetaTree);
};


// -------------- Not Last -----------------------------------------

// A class that implements the 'Not-Last' propagation algorithm for the unary
// resource constraint.
class NotLast {
 public:
  NotLast(Solver* const solver,
          IntervalVar* const * intervals,
          int size,
          bool mirror);
  ~NotLast() {
    STLDeleteElements(&by_start_min_);
  }
  bool Propagate();

 private:
  const int size_;
  ThetaTree theta_tree_;
  vector<DisjunctiveIndexedTask*> by_start_min_;
  vector<DisjunctiveIndexedTask*> by_end_max_;
  vector<DisjunctiveIndexedTask*> by_start_max_;
  vector<int64> new_lct_;
  DISALLOW_COPY_AND_ASSIGN(NotLast);
};

NotLast::NotLast(Solver* const solver,
                 IntervalVar* const * intervals,
                 int size,
                 bool mirror)
: size_(size),
  theta_tree_(size),
  by_start_min_(size),
  by_end_max_(size),
  by_start_max_(size),
  new_lct_(size, -1LL) {
  CHECK_GE(size_, 0);
  // Populate
  for (int i = 0; i < size; ++i) {
    IntervalVar* underlying = NULL;
    if (mirror) {
      underlying = solver->MakeMirrorInterval(intervals[i]);
    } else {
      underlying = intervals[i];
    }
    IntervalVar* const relaxed = solver->MakeIntervalRelaxedMin(underlying);
    by_start_min_[i] = new DisjunctiveIndexedTask(DisjunctiveTask(relaxed));
    by_end_max_[i] = by_start_min_[i];
    by_start_max_[i] = by_start_min_[i];
  }
}

bool NotLast::Propagate() {
  // ---- Init ----
  Sort(&by_start_max_, StartMaxLessThan<DisjunctiveTask>);
  Sort(&by_end_max_, EndMaxLessThan<DisjunctiveTask>);
  // Update start min positions
  Sort(&by_start_min_, StartMinLessThan<DisjunctiveTask>);
  for (int i = 0; i < size_; ++i) {
    by_start_min_[i]->set_start_min_index(i);
  }
  theta_tree_.Clear();
  for (int i = 0; i < size_; ++i) {
    new_lct_[i] = by_start_min_[i]->EndMax();
  }

  // --- Execute ----
  int j = 0;
  for (int i = 0; i < size_; ++i) {
    DisjunctiveIndexedTask* const twi = by_end_max_[i];
    while (j < size_ &&
           twi->EndMax() > by_start_max_[j]->StartMax()) {
      if (j > 0 && theta_tree_.ECT() > by_start_max_[j]->StartMax()) {
        const int64 new_end_max = by_start_max_[j - 1]->StartMax();
        new_lct_[by_start_max_[j]->start_min_index()] = new_end_max;
      }
      theta_tree_.Insert(by_start_max_[j]);
      j++;
    }
    const bool inserted = theta_tree_.IsInserted(twi);
    if (inserted) {
      theta_tree_.Remove(twi);
    }
    const int64 ect_theta_less_i = theta_tree_.ECT();
    if (inserted) {
      theta_tree_.Insert(twi);
    }
    if (ect_theta_less_i > twi->EndMax() && j > 0) {
      const int64 new_end_max = by_start_max_[j - 1]->EndMax();
      if (new_end_max > new_lct_[twi->start_min_index()]) {
        new_lct_[twi->start_min_index()] = new_end_max;
      }
    }
  }

  // Apply modifications
  bool modified = false;
  for (int i = 0; i < size_; ++i) {
    if (by_start_min_[i]->EndMax() > new_lct_[i]) {
      modified = true;
      by_start_min_[i]->mutable_interval()->SetEndMax(new_lct_[i]);
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
                                     IntervalVar* const * intervals,
                                     int size,
                                     bool mirror);
  ~EdgeFinderAndDetectablePrecedences() {
    STLDeleteElements(&by_start_min_);
  }
  int size() const { return size_; }
  IntervalVar* mutable_interval(int start_min_index) {
    return by_start_min_[start_min_index]->mutable_interval();
  }
  void UpdateEst();
  void OverloadChecking();
  bool DetectablePrecedences();
  bool EdgeFinder();

 private:
  Solver* const solver_;
  const int size_;

  // --- All the following member variables are essentially used as local ones:
  // no invariant is maintained about them, except for the fact that the vectors
  // always contains all the considered intervals, so any function that wants to
  // use them must first sort them in the right order.

  // All of these vectors store the same set of objects. Therefore, at
  // destruction time, STLDeleteElements should be called on only one of them.
  // It does not matter which one.

  ThetaTree theta_tree_;
  vector<DisjunctiveIndexedTask*> by_end_min_;
  vector<DisjunctiveIndexedTask*> by_start_min_;
  vector<DisjunctiveIndexedTask*> by_end_max_;
  vector<DisjunctiveIndexedTask*> by_start_max_;
  // new_est_[i] is the new start min for interval est_[i]->interval().
  vector<int64> new_est_;
  // new_lct_[i] is the new end max for interval est_[i]->interval().
  vector<int64> new_lct_;
  DisjunctiveLambdaThetaTree lt_tree_;
};

EdgeFinderAndDetectablePrecedences::EdgeFinderAndDetectablePrecedences(
    Solver* const solver,
    IntervalVar* const * intervals,
    int size,
    bool mirror)
  : solver_(solver),
    size_(size), theta_tree_(size), lt_tree_(size) {
  // Populate of the array of intervals
  for (int i = 0; i < size; ++i) {
    IntervalVar* underlying = NULL;
    if (mirror) {
      underlying = solver->MakeMirrorInterval(intervals[i]);
    } else {
      underlying = intervals[i];
    }
    IntervalVar* relaxed = solver->MakeIntervalRelaxedMax(underlying);
    DisjunctiveIndexedTask* const w =
        new DisjunctiveIndexedTask(DisjunctiveTask(relaxed));
    by_end_min_.push_back(w);
    by_start_min_.push_back(w);
    by_end_max_.push_back(w);
    by_start_max_.push_back(w);
    new_est_.push_back(kint64min);
  }
}

void EdgeFinderAndDetectablePrecedences::UpdateEst() {
  Sort(&by_start_min_, StartMinLessThan<DisjunctiveTask>);
  for (int i = 0; i < size_; ++i) {
    by_start_min_[i]->set_start_min_index(i);
  }
}

void EdgeFinderAndDetectablePrecedences::OverloadChecking() {
  // Init
  UpdateEst();
  Sort(&by_end_max_, EndMaxLessThan<DisjunctiveTask>);
  theta_tree_.Clear();

  for (int i = 0; i < size_; ++i) {
    DisjunctiveIndexedTask* const indexed_task = by_end_max_[i];
    theta_tree_.Insert(indexed_task);
    if (theta_tree_.ECT() > indexed_task->EndMax()) {
      solver_->Fail();
    }
  }
}

bool EdgeFinderAndDetectablePrecedences::DetectablePrecedences() {
  // Init
  UpdateEst();
  for (int i = 0; i < size_; ++i) {
    new_est_[i] = kint64min;
  }

  // Propagate in one direction
  Sort(&by_end_min_, EndMinLessThan<DisjunctiveTask>);
  Sort(&by_start_max_, StartMaxLessThan<DisjunctiveTask>);
  theta_tree_.Clear();
  int j = 0;
  for (int i = 0; i < size_; ++i) {
    DisjunctiveIndexedTask* const task_i = by_end_min_[i];
    if (j < size_) {
      DisjunctiveIndexedTask* task_j = by_start_max_[j];
      while (task_i->EndMin() > task_j->StartMax()) {
        theta_tree_.Insert(task_j);
        j++;
        if (j == size_)
          break;
        task_j = by_start_max_[j];
      }
    }
    const int64 esti = task_i->StartMin();
    bool inserted = theta_tree_.IsInserted(task_i);
    if (inserted) {
      theta_tree_.Remove(task_i);
    }
    const int64 oesti = theta_tree_.ECT();
    if (inserted) {
      theta_tree_.Insert(task_i);
    }
    if (oesti > esti) {
      new_est_[task_i->start_min_index()] = oesti;
    } else {
      new_est_[task_i->start_min_index()] = kint64min;
    }
  }

  // Apply modifications
  bool modified = false;
  for (int i = 0; i < size_; ++i) {
    if (new_est_[i] != kint64min) {
      modified = true;
      by_start_min_[i]->mutable_interval()->SetStartMin(new_est_[i]);
    }
  }
  return modified;
}

bool EdgeFinderAndDetectablePrecedences::EdgeFinder() {
  // Init
  UpdateEst();
  for (int i = 0; i < size_; ++i) {
    new_est_[i] = by_start_min_[i]->StartMin();
  }

  // Push in one direction.
  Sort(&by_end_max_, EndMaxLessThan<DisjunctiveTask>);
  lt_tree_.Clear();
  for (int i = 0; i < size_; ++i) {
    lt_tree_.Insert(by_start_min_[i]);
    DCHECK_EQ(i, by_start_min_[i]->start_min_index());
  }
  for (int j = size_ - 2; j >= 0; --j) {
    lt_tree_.Grey(by_end_max_[j+1]);
    DisjunctiveIndexedTask* const twj = by_end_max_[j];
    // We should have checked for overloading earlier.
    DCHECK_LE(lt_tree_.ECT(), twj->EndMax());
    while (lt_tree_.ECT_opt() > twj->EndMax()) {
      const int i = lt_tree_.Responsible_opt();
      DCHECK_GE(i, 0);
      if (lt_tree_.ECT() > new_est_[i]) {
        new_est_[i] = lt_tree_.ECT();
      }
      lt_tree_.Reset(i);
    }
  }

  // Apply modifications.
  bool modified = false;
  for (int i = 0; i < size_; ++i) {
    if (by_start_min_[i]->StartMin() < new_est_[i]) {
      modified = true;
      by_start_min_[i]->mutable_interval()->SetStartMin(new_est_[i]);
    }
  }
  return modified;
}

// ----------------- Sequence Constraint Decomposed  ------------

// A class that stores several propagators for the sequence constraint, and
// calls them until a fixpoint is reached.
class DecomposedSequenceConstraint : public Constraint {
 public:
  DecomposedSequenceConstraint(Solver* const s,
                               IntervalVar* const * intervals,
                               int size);
  virtual ~DecomposedSequenceConstraint() { }

  virtual void Post() {
    Demon* d = MakeDelayedConstraintDemon0(
        solver(),
        this,
        &DecomposedSequenceConstraint::InitialPropagate,
        "InitialPropagate");
    for (int32 i = 0; i < straight_.size(); ++i) {
      straight_.mutable_interval(i)->WhenAnything(d);
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
      } while (straight_not_last_.Propagate() ||
          mirror_not_last_.Propagate());
    } while (straight_.EdgeFinder() ||
        mirror_.EdgeFinder());
  }

 private:
  EdgeFinderAndDetectablePrecedences straight_;
  EdgeFinderAndDetectablePrecedences mirror_;
  NotLast straight_not_last_;
  NotLast mirror_not_last_;
  DISALLOW_COPY_AND_ASSIGN(DecomposedSequenceConstraint);
};

DecomposedSequenceConstraint::DecomposedSequenceConstraint(
    Solver* const s,
    IntervalVar* const * intervals,
    int size)
  : Constraint(s),
    straight_(s, intervals, size, false),
    mirror_(s, intervals, size, true),
    straight_not_last_(s, intervals, size, false),
    mirror_not_last_(s, intervals, size, true) {
}

}  // namespace

Constraint* MakeDecomposedSequenceConstraint(Solver* const s,
                                             IntervalVar* const * intervals,
                                             int size) {
  // Finds all intervals that may be performed
  vector<IntervalVar*> may_be_performed;
  may_be_performed.reserve(size);
  for (int i = 0; i < size; ++i) {
    if (intervals[i]->MayBePerformed()) {
      may_be_performed.push_back(intervals[i]);
    }
  }
  return s->RevAlloc(
      new DecomposedSequenceConstraint(s,
                                       may_be_performed.data(),
                                       may_be_performed.size()));
}

// =====================================================================
//  Cumulative
// =====================================================================

namespace {

CumulativeTask* MakeTask(Solver* solver, IntervalVar* interval, int64 demand) {
  return solver->RevAlloc(new CumulativeTask(interval, demand));
}

// A cumulative Theta node, where two energies, corresponding to 2 capacities,
// are stored.
class DualCapacityThetaNode {
 public:
  // Special value for task indices meaning 'no such task'.
  static const int kNone;

  // Identity constructor
  DualCapacityThetaNode()
  : energy_(0LL),
    energetic_end_min_(kint64min),
    residual_energetic_end_min_(kint64min) {}

  // Constructor for a single cumulative task in the Theta set
  DualCapacityThetaNode(int64 capacity, int64 residual_capacity,
                        const CumulativeTask& task)
  : energy_(task.EnergyMin()),
    energetic_end_min_(
        capacity * task.interval()->StartMin() + energy_),
    residual_energetic_end_min_(
        residual_capacity * task.interval()->StartMin() + energy_) {}

  // Getters
  int64 energy() const { return energy_; }
  int64 energetic_end_min() const { return energetic_end_min_; }
  int64 residual_energetic_end_min() const {
    return residual_energetic_end_min_;
  }

  // Copy from the given node
  void Set(const DualCapacityThetaNode& node) {
    energy_ = node.energy_;
    energetic_end_min_ = node.energetic_end_min_;
    residual_energetic_end_min_ = node.residual_energetic_end_min_;
  }

  // Sets this DualCapacityThetaNode to the result of the natural binary
  // operation over the two given operands, corresponding to the following set
  // operation: Theta = left.Theta union right.Theta
  //
  // No set operation actually occur: we only maintain the relevant quantities
  // associated with such sets.
  void Compute(const DualCapacityThetaNode& left,
               const DualCapacityThetaNode& right) {
    energy_ = left.energy_ + right.energy_;
    energetic_end_min_ =
        max(left.energetic_end_min_ + right.energy_,
            right.energetic_end_min_);
    residual_energetic_end_min_ =
        max(left.residual_energetic_end_min_ + right.energy_,
            right.residual_energetic_end_min_);
  }
 private:
  // Amount of resource consumed by the Theta set, in units of demand X time.
  // This is energy(Theta).
  int64 energy_;

  // Max_{subset S of Theta} (capacity * start_min(S) + energy(S))
  int64 energetic_end_min_;

  // Max_{subset S of Theta} (residual_capacity * start_min(S) + energy(S))
  int64 residual_energetic_end_min_;
  DISALLOW_COPY_AND_ASSIGN(DualCapacityThetaNode);
};

const int DualCapacityThetaNode::kNone = -1;

// A tree for dual capacity theta nodes
class DualCapacityThetaTree : public
  MonoidOperationTree<DualCapacityThetaNode> {
 public:
  static const int64 kNotInitialized;
  DualCapacityThetaTree(int size, int64 capacity)
  : MonoidOperationTree<DualCapacityThetaNode>(size),
    capacity_(capacity),
    residual_capacity_(-1) {}
  virtual ~DualCapacityThetaTree() {}
  void Insert(const CumulativeIndexedTask* indexed_task) {
    DualCapacityThetaNode thetaNode(
        capacity_, residual_capacity_, indexed_task->task());
    Set(indexed_task->start_min_index(), thetaNode);
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
    energy_alpha_ = argument.energy();
    energetic_end_min_alpha_ = argument.energetic_end_min();
    // We should reach a leaf that is not the identity
    DCHECK_GT(energetic_end_min_alpha_, kint64min);
  }
  bool ChooseGoLeft(const DualCapacityThetaNode& current,
                    const DualCapacityThetaNode& left_child,
                    const DualCapacityThetaNode& right_child) {
    if (right_child.residual_energetic_end_min() > energy_threshold_) {
      return false;  // enough energy on right
    } else {
      energy_threshold_ -= right_child.energy();
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
    energetic_end_min_alpha_ =
        max(energetic_end_min_alpha_,
            left_child.energetic_end_min() + energy_alpha_);
    energy_alpha_ += left_child.energy();
  }
  int64 GetEnvJC(const DualCapacityThetaNode& root) const {
    const int64 energy = root.energy();
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
  void Run() {
    interval_->SetStartMin(new_start_min_);
  }
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
  : updates_(size, 0), up_to_date_(false) {
  }
  const vector<int64>& updates() { return updates_; }
  bool up_to_date() const { return up_to_date_; }
  void Reset() { up_to_date_ = false; }
  void SetUpdate(int index, int64 update) {
    DCHECK(!up_to_date_);
    updates_[index] = update;
  }
  void SetUpToDate() { up_to_date_ = true; }
 private:
  vector<int64> updates_;
  bool up_to_date_;
  DISALLOW_COPY_AND_ASSIGN(UpdatesForADemand);
};

// One-sided cumulative edge finder.
class EdgeFinder : public Constraint {
 public:
  EdgeFinder(Solver* const solver,
             const vector<CumulativeTask*>& tasks,
             int64 capacity)
  : Constraint(solver),
    capacity_(capacity),
    size_(tasks.size()),
    by_start_min_(size_),
    by_end_max_(size_),
    by_end_min_(size_),
    lt_tree_(size_, capacity_) {
    // Populate
    for (int i = 0; i < size_; ++i) {
      CumulativeIndexedTask* const indexed_task =
          new CumulativeIndexedTask(*tasks[i]);
      by_start_min_[i] = indexed_task;
      by_end_max_[i] = indexed_task;
      by_end_min_[i] = indexed_task;
      const int64 demand = indexed_task->task().demand();
      // Create the UpdateForADemand if needed (the [] operator calls the
      // constructor). May rehash.
      if (update_map_[demand] == NULL) {
        update_map_[demand] = new UpdatesForADemand(size_);
      }
    }
  }
  virtual ~EdgeFinder() {
    STLDeleteElements(&by_start_min_);
    STLDeleteValues(&update_map_);
  }

  virtual void Post() {
    // Add the demons
    for (int i = 0; i < size_; ++i) {
      IntervalVar* const interval =
          by_start_min_[i]->mutable_interval();
      // Delay propagation, as this constraint is not incremental: we pay
      // O(n log n) each time the constraint is awakened.
      Demon* const demon = MakeDelayedConstraintDemon0(
          solver(), this, &EdgeFinder::InitialPropagate, "RangeChanged");
      interval->WhenAnything(demon);
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

 private:
  // Sets the fields in a proper state to run the propagation algorithm.
  void InitPropagation() {
    // Clear the update stack
    new_start_min_.clear();
    // Sort by start min.
    Sort(&by_start_min_, StartMinLessThan<CumulativeTask>);
    for (int i = 0; i < size_; ++i) {
      by_start_min_[i]->set_start_min_index(i);
    }
    // Sort by end max.
    Sort(&by_end_max_, EndMaxLessThan<CumulativeTask>);
    // Sort by end min.
    Sort(&by_end_min_, EndMinLessThan<CumulativeTask>);
    // Clear tree.
    lt_tree_.Clear();
    // Clear updates
    typedef UpdateMap::iterator iterator;
    for (iterator it = update_map_.begin(); it != update_map_.end(); ++it) {
      it->second->Reset();
    }
  }

  // Computes all possible update values for tasks of given demand, and stores
  // these values in update_map_[demand].
  // Runs in O(n log n).
  // This corresponds to lines 2--13 in algorithm 1.3 in Petr Vilim's paper.
  void ComputeConditionalStartMins(int64 demand) {
    DCHECK_GT(demand, 0);
    UpdatesForADemand* const updates = update_map_[demand];
    DCHECK(!updates->up_to_date());
    DualCapacityThetaTree dual_capa_tree(size_, capacity_);
    const int64 residual_capacity = capacity_ - demand;
    dual_capa_tree.SetResidualCapacity(residual_capacity);
    // It's important to initialize the update at IntervalVar::kMinValidValue
    // rather than at kInt64min, because its opposite may be used if it's a
    // mirror variable, and
    // -kInt64min = -(-kInt64max - 1) = kInt64max + 1 = -kInt64min
    int64 update = IntervalVar::kMinValidValue;
    for (int i = 0; i < size_; ++i) {
      const int64 current_end_max = by_end_max_[i]->EndMax();
      dual_capa_tree.Insert(by_end_max_[i]);
      const int64 energy_threshold = residual_capacity * current_end_max;
      const DualCapacityThetaNode& root = dual_capa_tree.result();
      const int64 res_energetic_end_min = root.residual_energetic_end_min();
      if (res_energetic_end_min > energy_threshold) {
        EnvJCComputeDiver diver(energy_threshold);
        dual_capa_tree.DiveInTree(&diver);
        const int64 enjv = diver.GetEnvJC(dual_capa_tree.result());
        const int64 numerator = enjv - energy_threshold;
        const int64 diff = CeilOfRatio(numerator, demand);
        update = std::max(update, diff);
      }
      updates->SetUpdate(i, update);
    }
    updates->SetUpToDate();
  }

  // Returns the new start min that can be inferred for task_to_push if it is
  // proved that it cannot end before by_end_max[end_max_index] does.
  int64 ConditionalStartMin(const CumulativeIndexedTask& task_to_push,
                            int end_max_index) {
    const int64 demand = task_to_push.task().demand();
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
    for (int i = 0; i < size_; ++i) {
      CumulativeIndexedTask* const task = by_end_min_[i];
      const int64 end_min = task->EndMin();
      while (end_max_index < size_ &&
             by_end_max_[end_max_index]->EndMax() <= end_min) {
        max_start_min = std::max(max_start_min,
                                 by_end_max_[end_max_index]->StartMin());
        ++end_max_index;
      }
      if (end_max_index > 0 &&
          task->StartMin() <= max_start_min &&
          task->EndMax() > task->EndMin()) {
        DCHECK_LE(by_end_max_[end_max_index - 1]->EndMax(), end_min);
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
        StartMinUpdater startMinUpdater(task->mutable_interval(), update);
        new_start_min_.push_back(startMinUpdater);
      }
    }
  }

  // Fill the theta-lambda-tree, and check for overloading.
  void FillInTree() {
    for (int i = 0; i < size_; ++i) {
      CumulativeIndexedTask* const indexed_task = by_end_max_[i];
      lt_tree_.Insert(indexed_task);
      // Maximum energetic end min without overload.
      const int64 max_feasible = capacity_ * indexed_task->EndMax();
      if (lt_tree_.energetic_end_min() > max_feasible) {
        solver()->Fail();
      }
    }
  }

  // The heart of the propagation algorithm. Should be called with all tasks
  // being in the Theta set. It detects tasks that need to be pushed.
  void PropagateBasedOnEnergy() {
    for (int j = size_ - 2; j >= 0; --j) {
      lt_tree_.Grey(by_end_max_[j+1]);
      CumulativeIndexedTask* const twj = by_end_max_[j];
      // We should have checked for overload earlier.
      DCHECK_LE(lt_tree_.energetic_end_min(), capacity_ * twj->EndMax());
      while (lt_tree_.energetic_end_min_opt() > capacity_ * twj->EndMax()) {
        const int i = lt_tree_.argmax_energetic_end_min_opt();
        DCHECK_GE(i, 0);
        PropagateTaskCannotEndBefore(i, j);
        lt_tree_.Reset(i);
      }
    }
  }

  // Takes into account the fact that the task of given index cannot end before
  // the given new end min.
  void PropagateTaskCannotEndBefore(int start_min_index, int end_max_index) {
    CumulativeIndexedTask* const task_to_push = by_start_min_[start_min_index];
    const int64 update = ConditionalStartMin(*task_to_push, end_max_index);
    StartMinUpdater startMinUpdater(task_to_push->mutable_interval(), update);
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

  // Number of tasks sharing this cumulative resource.
  const int size_;

  // Cumulative tasks, ordered by non-decreasing start min.
  vector<CumulativeIndexedTask*> by_start_min_;

  // Cumulative tasks, ordered by non-decreasing end max.
  vector<CumulativeIndexedTask*> by_end_max_;

  // Cumulative tasks, ordered by non-decreasing end min.
  vector<CumulativeIndexedTask*> by_end_min_;

  // Cumulative theta-lamba tree.
  CumulativeLambdaThetaTree lt_tree_;

  // Stack of updates to the new start min to do.
  vector<StartMinUpdater> new_start_min_;

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
class CumulativeTimeTable : public Constraint {
 public:
  CumulativeTimeTable(Solver* const solver,
            const vector<CumulativeTask*>& tasks,
            int64 capacity)
  : Constraint(solver),
    by_start_min_(tasks),
    capacity_(capacity) {
    // There may be up to 2 delta's per interval (one on each side),
    // plus two sentinels
    const int profile_max_size = 2 * NumTasks() + 2;
    profile_non_unique_time_.reserve(profile_max_size);
    profile_unique_time_.reserve(profile_max_size);
  }
  virtual void InitialPropagate() {
    BuildProfile();
    PushTasks();
  }
  virtual void Post() {
    Demon* d = MakeDelayedConstraintDemon0(
        solver(),
        this,
        &CumulativeTimeTable::InitialPropagate,
        "InitialPropagate");
    for (int i = 0; i < NumTasks(); ++i) {
      by_start_min_[i]->mutable_interval()->WhenAnything(d);
    }
  }
  int NumTasks() const { return by_start_min_.size(); }

 private:

  // Build the usage profile. Runs in O(n log n).
  void BuildProfile() {
    // Build profile with non unique time
    profile_non_unique_time_.clear();
    for (int i = 0; i < NumTasks(); ++i) {
      const CumulativeTask* task = by_start_min_[i];
      const IntervalVar* const interval = task->interval();
      const int64 start_max = interval->StartMax();
      const int64 end_min = interval->EndMin();
      if (interval->MustBePerformed() && start_max < end_min) {
        const int64 demand = task->demand();
        profile_non_unique_time_.push_back(ProfileDelta(start_max, +demand));
        profile_non_unique_time_.push_back(ProfileDelta(end_min, -demand));
      }
    }
    // Sort
    std::sort(profile_non_unique_time_.begin(),
              profile_non_unique_time_.end(),
              TimeLessThan);
    // Build profile with unique times
    int64 usage = 0;
    profile_unique_time_.clear();
    profile_unique_time_.push_back(ProfileDelta(kint64min, 0));
    for (int i = 0; i < profile_non_unique_time_.size(); ++i) {
      const ProfileDelta& profile_delta = profile_non_unique_time_[i];
      if (profile_delta.time == profile_unique_time_.back().time) {
        profile_unique_time_.back().delta += profile_delta.delta;
      } else {
        if (usage > capacity_) {
          solver()->Fail();
        }
        profile_unique_time_.push_back(profile_delta);
      }
      usage += profile_delta.delta;
    }
    DCHECK_EQ(0, usage);
    profile_unique_time_.push_back(ProfileDelta(kint64max, 0));
  }

  // Update the start min for all tasks. Runs in O(n^2) and Omega(n).
  void PushTasks() {
    Sort(&by_start_min_, TaskStartMinLessThan);
    int64 usage = 0;
    int profile_index = 0;
    for (int task_index = 0 ; task_index < NumTasks(); ++task_index) {
      CumulativeTask* const task = by_start_min_[task_index];
      while (task->interval()->StartMin() >
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
  // exceed capacity_ - task->demand() on the interval
  // [new_start_min, new_start_min + task->interval()->DurationMin() ).
  void PushTask(CumulativeTask* const task,
                  int profile_index,
                  int64 usage) {
      const IntervalVar* const interval = task->interval();
      int64 new_start_min = interval->StartMin();
      // Influence of current task
      const int64 start_max = interval->StartMax();
      const int64 end_min = interval->EndMin();
      ProfileDelta delta_start(start_max, 0);
      ProfileDelta delta_end(end_min, 0);
      const int64 demand = task->demand();
      if (interval->MustBePerformed() && start_max < end_min) {
        delta_start.delta = +demand;
        delta_end.delta = -demand;
      }
      const int64 residual_capacity = capacity_ - demand;
      const int64 duration = task->interval()->DurationMin();
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
      task->mutable_interval()->SetStartMin(new_start_min);
    }

  typedef vector<ProfileDelta> Profile;

  Profile profile_unique_time_;
  Profile profile_non_unique_time_;
  vector<CumulativeTask*> by_start_min_;
  const int64 capacity_;

  DISALLOW_COPY_AND_ASSIGN(CumulativeTimeTable);
};

class CumulativeConstraint : public Constraint {
 public:
  CumulativeConstraint(Solver* const s,
                       IntervalVar* const * intervals,
                       int64 const * demands,
                       int size,
                       int64 capacity,
                       const string& name)
  : Constraint(s),
    capacity_(capacity),
    tasks_(new CumulativeTask*[size]),
    size_(size) {
    for (int i = 0; i < size; ++i) {
      tasks_[i] = MakeTask(solver(), intervals[i], demands[i]);
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

 private:
  // Post temporal disjunctions for tasks that cannot overlap.
  void PostAllDisjunctions() {
    for (int i = 0; i < size_; ++i) {
      IntervalVar* interval_i = tasks_[i]->mutable_interval();
      if (interval_i->MayBePerformed()) {
        for (int j = i + 1; j < size_; ++j) {
          IntervalVar* interval_j = tasks_[j]->mutable_interval();
          if (interval_j->MayBePerformed()) {
            if (tasks_[i]->demand() + tasks_[j]->demand() > capacity_) {
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
    Constraint* constraint = NULL;
    {  // Need a block to avoid memory leaks in case the AddConstraint fails
      vector<IntervalVar*> high_demand_intervals;
      high_demand_intervals.reserve(size_);
      for (int i = 0; i < size_; ++i) {
        const int64 demand = tasks_[i]->demand();
        // Consider two tasks with demand d1 and d2 such that
        // d1 * 2 > capacity_ and d2 * 2 > capacity_.
        // Then d1 + d2 = 1/2 (d1 * 2 + d2 * 2)
        //              > 1/2 (capacity_ + capacity_)
        //              > capacity_.
        // Therefore these two tasks cannot overlap.
        if (demand * 2 > capacity_ && tasks_[i]->interval()->MayBePerformed()) {
          high_demand_intervals.push_back(tasks_[i]->mutable_interval());
        }
      }
      if (high_demand_intervals.size() >= 2) {
        // If there are less than 2 such intervals, the constraint would do
        // nothing
        string seq_name = StrCat(name(), "-HighDemandSequence");
        constraint = solver()->MakeSequence(high_demand_intervals, seq_name);
      }
    }
    if (constraint != NULL) {
      solver()->AddConstraint(constraint);
    }
  }

  // Creates a possibly mirrored relaxed task corresponding to the given task.
  CumulativeTask* MakeRelaxedTask(CumulativeTask* original_task, bool mirror) {
    IntervalVar* const original_interval = original_task->mutable_interval();
    IntervalVar* const interval = mirror ?
        solver()->MakeMirrorInterval(original_interval) : original_interval;
    IntervalVar* const relaxed_max = solver()->MakeIntervalRelaxedMax(interval);
    CumulativeTask* task = new CumulativeTask(relaxed_max,
                                              original_task->demand());
    return solver()->RevAlloc(task);
  }

  // Populate the given vector with useful tasks, meaning the ones on which
  // some propagation can be done
  void PopulateVectorUsefulTasks(bool mirror,
                                 vector<CumulativeTask*>* useful_tasks) {
    DCHECK(useful_tasks->empty());
    for (int i = 0; i < size_; ++i) {
      CumulativeTask* const original_task = tasks_[i];
      IntervalVar* interval = original_task->mutable_interval();
      // Check if exceed capacity
      if (original_task->demand() > capacity_) {
       interval->SetPerformed(false);
      }
      // Add to the useful_task vector if it may be performed and that it
      // actually consumes some of the resource.
      if (interval->MayBePerformed() && original_task->demand() > 0) {
        useful_tasks->push_back(MakeRelaxedTask(original_task, mirror));
      }
    }
  }

  // Makes and return an edge-finder or a time table, or NULL if it is not
  // necessary.
  Constraint* MakeOneSidedConstraint(bool mirror, bool edge_finder) {
    vector<CumulativeTask*> useful_tasks;
    PopulateVectorUsefulTasks(mirror, &useful_tasks);
    if (useful_tasks.empty()) {
      return NULL;
    } else {
      Constraint* constraint;
      if (edge_finder) {
        constraint = new EdgeFinder(solver(), useful_tasks, capacity_);
      } else {
        constraint = new CumulativeTimeTable(solver(), useful_tasks, capacity_);
      }
      return solver()->RevAlloc(constraint);
    }
  }

  // Post a straight or mirrored edge-finder, if needed
  void PostOneSidedConstraint(bool mirror, bool edge_finder) {
    Constraint* constraint = MakeOneSidedConstraint(mirror, edge_finder);
    if (constraint != NULL) {
      solver()->AddConstraint(constraint);
    }
  }

  // Capacity of the cumulative resource
  const int64 capacity_;

  // The tasks that share the cumulative resource
  const scoped_array<CumulativeTask*> tasks_;

  // Number of tasks
  const int size_;

  DISALLOW_COPY_AND_ASSIGN(CumulativeConstraint);
};

}  // namespace

// ----------------- Factory methods -------------------------------

Constraint* Solver::MakeCumulative(IntervalVar* const* intervals,
                                   const int64* demands,
                                   int size,
                                   int64 capacity,
                                   const string& name) {
  for (int i = 0; i < size; ++i) {
    CHECK_GE(demands[i], 0);
  }
  return RevAlloc(new CumulativeConstraint(
      this, intervals, demands, size, capacity, name));
}

Constraint* Solver::MakeCumulative(const vector<IntervalVar*>& intervals,
                                   const vector<int64>& demands,
                                   int64 capacity,
                                   const string& name) {
  CHECK_EQ(intervals.size(), demands.size());
  IntervalVar* const* intervals_array = intervals.data();
  const int64* demands_array = demands.data();
  const int size = intervals.size();
  return MakeCumulative(intervals_array, demands_array, size, capacity, name);
}

}  // namespace operations_research
