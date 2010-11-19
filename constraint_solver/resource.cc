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
#include "base/stl_util-inl.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/monoid_operation_tree.h"

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

template<class Task, typename Compare>
void Sort(vector<IndexedTask<Task>*>* vector, Compare compare) {
  std::sort(vector->begin(), vector->end(), compare);
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
    DCHECK(energy_opt_ >= energy_);
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
  int64 ECT() const {
    return ConvertEnergeticEndMinToEndMin(energetic_end_min());
  }
  int64 ECT_opt() const {
    return ConvertEnergeticEndMinToEndMin(result().energetic_end_min_opt());
  }
  int Responsible_opt() const {
    return result().argmax_energetic_end_min_opt();
  }

 private:
  // Takes the energetic end min of a set of tasks and returns the end min of
  // that set of tasks.
  int64 ConvertEnergeticEndMinToEndMin(int64 energetic_end_min) const {
    // Computes ceil(energetic_end_min / capacity_), without using doubles.
    const int64 numerator = energetic_end_min + capacity_ - 1;
    DCHECK(numerator >= 0);  // NOLINT - DCHECK_GE not supported
    return numerator / capacity_;
  }
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
    if (lt_tree_.ECT() > twj->EndMax()) {
      solver_->Fail();  // Resource is overloaded
    }
    while (lt_tree_.ECT_opt() > twj->EndMax()) {
      const int i = lt_tree_.Responsible_opt();
      DCHECK_GE(i, 0);
      const int act_i = by_start_min_[i]->start_min_index();
      if (lt_tree_.ECT() > new_est_[act_i]) {
        new_est_[act_i] = lt_tree_.ECT();
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
    lt_tree_(size_, capacity_) {
    // Populate
    for (int i = 0; i < size_; ++i) {
      CumulativeIndexedTask* const indexed_task =
          new CumulativeIndexedTask(*tasks[i]);
      by_start_min_[i] = indexed_task;
      by_end_max_[i] = indexed_task;
    }
  }
  virtual ~EdgeFinder() {
    STLDeleteElements(&by_start_min_);
  }

  int size() const { return size_; }

  virtual void Post() {
    // Add the demons
    for (int i = 0; i < size(); ++i) {
      IntervalVar* const interval =
          by_start_min_[i]->mutable_interval();
      // Delay propagation, as this constraint is not incremental: we pay
      // O(n log n) each time the constraint is awakened.
      Demon* const demon = MakeDelayedConstraintDemon0(
          solver(), this, &EdgeFinder::InitialPropagate, "RangeChanged");
      interval->WhenAnything(demon);
    }
  }

  virtual void InitialPropagate() {
    InitPropagation();
    FillInTree();
    // TODO(user) adjust start mins
  }

 private:
  void InitPropagation() {
    // Sort by start min
    Sort(&by_start_min_, StartMinLessThan<CumulativeTask>);
    for (int i = 0; i < size_; ++i) {
      by_start_min_[i]->set_start_min_index(i);
    }
    // Sort by end max
    Sort(&by_end_max_, EndMaxLessThan<CumulativeTask>);
    // Clear tree
    lt_tree_.Clear();
  }

  // Fill the theta-lambda-tree, and check for overloading
  void FillInTree() {
    for (int i = 0; i < size_; ++i) {
      CumulativeIndexedTask* const indexed_task = by_end_max_[i];
      lt_tree_.Insert(indexed_task);
      // Maximum energetic end min without overload
      const int64 max_feasible = capacity_ * indexed_task->EndMax();
      if (lt_tree_.energetic_end_min() > max_feasible) {
        solver()->Fail();
      }
    }
  }

  const int64 capacity_;
  const int size_;
  vector<CumulativeIndexedTask*> by_start_min_;
  vector<CumulativeIndexedTask*> by_end_max_;
  CumulativeLambdaThetaTree lt_tree_;
  DISALLOW_COPY_AND_ASSIGN(EdgeFinder);
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
    // Note that this is different from the way things work in the disjunctive
    // case, where we post a single DecomposedSequenceConstraint that manages
    // the demons and the fixed point algorithm. The reason is that here we have
    // only one propagator, versus several for the disjunctive case. Because of
    // this, there is no reason to do the fixpoint by hand, and we can just rely
    // on the solver to call the propagators in arbitrary order.
    PostOneSidedEdgeFinder(false);
    PostOneSidedEdgeFinder(true);
  }

  virtual void InitialPropagate() {
    // Nothing to do: this constraint delegates all the work to other classes
  }

 private:
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

  // Makes and return an edge-finder, or NULL if it is not necessary.
  Constraint* MakeEdgeFinder(bool mirror) {
    vector<CumulativeTask*> useful_tasks;
    PopulateVectorUsefulTasks(mirror, &useful_tasks);
    if (useful_tasks.empty()) {
      return NULL;
    } else {
      EdgeFinder* const ef = new EdgeFinder(solver(), useful_tasks, capacity_);
      return solver()->RevAlloc(ef);
    }
  }

  // Post a straight or mirrored edge-finder, if needed
  void PostOneSidedEdgeFinder(bool mirror) {
    Constraint* edgeFinder = MakeEdgeFinder(mirror);
    if (edgeFinder != NULL) {
      solver()->AddConstraint(edgeFinder);
    }
  }

 private:
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

Constraint* Solver::MakeCumulative(
      IntervalVar* const * intervals,
      const int64 * demands,
      int size,
      int64 capacity,
      const string& name) {
  for (int i = 0; i < size; ++i) {
    CHECK_GE(demands[i], 0);
  }
  return RevAlloc(new CumulativeConstraint(
      this, intervals, demands, size, capacity, name));
}

Constraint* Solver::MakeCumulative(
      vector<IntervalVar*>& intervals,
      const vector<int64>& demands,
      int64 capacity,
      const string& name) {
  CHECK_EQ(intervals.size(), demands.size());
  IntervalVar** intervals_array = intervals.data();
  const int64* demands_array = demands.data();
  const int size = intervals.size();
  return MakeCumulative(intervals_array, demands_array, size, capacity, name);
}

}  // namespace operations_research
