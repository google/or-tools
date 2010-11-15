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

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/stl_util-inl.h"
#include "constraint_solver/constraint_solveri.h"

namespace operations_research {

// ----- interval <unary relation> date -----

namespace {
const char* kUnaryNames[] = {
  "ENDS_AFTER",
  "ENDS_AT",
  "ENDS_BEFORE",
  "STARTS_AFTER",
  "STARTS_AT",
  "STARTS_BEFORE",
  "CROSS_DATE",
  "AVOID_DATE",
};

const char* kBinaryNames[] = {
  "ENDS_AFTER_END",
  "ENDS_AFTER_START",
  "ENDS_AT_END",
  "ENDS_AT_START",
  "STARTS_AFTER_END",
  "STARTS_AFTER_START",
  "STARTS_AT_END",
  "STARTS_AT_START"
};
}  // namespace

class IntervalUnaryRelation : public Constraint {
 public:
  IntervalUnaryRelation(Solver* const s,
                        IntervalVar* const t,
                        int64 d,
                        Solver::UnaryIntervalRelation rel)
      : Constraint(s), t_(t), d_(d), rel_(rel) {}
  virtual ~IntervalUnaryRelation() {}

  virtual void Post();
  virtual void InitialPropagate();

  virtual string DebugString() const {
    return StringPrintf("(%s %s %" GG_LL_FORMAT "d)",
                        t_->DebugString().c_str(), kUnaryNames[rel_], d_);
  }
 private:
  IntervalVar* const t_;
  const int64 d_;
  const Solver::UnaryIntervalRelation rel_;
};

void IntervalUnaryRelation::Post() {
  if (t_->MayBePerformed()) {
    Demon* d = solver()->MakeConstraintInitialPropagateCallback(this);
    t_->WhenStartRange(d);
    t_->WhenDurationRange(d);
    t_->WhenEndRange(d);
    t_->WhenPerformedBound(d);
  }
}

void IntervalUnaryRelation::InitialPropagate() {
  if (t_->MayBePerformed()) {
    switch (rel_) {
      case Solver::ENDS_AFTER:
        t_->SetEndMin(d_);
        break;
      case Solver::ENDS_AT:
        t_->SetEndRange(d_, d_);
        break;
      case Solver::ENDS_BEFORE:
        t_->SetEndMax(d_);
        break;
      case Solver::STARTS_AFTER:
        t_->SetStartMin(d_);
        break;
      case Solver::STARTS_AT:
        t_->SetStartRange(d_, d_);
        break;

      case Solver::STARTS_BEFORE:
        t_->SetStartMax(d_);
        break;
      case Solver::CROSS_DATE:
        t_->SetStartMax(d_);
        t_->SetEndMin(d_);
        break;
      case Solver::AVOID_DATE:
        if (t_->EndMin() > d_) {
          t_->SetStartMin(d_);
        } else if (t_->StartMax() < d_) {
          t_->SetEndMax(d_);
        }
        break;
    }
  }
}

Constraint* Solver::MakeIntervalVarRelation(IntervalVar* const t,
                                            Solver::UnaryIntervalRelation r,
                                            int64 d) {
  return RevAlloc(new IntervalUnaryRelation(this, t, d, r));
}

// ----- interval <binary relation> interval -----

class IntervalBinaryRelation : public Constraint {
 public:
  IntervalBinaryRelation(Solver* const s,
                         IntervalVar* const t1,
                         IntervalVar* const t2,
                         Solver::BinaryIntervalRelation rel)
      : Constraint(s), t1_(t1), t2_(t2), rel_(rel) {}
  virtual ~IntervalBinaryRelation() {}

  virtual void Post();
  virtual void InitialPropagate();

  virtual string DebugString() const {
    return StringPrintf("(%s %s %s)",
                        t1_->DebugString().c_str(),
                        kBinaryNames[rel_],
                        t2_->DebugString().c_str());
  }
 private:
  IntervalVar* const t1_;
  IntervalVar* const t2_;
  const Solver::BinaryIntervalRelation rel_;
};

void IntervalBinaryRelation::Post() {
  if (t1_->MayBePerformed() && t2_->MayBePerformed()) {
    Demon* d = solver()->MakeConstraintInitialPropagateCallback(this);
    t1_->WhenStartRange(d);
    t1_->WhenDurationRange(d);
    t1_->WhenEndRange(d);
    t1_->WhenPerformedBound(d);
    t2_->WhenStartRange(d);
    t2_->WhenDurationRange(d);
    t2_->WhenEndRange(d);
    t2_->WhenPerformedBound(d);
  }
}

// TODO(user) : make code more compact, use function pointers?
void IntervalBinaryRelation::InitialPropagate() {
  switch (rel_) {
    case Solver::ENDS_AFTER_END:
      if (t2_->MustBePerformed() && t1_->MayBePerformed()) {
        t1_->SetEndMin(t2_->EndMin());
      }
      if (t1_->MustBePerformed() && t2_->MayBePerformed()) {
        t2_->SetEndMax(t1_->EndMax());
      }
      break;
    case Solver::ENDS_AFTER_START:
      if (t2_->MustBePerformed() && t1_->MayBePerformed()) {
        t1_->SetEndMin(t2_->StartMin());
      }
      if (t1_->MustBePerformed() && t2_->MayBePerformed()) {
        t2_->SetStartMax(t1_->EndMax());
      }
      break;
    case Solver::ENDS_AT_END:
      if (t2_->MustBePerformed() && t1_->MayBePerformed()) {
        t1_->SetEndRange(t2_->EndMin(), t2_->EndMax());
      }
      if (t1_->MustBePerformed() && t2_->MayBePerformed()) {
        t2_->SetEndRange(t1_->EndMin(), t1_->EndMax());
      }
      break;
    case Solver::ENDS_AT_START:
      if (t2_->MustBePerformed() && t1_->MayBePerformed()) {
        t1_->SetEndRange(t2_->StartMin(), t2_->StartMax());
      }
      if (t1_->MustBePerformed() && t2_->MayBePerformed()) {
        t2_->SetStartRange(t1_->EndMin(), t1_->EndMax());
      }
      break;
    case Solver::STARTS_AFTER_END:
      if (t2_->MustBePerformed() && t1_->MayBePerformed()) {
        t1_->SetStartMin(t2_->EndMin());
      }
      if (t1_->MustBePerformed() && t2_->MayBePerformed()) {
        t2_->SetEndMax(t1_->StartMax());
      }
      break;
    case Solver::STARTS_AFTER_START:
      if (t2_->MustBePerformed() && t1_->MayBePerformed()) {
        t1_->SetStartMin(t2_->StartMin());
      }
      if (t1_->MustBePerformed() && t2_->MayBePerformed()) {
        t2_->SetEndMax(t1_->StartMax());
      }
      break;
    case Solver::STARTS_AT_END:
      if (t2_->MustBePerformed() && t1_->MayBePerformed()) {
        t1_->SetStartRange(t2_->EndMin(), t2_->EndMax());
      }
      if (t1_->MustBePerformed() && t2_->MayBePerformed()) {
        t2_->SetEndRange(t1_->StartMin(), t1_->StartMax());
      }
      break;
    case Solver::STARTS_AT_START:
      if (t2_->MustBePerformed() && t1_->MayBePerformed()) {
        t1_->SetStartRange(t2_->StartMin(), t2_->StartMax());
      }
      if (t1_->MustBePerformed() && t2_->MayBePerformed()) {
        t2_->SetStartRange(t1_->StartMin(), t1_->StartMax());
      }
      break;
  }
}

Constraint* Solver::MakeIntervalVarRelation(IntervalVar* const t1,
                                            Solver::BinaryIntervalRelation r,
                                            IntervalVar* const t2) {
  return RevAlloc(new IntervalBinaryRelation(this, t1, t2, r));
}

// ----- activity a before activity b or activity b before activity a -----

class TemporalDisjunction : public Constraint {
 public:
  enum State { ONE_BEFORE_TWO, TWO_BEFORE_ONE, UNDECIDED };

  TemporalDisjunction(Solver* const s,
                      IntervalVar* const t1,
                      IntervalVar* const t2,
                      IntVar* const alt)
      : Constraint(s), t1_(t1), t2_(t2), alt_(alt), state_(UNDECIDED) {}
  virtual ~TemporalDisjunction() {}

  virtual void Post();
  virtual void InitialPropagate();
  virtual string DebugString() const;

  void RangeDemon1();
  void RangeDemon2();
  void RangeAlt();
  void Decide(State s);
  void TryToDecide();
 private:
  IntervalVar* const t1_;
  IntervalVar* const t2_;
  IntVar* const alt_;
  State state_;
};

void TemporalDisjunction::Post() {
  Solver* const s = solver();
  Demon* d = MakeConstraintDemon0(s,
                                  this,
                                  &TemporalDisjunction::RangeDemon1,
                                  "RangeDemon1");
  t1_->WhenStartRange(d);
  d = MakeConstraintDemon0(s,
                           this,
                           &TemporalDisjunction::RangeDemon2,
                           "RangeDemon2");
  t2_->WhenStartRange(d);
  if (alt_ != NULL) {
    d = MakeConstraintDemon0(s,
                             this,
                             &TemporalDisjunction::RangeAlt,
                             "RangeAlt");
    alt_->WhenRange(d);
  }
}

void TemporalDisjunction::InitialPropagate() {
  if (alt_ != NULL) {
    alt_->SetRange(0, 1);
  }
  if (alt_ != NULL && alt_->Bound()) {
    RangeAlt();
  } else {
    RangeDemon1();
    RangeDemon2();
  }
}

string TemporalDisjunction::DebugString() const {
  string out;
  SStringPrintf(&out, "TemporalDisjunction(%s, %s",
                t1_->DebugString().c_str(), t2_->DebugString().c_str());
  if (alt_ != NULL) {
    StringAppendF(&out, " => %s", alt_->DebugString().c_str());
  }
  out += ") ";
  return out;
}

void TemporalDisjunction::TryToDecide() {
  DCHECK_EQ(UNDECIDED, state_);
  if (t1_->MayBePerformed() && t2_->MayBePerformed() &&
      (t1_->MustBePerformed() || t2_->MustBePerformed())) {
    if (t1_->EndMin() > t2_->StartMax()) {
      Decide(TWO_BEFORE_ONE);
    } else if (t2_->EndMin() > t1_->StartMax()) {
      Decide(ONE_BEFORE_TWO);
    }
  }
}

void TemporalDisjunction::RangeDemon1() {
  switch (state_) {
    case ONE_BEFORE_TWO: {
      if (t1_->MustBePerformed() && t2_->MayBePerformed()) {
        t2_->SetStartMin(t1_->EndMin());
      }
      break;
    }
    case TWO_BEFORE_ONE: {
      if (t1_->MustBePerformed() && t2_->MayBePerformed()) {
        t2_->SetEndMax(t1_->StartMax());
      }
      break;
    }
    case UNDECIDED: {
      TryToDecide();
    }
  }
}

void TemporalDisjunction::RangeDemon2() {
  if (t1_->MayBePerformed() || t2_->MayBePerformed()) {
    switch (state_) {
      case ONE_BEFORE_TWO: {
        if (t2_->MustBePerformed() && t1_->MayBePerformed()) {
          t1_->SetEndMax(t2_->StartMax());
        }
        break;
      }
      case TWO_BEFORE_ONE: {
        if (t2_->MustBePerformed() && t1_->MayBePerformed()) {
          t1_->SetStartMin(t2_->EndMin());
        }
        break;
      }
      case UNDECIDED: {
        TryToDecide();
      }
    }
  }
}

void TemporalDisjunction::RangeAlt() {
  DCHECK(alt_ != NULL);
  if (alt_->Value() == 0) {
    Decide(ONE_BEFORE_TWO);
  } else {
    Decide(TWO_BEFORE_ONE);
  }
}

void TemporalDisjunction::Decide(State s) {
  // Should Decide on a fixed state?
  DCHECK_NE(s, UNDECIDED);
  if (state_ != UNDECIDED && state_ != s) {
    solver()->Fail();
  }
  solver()->SaveValue(reinterpret_cast<int*>(&state_));
  state_ = s;
  if (alt_ != NULL) {
    if (s == ONE_BEFORE_TWO) {
      alt_->SetValue(0);
    } else {
      alt_->SetValue(1);
    }
  }
  RangeDemon1();
  RangeDemon2();
}

Constraint* Solver::MakeTemporalDisjunction(IntervalVar* const t1,
                                            IntervalVar* const t2,
                                            IntVar* const alt) {
  return RevAlloc(new TemporalDisjunction(this, t1, t2, alt));
}

Constraint* Solver::MakeTemporalDisjunction(IntervalVar* const t1,
                                            IntervalVar* const t2) {
  return RevAlloc(new TemporalDisjunction(this, t1, t2, NULL));
}

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
    t->WhenStartRange(d);
    t->WhenEndRange(d);
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
class IntervalWrapper {
 public:
  explicit IntervalWrapper(int index, IntervalVar* const t)
      : t_(t), index_(index), est_pos_(-1) {}

  int index() const { return index_; }

  IntervalVar* const interval() const { return t_; }

  int est_pos() const { return est_pos_; }
  void set_est_pos(int pos) { est_pos_ = pos; }

  string DebugString() const {
    return StringPrintf("Wrapper(%s, index = %d, est_pos = %d)",
                        t_->DebugString().c_str(), index_, est_pos_);
  }
 private:
  IntervalVar* const t_;
  const int index_;
  int est_pos_;
  DISALLOW_COPY_AND_ASSIGN(IntervalWrapper);
};

// Comparison methods, used by the STL sort.
bool StartMinLessThan(const IntervalWrapper* const w1,
                  const IntervalWrapper* const w2) {
  return (w1->interval()->StartMin() < w2->interval()->StartMin());
}

bool EndMaxLessThan(const IntervalWrapper* const w1,
                  const IntervalWrapper* const w2) {
  return (w1->interval()->EndMax() < w2->interval()->EndMax());
}

bool StartMaxLessThan(const IntervalWrapper* const w1,
                  const IntervalWrapper* const w2) {
  return (w1->interval()->StartMax() < w2->interval()->StartMax());
}

bool EndMinLessThan(const IntervalWrapper* const w1,
                  const IntervalWrapper* const w2) {
  return (w1->interval()->EndMin() < w2->interval()->EndMin());
}

// This is based on Petr Vilim (public) PhD work. All names comes from his work.
// See http://vilim.eu/petr.
// A theta-tree is a container for a set of intervals supporting the following
// operations:
// * Insertions and deletion in O(log size_), with size_ the maximal number of
//     tasks the tree may contain;
// * Querying the following quantity in O(1):
//     Max_{subset S of the set of contained intervals} (
//             Min_{i in S}(i.StartMin) + Sum_{i in S}(i.DurationMin) )
class ThetaTree : public BaseObject {
 public:

  struct Node {
    Node() : interval(NULL), total_processing(0), total_ect(kint64min) {}
    IntervalVar* interval;
    int64 total_processing;
    int64 total_ect;
  };

  explicit ThetaTree(int size);
  virtual ~ThetaTree() {}

  void Insert(IntervalVar* const t, int pos);
  void Remove(int pos);
  int64 ECT() { return ect(0); }
  void Clear();
  bool Inserted(int pos) {
    return nodes_[pos + isize_].interval != NULL;
  }

  virtual string DebugString() const;
 private:
  void ReCompute(int pos);
  void ReComputeAux(int pos);
  int64 processing(int pos) { return nodes_[pos].total_processing; }
  int64 ect(int pos) { return nodes_[pos].total_ect; }
  int father(int pos) const { return (pos - 1) >> 1; }
  int left(int pos) const { return (pos << 1) + 1; }
  int right(int pos) const { return (pos + 1) << 1; }

  const int size_;
  int isize_;
  vector<Node> nodes_;
};

ThetaTree::ThetaTree(int size) : size_(size) {
  // The tasks will be stored at the leaves only.
  // Non-leaf nodes will exist only to do the computations.
  // Compute the number of non-leaf nodes.
  isize_ = 1;
  while (isize_ < size) {
    isize_ <<= 1;
  }
  isize_--;
  isize_ = max(isize_, 1);
  // The number of leaves is isize_ + 1, so the total number of nodes is
  // 2 * isize_ + 1.
  const int num_nodes_in_tree = (isize_ << 1) + 1;
  DCHECK_GE(num_nodes_in_tree, 3);
  nodes_.resize(num_nodes_in_tree);
}

void ThetaTree::Clear() {
  for (vector<Node>::iterator it = nodes_.begin();
       it != nodes_.end();
       ++it) {
    (*it).interval = NULL;
    (*it).total_processing = 0LL;
    (*it).total_ect = kint64min;
  }
}

void ThetaTree::Insert(IntervalVar* const t, int pos) {
  const int curr_pos = isize_ + pos;
  Node& n = nodes_[curr_pos];
  DCHECK(n.interval == NULL);
  n.interval = t;
  n.total_ect = t->EndMin();
  n.total_processing = t->DurationMin();
  ReCompute(father(curr_pos));
}

void ThetaTree::Remove(int pos) {
  const int curr_pos = isize_ + pos;
  Node& n = nodes_[curr_pos];
  DCHECK(n.interval != NULL);
  n.interval = NULL;
  n.total_ect = kint64min;
  n.total_processing = 0LL;
  ReCompute(father(curr_pos));
}

void ThetaTree::ReComputeAux(int pos) {
  const int64 pl = processing(left(pos));
  const int64 pr = processing(right(pos));
  nodes_[pos].total_processing = pl + pr;
  const int64 el = ect(left(pos));
  const int64 er = ect(right(pos));
  const int64 en = max(er, el + pr);
  nodes_[pos].total_ect = en;
}

void ThetaTree::ReCompute(int pos) {
  while (pos > 0) {
    ReComputeAux(pos);
    pos = father(pos);
  }
  // Fast recompute the top node. We do not need all info.
  nodes_[0].total_ect = max(nodes_[2].total_ect,
                            nodes_[1].total_ect + nodes_[2].total_processing);
}

string ThetaTree::DebugString() const {
  string out;
  for (int i = 0; i < isize_ + size_; ++i) {
    int64 p = nodes_[i].total_processing;
    int64 e = nodes_[i].total_ect;
    IntervalVar* t = nodes_[i].interval;
    StringAppendF(&out, "(%d: p = %" GG_LL_FORMAT "d, e = %"
                  GG_LL_FORMAT "d", i, p, (e < 0 ? -1 : e));
    if (t != NULL) {
      StringAppendF(&out, ", t = %s", t->DebugString().c_str());
    }
    out += ") ";
  }
  return out;
}

// ----- Lambda Theta Tree -----

// TODO(user) Can we merge tLambdaThetaTree and ThetaTree?
class LambdaThetaTree : public BaseObject {
 public:

  struct ENode {
    ENode() : interval(NULL), processing(0LL), ect(kint64min),
             processing_opt(0LL), ect_opt(kint64min),
             responsible_ect(-1), responsible_processing(-1) {}
    IntervalVar* interval;
    int64 processing;
    int64 ect;
    int64 processing_opt;
    int64 ect_opt;
    int responsible_ect;
    int responsible_processing;
  };

  explicit LambdaThetaTree(int size);
  virtual ~LambdaThetaTree() {}

  void Insert(IntervalVar* const t, int pos);
  void Grey(int pos);
  void Remove(int pos);
  int64 ECT() { return ect(0); }
  int64 ECT_opt() { return ect_opt(0); }
  int Responsible_opt() { return responsible_ect(0); }
  void Clear();
  bool Inserted(int pos) {
    return nodes_[pos + isize_].interval != NULL;
  }
  virtual string DebugString() const;
 private:
  void ReCompute(int pos);
  void ReComputeAux(int pos);
  void ReComputeTop();
  int64 processing(int pos) { return nodes_[pos].processing; }
  int64 ect(int pos) { return nodes_[pos].ect; }
  int64 processing_opt(int pos) { return nodes_[pos].processing_opt; }
  int64 ect_opt(int pos) { return nodes_[pos].ect_opt; }
  int responsible_ect(int pos) { return nodes_[pos].responsible_ect; }
  int responsible_processing(int pos) {
    return nodes_[pos].responsible_processing;
  }
  int father(int pos) const { return (pos - 1) >> 1; }
  int left(int pos) const { return (pos << 1) + 1; }
  int right(int pos) const { return (pos + 1) << 1; }
  const int size_;
  int isize_;
  vector<ENode> nodes_;
};

LambdaThetaTree::LambdaThetaTree(int size) : size_(size) {
  // Compute the number of internal nodes
  isize_ = 1;
  while (isize_ < size) {
    isize_ <<= 1;
  }
  isize_--;
  isize_ = max(isize_, 1);
  // Resize a bit bigger such that the bottom layer is full.
  const int num_nodes_in_tree = (isize_ << 1) + 1;
  DCHECK_GE(num_nodes_in_tree, 3);
  nodes_.resize(num_nodes_in_tree);
}

void LambdaThetaTree::Clear() {
  for (vector<ENode>::iterator it = nodes_.begin();
       it != nodes_.end();
       ++it) {
    (*it).interval = NULL;
    (*it).processing = 0LL;
    (*it).ect = kint64min;
    (*it).processing_opt = 0LL;
    (*it).ect_opt = kint64min;
    (*it).responsible_ect = -1;
    (*it).responsible_processing = -1;
  }
}

void LambdaThetaTree::Insert(IntervalVar* const t, int pos) {
  const int curr_pos = isize_ + pos;
  ENode& n = nodes_[curr_pos];
  DCHECK(n.interval == NULL);
  n.interval = t;
  n.ect = t->EndMin();
  n.processing = t->DurationMin();
  n.ect_opt = t->EndMin();
  n.processing_opt = t->DurationMin();
  n.responsible_ect = -1;
  n.responsible_processing = -1;
  ReCompute(father(curr_pos));
}

void LambdaThetaTree::Grey(int pos) {
  const int curr_pos = isize_ + pos;
  ENode& n = nodes_[curr_pos];
  DCHECK(n.interval != NULL);
  n.ect = kint64min;
  n.processing = 0LL;
  n.responsible_ect = pos;
  n.responsible_processing = pos;
  ReCompute(father(curr_pos));
}

void LambdaThetaTree::Remove(int pos) {
  const int curr_pos = isize_ + pos;
  ENode& n = nodes_[curr_pos];
  DCHECK(n.interval != NULL);
  n.interval = NULL;
  n.ect = kint64min;
  n.processing = 0LL;
  n.ect_opt = kint64min;
  n.processing_opt = 0LL;
  n.responsible_ect = -1;
  n.responsible_processing = -1;
  ReCompute(father(curr_pos));
}

void LambdaThetaTree::ReComputeAux(int pos) {
  ENode& n = nodes_[pos];
  const int64 pr = processing(right(pos));
  n.processing = processing(left(pos)) + pr;
  n.ect =  max(ect(right(pos)), ect(left(pos)) + pr);
  if (responsible_ect(left(pos)) == -1 && responsible_ect(right(pos)) == -1) {
    n.processing_opt = n.processing;
    n.ect_opt = n.ect;
    n.responsible_ect = -1;
    n.responsible_processing = -1;
  } else {
    const int64 lo = processing_opt(left(pos)) + processing(right(pos));
    const int64 ro = processing(left(pos)) + processing_opt(right(pos));
    if (lo > ro) {
      n.processing_opt = lo;
      n.responsible_processing = responsible_processing(left(pos));
    } else {
      n.processing_opt = ro;
      n.responsible_processing = responsible_processing(right(pos));
    }
    const int64 ect1 = ect_opt(right(pos));
    const int64 ect2 = ect(left(pos)) + processing_opt(right(pos));
    const int64 ect3 = ect_opt(left(pos)) + processing(right(pos));
    if (ect1 >= ect2 && ect1 >= ect3) {  // ect1 max
      n.ect_opt = ect1;
      n.responsible_ect = responsible_ect(right(pos));
    } else if (ect2 >= ect1 && ect2 >= ect3) {  // ect2 max
      n.ect_opt = ect2;
      n.responsible_ect = responsible_processing(right(pos));
    } else {  // ect3 max
      n.ect_opt = ect3;
      n.responsible_ect = responsible_ect(left(pos));
    }
    DCHECK_GE(n.processing_opt, n.processing);
    DCHECK((n.responsible_processing != -1) ||
           (n.processing_opt == n.processing));
  }
}

void LambdaThetaTree::ReComputeTop() {
  ENode& n = nodes_[0];
  n.ect = max(ect(2), ect(1) + processing(2));
  if (responsible_ect(1) == -1 && responsible_ect(2) == -1) {
    n.processing_opt = n.processing;
    n.ect_opt = n.ect;
    n.responsible_ect = -1;
    n.responsible_processing = -1;
  } else {
    const int64 ect1 = ect_opt(2);
    const int64 ect2 = ect(1) + processing_opt(2);
    const int64 ect3 = ect_opt(1) + processing(2);
    if (ect1 >= ect2 && ect1 >= ect3) {
      n.ect_opt = ect1;
      n.responsible_ect = responsible_ect(2);
    } else if (ect2 >= ect1 && ect2 >= ect3) {
      n.ect_opt = ect2;
      n.responsible_ect = responsible_processing(2);
    } else {  // ect3 >= ect1 && ect3 >= ect2
      n.ect_opt = ect3;
      n.responsible_ect = responsible_ect(1);
    }
  }
}

void LambdaThetaTree::ReCompute(int pos) {
  DCHECK_LT(pos, isize_);
  while (pos > 0) {
    ReComputeAux(pos);
    pos = father(pos);
  }
  ReComputeTop();
}

string LambdaThetaTree::DebugString() const {
  string out;
  for (int i = 0; i < isize_ + size_; ++i) {
    int64 p = nodes_[i].processing;
    int64 e = nodes_[i].ect;
    int64 po = nodes_[i].processing_opt;
    int64 eo = nodes_[i].ect_opt;
    int re = nodes_[i].responsible_ect;
    int rp = nodes_[i].responsible_processing;
    IntervalVar* t = nodes_[i].interval;
    StringAppendF(&out,
                  "(%d: p = %" GG_LL_FORMAT "d, e = %"
                  GG_LL_FORMAT "d, po = %" GG_LL_FORMAT "d, "
                  "eo = %" GG_LL_FORMAT "d, re = %d, rp = %d",
                  i, p, (e < 0 ? -1 : e), po, (eo < 0 ? -1 : eo), re, rp);
    if (t != NULL) {
      StringAppendF(&out, ", t = %s", t->DebugString().c_str());
    }
    out += ") ";
  }
  return out;
}

// -------------- Not Last -----------------------------------------

// A class that implements the 'Not-Last' propagation algorithm for the unary
// resource constraint.
class NotLast {
 public:
  NotLast(Solver* const solver,
          IntervalVar* const * intervals,
          int size,
          bool mirror);
  ~NotLast();
  bool Propagate();

 private:
  const int size_;
  // The interval variables on which propagation is done. As not-last pushes
  // to the left, these are open on left for optional intervals.
  scoped_array<IntervalVar*> intervals_;
  ThetaTree theta_tree_;
  vector<IntervalWrapper*> est_;
  vector<IntervalWrapper*> lct_;
  vector<IntervalWrapper*> lst_;
  vector<int64> new_lct_;
  DISALLOW_COPY_AND_ASSIGN(NotLast);
};

NotLast::NotLast(Solver* const solver,
                 IntervalVar* const * intervals,
                 int size,
                 bool mirror)
: size_(size),
  intervals_(new IntervalVar*[size]),
  theta_tree_(size),
  est_(size),
  lct_(size),
  lst_(size),
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
    intervals_[i] = solver->MakeIntervalRelaxedMin(underlying);
    est_[i] = new IntervalWrapper(i, intervals_[i]);
    lct_[i] = est_[i];
    lst_[i] = est_[i];
  }
}

NotLast::~NotLast() {
  STLDeleteElements(&est_);
}

bool NotLast::Propagate() {
  // ---- Init ----
  theta_tree_.Clear();
  for (int i = 0; i < size_; ++i) {
    new_lct_[i] = intervals_[i]->EndMax();
  }
  sort(lst_.begin(), lst_.end(), StartMaxLessThan);
  sort(lct_.begin(), lct_.end(), EndMaxLessThan);
  // Update EST
  sort(est_.begin(), est_.end(), StartMinLessThan);
  for (int i = 0; i < size_; ++i) {
    est_[i]->set_est_pos(i);
  }

  // --- Execute ----
  int j = 0;
  for (int i = 0; i < size_; ++i) {
    IntervalWrapper* const twi = lct_[i];
    while (j < size_ &&
           twi->interval()->EndMax() > lst_[j]->interval()->StartMax()) {
      if (j > 0 && theta_tree_.ECT() > lst_[j]->interval()->StartMax()) {
        new_lct_[lst_[j]->index()] = lst_[j - 1]->interval()->StartMax();
      }
      theta_tree_.Insert(lst_[j]->interval(), lst_[j]->est_pos());
      j++;
    }
    const bool inserted = theta_tree_.Inserted(twi->est_pos());
    if (inserted) {
      theta_tree_.Remove(twi->est_pos());
    }
    const int64 ect_theta_less_i = theta_tree_.ECT();
    if (inserted) {
      theta_tree_.Insert(twi->interval(), twi->est_pos());
    }
    if (ect_theta_less_i > twi->interval()->EndMax() && j > 0) {
      new_lct_[twi->index()] =
          min(new_lct_[twi->index()], lst_[j - 1]->interval()->EndMax());
    }
  }

  // Apply modifications
  bool modified = false;
  for (int i = 0; i < size_; ++i) {
    if (intervals_[i]->EndMax() > new_lct_[i]) {
      modified = true;
      intervals_[i]->SetEndMax(new_lct_[i]);
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
    STLDeleteElements(&wrappers_);
  }
  int size() const { return size_; }
  IntervalVar* const * intervals() { return intervals_.get(); }
  void UpdateEst();
  void OverloadChecking();
  bool DetectablePrecedences();
  bool EdgeFinder();

 private:
  Solver* const solver_;
  // The interval variables on which propagation is done. As these algorithms
  // push to the right, these are open on right for optional intervals.
  scoped_array<IntervalVar*> intervals_;
  const int size_;
  vector<IntervalWrapper*> wrappers_;

  // --- All the following member variables are essentially used as local ones:
  // no invariant is maintained about them, except for the fact that the vectors
  // always contains all the considered intervals, so any function that wants to
  // use them must first sort them in the right order.

  ThetaTree theta_tree_;
  vector<IntervalWrapper*> ect_;
  vector<IntervalWrapper*> est_;
  vector<IntervalWrapper*> lct_;
  vector<IntervalWrapper*> lst_;
  vector<int64> new_est_;
  vector<int64> new_lct_;
  LambdaThetaTree lt_tree_;
};

EdgeFinderAndDetectablePrecedences::EdgeFinderAndDetectablePrecedences(
    Solver* const solver,
    IntervalVar* const * intervals,
    int size,
    bool mirror)
  : solver_(solver),
    intervals_(new IntervalVar*[size]),
    size_(size), theta_tree_(size), lt_tree_(size) {
  // Populate of the array of intervals
  for (int i = 0; i < size; ++i) {
    IntervalVar* underlying = NULL;
    if (mirror) {
      underlying = solver->MakeMirrorInterval(intervals[i]);
    } else {
      underlying = intervals[i];
    }
    intervals_[i] = solver->MakeIntervalRelaxedMax(underlying);
  }
  for (int i = 0; i < size; ++i) {
    IntervalWrapper* const w = new IntervalWrapper(i, intervals_[i]);
    wrappers_.push_back(w);
    ect_.push_back(w);
    est_.push_back(w);
    lct_.push_back(w);
    lst_.push_back(w);
    new_est_.push_back(kint64min);
  }
}

void EdgeFinderAndDetectablePrecedences::UpdateEst() {
  std::sort(est_.begin(), est_.end(), StartMinLessThan);
  for (int i = 0; i < size_; ++i) {
    est_[i]->set_est_pos(i);
  }
}

void EdgeFinderAndDetectablePrecedences::OverloadChecking() {
  // Init
  UpdateEst();
  std::sort(lct_.begin(), lct_.end(), EndMaxLessThan);
  theta_tree_.Clear();

  for (int i = 0; i < size_; ++i) {
    IntervalWrapper* const iw = lct_[i];
    theta_tree_.Insert(iw->interval(), iw->est_pos());
    if (theta_tree_.ECT() > iw->interval()->EndMax()) {
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
  std::sort(ect_.begin(), ect_.end(), EndMinLessThan);
  std::sort(lst_.begin(), lst_.end(), StartMaxLessThan);
  theta_tree_.Clear();
  int j = 0;
  for (int i = 0; i < size_; ++i) {
    IntervalWrapper* const twi = ect_[i];
    if (j < size_) {
      IntervalWrapper* twj = lst_[j];
      while (twi->interval()->EndMin() > twj->interval()->StartMax()) {
        theta_tree_.Insert(twj->interval(), twj->est_pos());
        j++;
        if (j == size_)
          break;
        twj = lst_[j];
      }
    }
    const int64 esti = twi->interval()->StartMin();
    bool inserted = theta_tree_.Inserted(twi->est_pos());
    if (inserted) {
      theta_tree_.Remove(twi->est_pos());
    }
    const int64 oesti = theta_tree_.ECT();
    if (inserted) {
      theta_tree_.Insert(twi->interval(), twi->est_pos());
    }
    if (oesti > esti) {
      new_est_[twi->index()] = oesti;
    } else {
      new_est_[twi->index()] = kint64min;
    }
  }

  // Apply modifications
  bool modified = false;
  for (int i = 0; i < size_; ++i) {
    if (new_est_[i] != kint64min) {
      modified = true;
      intervals_[i]->SetStartMin(new_est_[i]);
    }
  }
  return modified;
}

bool EdgeFinderAndDetectablePrecedences::EdgeFinder() {
  // Init
  UpdateEst();
  for (int i = 0; i < size_; ++i) {
    new_est_[i] = intervals_[i]->StartMin();
  }

  // Push in one direction.
  std::sort(lct_.begin(), lct_.end(), EndMaxLessThan);
  lt_tree_.Clear();
  for (int i = 0; i < size_; ++i) {
    lt_tree_.Insert(est_[i]->interval(), i);
    DCHECK_EQ(i, est_[i]->est_pos());
  }
  for (int j = size_ - 2; j >= 0; --j) {
    lt_tree_.Grey(lct_[j+1]->est_pos());
    IntervalWrapper* const twj = lct_[j];
    if (lt_tree_.ECT() > twj->interval()->EndMax()) {
      solver_->Fail();  // Resource is overloaded
    }
    while (lt_tree_.ECT_opt() > twj->interval()->EndMax()) {
      const int i = lt_tree_.Responsible_opt();
      DCHECK_GE(i, 0);
      const int act_i = est_[i]->index();
      if (lt_tree_.ECT() > new_est_[act_i]) {
        new_est_[act_i] = lt_tree_.ECT();
      }
      lt_tree_.Remove(i);
    }
  }

  // Apply modifications.
  bool modified = false;
  for (int i = 0; i < size_; ++i) {
    if (intervals_[i]->StartMin() < new_est_[i]) {
      modified = true;
      intervals_[i]->SetStartMin(new_est_[i]);
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
      IntervalVar* interval_var = straight_.intervals()[i];
      interval_var->WhenStartRange(d);
      interval_var->WhenDurationRange(d);
      interval_var->WhenEndRange(d);
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

}  // namespace operations_research
