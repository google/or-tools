// Copyright 2010-2011 Google
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

#include <algorithm>
#include <list>
#include <map>

#include "base/callback.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/sysinfo.h"

#include "base/timer.h"
#include "base/concise_iterator.h"
#include "base/map-util.h"
#include "base/stl_util-inl.h"
#include "base/hash.h"
#include "base/random.h"
#include "constraint_solver/constraint_solveri.h"
#include "constraint_solver/search_limit.pb.h"

DEFINE_bool(cp_use_sparse_gls_penalties, false,
            "Use sparse implementation to store Guided Local Search penalties");

namespace operations_research {

// ---------- Search Log ---------

SearchLog::SearchLog(Solver* const s,
                     OptimizeVar* const obj,
                     IntVar* const var,
                     ResultCallback<string>* display_callback,
                     int period)
    : SearchMonitor(s),
      period_(period),
      timer_(new WallTimer),
      var_(var),
      obj_(obj),
      display_callback_(display_callback),
      nsol_(0),
      tick_(0LL),
      objective_min_(kint64max),
      objective_max_(kint64min),
      min_right_depth_(kint32max),
      max_depth_(0),
      sliding_min_depth_(0),
      sliding_max_depth_(0) {
  CHECK(obj == NULL || var == NULL) << "Either var or obj need to be NULL.";
  if (display_callback_ != NULL) {
    display_callback_->CheckIsRepeatable();
  }
}

SearchLog::~SearchLog() {}

void SearchLog::EnterSearch() {
  const string buffer = StringPrintf("Start search, %s", MemoryUsage().c_str());
  OutputLine(buffer);
  timer_->Restart();
  min_right_depth_ = kint32max;
}

void SearchLog::ExitSearch() {
  const int64 branches = solver()->branches();
  int64 ms = timer_->GetInMs();
  if (ms == 0) {
    ms = 1;
  }
  const string buffer = StringPrintf(
      "End search (time = %" GG_LL_FORMAT "d ms, branches = %"
      GG_LL_FORMAT "d, failures = %" GG_LL_FORMAT
      "d, %s, speed = %" GG_LL_FORMAT "d branches/s)",
      ms, branches, solver()->failures(),
      MemoryUsage().c_str(), branches * 1000 / ms);
  OutputLine(buffer);
}

bool SearchLog::AtSolution() {
  Maintain();
  const int depth = solver()->SearchDepth();
  string obj_str = "";
  int64 current = 0;
  bool objective_updated = false;
  if (obj_ != NULL) {
    current = obj_->Var()->Value();
    obj_str = obj_->Print();
    objective_updated = true;
  } else if (var_!= NULL) {
    current = var_->Value();
    StringAppendF(&obj_str,  "%" GG_LL_FORMAT "d, ", current);
    objective_updated = true;
  }
  if (objective_updated) {
    if (current >= objective_min_) {
      StringAppendF(&obj_str,
                    "objective minimum = %" GG_LL_FORMAT "d, ",
                    objective_min_);
    } else {
      objective_min_ = current;
    }
    if (current <= objective_max_) {
      StringAppendF(&obj_str,
                    "objective maximum = %" GG_LL_FORMAT "d, ",
                    objective_max_);
    } else {
      objective_max_ = current;
    }
  }
  string log;
  StringAppendF(&log,
                "Solution #%d (%stime = %" GG_LL_FORMAT
                "d ms, branches = %" GG_LL_FORMAT "d,"
                " failures = %" GG_LL_FORMAT "d, depth = %d",
                nsol_++, obj_str.c_str(), timer_->GetInMs(),
                solver()->branches(),
                solver()->failures(),
                depth);
  if (solver()->neighbors() != 0) {
    StringAppendF(&log,
                  ", neighbors = %" GG_LL_FORMAT
                  "d, filtered neighbors = %" GG_LL_FORMAT "d,"
                  " accepted neighbors = %" GG_LL_FORMAT "d",
                  solver()->neighbors(),
                  solver()->filtered_neighbors(),
                  solver()->accepted_neighbors());
  }
  StringAppendF(&log, ", %s)", MemoryUsage().c_str());
  LG << log;
  if (display_callback_.get() != NULL) {
    LG << display_callback_->Run();
  }
  return false;
}

void SearchLog::BeginFail() {
  Maintain();
}

void SearchLog::NoMoreSolutions() {
  string buffer = StringPrintf(
      "Finished search tree, time = %" GG_LL_FORMAT
      "d ms, branches = %" GG_LL_FORMAT "d,"
      " failures = %" GG_LL_FORMAT "d",
      timer_->GetInMs(),
      solver()->branches(),
      solver()->failures());
  if (solver()->neighbors() != 0) {
    StringAppendF(&buffer,
                  ", neighbors = %" GG_LL_FORMAT
                  "d, filtered neighbors = %" GG_LL_FORMAT "d,"
                  " accepted neigbors = %" GG_LL_FORMAT "d",
                  solver()->neighbors(),
                  solver()->filtered_neighbors(),
                  solver()->accepted_neighbors());
  }
  StringAppendF(&buffer, ", %s)", MemoryUsage().c_str());
  OutputLine(buffer);
}

void SearchLog::ApplyDecision(Decision* const d) {
  Maintain();
  const int64 b = solver()->branches();
  if (b % period_ == 0 && b > 0) {
    OutputDecision();
  }
}

void SearchLog::RefuteDecision(Decision* const d) {
  min_right_depth_ = std::min(min_right_depth_, solver()->SearchDepth());
  ApplyDecision(d);
}

void SearchLog::OutputDecision() {
  string buffer = StringPrintf(
      "%" GG_LL_FORMAT "d branches, %" GG_LL_FORMAT "d ms, %"
      GG_LL_FORMAT "d failures",
      solver()->branches(), timer_->GetInMs(), solver()->failures());
  if (min_right_depth_ != kint32max && max_depth_ != 0) {
    const int depth = solver()->SearchDepth();
    StringAppendF(
        &buffer, ", tree pos=%d/%d/%d minref=%d max=%d",
        sliding_min_depth_, depth, sliding_max_depth_,
        min_right_depth_, max_depth_);
    sliding_min_depth_ = depth;
    sliding_max_depth_ = depth;
  }
  if (obj_ != NULL
      && objective_min_ != kint64max
      && objective_max_ != kint64min) {
    StringAppendF(&buffer,
                  ", objective minimum = %" GG_LL_FORMAT "d"
                  ", objective maximum = %" GG_LL_FORMAT "d",
                  objective_min_, objective_max_);
  }
  OutputLine(buffer);
}

void SearchLog::Maintain() {
  const int current_depth = solver()->SearchDepth();
  sliding_min_depth_ = std::min(current_depth, sliding_min_depth_);
  sliding_max_depth_ = std::max(current_depth, sliding_max_depth_);
  max_depth_ = std::max(current_depth, max_depth_);
}

void SearchLog::BeginInitialPropagation() {
  tick_ = timer_->GetInMs();
}

void SearchLog::EndInitialPropagation() {
  const int64 delta = std::max(timer_->GetInMs() - tick_, 0LL);
  const string buffer = StringPrintf(
      "Root node processed (time = %" GG_LL_FORMAT
      "d ms, constraints = %d, %s)",
      delta, solver()->constraints(), MemoryUsage().c_str());
  OutputLine(buffer);
}

void SearchLog::OutputLine(const string& line) {
  LG << line;
}

string SearchLog::MemoryUsage() {
  static const int64 kDisplayThreshold = 2;
  static const int64 kKiloByte = 1024;
  static const int64 kMegaByte = kKiloByte * kKiloByte;
  static const int64 kGigaByte = kMegaByte * kKiloByte;
  const int64 memory_usage = Solver::MemoryUsage();
  if (memory_usage > kDisplayThreshold * kGigaByte) {
    return StringPrintf("memory used = %.2lf GB",
                        memory_usage  * 1.0 / kGigaByte);
  } else if (memory_usage > kDisplayThreshold * kMegaByte) {
    return StringPrintf("memory used = %.2lf MB",
                        memory_usage  * 1.0 / kMegaByte);
  } else if (memory_usage > kDisplayThreshold * kKiloByte) {
    return StringPrintf("memory used = %2lf KB",
                        memory_usage * 1.0 / kKiloByte);
  } else {
    return StringPrintf("memory used = %" GG_LL_FORMAT "d", memory_usage);
  }
}

SearchMonitor* Solver::MakeSearchLog(int period) {
  return RevAlloc(new SearchLog(this, NULL, NULL, NULL, period));
}

SearchMonitor* Solver::MakeSearchLog(int period, IntVar* const var) {
  return RevAlloc(new SearchLog(this, NULL, var, NULL, period));
}

SearchMonitor* Solver::MakeSearchLog(int period,
                                     ResultCallback<string>* display_callback) {
  return RevAlloc(new SearchLog(this, NULL, NULL, display_callback, period));
}

SearchMonitor* Solver::MakeSearchLog(int period, IntVar* const var,
                                     ResultCallback<string>* display_callback) {
  return RevAlloc(new SearchLog(this, NULL, var, display_callback, period));
}

SearchMonitor* Solver::MakeSearchLog(int period, OptimizeVar* const obj) {
  return RevAlloc(new SearchLog(this, obj, NULL, NULL, period));
}

SearchMonitor* Solver::MakeSearchLog(int period, OptimizeVar* const obj,
                                     ResultCallback<string>* display_callback) {
  return RevAlloc(new SearchLog(this, obj, NULL, display_callback, period));
}

// ---------- Search Trace ----------


class SearchTrace : public SearchMonitor {
 public:
  SearchTrace(Solver* const s, const string& prefix)
      : SearchMonitor(s), prefix_(prefix) {}
  virtual ~SearchTrace() {}

  virtual void EnterSearch() {
    LG << prefix_ << " EnterSearch(" << solver()->SolveDepth() << ")";
  }
  virtual void RestartSearch() {
    LG << prefix_ << " RestartSearch(" << solver()->SolveDepth() << ")";
  }
  virtual void ExitSearch() {
    LG << prefix_ << " ExitSearch(" << solver()->SolveDepth() << ")";
  }
  virtual void BeginNextDecision(DecisionBuilder* const b) {
    LG << prefix_ << " BeginNextDecision(" << b << ") ";
  }
  virtual void EndNextDecision(DecisionBuilder* const b, Decision* const d) {
    if (d) {
      LG << prefix_ << " EndNextDecision(" << b << ", " << d << ") ";
    } else {
      LG << prefix_ << " EndNextDecision(" << b << ") ";
    }
  }
  virtual void ApplyDecision(Decision* const d) {
    LG << prefix_ << " ApplyDecision(" << d << ") ";
  }
  virtual void RefuteDecision(Decision* const d) {
    LG << prefix_ << " RefuteDecision(" << d << ") ";
  }
  virtual void AfterDecision(Decision* const d, bool apply) {
    LG << prefix_ << " AfterDecision(" << d << ", " << apply << ") ";
  }
  virtual void BeginFail() {
    LG << prefix_ << " BeginFail(" << solver()->SearchDepth() << ")";
  }
  virtual void EndFail() {
    LG << prefix_ << " EndFail(" << solver()->SearchDepth() << ")";
  }
  virtual void BeginInitialPropagation() {
    LG << prefix_ << " BeginInitialPropagation()";
  }
  virtual void EndInitialPropagation() {
    LG << prefix_ << " EndInitialPropagation()";
  }
  virtual bool AtSolution() {
    LG << prefix_ << " AtSolution()";
    return false;
  }
  virtual bool AcceptSolution() {
    LG << prefix_ << " AcceptSolution()";
    return true;
  }
  virtual void NoMoreSolutions() {
    LG << prefix_ << " NoMoreSolutions()";
  }
 private:
  const string prefix_;
};

SearchMonitor* Solver::MakeSearchTrace(const string& prefix) {
  return RevAlloc(new SearchTrace(this, prefix));
}


// ---------- Compose Decision Builder ----------

class ComposeDecisionBuilder : public DecisionBuilder {
 public:
  ComposeDecisionBuilder();
  explicit ComposeDecisionBuilder(const std::vector<DecisionBuilder*>& dbs)
      : builders_(dbs), start_index_(0) {}
  virtual ~ComposeDecisionBuilder() {}
  virtual Decision* Next(Solver* const s);
  virtual string DebugString() const;
  void add(DecisionBuilder* const db);
 private:
  std::vector<DecisionBuilder*> builders_;
  int start_index_;
};

ComposeDecisionBuilder::ComposeDecisionBuilder()
    : start_index_(0) {}

Decision* ComposeDecisionBuilder::Next(Solver* const s) {
  const int size = builders_.size();
  for (int i = start_index_; i < size; ++i) {
    Decision* d = builders_[i]->Next(s);
    if (d != NULL) {
      s->SaveAndSetValue(&start_index_, i);
      return d;
    }
  }
  s->SaveAndSetValue(&start_index_, size);
  return NULL;
}

string ComposeDecisionBuilder::DebugString() const {
  string out = "ComposeDecisionBuilder(";
  for (ConstIter<std::vector<DecisionBuilder*> > it(builders_); !it.at_end(); ++it) {
    out += (*it)->DebugString() + " ";
  }
  out += ")";
  return out;
}

void ComposeDecisionBuilder::add(DecisionBuilder* const db) {
  builders_.push_back(db);
}

DecisionBuilder* Solver::Compose(DecisionBuilder* const db1,
                                 DecisionBuilder* const db2) {
  ComposeDecisionBuilder* c = RevAlloc(new ComposeDecisionBuilder());
  c->add(db1);
  c->add(db2);
  return c;
}

DecisionBuilder* Solver::Compose(DecisionBuilder* const db1,
                                 DecisionBuilder* const db2,
                                 DecisionBuilder* const db3) {
  ComposeDecisionBuilder* c = RevAlloc(new ComposeDecisionBuilder());
  c->add(db1);
  c->add(db2);
  c->add(db3);
  return c;
}

DecisionBuilder* Solver::Compose(DecisionBuilder* const db1,
                                 DecisionBuilder* const db2,
                                 DecisionBuilder* const db3,
                                 DecisionBuilder* const db4) {
  ComposeDecisionBuilder* c = RevAlloc(new ComposeDecisionBuilder());
  c->add(db1);
  c->add(db2);
  c->add(db3);
  c->add(db4);
  return c;
}

DecisionBuilder* Solver::Compose(const std::vector<DecisionBuilder*>& dbs) {
  return  RevAlloc(new ComposeDecisionBuilder(dbs));
}

// ----------  Variable Assignments ----------

// ----- BaseAssignmentSelector -----

class BaseVariableAssignmentSelector : public BaseObject {
 public:
  BaseVariableAssignmentSelector() {}
  virtual ~BaseVariableAssignmentSelector() {}
  virtual int64 SelectValue(const IntVar* const v, int64 id) = 0;
  virtual IntVar* SelectVariable(Solver* const s, int64* id) = 0;
};

// ----- Variable selector -----

class VariableSelector : public BaseObject {
 public:
  VariableSelector(const IntVar* const* vars, int size)
  : vars_(NULL), size_(size) {
    CHECK_GE(size_, 0);
    if (size_ > 0) {
      vars_.reset(new IntVar*[size_]);
      memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    }
  }
  virtual ~VariableSelector() {}
  virtual IntVar* Select(Solver* const s, int64* id) = 0;
  string VarDebugString() const;
 protected:
  scoped_array<IntVar*> vars_;
  int size_;
};

string VariableSelector::VarDebugString() const {
  string out = "(";
  for (int i = 0; i < size_; ++i) {
    out += vars_[i]->DebugString() + " ";
  }
  out += ")";
  return out;
}

// ----- Choose first unbound --

class FirstUnboundSelector : public VariableSelector {
 public:
  FirstUnboundSelector(const IntVar* const* vars, int size)
      : VariableSelector(vars, size), first_(0) {}
  virtual ~FirstUnboundSelector() {}
  virtual IntVar* Select(Solver* const s, int64* id);
  virtual string DebugString() const { return "ChooseFirstUnbound"; }
 private:
  int first_;
};

IntVar* FirstUnboundSelector::Select(Solver* const s, int64* id) {
  for (int i = first_; i < size_; ++i) {
    IntVar* const var = vars_[i];
    if (!var->Bound()) {
      s->SaveAndSetValue(&first_, i);
      *id = i;
      return var;
    }
  }
  s->SaveAndSetValue(&first_, size_);
  *id = size_;
  return NULL;
}

// ----- Choose Min Size Lowest Min -----

class MinSizeLowestMinSelector : public VariableSelector {
 public:
  MinSizeLowestMinSelector(const IntVar* const* vars, int size)
      : VariableSelector(vars, size) {}
  virtual ~MinSizeLowestMinSelector() {}
  virtual IntVar* Select(Solver* const s, int64* id);
  virtual string DebugString() const { return "MinSizeLowestMinSelector"; }
};

IntVar* MinSizeLowestMinSelector::Select(Solver* const s, int64* id) {
  IntVar* result = NULL;
  int64 best_size = kint64max;
  int64 best_min = kint64max;
  int index = -1;
  for (int i = 0; i < size_; ++i) {
    IntVar* const var = vars_[i];
    if (!var->Bound()) {
      if (var->Size() < best_size ||
          (var->Size() == best_size && var->Min() < best_min)) {
        best_size = var->Size();
        best_min = var->Min();
        index = i;
        result = var;
      }
    }
  }
  if (index == -1) {
    *id = size_;
    return NULL;
  } else {
    *id = index;
    return result;
  }
}

// ----- Choose Min Size Highest Min -----

class MinSizeHighestMinSelector : public VariableSelector {
 public:
  MinSizeHighestMinSelector(const IntVar* const* vars, int size)
      : VariableSelector(vars, size) {}
  virtual ~MinSizeHighestMinSelector() {}
  virtual IntVar* Select(Solver* const s, int64* id);
  virtual string DebugString() const { return "MinSizeHighestMinSelector"; }
};

IntVar* MinSizeHighestMinSelector::Select(Solver* const s, int64* id) {
  IntVar* result = NULL;
  int64 best_size = kint64max;
  int64 best_min = kint64min;
  int index = -1;
  for (int i = 0; i < size_; ++i) {
    IntVar* const var = vars_[i];
    if (!var->Bound()) {
      if (var->Size() < best_size ||
          (var->Size() == best_size && var->Min() > best_min)) {
        best_size = var->Size();
        best_min = var->Min();
        index = i;
        result = var;
      }
    }
  }
  if (index == -1) {
    *id = size_;
    return NULL;
  } else {
    *id = index;
    return result;
  }
}

// ----- Choose Min Size Lowest Max -----

class MinSizeLowestMaxSelector : public VariableSelector {
 public:
  MinSizeLowestMaxSelector(const IntVar* const* vars, int size)
      : VariableSelector(vars, size) {}
  virtual ~MinSizeLowestMaxSelector() {}
  virtual IntVar* Select(Solver* const s, int64* id);
  virtual string DebugString() const { return "MinSizeLowestMaxSelector"; }
};

IntVar* MinSizeLowestMaxSelector::Select(Solver* const s, int64* id) {
  IntVar* result = NULL;
  int64 best_size = kint64max;
  int64 best_max = kint64max;
  int index = -1;
  for (int i = 0; i < size_; ++i) {
    IntVar* const var = vars_[i];
    if (!var->Bound()) {
      if (var->Size() < best_size ||
          (var->Size() == best_size && var->Max() < best_max)) {
        best_size = var->Size();
        best_max = var->Max();
        index = i;
        result = var;
      }
    }
  }
  if (index == -1) {
    *id = size_;
    return NULL;
  } else {
    *id = index;
    return result;
  }
}

// ----- Choose Min Size Highest Max -----

class MinSizeHighestMaxSelector : public VariableSelector {
 public:
  MinSizeHighestMaxSelector(const IntVar* const* vars, int size)
      : VariableSelector(vars, size) {}
  virtual ~MinSizeHighestMaxSelector() {}
  virtual IntVar* Select(Solver* const s, int64* id);
  virtual string DebugString() const { return "MinSizeHighestMaxSelector"; }
};

IntVar* MinSizeHighestMaxSelector::Select(Solver* const s, int64* id) {
  IntVar* result = NULL;
  int64 best_size = kint64max;
  int64 best_max = kint64min;
  int index = -1;
  for (int i = 0; i < size_; ++i) {
    IntVar* const var = vars_[i];
    if (!var->Bound()) {
      if (var->Size() < best_size ||
          (var->Size() == best_size && var->Max() > best_max)) {
        best_size = var->Size();
        best_max = var->Max();
        index = i;
        result = var;
      }
    }
  }
  if (index == -1) {
    *id = size_;
    return NULL;
  } else {
    *id = index;
    return result;
  }
}

// ----- Choose random unbound --

class RandomSelector : public VariableSelector {
 public:
  RandomSelector(const IntVar* const* vars, int size)
      : VariableSelector(vars, size) {}
  virtual ~RandomSelector() {}
  virtual IntVar* Select(Solver* const s, int64* id);
  virtual string DebugString() const { return "RandomSelector"; }
};

IntVar* RandomSelector::Select(Solver* const s, int64* id) {
  const int shift = s->Rand32(size_);
  for (int i = 0; i < size_; ++i) {
    const int index = (i + shift) % size_;
    IntVar* const var = vars_[index];
    if (!var->Bound()) {
      *id = index;
      return var;
    }
  }
  *id = size_;
  return NULL;
}

// ----- Choose min eval -----

class CheapestVarSelector : public VariableSelector {
 public:
  CheapestVarSelector(const IntVar* const* vars,
                      int size,
                      ResultCallback1<int64, int64>* var_eval)
      : VariableSelector(vars, size), var_evaluator_(var_eval) {}
  virtual ~CheapestVarSelector() {}
  virtual IntVar* Select(Solver* const s, int64* id);
  virtual string DebugString() const { return "CheapestVarSelector"; }
 private:
  scoped_ptr<ResultCallback1<int64, int64> > var_evaluator_;
};

IntVar* CheapestVarSelector::Select(Solver* const s, int64* id) {
  IntVar* result = NULL;
  int64 best_eval = kint64max;
  int index = -1;
  for (int i = 0; i < size_; ++i) {
    IntVar* const var = vars_[i];
    if (!var->Bound()) {
      const int64 eval = var_evaluator_->Run(i);
      if (eval < best_eval) {
        best_eval = eval;
        index = i;
        result = var;
      }
    }
  }
  if (index == -1) {
    *id = size_;
    return NULL;
  } else {
    *id = index;
    return result;
  }
}

// ----- Path selector -----
// Follow path, where var[i] is represents the next of i

class PathSelector : public VariableSelector {
 public:
  PathSelector(const IntVar* const* vars, int size)
       : VariableSelector(vars, size), first_(kint64max) {}
  virtual ~PathSelector() {}
  virtual IntVar* Select(Solver* const s, int64* id);
  virtual string DebugString() const { return "ChooseNextOnPath"; }
 private:
  bool UpdateIndex(int64* index) const;
  bool FindPathStart(int64* index) const;

  int64 first_;
};

IntVar* PathSelector::Select(Solver* const s, int64* id) {
  *id = first_;
  if (!UpdateIndex(id)) {
    return NULL;
  }
  int count = 0;
  while (vars_[*id]->Bound()) {
    *id = vars_[*id]->Value();
    if (!UpdateIndex(id)) {
      return NULL;
    }
    ++count;
    if (count >= size_ && !FindPathStart(id)) {  // Cycle detected
      return NULL;
    }
  }
  IntVar* const var = vars_[*id];
  s->SaveAndSetValue(&first_, *id);
  return var;
}

bool PathSelector::UpdateIndex(int64* index) const {
  if (*index >= size_) {
    if (!FindPathStart(index)) {
      return false;
    }
  }
  return true;
}

// Pick an unbound variable to which no other variable can point: it will be
// a good start for a path. If none is found pick the first unbound one.
bool PathSelector::FindPathStart(int64* index) const {
  // Pick path start
  for (int i = size_ - 1; i >= 0; --i) {
    if (!vars_[i]->Bound()) {
      bool has_possible_prev = false;
      for (int j = 0; j < size_; ++j) {
        if (vars_[j]->Contains(i)) {
          has_possible_prev = true;
          break;
        }
      }
      if (!has_possible_prev) {
        *index = i;
        return true;
      }
    }
  }
  // Pick first unbound
  for (int i = 0; i < size_; ++i) {
    if (!vars_[i]->Bound()) {
      *index = i;
      return true;
    }
  }
  return false;
}

// ----- Value selector -----

class ValueSelector : public BaseObject {
 public:
  ValueSelector() {}
  virtual ~ValueSelector() {}
  virtual int64 Select(const IntVar* const v, int64 id) = 0;
};

// ----- Select min -----

class MinValueSelector : public ValueSelector {
 public:
  MinValueSelector() {}
  virtual ~MinValueSelector() {}
  virtual int64 Select(const IntVar* const v, int64 id) { return v->Min(); }
  string DebugString() const { return "AssignMin"; }
};

// ----- Select max -----

class MaxValueSelector : public ValueSelector {
 public:
  MaxValueSelector() {}
  virtual ~MaxValueSelector() {}
  virtual int64 Select(const IntVar* const v, int64 id) { return v->Max(); }
  string DebugString() const { return "AssignMax"; }
};

// ----- Select random -----

class RandomValueSelector : public ValueSelector {
 public:
  RandomValueSelector() {}
  virtual ~RandomValueSelector() {}
  virtual int64 Select(const IntVar* const v, int64 id);
  string DebugString() const { return "AssignRandom"; }
};

int64 RandomValueSelector::Select(const IntVar* const v, int64 id) {
  const int64 span = v->Max() - v->Min() + 1;
  const int64 size = v->Size();
  Solver* const s = v->solver();
  if (size > span / 4) {  // Dense enough, we can try to find the
    // value randomly.
    for (;;) {
      const int64 value = v->Min() + s->Rand64(span);
      if (v->Contains(value)) {
        return value;
      }
    }
  } else {  // Not dense enough, we will count.
    int64 index = s->Rand64(size);
    if (index <= size / 2) {
      for (int64 i = v->Min(); i <= v->Max(); ++i) {
        if (v->Contains(i)) {
          if (--index == 0) {
            return i;
          }
        }
      }
      CHECK_LE(index, 0);
    } else {
      for (int64 i = v->Max(); i > v->Min(); --i) {
        if (v->Contains(i)) {
          if (--index == 0) {
            return i;
          }
        }
      }
      CHECK_LE(index, 0);
    }
  }
  return 0LL;
}

// ----- Select center -----

class CenterValueSelector : public ValueSelector {
 public:
  CenterValueSelector() {}
  virtual ~CenterValueSelector() {}
  virtual int64 Select(const IntVar* const v, int64 id);
  string DebugString() const { return "AssignCenter"; }
};

int64 CenterValueSelector::Select(const IntVar* const v, int64 id) {
  const int64 vmin = v->Min();
  const int64 vmax = v->Max();
  const int64 mid = (vmin + vmax) / 2;
  if (v->Contains(mid)) {
    return mid;
  }
  const int64 diameter = vmax - mid;  // always greater than mid - vmix.
  for (int64 i = 1; i <= diameter; ++i) {
    if (v->Contains(mid + i)) {
      return mid + i;
    }
    if (v->Contains(mid - i)) {
      return mid - i;
    }
  }
  return 0LL;
}

// ----- Best value -----

class CheapestValueSelector : public ValueSelector {
 public:
  CheapestValueSelector(ResultCallback2<int64, int64, int64>* eval,
                        ResultCallback1<int64, int64>* tie_breaker)
      : eval_(eval), tie_breaker_(tie_breaker) {}
  virtual ~CheapestValueSelector() {}
  virtual int64 Select(const IntVar* const v, int64 id);
  string DebugString() const { return "CheapestValue"; }
 private:
  scoped_ptr<ResultCallback2<int64, int64, int64> > eval_;
  scoped_ptr<ResultCallback1<int64, int64> > tie_breaker_;
  std::vector<int64> cache_;
};

int64 CheapestValueSelector::Select(const IntVar* const v, int64 id) {
  cache_.clear();
  int64 best = kint64max;
  scoped_ptr<IntVarIterator> it(v->MakeDomainIterator(false));
  for (it->Init(); it->Ok(); it->Next()) {
    const int i = it->Value();
    int64 eval = eval_->Run(id, i);
    if (eval < best) {
      best = eval;
      cache_.clear();
      cache_.push_back(i);
    } else if (eval == best) {
      cache_.push_back(i);
    }
  }
  DCHECK_GT(cache_.size(), 0);
  if (tie_breaker_.get() == NULL || cache_.size() == 1) {
    return cache_.back();
  } else {
    return cache_[tie_breaker_->Run(cache_.size())];
  }
}

// ----- VariableAssignmentSelector -----

class VariableAssignmentSelector : public BaseVariableAssignmentSelector {
 public:
  VariableAssignmentSelector(VariableSelector* const var_selector,
                             ValueSelector* const value_selector)
      : var_selector_(var_selector), value_selector_(value_selector) {}
  virtual ~VariableAssignmentSelector() {}
  virtual int64 SelectValue(const IntVar* const var, int64 id) {
    return value_selector_->Select(var, id);
  }
  virtual IntVar* SelectVariable(Solver* const s, int64* id) {
    return var_selector_->Select(s, id);
  }
  string DebugString() const;
 private:
  VariableSelector* const var_selector_;
  ValueSelector* const value_selector_;
};

string VariableAssignmentSelector::DebugString() const {
  return var_selector_->DebugString() + "_"
      + value_selector_->DebugString()
      + var_selector_->VarDebugString();
}

// ----- Base Global Evaluator-based selector -----

class BaseEvaluatorSelector : public BaseVariableAssignmentSelector {
 public:
  BaseEvaluatorSelector(const IntVar* const* vars, int size,
                        ResultCallback2<int64, int64, int64>* evaluator);
  ~BaseEvaluatorSelector() {}
 protected:
  struct Element {
    Element() : var(0), value(0) {}
    Element(int i, int64 j) : var(i), value(j) {}
    int var;
    int64 value;
  };

  string DebugStringInternal(const string& name) const;

  scoped_array<IntVar*> vars_;
  int size_;
  scoped_ptr<ResultCallback2<int64, int64, int64> > evaluator_;
};

BaseEvaluatorSelector::BaseEvaluatorSelector(
    const IntVar* const* vars,
    int size,
    ResultCallback2<int64, int64, int64>* evaluator)
    : vars_(NULL), size_(size), evaluator_(evaluator) {
  CHECK_GE(size_, 0);
  if (size_ > 0) {
    vars_.reset(new IntVar*[size_]);
    memcpy(vars_.get(), vars, size_ * sizeof(*vars));
  }
}


string BaseEvaluatorSelector::DebugStringInternal(const string& name) const {
  string out = name + "(";
  for (int i = 0; i < size_; ++i) {
    out += vars_[i]->DebugString() + " ";
  }
  out += ")";
  return out;
}

// ----- Global Dynamic Evaluator-based selector -----

class DynamicEvaluatorSelector : public BaseEvaluatorSelector {
 public:
  DynamicEvaluatorSelector(const IntVar* const* vars, int size,
                    ResultCallback2<int64, int64, int64>* evaluator,
                    ResultCallback1<int64, int64>* tie_breaker);
  virtual ~DynamicEvaluatorSelector() {}
  virtual int64 SelectValue(const IntVar* const var, int64 id);
  virtual IntVar* SelectVariable(Solver* const s, int64* id);
  virtual string DebugString() const;
 private:
  int first_;
  scoped_ptr<ResultCallback1<int64, int64> > tie_breaker_;
  std::vector<Element> cache_;
};

DynamicEvaluatorSelector::DynamicEvaluatorSelector(
    const IntVar* const* vars, int size,
    ResultCallback2<int64, int64, int64>* evaluator,
    ResultCallback1<int64, int64>* tie_breaker)
    : BaseEvaluatorSelector(vars, size, evaluator),
      first_(-1), tie_breaker_(tie_breaker) {}

int64 DynamicEvaluatorSelector::SelectValue(const IntVar* const var, int64 id) {
  return cache_[first_].value;
}

IntVar* DynamicEvaluatorSelector::SelectVariable(Solver* const s, int64* id) {
  int64 best_evaluation = kint64max;
  cache_.clear();
  for (int i = 0; i < size_; ++i) {
    const IntVar* const var = vars_[i];
    if (!var->Bound()) {
      scoped_ptr<IntVarIterator> it(var->MakeDomainIterator(false));
      for (it->Init(); it->Ok(); it->Next()) {
        const int j = it->Value();
        const int64 value = evaluator_->Run(i, j);
        if (value < best_evaluation) {
          best_evaluation = value;
          cache_.clear();
          cache_.push_back(Element(i, j));
        } else if (value == best_evaluation && tie_breaker_.get() != NULL) {
          cache_.push_back(Element(i, j));
        }
      }
    }
  }
  if (cache_.size() == 0) {
    *id = kint64max;
    return NULL;
  }
  if (tie_breaker_.get() == NULL || cache_.size() == 1) {
    *id = cache_[0].var;
    first_ = 0;
    return vars_[*id];
  } else {
    first_ = tie_breaker_->Run(cache_.size());
    *id = cache_[first_].var;
    return vars_[*id];
  }
}

string DynamicEvaluatorSelector::DebugString() const {
  return DebugStringInternal("AssignVariablesOnDynamicEvaluator");
}

// ----- Global Dynamic Evaluator-based selector -----

class StaticEvaluatorSelector : public BaseEvaluatorSelector {
 public:
  StaticEvaluatorSelector(const IntVar* const* vars, int size,
                    ResultCallback2<int64, int64, int64>* evaluator);
  virtual ~StaticEvaluatorSelector() {}
  virtual int64 SelectValue(const IntVar* const var, int64 id);
  virtual IntVar* SelectVariable(Solver* const s, int64* id);
  virtual string DebugString() const;
 private:
  class Compare {
   public:
    explicit Compare(ResultCallback2<int64, int64, int64>* evaluator)
        : evaluator_(evaluator) {}
    bool operator() (const Element& lhs, const Element& rhs) const {
      const int64 value_lhs = Value(lhs);
      const int64 value_rhs = Value(rhs);
      return value_lhs < value_rhs
          || (value_lhs == value_rhs && lhs.var < rhs.var);
    }
    int64 Value(const Element& element) const {
      return evaluator_->Run(element.var, element.value);
    }
   private:
    ResultCallback2<int64, int64, int64>* evaluator_;
  };

  Compare comp_;
  scoped_array<Element> elements_;
  int element_size_;
  int first_;
};

StaticEvaluatorSelector::StaticEvaluatorSelector(
    const IntVar* const* vars, int size,
    ResultCallback2<int64, int64, int64>* evaluator)
    : BaseEvaluatorSelector(vars, size, evaluator),
      comp_(evaluator), elements_(NULL), element_size_(0), first_(-1) {}

int64 StaticEvaluatorSelector::SelectValue(const IntVar* const var, int64 id) {
  return elements_[first_].value;
}

IntVar* StaticEvaluatorSelector::SelectVariable(Solver* const s, int64* id) {
  if (first_ == -1) {  // first call to select. update assignment costs
    // Two phases: compute size then filland sort
    element_size_ = 0;
    for (int i = 0; i < size_; ++i) {
      if (!vars_[i]->Bound()) {
        element_size_ += vars_[i]->Size();
      }
    }
    elements_.reset(new Element[element_size_]);
    int count = 0;
    for (int i = 0; i < size_; ++i) {
      const IntVar* const var = vars_[i];
      if (!var->Bound()) {
        scoped_ptr<IntVarIterator> it(var->MakeDomainIterator(false));
        for (it->Init(); it->Ok(); it->Next()) {
          const int j = it->Value();
          Element element(i, j);
          elements_[count] = element;
          ++count;
        }
      }
    }
    std::sort(elements_.get(), elements_.get() + element_size_, comp_);
    s->SaveAndSetValue(&first_, 0);
  }
  for (int i = first_; i < element_size_; ++i) {
    const Element& element = elements_[i];
    IntVar* const var = vars_[element.var];
    if (!var->Bound() && var->Contains(element.value)) {
      s->SaveAndSetValue(&first_, i);
      *id = element.var;
      return var;
    }
  }
  s->SaveAndSetValue(&first_, element_size_);
  *id = size_;
  return NULL;
}

string StaticEvaluatorSelector::DebugString() const {
  return DebugStringInternal("AssignVariablesOnStaticEvaluator");
}

// ----- AssignOneVariableValue decision -----

class AssignOneVariableValue : public Decision {
 public:
  AssignOneVariableValue(IntVar* const v, int64 val);
  virtual ~AssignOneVariableValue() {}
  virtual void Apply(Solver* const s);
  virtual void Refute(Solver* const s);
  virtual string DebugString() const;
  virtual void Accept(DecisionVisitor* const visitor) const {
    visitor->VisitSetVariableValue(var_, value_);
  }
 private:
  IntVar* const var_;
  int64 value_;
};

AssignOneVariableValue::AssignOneVariableValue(IntVar* const v, int64 val)
    : var_(v), value_(val) {
}

string AssignOneVariableValue::DebugString() const {
  return StringPrintf("[%s == %" GG_LL_FORMAT "d]",
                      var_->DebugString().c_str(), value_);
}

void AssignOneVariableValue::Apply(Solver* const s) {
  var_->SetValue(value_);
}

void AssignOneVariableValue::Refute(Solver* const s) {
  var_->RemoveValue(value_);
}

Decision* Solver::MakeAssignVariableValue(IntVar* const v, int64 val) {
  return RevAlloc(new AssignOneVariableValue(v, val));
}

// ----- AssignOneVariableValueOrFail decision -----

class AssignOneVariableValueOrFail : public Decision {
 public:
  AssignOneVariableValueOrFail(IntVar* const v, int64 value);
  virtual ~AssignOneVariableValueOrFail() {}
  virtual void Apply(Solver* const s);
  virtual void Refute(Solver* const s);
  virtual string DebugString() const;
  virtual void Accept(DecisionVisitor* const visitor) const {
    visitor->VisitSetVariableValue(var_, value_);
  }
 private:
  IntVar* const var_;
  const int64 value_;
};

AssignOneVariableValueOrFail::AssignOneVariableValueOrFail(IntVar* const v,
                                                           int64 value)
    : var_(v), value_(value) {
}

string AssignOneVariableValueOrFail::DebugString() const {
  return StringPrintf("[%s == %" GG_LL_FORMAT "d]",
                      var_->DebugString().c_str(), value_);
}

void AssignOneVariableValueOrFail::Apply(Solver* const s) {
  var_->SetValue(value_);
}

void AssignOneVariableValueOrFail::Refute(Solver* const s) {
  s->Fail();
}

Decision* Solver::MakeAssignVariableValueOrFail(IntVar* const v, int64 value) {
  return RevAlloc(new AssignOneVariableValueOrFail(v, value));
}

// ----- AssignOneVariableValue decision -----

class SplitOneVariable : public Decision {
 public:
  SplitOneVariable(IntVar* const v, int64 val, bool start_with_lower_half);
  virtual ~SplitOneVariable() {}
  virtual void Apply(Solver* const s);
  virtual void Refute(Solver* const s);
  virtual string DebugString() const;
  virtual void Accept(DecisionVisitor* const visitor) const {
    visitor->VisitSplitVariableDomain(var_, value_, start_with_lower_half_);
  }
 private:
  IntVar* const var_;
  const int64 value_;
  const bool start_with_lower_half_;
};

SplitOneVariable::SplitOneVariable(IntVar* const v,
                                   int64 val,
                                   bool start_with_lower_half)
    : var_(v), value_(val), start_with_lower_half_(start_with_lower_half) {
}

string SplitOneVariable::DebugString() const {
  if (start_with_lower_half_) {
    return StringPrintf("[%s <= %" GG_LL_FORMAT "d]",
                        var_->DebugString().c_str(), value_);
  } else {
    return StringPrintf("[%s >= %" GG_LL_FORMAT "d]",
                        var_->DebugString().c_str(), value_);
  }
}

void SplitOneVariable::Apply(Solver* const s) {
  if (start_with_lower_half_) {
    var_->SetMax(value_);
  } else {
    var_->SetMin(value_);
  }
}

void SplitOneVariable::Refute(Solver* const s) {
  if (start_with_lower_half_) {
    var_->SetMin(value_ + 1);
  } else {
    var_->SetMax(value_ - 1);
  }
}

Decision* Solver::MakeSplitVariableDomain(IntVar* const v,
                                          int64 val,
                                          bool start_with_lower_half) {
  return RevAlloc(new SplitOneVariable(v, val, start_with_lower_half));
}

// ----- AssignVariablesValues decision -----

class AssignVariablesValues : public Decision {
 public:
  AssignVariablesValues(const IntVar* const* vars, int size,
                        const int64* const values);
  virtual ~AssignVariablesValues() {}
  virtual void Apply(Solver* const s);
  virtual void Refute(Solver* const s);
  virtual string DebugString() const;
  virtual void Accept(DecisionVisitor* const visitor) const {
    for (int i = 0; i < size_; ++i) {
      visitor->VisitSetVariableValue(vars_[i], values_[i]);
    }
  }
 private:
  scoped_array<IntVar*> vars_;
  scoped_array<int64> values_;
  int size_;
};

AssignVariablesValues::AssignVariablesValues(const IntVar* const* vars,
                                             int size,
                                             const int64* const values)
  : vars_(NULL), values_(NULL), size_(size) {
  CHECK_GE(size_, 0);
  if (size_ > 0) {
    vars_.reset(new IntVar*[size_]);
    memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    values_.reset(new int64[size_]);
    memcpy(values_.get(), values, size_ * sizeof(*values));
  }
}

string AssignVariablesValues::DebugString() const {
  string out;
  for (int i = 0; i < size_; ++i) {
    StringAppendF(&out, "[%s == %" GG_LL_FORMAT "d]",
                  vars_[i]->DebugString().c_str(), values_[i]);
  }
  return out;
}

void AssignVariablesValues::Apply(Solver* const s) {
  for (int i = 0; i < size_; ++i) {
    vars_[i]->SetValue(values_[i]);
  }
}

void AssignVariablesValues::Refute(Solver* const s) {
  std::vector<IntVar*> terms;
  for (int i = 0; i < size_; ++i) {
    IntVar* term = s->MakeBoolVar();
    s->MakeIsDifferentCstCt(vars_[i], values_[i], term);
    terms.push_back(term);
  }
  s->AddConstraint(s->MakeSumGreaterOrEqual(terms, 1));
}

Decision* Solver::MakeAssignVariablesValues(const IntVar* const* vars, int size,
                                            const int64* const values) {
  return RevAlloc(new AssignVariablesValues(vars, size, values));
}

Decision* Solver::MakeAssignVariablesValues(const std::vector<IntVar*>& vars,
                                            const std::vector<int64>& values) {
  CHECK_EQ(vars.size(), values.size());
  return RevAlloc(new AssignVariablesValues(vars.data(), vars.size(),
                                            values.data()));
}

// ----- AssignAllVariables -----

class BaseAssignVariables : public DecisionBuilder {
 public:
  explicit BaseAssignVariables(BaseVariableAssignmentSelector* const selector)
      : selector_(selector) {}
  virtual ~BaseAssignVariables();
  virtual Decision* Next(Solver* const s);
  virtual string DebugString() const;
  static BaseAssignVariables* MakePhase(Solver* const s,
                                        const IntVar* const* vars,
                                        int size,
                                        VariableSelector* const var_selector,
                                        ValueSelector* const value_selector);
  static VariableSelector* MakeVariableSelector(Solver* const s,
                                                const IntVar* const* vars,
                                                int size,
                                                Solver::IntVarStrategy str) {
    VariableSelector* var_selector = NULL;
    switch (str) {
      case Solver::INT_VAR_DEFAULT:
      case Solver::INT_VAR_SIMPLE:
      case Solver::CHOOSE_FIRST_UNBOUND:
        var_selector = s->RevAlloc(new FirstUnboundSelector(vars, size));
        break;
      case  Solver::CHOOSE_RANDOM:
        var_selector = s->RevAlloc(new RandomSelector(vars, size));
        break;
      case  Solver::CHOOSE_MIN_SIZE_LOWEST_MIN:
        var_selector = s->RevAlloc(new MinSizeLowestMinSelector(vars, size));
        break;
      case  Solver::CHOOSE_MIN_SIZE_HIGHEST_MIN:
        var_selector = s->RevAlloc(new MinSizeHighestMinSelector(vars, size));
        break;
      case  Solver::CHOOSE_MIN_SIZE_LOWEST_MAX:
        var_selector = s->RevAlloc(new MinSizeLowestMaxSelector(vars, size));
        break;
      case  Solver::CHOOSE_MIN_SIZE_HIGHEST_MAX:
        var_selector = s->RevAlloc(new MinSizeHighestMaxSelector(vars, size));
        break;
      case  Solver::CHOOSE_PATH:
        var_selector = s->RevAlloc(new PathSelector(vars, size));
        break;
      default:
        LOG(FATAL) << "Unknown int var strategy " << str;
        break;
    }
    return var_selector;
  }

  static ValueSelector* MakeValueSelector(Solver* const s,
                                          Solver::IntValueStrategy val_str) {
    ValueSelector* value_selector = NULL;
    switch (val_str) {
      case Solver::INT_VALUE_DEFAULT:
      case Solver::INT_VALUE_SIMPLE:
      case  Solver::ASSIGN_MIN_VALUE:
        value_selector = s->RevAlloc(new MinValueSelector);
        break;
      case  Solver::ASSIGN_MAX_VALUE:
        value_selector = s->RevAlloc(new MaxValueSelector);
        break;
      case  Solver::ASSIGN_RANDOM_VALUE:
        value_selector = s->RevAlloc(new RandomValueSelector);
        break;
      case  Solver::ASSIGN_CENTER_VALUE:
        value_selector = s->RevAlloc(new CenterValueSelector);
        break;
      default:
        LOG(FATAL) << "Unknown int value strategy " << val_str;
        break;
    }
    return value_selector;
  }

 protected:
  BaseVariableAssignmentSelector* const selector_;
};

BaseAssignVariables::~BaseAssignVariables() {}

Decision* BaseAssignVariables::Next(Solver* const s) {
  int64 id = 0;
  IntVar* const var = selector_->SelectVariable(s, &id);
  if (NULL != var) {
    const int64 value = selector_->SelectValue(var, id);
    return s->RevAlloc(new AssignOneVariableValue(var, value));
  }
  return NULL;
}

string BaseAssignVariables::DebugString() const {
  return selector_->DebugString();
}

BaseAssignVariables*
BaseAssignVariables::MakePhase(Solver* const s,
                               const IntVar* const* vars,
                               int size,
                               VariableSelector* const var_selector,
                               ValueSelector* const value_selector) {
  BaseVariableAssignmentSelector* selector =
      s->RevAlloc(new VariableAssignmentSelector(var_selector, value_selector));
  return s->RevAlloc(new BaseAssignVariables(selector));
}

DecisionBuilder* Solver::MakePhase(IntVar* const v0,
                                   Solver::IntVarStrategy var_str,
                                   Solver::IntValueStrategy val_str) {
  scoped_array<IntVar*> vars(new IntVar*[1]);
  vars[0] = v0;
  return MakePhase(vars.get(), 1, var_str, val_str);
}

DecisionBuilder* Solver::MakePhase(IntVar* const v0,
                                   IntVar* const v1,
                                   Solver::IntVarStrategy var_str,
                                   Solver::IntValueStrategy val_str) {
  scoped_array<IntVar*> vars(new IntVar*[2]);
  vars[0] = v0;
  vars[1] = v1;
  return MakePhase(vars.get(), 2, var_str, val_str);
}

DecisionBuilder* Solver::MakePhase(IntVar* const v0,
                                   IntVar* const v1,
                                   IntVar* const v2,
                                   Solver::IntVarStrategy var_str,
                                   Solver::IntValueStrategy val_str) {
  scoped_array<IntVar*> vars(new IntVar*[3]);
  vars[0] = v0;
  vars[1] = v1;
  vars[2] = v2;
  return MakePhase(vars.get(), 3, var_str, val_str);
}

DecisionBuilder* Solver::MakePhase(IntVar* const v0,
                                   IntVar* const v1,
                                   IntVar* const v2,
                                   IntVar* const v3,
                                   Solver::IntVarStrategy var_str,
                                   Solver::IntValueStrategy val_str) {
  scoped_array<IntVar*> vars(new IntVar*[4]);
  vars[0] = v0;
  vars[1] = v1;
  vars[2] = v2;
  vars[3] = v3;
  return MakePhase(vars.get(), 4, var_str, val_str);
}

DecisionBuilder* Solver::MakePhase(const std::vector<IntVar*>& vars,
                                   Solver::IntVarStrategy var_str,
                                   Solver::IntValueStrategy val_str) {
  return MakePhase(vars.data(), vars.size(), var_str, val_str);
}

DecisionBuilder* Solver::MakePhase(const IntVar* const* vars,
                                   int size,
                                   Solver::IntVarStrategy var_str,
                                   Solver::IntValueStrategy val_str) {
  VariableSelector* const var_selector =
      BaseAssignVariables::MakeVariableSelector(this,
                                                vars,
                                                size,
                                                var_str);
  ValueSelector* const value_selector =
      BaseAssignVariables::MakeValueSelector(this, val_str);
  return BaseAssignVariables::MakePhase(this,
                                        vars,
                                        size,
                                        var_selector,
                                        value_selector);
}

DecisionBuilder* Solver::MakePhase(const std::vector<IntVar*>& vars,
                                   ResultCallback1<int64, int64>* var_evaluator,
                                   Solver::IntValueStrategy val_str) {
  return MakePhase(vars.data(), vars.size(), var_evaluator, val_str);
}

DecisionBuilder* Solver::MakePhase(const IntVar* const* vars,
                                   int size,
                                   ResultCallback1<int64, int64>* var_evaluator,
                                   Solver::IntValueStrategy val_str) {
  var_evaluator->CheckIsRepeatable();
  VariableSelector* const var_selector =
      RevAlloc(new CheapestVarSelector(vars, size, var_evaluator));
  ValueSelector* const value_selector =
      BaseAssignVariables::MakeValueSelector(this, val_str);
  return BaseAssignVariables::MakePhase(this,
                                        vars,
                                        size,
                                        var_selector,
                                        value_selector);
}

DecisionBuilder* Solver::MakePhase(
    const std::vector<IntVar*>& vars,
    Solver::IntVarStrategy var_str,
    ResultCallback2<int64, int64, int64>* value_evaluator) {
  return MakePhase(vars.data(), vars.size(), var_str, value_evaluator);
}

DecisionBuilder* Solver::MakePhase(
    const IntVar* const* vars,
    int size,
    Solver::IntVarStrategy var_str,
    ResultCallback2<int64, int64, int64>* value_evaluator) {
  VariableSelector* const var_selector =
      BaseAssignVariables::MakeVariableSelector(this,
                                                vars,
                                                size,
                                                var_str);
  value_evaluator->CheckIsRepeatable();
  ValueSelector* value_selector =
      RevAlloc(new CheapestValueSelector(value_evaluator, NULL));
  return BaseAssignVariables::MakePhase(this,
                                        vars,
                                        size,
                                        var_selector,
                                        value_selector);
}

DecisionBuilder* Solver::MakePhase(
    const std::vector<IntVar*>& vars,
    ResultCallback1<int64, int64>* var_evaluator,
    ResultCallback2<int64, int64, int64>* value_evaluator) {
  return MakePhase(vars.data(), vars.size(), var_evaluator, value_evaluator);
}

DecisionBuilder* Solver::MakePhase(
    const IntVar* const* vars,
    int size,
    ResultCallback1<int64, int64>* var_evaluator,
    ResultCallback2<int64, int64, int64>* value_evaluator) {
  var_evaluator->CheckIsRepeatable();
  VariableSelector* const var_selector =
      RevAlloc(new CheapestVarSelector(vars, size, var_evaluator));
  value_evaluator->CheckIsRepeatable();
  ValueSelector* value_selector =
      RevAlloc(new CheapestValueSelector(value_evaluator, NULL));
  return BaseAssignVariables::MakePhase(this,
                                        vars,
                                        size,
                                        var_selector,
                                        value_selector);
}

DecisionBuilder* Solver::MakePhase(
    const std::vector<IntVar*>& vars,
    Solver::IntVarStrategy var_str,
    ResultCallback2<int64, int64, int64>* value_evaluator,
                                   ResultCallback1<int64, int64>* tie_breaker) {
  return MakePhase(vars.data(), vars.size(), var_str, value_evaluator,
                   tie_breaker);
}

DecisionBuilder* Solver::MakePhase(
    const IntVar* const* vars,
    int size,
    Solver::IntVarStrategy var_str,
    ResultCallback2<int64, int64, int64>* value_evaluator,
    ResultCallback1<int64, int64>* tie_breaker) {
  VariableSelector* const var_selector =
      BaseAssignVariables::MakeVariableSelector(this,
                                                vars,
                                                size,
                                                var_str);
  value_evaluator->CheckIsRepeatable();
  ValueSelector* value_selector =
      RevAlloc(new CheapestValueSelector(value_evaluator, tie_breaker));
  return BaseAssignVariables::MakePhase(this,
                                        vars,
                                        size,
                                        var_selector,
                                        value_selector);
}

DecisionBuilder* Solver::MakePhase(
    const std::vector<IntVar*>& vars,
    ResultCallback1<int64, int64>* var_evaluator,
    ResultCallback2<int64, int64, int64>* value_evaluator,
    ResultCallback1<int64, int64>* tie_breaker) {
  return MakePhase(vars.data(), vars.size(), var_evaluator,
                   value_evaluator, tie_breaker);
}

DecisionBuilder* Solver::MakePhase(
    const IntVar* const* vars,
    int size,
    ResultCallback1<int64, int64>* var_evaluator,
    ResultCallback2<int64, int64, int64>* value_evaluator,
    ResultCallback1<int64, int64>* tie_breaker) {
  var_evaluator->CheckIsRepeatable();
  VariableSelector* const var_selector =
      RevAlloc(new CheapestVarSelector(vars, size, var_evaluator));
  value_evaluator->CheckIsRepeatable();
  ValueSelector* value_selector =
      RevAlloc(new CheapestValueSelector(value_evaluator, tie_breaker));
  return BaseAssignVariables::MakePhase(this,
                                        vars,
                                        size,
                                        var_selector,
                                        value_selector);
}

DecisionBuilder* Solver::MakePhase(const std::vector<IntVar*>& vars,
                                   ResultCallback2<int64, int64, int64>* eval,
                                   Solver::EvaluatorStrategy str) {
  return MakePhase(vars.data(), vars.size(), eval, str);
}

DecisionBuilder* Solver::MakePhase(const IntVar* const* vars,
                                   int size,
                                   ResultCallback2<int64, int64, int64>* eval,
                                   Solver::EvaluatorStrategy str) {
  return MakePhase(vars, size, eval, NULL, str);
}

DecisionBuilder* Solver::MakePhase(const std::vector<IntVar*>& vars,
                                   ResultCallback2<int64, int64, int64>* eval,
                                   ResultCallback1<int64, int64>* tie_breaker,
                                   Solver::EvaluatorStrategy str) {
  return MakePhase(vars.data(), vars.size(), eval, tie_breaker, str);
}

DecisionBuilder* Solver::MakePhase(const IntVar* const* vars,
                                   int size,
                                   ResultCallback2<int64, int64, int64>* eval,
                                   ResultCallback1<int64, int64>* tie_breaker,
                                   Solver::EvaluatorStrategy str) {
  eval->CheckIsRepeatable();
  if (tie_breaker) {
    tie_breaker->CheckIsRepeatable();
  }
  BaseVariableAssignmentSelector* selector = NULL;
  switch (str) {
    case Solver::CHOOSE_STATIC_GLOBAL_BEST: {
      // TODO(user): support tie breaker
      selector = RevAlloc(new StaticEvaluatorSelector(vars, size, eval));
      break;
    }
    case Solver::CHOOSE_DYNAMIC_GLOBAL_BEST: {
      selector = RevAlloc(new DynamicEvaluatorSelector(vars, size, eval,
                                                       tie_breaker));
      break;
    }
  }
  return RevAlloc(new BaseAssignVariables(selector));
}

// ----- AssignAllVariablesFromAssignment decision builder -----

class AssignVariablesFromAssignment : public DecisionBuilder {
 public:
  AssignVariablesFromAssignment(const Assignment* const assignment,
                                DecisionBuilder* const db,
                                const IntVar* const* vars,
                                int size) :
                                assignment_(assignment),
                                db_(db),
                                size_(size),
                                iter_(0) {
    CHECK_GE(size_, 0);
    if (size_ > 0) {
      vars_.reset(new IntVar*[size_]);
      memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    }
  }

  ~AssignVariablesFromAssignment() {}

  Decision* Next(Solver* const s) {
    if (iter_ < size_) {
      IntVar* const var = vars_[iter_++];
      return s->RevAlloc(new AssignOneVariableValue(var,
                                                    assignment_->Value(var)));
    } else {
      return db_->Next(s);
    }
  }

 private:
  const Assignment* const assignment_;
  DecisionBuilder* const db_;
  scoped_array<IntVar*> vars_;
  int size_;
  int iter_;
};


DecisionBuilder* Solver::MakeDecisionBuilderFromAssignment(
                                                  Assignment* const assignment,
                                                  DecisionBuilder* const db,
                                                  const IntVar* const* vars,
                                                  int size) {
  return RevAlloc(new AssignVariablesFromAssignment(assignment,
                                                    db,
                                                    vars,
                                                    size));
}

// ---------- Solution Collectors -----------

// ----- Base Class -----

SolutionCollector::SolutionCollector(Solver* const s, const Assignment* const a)
    : SearchMonitor(s), prototype_(a == NULL ? NULL : new Assignment(a)) {}

SolutionCollector::SolutionCollector(Solver* const s)
    : SearchMonitor(s), prototype_(new Assignment(s)) {}

SolutionCollector::~SolutionCollector() {
  STLDeleteElements(&solutions_);
  STLDeleteElements(&recycle_solutions_);
}

void SolutionCollector::Add(IntVar* const var) {
  if (prototype_.get() != NULL) {
    prototype_->Add(var);
  }
}

void SolutionCollector::Add(const std::vector<IntVar*>& vars)  {
  if (prototype_.get() != NULL) {
    prototype_->Add(vars);
  }
}

void SolutionCollector::Add(IntervalVar* const var)  {
  if (prototype_.get() != NULL) {
    prototype_->Add(var);
  }
}

void SolutionCollector::Add(const std::vector<IntervalVar*>& vars)  {
  if (prototype_.get() != NULL) {
    prototype_->Add(vars);
  }
}

void SolutionCollector::AddObjective(IntVar* const objective)  {
  if (prototype_.get() != NULL && objective != NULL) {
    prototype_->AddObjective(objective);
  }
}

void SolutionCollector::EnterSearch() {
  STLDeleteElements(&solutions_);
  STLDeleteElements(&recycle_solutions_);
  solutions_.clear();
  recycle_solutions_.clear();
  times_.clear();
  branches_.clear();
  failures_.clear();
  objective_values_.clear();
}

void SolutionCollector::PushSolution() {
  Assignment* new_sol = NULL;
  if (prototype_.get() != NULL) {
    if (!recycle_solutions_.empty()) {
      new_sol = recycle_solutions_.back();
      DCHECK(new_sol != NULL);
      recycle_solutions_.pop_back();
    } else {
      new_sol = new Assignment(prototype_.get());
    }
    new_sol->Store();
  }
  Solver* const s = solver();
  solutions_.push_back(new_sol);
  times_.push_back(s->wall_time());
  branches_.push_back(s->branches());
  failures_.push_back(s->failures());
  if (new_sol != NULL) {
    objective_values_.push_back(new_sol->ObjectiveValue());
  } else {
    objective_values_.push_back(0);
  }
}

void SolutionCollector::PopSolution() {
  if (!solutions_.empty()) {
    Assignment* popped = solutions_.back();
    solutions_.pop_back();
    if (popped != NULL) {
      recycle_solutions_.push_back(popped);
    }
    times_.pop_back();
    branches_.pop_back();
    failures_.pop_back();
    objective_values_.pop_back();
  }
}

void SolutionCollector::check_index(int n) const {
  CHECK_GE(n, 0) << "wrong index in solution getter";
  CHECK_LT(n, solutions_.size()) << "wrong index in solution getter";
}

Assignment* SolutionCollector::solution(int n) const {
  check_index(n);
  return solutions_[n];
}

int SolutionCollector::solution_count() const {
  return solutions_.size();
}

int64 SolutionCollector::wall_time(int n) const {
  check_index(n);
  return times_[n];
}

int64 SolutionCollector::branches(int n) const {
  check_index(n);
  return branches_[n];
}

int64 SolutionCollector::failures(int n) const {
  check_index(n);
  return failures_[n];
}

int64 SolutionCollector::objective_value(int n) const {
  check_index(n);
  return objective_values_[n];
}

int64 SolutionCollector::Value(int n, IntVar* const var) const {
  check_index(n);
  return solutions_[n]->Value(var);
}

int64 SolutionCollector::StartValue(int n, IntervalVar* const var) const {
  check_index(n);
  return solutions_[n]->StartValue(var);
}

int64 SolutionCollector::DurationValue(int n, IntervalVar* const var) const {
  check_index(n);
  return solutions_[n]->DurationValue(var);
}

int64 SolutionCollector::EndValue(int n, IntervalVar* const var) const {
  check_index(n);
  return solutions_[n]->EndValue(var);
}

int64 SolutionCollector::PerformedValue(int n, IntervalVar* const var) const {
  check_index(n);
  return solutions_[n]->PerformedValue(var);
}

// ----- First Solution Collector -----

// Collect first solution, useful when looking satisfaction problems
class FirstSolutionCollector : public SolutionCollector {
 public:
  FirstSolutionCollector(Solver* const s, const Assignment* const a);
  explicit FirstSolutionCollector(Solver* const s);
  virtual ~FirstSolutionCollector();
  virtual void EnterSearch();
  virtual bool AtSolution();
  virtual string DebugString() const;
 private:
  bool done_;
};

FirstSolutionCollector::FirstSolutionCollector(Solver* const s,
                                               const Assignment* const a)
    : SolutionCollector(s, a), done_(false) {}

FirstSolutionCollector::FirstSolutionCollector(Solver* const s)
    : SolutionCollector(s), done_(false) {}

FirstSolutionCollector::~FirstSolutionCollector() {}

void FirstSolutionCollector::EnterSearch() {
  SolutionCollector::EnterSearch();
  done_ = false;
}

bool FirstSolutionCollector::AtSolution() {
  if (!done_) {
    PushSolution();
    done_ = true;
  }
  return false;
}

string FirstSolutionCollector::DebugString() const {
  if (prototype_.get() == NULL) {
    return "FirstSolutionCollector()";
  } else {
    return "FirstSolutionCollector(" + prototype_->DebugString() + ")";
  }
}

SolutionCollector* Solver::MakeFirstSolutionCollector(
    const Assignment* const a) {
  return RevAlloc(new FirstSolutionCollector(this, a));
}

SolutionCollector* Solver::MakeFirstSolutionCollector() {
  return RevAlloc(new FirstSolutionCollector(this));
}

// ----- Last Solution Collector -----

// Collect last solution, useful when optimizing
class LastSolutionCollector : public SolutionCollector {
 public:
  LastSolutionCollector(Solver* const s, const Assignment* const a);
  explicit LastSolutionCollector(Solver* const s);
  virtual ~LastSolutionCollector();
  virtual bool AtSolution();
  virtual string DebugString() const;
};

LastSolutionCollector::LastSolutionCollector(Solver* const s,
                                             const Assignment* const a)
    : SolutionCollector(s, a) {}

LastSolutionCollector::LastSolutionCollector(Solver* const s)
    : SolutionCollector(s) {}


LastSolutionCollector::~LastSolutionCollector() {}

bool LastSolutionCollector::AtSolution() {
  PopSolution();
  PushSolution();
  return true;
}

string LastSolutionCollector::DebugString() const {
  if (prototype_.get() == NULL) {
    return "LastSolutionCollector()";
  } else {
    return "LastSolutionCollector(" + prototype_->DebugString() + ")";
  }
}

SolutionCollector* Solver::MakeLastSolutionCollector(
    const Assignment* const a) {
  return RevAlloc(new LastSolutionCollector(this, a));
}

SolutionCollector* Solver::MakeLastSolutionCollector() {
  return RevAlloc(new LastSolutionCollector(this));
}

// ----- Best Solution Collector -----

class BestValueSolutionCollector : public SolutionCollector {
 public:
  BestValueSolutionCollector(Solver* const s,
                             const Assignment* const a,
                             bool maximize);
  BestValueSolutionCollector(Solver* const s, bool maximize);
  virtual ~BestValueSolutionCollector() {}
  virtual void EnterSearch();
  virtual bool AtSolution();
  virtual string DebugString() const;
 public:
  const bool maximize_;
  int64 best_;
};

BestValueSolutionCollector::BestValueSolutionCollector(
    Solver* const s,
    const Assignment* const a,
    bool maximize)
    : SolutionCollector(s, a),
      maximize_(maximize),
      best_(maximize ? kint64min : kint64max) {}

BestValueSolutionCollector::BestValueSolutionCollector(Solver* const s,
                                                       bool maximize)
    : SolutionCollector(s),
      maximize_(maximize),
      best_(maximize ? kint64min : kint64max) {}

void BestValueSolutionCollector::EnterSearch() {
  SolutionCollector::EnterSearch();
  best_ = maximize_ ? kint64min : kint64max;
}

bool BestValueSolutionCollector::AtSolution() {
  if (prototype_.get() != NULL) {
    const IntVar* objective = prototype_->Objective();
    if (objective != NULL) {
      if (maximize_ && objective->Max() > best_) {
        PopSolution();
        PushSolution();
        best_ = objective->Max();
      } else if (!maximize_ && objective->Min() < best_) {
        PopSolution();
        PushSolution();
        best_ = objective->Min();
      }
    }
  }
  return true;
}

string BestValueSolutionCollector::DebugString() const {
  if (prototype_.get() == NULL) {
    return "BestValueSolutionCollector()";
  } else {
    return "BestValueSolutionCollector(" + prototype_->DebugString() + ")";
  }
}

SolutionCollector* Solver::MakeBestValueSolutionCollector(
    const Assignment* const a, bool maximize) {
  return RevAlloc(new BestValueSolutionCollector(this, a, maximize));
}

SolutionCollector* Solver::MakeBestValueSolutionCollector(bool maximize) {
  return RevAlloc(new BestValueSolutionCollector(this, maximize));
}

// ----- All Solution Collector -----

// collect all solutions
class AllSolutionCollector : public SolutionCollector {
 public:
  AllSolutionCollector(Solver* const s, const Assignment* const a);
  explicit AllSolutionCollector(Solver* const s);
  virtual ~AllSolutionCollector();
  virtual bool AtSolution();
  virtual string DebugString() const;
};

AllSolutionCollector::AllSolutionCollector(Solver* const s,
                                           const Assignment* const a)
    : SolutionCollector(s, a) {}

AllSolutionCollector::AllSolutionCollector(Solver* const s)
    : SolutionCollector(s) {}

AllSolutionCollector::~AllSolutionCollector() {}

bool AllSolutionCollector::AtSolution()  {
  PushSolution();
  return true;
}

string AllSolutionCollector::DebugString() const {
  if (prototype_.get() == NULL) {
    return "AllSolutionCollector()";
  } else {
    return "AllSolutionCollector(" + prototype_->DebugString() + ")";
  }
}

SolutionCollector* Solver::MakeAllSolutionCollector(const Assignment* const a) {
  return RevAlloc(new AllSolutionCollector(this, a));
}

SolutionCollector* Solver::MakeAllSolutionCollector() {
  return RevAlloc(new AllSolutionCollector(this));
}

// ---------- Objective Management ----------

OptimizeVar::OptimizeVar(Solver* const s,
                         bool maximize,
                         IntVar* const a,
                         int64 step)
    : SearchMonitor(s), var_(a), step_(step), best_(kint64max),
      maximize_(maximize) {
  CHECK_GT(step, 0);
}

OptimizeVar::~OptimizeVar() {}

void OptimizeVar::EnterSearch() {
  if (maximize_) {
    best_ = kint64min;
  } else {
    best_ = kint64max;
  }
}

void OptimizeVar::RestartSearch() {
  ApplyBound();
}

void OptimizeVar::ApplyBound() {
  if (maximize_) {
    var_->SetMin(best_ + step_);
  } else {
    var_->SetMax(best_ - step_);
  }
}

void OptimizeVar::RefuteDecision(Decision* const d) {
  ApplyBound();
}

bool OptimizeVar::AcceptSolution() {
  const int64 val = var_->Value();
  // This code should never returns false in sequential mode because
  // ApplyBound should have been called before. In parallel, this is
  // no longer true. That is why we keep it there, just in case.
  return (maximize_ && val > best_) || (!maximize_ && val < best_);
}

bool OptimizeVar::AtSolution() {
  int64 val = var_->Value();
  if (maximize_) {
    CHECK_GT(val, best_);
    best_ = val;
  } else {
    CHECK_LT(val, best_);
    best_ = val;
  }
  return true;
}

string OptimizeVar::Print() const {
  return StringPrintf("objective value = %" GG_LL_FORMAT "d, ", var_->Value());
}

string OptimizeVar::DebugString() const {
  string out;
  if (maximize_) {
    out = "MaximizeVar(";
  } else {
    out = "MinimizeVar(";
  }
  StringAppendF(&out, "%s, step = %" GG_LL_FORMAT
                "d, best = %" GG_LL_FORMAT "d)",
                var_->DebugString().c_str(), step_, best_);
  return out;
}


OptimizeVar* Solver::MakeMinimize(IntVar* const v, int64 step) {
  return RevAlloc(new OptimizeVar(this, false, v, step));
}

OptimizeVar* Solver::MakeMaximize(IntVar* const v, int64 step) {
  return RevAlloc(new OptimizeVar(this, true, v, step));
}

OptimizeVar* Solver::MakeOptimize(bool maximize, IntVar* const v, int64 step) {
  return RevAlloc(new OptimizeVar(this, maximize, v, step));
}

class WeightedOptimizeVar: public OptimizeVar {
 public:
  WeightedOptimizeVar(Solver* solver,
                      bool maximize,
                      const std::vector<IntVar*>& sub_objectives,
                      const std::vector<int64>& weights,
                      int64 step)
      : OptimizeVar(solver,
                    maximize,
                    solver->MakeScalProd(sub_objectives, weights)->Var(),
                    step),
        size_(weights.size()) {
    CHECK_EQ(sub_objectives.size(), weights.size());
    sub_objectives_.reset(new IntVar*[size_]);
    memcpy(sub_objectives_.get(),
           sub_objectives.data(), size_ * sizeof(*sub_objectives.data()));
    weights_.reset(new int64[size_]);
    memcpy(weights_.get(),
           weights.data(), size_ * sizeof(*weights.data()));
  }

  WeightedOptimizeVar(Solver* solver,
                      bool maximize,
                      const std::vector<IntVar*>& sub_objectives,
                      const std::vector<int>& weights,
                      int64 step)
      : OptimizeVar(solver,
                    maximize,
                    solver->MakeScalProd(sub_objectives, weights)->Var(),
                    step),
        size_(weights.size()) {
    CHECK_EQ(sub_objectives.size(), weights.size());
    sub_objectives_.reset(new IntVar*[size_]);
    memcpy(sub_objectives_.get(),
           sub_objectives.data(), size_ * sizeof(*sub_objectives.data()));
    weights_.reset(new int64[size_]);
    for (int i = 0; i < size_; ++i) {
      weights_[i] = weights[i];
    }
  }

  virtual ~WeightedOptimizeVar() {}
  virtual string Print() const;

 private:
  const int64 size_;
  scoped_array<IntVar*> sub_objectives_;
  scoped_array<int64> weights_;

  DISALLOW_COPY_AND_ASSIGN(WeightedOptimizeVar);
};

string WeightedOptimizeVar::Print() const {
  string result(OptimizeVar::Print());
  StringAppendF(&result, "\nWeighted Objective:\n");
  for (int i = 0; i < size_; ++i) {
    StringAppendF(&result, "Variable %s,\tvalue %lld,\tweight %lld\n",
                  sub_objectives_[i]->name().c_str(),
                  sub_objectives_[i]->Value(),
                  weights_[i]);
  }
  return result;
}

OptimizeVar* Solver::MakeWeightedOptimize(bool maximize,
                                          const std::vector<IntVar*>& sub_objectives,
                                          const std::vector<int64>& weights,
                                          int64 step) {
  return RevAlloc(new WeightedOptimizeVar(this,
                                          maximize,
                                          sub_objectives, weights,
                                          step));
}

OptimizeVar* Solver::MakeWeightedMinimize(const std::vector<IntVar*>& sub_objectives,
                                          const std::vector<int64>& weights,
                                          int64 step) {
  return RevAlloc(new WeightedOptimizeVar(this,
                                          false,
                                          sub_objectives, weights,
                                          step));
}

OptimizeVar* Solver::MakeWeightedMaximize(const std::vector<IntVar*>& sub_objectives,
                                          const std::vector<int64>& weights,
                                          int64 step) {
  return RevAlloc(new WeightedOptimizeVar(this,
                                          true,
                                          sub_objectives, weights,
                                          step));
}

OptimizeVar* Solver::MakeWeightedOptimize(bool maximize,
                                          const std::vector<IntVar*>& sub_objectives,
                                          const std::vector<int>& weights,
                                          int64 step) {
  return RevAlloc(new WeightedOptimizeVar(this,
                                          maximize,
                                          sub_objectives, weights,
                                          step));
}

OptimizeVar* Solver::MakeWeightedMinimize(const std::vector<IntVar*>& sub_objectives,
                                          const std::vector<int>& weights,
                                          int64 step) {
  return RevAlloc(new WeightedOptimizeVar(this,
                                          false,
                                          sub_objectives, weights,
                                          step));
}

OptimizeVar* Solver::MakeWeightedMaximize(const std::vector<IntVar*>& sub_objectives,
                                          const std::vector<int>& weights,
                                          int64 step) {
  return RevAlloc(new WeightedOptimizeVar(this,
                                          true,
                                          sub_objectives, weights,
                                          step));
}


// ---------- Metaheuristics ---------

class Metaheuristic : public SearchMonitor {
 public:
  Metaheuristic(Solver* const solver,
                bool maximize,
                IntVar* objective,
                int64 step);
  virtual ~Metaheuristic() {}
  virtual void RefuteDecision(Decision* const d);
 protected:
  IntVar* const objective_;
  int64 step_;
  int64 current_;
  int64 best_;
  bool maximize_;
};

Metaheuristic::Metaheuristic(Solver* const solver,
                             bool maximize,
                             IntVar* objective,
                             int64 step)
    : SearchMonitor(solver),
      objective_(objective),
      step_(step),
      current_(kint64max),
      best_(kint64max),
      maximize_(maximize) {}

void Metaheuristic::RefuteDecision(Decision* d) {
  if (maximize_) {
    if (objective_->Max() < best_ + step_) {
      solver()->Fail();
    }
  } else if (objective_->Min() > best_ - step_) {
    solver()->Fail();
  }
}

// ---------- Tabu Search ----------

class TabuSearch : public Metaheuristic {
 public:
  TabuSearch(Solver* const s,
             bool maximize,
             IntVar* objective,
             int64 step,
             const IntVar* const* vars,
             int size,
             int64 keep_tenure,
             int64 forbid_tenure,
             double tabu_factor);
  virtual ~TabuSearch() {}
  virtual void EnterSearch();
  virtual void ApplyDecision(Decision* d);
  virtual bool AtSolution();
  virtual bool LocalOptimum();
  virtual void AcceptNeighbor();
  virtual string DebugString() const {
    return "Tabu Search";
  }
 private:
  struct VarValue {
    VarValue(IntVar* const var, int64 value, int64 stamp)
        : var_(var), value_(value), stamp_(stamp) {}
    IntVar* const var_;
    const int64 value_;
    const int64 stamp_;
  };
  typedef std::list<VarValue> TabuList;

  void AgeList(int64 tenure, TabuList* list);
  void AgeLists();

  scoped_array<IntVar*> vars_;
  const int64 size_;
  Assignment assignment_;
  int64 last_;
  TabuList keep_tabu_list_;
  int64 keep_tenure_;
  TabuList forbid_tabu_list_;
  int64 forbid_tenure_;
  double tabu_factor_;
  int64 stamp_;
  DISALLOW_COPY_AND_ASSIGN(TabuSearch);
};

TabuSearch::TabuSearch(Solver* const s,
                       bool maximize,
                       IntVar* objective,
                       int64 step,
                       const IntVar* const* vars,
                       int size,
                       int64 keep_tenure,
                       int64 forbid_tenure,
                       double tabu_factor)
    : Metaheuristic(s, maximize, objective, step),
      size_(size),
      assignment_(s),
      last_(kint64max),
      keep_tenure_(keep_tenure),
      forbid_tenure_(forbid_tenure),
      tabu_factor_(tabu_factor),
      stamp_(0) {
  CHECK_GE(size_, 0);
  if (size_ > 0) {
    vars_.reset(new IntVar*[size_]);
    memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    assignment_.Add(vars_.get(), size);
  }
}

void TabuSearch::EnterSearch() {
  if (maximize_) {
    best_ = objective_->Min();
  } else {
    best_ = objective_->Max();
  }
  current_ = best_;
}

void TabuSearch::ApplyDecision(Decision* const d) {
  Solver* const s = solver();
  if (d == s->balancing_decision()) {
    return;
  }
  // Aspiration criterion
  // Accept a neighbor if it improves the best solution found so far
  IntVar* aspiration = s->MakeBoolVar();
  if (maximize_) {
    s->AddConstraint(s->MakeIsGreaterOrEqualCstCt(objective_,
                                                  best_ + step_,
                                                  aspiration));
  } else {
    s->AddConstraint(s->MakeIsLessOrEqualCstCt(objective_,
                                               best_ - step_,
                                               aspiration));
  }

  // Tabu criterion
  // A variable in the "keep" list must keep its value, a variable in the
  // "forbid" list must not take its value in the list. The tabu criterion is
  // softened by the tabu factor which gives the number of violations to
  // the tabu criterion which is tolerated; a factor of 1 means no violations
  // allowed, a factor of 0 means all violations allowed.
  std::vector<IntVar*> tabu_vars;
  for (ConstIter<TabuList > it(keep_tabu_list_); !it.at_end(); ++it) {
    IntVar* tabu_var = s->MakeBoolVar();
    Constraint* keep_cst =
        s->MakeIsEqualCstCt((*it).var_, (*it).value_, tabu_var);
    s->AddConstraint(keep_cst);
    tabu_vars.push_back(tabu_var);
  }
  for (ConstIter<TabuList > it(forbid_tabu_list_); !it.at_end(); ++it) {
    IntVar* tabu_var = s->MakeBoolVar();
    Constraint* forbid_cst =
        s->MakeIsDifferentCstCt((*it).var_, (*it).value_, tabu_var);
    s->AddConstraint(forbid_cst);
    tabu_vars.push_back(tabu_var);
  }
  if (tabu_vars.size() > 0) {
    IntVar* tabu = s->MakeBoolVar();
    s->AddConstraint(s->MakeIsGreaterOrEqualCstCt(s->MakeSum(tabu_vars)->Var(),
                                                  tabu_vars.size() *
                                                  tabu_factor_,
                                                  tabu));
    s->AddConstraint(s->MakeGreaterOrEqual(s->MakeSum(aspiration, tabu), 1LL));
  }

  // Go downhill to the next local optimum
  if (maximize_) {
    s->AddConstraint(s->MakeGreaterOrEqual(objective_, current_ + step_));
  } else {
    s->AddConstraint(s->MakeLessOrEqual(objective_, current_ - step_));
  }

  // Avoid cost plateau's which lead to tabu cycles
  s->AddConstraint(s->MakeNonEquality(objective_, last_));
}

bool TabuSearch::AtSolution() {
  int64 val = objective_->Value();
  if (maximize_) {
    best_ = std::max(val, best_);
  } else {
    best_ = std::min(val, best_);
  }
  current_ = val;
  last_ = val;

  // New solution found: add new assignments to tabu lists; this is only
  // done after the first local optimum (stamp_ != 0)
  if (0 != stamp_) {
    for (int i = 0; i < size_; ++i) {
      IntVar* const var = vars_[i];
      const int64 old_value = assignment_.Value(var);
      const int64 new_value = var->Value();
      if (old_value != new_value) {
        VarValue keep_value(var, new_value, stamp_);
        keep_tabu_list_.push_front(keep_value);
        VarValue forbid_value(var, old_value, stamp_);
        forbid_tabu_list_.push_front(forbid_value);
      }
    }
  }
  assignment_.Store();

  return true;
}

bool TabuSearch::LocalOptimum() {
  AgeLists();
  if (maximize_) {
    current_ = kint64min;
  } else {
    current_ = kint64max;
  }
  return true;
}

void TabuSearch::AcceptNeighbor() {
  if (0 != stamp_) {
    AgeLists();
  }
}

void TabuSearch::AgeList(int64 tenure, TabuList* list) {
  while (!list->empty() && list->back().stamp_ < stamp_ - tenure) {
    list->pop_back();
  }
}

void TabuSearch::AgeLists() {
  AgeList(keep_tenure_, &keep_tabu_list_);
  AgeList(forbid_tenure_, &forbid_tabu_list_);
  ++stamp_;
}

SearchMonitor* Solver::MakeTabuSearch(bool maximize,
                                      IntVar* const v,
                                      int64 step,
                                      const std::vector<IntVar*>& vars,
                                      int64 keep_tenure,
                                      int64 forbid_tenure,
                                      double tabu_factor) {
  return RevAlloc(new TabuSearch(this,
                                 maximize,
                                 v,
                                 step,
                                 vars.data(),
                                 vars.size(),
                                 keep_tenure,
                                 forbid_tenure,
                                 tabu_factor));
}

SearchMonitor* Solver::MakeTabuSearch(bool maximize,
                                      IntVar* const v,
                                      int64 step,
                                      const IntVar* const* vars,
                                      int size,
                                      int64 keep_tenure,
                                      int64 forbid_tenure,
                                      double tabu_factor) {
  return RevAlloc(new TabuSearch(this,
                                 maximize,
                                 v,
                                 step,
                                 vars,
                                 size,
                                 keep_tenure,
                                 forbid_tenure,
                                 tabu_factor));
}

// ---------- Simulated Annealing ----------

class SimulatedAnnealing : public Metaheuristic {
 public:
  SimulatedAnnealing(Solver* const s,
                     bool maximize,
                     IntVar* objective,
                     int64 step,
                     int64 initial_temperature);
  virtual ~SimulatedAnnealing() {}
  virtual void EnterSearch();
  virtual void ApplyDecision(Decision* d);
  virtual bool AtSolution();
  virtual bool LocalOptimum();
  virtual void AcceptNeighbor();
  virtual string DebugString() const {
    return "Simulated Annealing";
  }
 private:
  float Temperature() const;

  const int64 temperature0_;
  int64 iteration_;
  ACMRandom rand_;
};

SimulatedAnnealing::SimulatedAnnealing(Solver* const s,
                                       bool maximize,
                                       IntVar* objective,
                                       int64 step,
                                       int64 initial_temperature)
    : Metaheuristic(s, maximize, objective, step),
      temperature0_(initial_temperature),
      iteration_(0),
      rand_(654) {}

void SimulatedAnnealing::EnterSearch() {
  if (maximize_) {
    best_ = objective_->Min();
  } else {
    best_ = objective_->Max();
  }
  current_ = best_;
}

void SimulatedAnnealing::ApplyDecision(Decision* const d) {
  Solver* const s = solver();
  if (d == s->balancing_decision()) {
    return;
  }
  #if defined(_MSC_VER)
  const int64  energy_bound = Temperature() * log(rand_.RndFloat()) / log(2.0L);
#else
  const int64  energy_bound = Temperature() * log2(rand_.RndFloat());
#endif
  if (maximize_) {
    const int64 bound =
        (current_ > kint64min) ? current_ + step_ + energy_bound : current_;
    s->AddConstraint(s->MakeGreaterOrEqual(objective_, bound));
  } else {
    const int64 bound =
        (current_ < kint64max) ? current_ - step_ - energy_bound : current_;
    s->AddConstraint(s->MakeLessOrEqual(objective_, bound));
  }
}

bool SimulatedAnnealing::AtSolution() {
  const int64 val = objective_->Value();
  if (maximize_) {
    best_ = std::max(val, best_);
  } else {
    best_ = std::min(val, best_);
  }
  current_ = val;
  return true;
}

bool SimulatedAnnealing::LocalOptimum() {
  if (maximize_) {
    current_ = kint64min;
  } else {
    current_ = kint64max;
  }
  ++iteration_;
  return Temperature() > 0;
}

void SimulatedAnnealing::AcceptNeighbor() {
  if (iteration_ > 0) {
    ++iteration_;
  }
}

float SimulatedAnnealing::Temperature() const {
  if (iteration_ > 0) {
    return (1.0 * temperature0_) / iteration_;  // Cauchy annealing
  } else {
    return 0.;
  }
}

SearchMonitor* Solver::MakeSimulatedAnnealing(bool maximize,
                                              IntVar* const v,
                                              int64 step,
                                              int64 initial_temperature) {
  return RevAlloc(new SimulatedAnnealing(this,
                                         maximize,
                                         v,
                                         step,
                                         initial_temperature));
}

// ---------- Guided Local Search ----------

typedef pair<int64, int64> Arc;

// Base GLS penalties abstract class. Maintains the penalty frequency for each
// (variable, value) arc.
class GuidedLocalSearchPenalties {
 public:
  virtual ~GuidedLocalSearchPenalties() {}
  virtual bool HasValues() const = 0;
  virtual void Increment(const Arc& arc) = 0;
  virtual int64 Value(const Arc& arc) const = 0;
};

// Dense GLS penalties implementation using a matrix to store penalties.
class GuidedLocalSearchPenaltiesTable : public GuidedLocalSearchPenalties {
 public:
  explicit GuidedLocalSearchPenaltiesTable(int size);
  virtual ~GuidedLocalSearchPenaltiesTable() {}
  virtual bool HasValues() const { return has_values_; }
  virtual void Increment(const Arc& arc);
  virtual int64 Value(const Arc& arc) const;
 private:
  std::vector<std::vector<int64> > penalties_;
  bool has_values_;
};

GuidedLocalSearchPenaltiesTable::GuidedLocalSearchPenaltiesTable(int size)
  : penalties_(size), has_values_(false) {
}

void GuidedLocalSearchPenaltiesTable::Increment(const Arc& arc) {
  std::vector<int64>& first_penalties = penalties_[arc.first];
  const int64 second = arc.second;
  if (second >= first_penalties.size()) {
    first_penalties.resize(second + 1, 0LL);
  }
  ++first_penalties[second];
  has_values_ = true;
}

int64 GuidedLocalSearchPenaltiesTable::Value(const Arc& arc) const {
  const std::vector<int64>& first_penalties = penalties_[arc.first];
  const int64 second = arc.second;
  if (second >= first_penalties.size()) {
    return 0LL;
  } else {
    return first_penalties[second];
  }
}

// Sparse GLS penalties implementation using a hash_map to store penalties.

class GuidedLocalSearchPenaltiesMap : public GuidedLocalSearchPenalties {
 public:
  explicit GuidedLocalSearchPenaltiesMap(int size);
  virtual ~GuidedLocalSearchPenaltiesMap() {}
  virtual bool HasValues() const { return (penalties_.size() != 0); }
  virtual void Increment(const Arc& arc);
  virtual int64 Value(const Arc& arc) const;
 private:
  Bitmap penalized_;
#if defined(_MSC_VER)
  hash_map<Arc, int64, PairInt64Hasher> penalties_;
#else
  hash_map<Arc, int64> penalties_;
#endif
};

GuidedLocalSearchPenaltiesMap::GuidedLocalSearchPenaltiesMap(int size)
  : penalized_(size, false) {}

void GuidedLocalSearchPenaltiesMap::Increment(const Arc& arc) {
  ++penalties_[arc];
  penalized_.Set(arc.first, true);
}

int64 GuidedLocalSearchPenaltiesMap::Value(const Arc& arc) const {
  if (penalized_.Get(arc.first)) {
    return FindWithDefault(penalties_, arc, 0LL);
  }
  return 0LL;
}

class GuidedLocalSearch : public Metaheuristic {
 public:
  GuidedLocalSearch(Solver* const s,
                    IntVar* objective,
                    bool maximize,
                    int64 step,
                    const IntVar* const* vars,
                    int size,
                    double penalty_factor);
  virtual ~GuidedLocalSearch() {}
  virtual void EnterSearch();
  virtual bool AcceptDelta(Assignment* delta, Assignment* deltadelta);
  virtual void ApplyDecision(Decision* d);
  virtual bool AtSolution();
  virtual bool LocalOptimum();
  virtual int64 AssignmentElementPenalty(const Assignment& assignment,
                                         int index) = 0;
  virtual int64 AssignmentPenalty(const Assignment& assignment,
                                  int index,
                                  int64 next) = 0;
  virtual bool EvaluateElementValue(const Assignment::IntContainer& container,
                                    int64 index,
                                    int* container_index,
                                    int64* penalty) = 0;
  virtual IntExpr* MakeElementPenalty(int index) = 0;
  virtual string DebugString() const {
    return "Guided Local Search";
  }
 protected:
  struct Comparator {
    bool operator() (const pair<Arc, double>& i,
                     const pair<Arc, double>& j) {
      return i.second > j.second;
    }
  };

  int64 Evaluate(const Assignment* delta,
                 int64 current_penalty,
                 const int64* const out_values,
                 bool cache_delta_values);

  IntVar* penalized_objective_;
  Assignment assignment_;
  int64 assignment_penalized_value_;
  int64 old_penalized_value_;
  scoped_array<IntVar*> vars_;
  const int64 size_;
  hash_map<const IntVar*, int64> indices_;
  const double penalty_factor_;
  scoped_ptr<GuidedLocalSearchPenalties> penalties_;
  scoped_array<int64> current_penalized_values_;
  scoped_array<int64> delta_cache_;
  bool incremental_;
};

GuidedLocalSearch::GuidedLocalSearch(Solver* const s,
                                     IntVar* objective,
                                     bool maximize,
                                     int64 step,
                                     const IntVar* const* vars,
                                     int size,
                                     double penalty_factor)
    : Metaheuristic(s, maximize, objective, step),
      penalized_objective_(NULL),
      assignment_(s),
      assignment_penalized_value_(0),
      old_penalized_value_(0),
      size_(size),
      penalty_factor_(penalty_factor),
      incremental_(false) {
  DCHECK_GE(size_, 0);
  if (size_ > 0) {
    vars_.reset(new IntVar*[size_]);
    memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    assignment_.Add(vars_.get(), size);
    current_penalized_values_.reset(new int64[size]);
    delta_cache_.reset(new int64[size]);
    memset(current_penalized_values_.get(), 0,
           size_ * sizeof(*current_penalized_values_.get()));
  }
  for (int i = 0; i < size; ++i) {
    indices_[vars_[i]] = i;
  }
  if (FLAGS_cp_use_sparse_gls_penalties) {
    penalties_.reset(new GuidedLocalSearchPenaltiesMap(size));
  } else {
    penalties_.reset(new GuidedLocalSearchPenaltiesTable(size));
  }
}

void GuidedLocalSearch::EnterSearch() {
  if (maximize_) {
    current_ = objective_->Min();
  } else {
    current_ = objective_->Max();
  }
  best_ = current_;
}

// Add the following constraint (includes aspiration criterion):
// if minimizing,
//      objective =< Max(current penalized cost - penalized_objective - step,
//                       best solution cost - step)
// if maximizing,
//      objective >= Min(current penalized cost - penalized_objective + step,
//                       best solution cost + step)
void GuidedLocalSearch::ApplyDecision(Decision* const d) {
  if (d == solver()->balancing_decision()) {
    return;
  }
  std::vector<IntVar*> elements;
  assignment_penalized_value_ = 0;
  if (penalties_->HasValues()) {
    for (int i = 0; i < size_; ++i) {
      IntExpr* expr = MakeElementPenalty(i);
      elements.push_back(expr->Var());
      const int64 penalty = AssignmentElementPenalty(assignment_, i);
      current_penalized_values_[i] = penalty;
      delta_cache_[i] = penalty;
      assignment_penalized_value_ += penalty;
    }
    old_penalized_value_ = assignment_penalized_value_;
    incremental_ = false;
    penalized_objective_ = solver()->MakeSum(elements)->Var();
    if (maximize_) {
      IntExpr* min_pen_exp = solver()->MakeDifference(current_ + step_,
                                                      penalized_objective_);
      IntVar* min_exp = solver()->MakeMin(min_pen_exp, best_ + step_)->Var();
      solver()->AddConstraint(solver()->MakeGreaterOrEqual(objective_,
                                                           min_exp));
    } else {
      IntExpr* max_pen_exp = solver()->MakeDifference(current_ - step_,
                                                      penalized_objective_);
      IntVar* max_exp = solver()->MakeMax(max_pen_exp, best_ - step_)->Var();
      solver()->AddConstraint(solver()->MakeLessOrEqual(objective_, max_exp));
    }
  } else {
    penalized_objective_ = NULL;
    if (maximize_) {
      objective_->SetMin(current_ + step_);
    } else {
      objective_->SetMax(current_ - step_);
    }
  }
}

bool GuidedLocalSearch::AtSolution() {
  current_ = objective_->Value();
  if (maximize_) {
    best_ = std::max(current_, best_);
  } else {
    best_ = std::min(current_, best_);
  }
  if (penalized_objective_ != NULL) {  // In case no move has been found
    current_ += penalized_objective_->Value();
  }
  assignment_.Store();
  return true;
}

// GLS filtering; compute the penalized value corresponding to the delta and
// modify objective bound accordingly.
bool GuidedLocalSearch::AcceptDelta(Assignment* delta, Assignment* deltadelta) {
  if ((delta != NULL || deltadelta != NULL) && penalties_->HasValues()) {
    int64 penalty = 0;
    if (!deltadelta->Empty()) {
      if (!incremental_) {
        penalty = Evaluate(delta,
                           assignment_penalized_value_,
                           current_penalized_values_.get(),
                           true);
      } else {
        penalty = Evaluate(deltadelta,
                           old_penalized_value_,
                           delta_cache_.get(),
                           true);
      }
      incremental_ = true;
    } else {
      if (incremental_) {
        for (int i = 0; i < size_; ++i) {
          delta_cache_[i] = current_penalized_values_[i];
        }
        old_penalized_value_ = assignment_penalized_value_;
      }
      incremental_ = false;
      penalty = Evaluate(delta,
                         assignment_penalized_value_,
                         current_penalized_values_.get(),
                         false);
    }
    old_penalized_value_ = penalty;
    if (!delta->HasObjective()) {
      delta->AddObjective(objective_);
    }
    if (delta->Objective() == objective_) {
      if (maximize_) {
        delta->SetObjectiveMin(std::max(std::min(current_ + step_ - penalty,
                                                 best_ + step_),
                                        delta->ObjectiveMin()));
      } else {
        delta->SetObjectiveMax(std::min(std::max(current_ - step_ - penalty,
                                                 best_ - step_),
                                        delta->ObjectiveMax()));
      }
    }
  }
  return true;
}

int64 GuidedLocalSearch::Evaluate(const Assignment* delta,
                                  int64 current_penalty,
                                  const int64* const out_values,
                                  bool cache_delta_values) {
  int64 penalty = current_penalty;
  const Assignment::IntContainer& container = delta->IntVarContainer();
  const int size = container.Size();
  for (int i = 0; i < size; ++i) {
    const IntVarElement& new_element = container.Element(i);
    const IntVar* var = new_element.Var();
    int64 index = -1;
    if (FindCopy(indices_, var, &index)) {
      penalty -= out_values[index];
      int64 new_penalty = 0;
      if (EvaluateElementValue(container, index, &i, &new_penalty)) {
        penalty += new_penalty;
        if (cache_delta_values) {
          delta_cache_[index] = new_penalty;
        }
      }
    }
  }
  return penalty;
}

// Penalize all the most expensive arcs (var, value) according to their utility:
// utility(i, j) = cost(i, j) / (1 + penalty(i, j))
bool GuidedLocalSearch::LocalOptimum() {
  std::vector<pair<Arc, double> > utility(size_);
  for (int i = 0; i < size_; ++i) {
    const int64 var_value = assignment_.Value(vars_[i]);
    const int64 value =
        (var_value != i) ? AssignmentPenalty(assignment_, i, var_value) : 0;
    const Arc arc(i, var_value);
    const int64 penalty = penalties_->Value(arc);
    utility[i] = pair<Arc, double>(arc, value / (penalty + 1.0));
  }
  Comparator comparator;
  std::sort(utility.begin(), utility.end(), comparator);
  int64 utility_value = utility[0].second;
  penalties_->Increment(utility[0].first);
  for (int i = 1;
       i < utility.size() && utility_value == utility[i].second;
       ++i) {
    penalties_->Increment(utility[i].first);
  }
  if (maximize_) {
    current_ = kint64min;
  } else {
    current_ = kint64max;
  }
  return true;
}

class BinaryGuidedLocalSearch : public GuidedLocalSearch {
 public:
  BinaryGuidedLocalSearch(Solver* const solver,
                          IntVar* const objective,
                          Solver::IndexEvaluator2* objective_function,
                          bool maximize,
                          int64 step,
                          const IntVar* const* vars,
                          int size,
                          double penalty_factor);
  virtual ~BinaryGuidedLocalSearch() {}
  virtual IntExpr* MakeElementPenalty(int index);
  virtual int64 AssignmentElementPenalty(const Assignment& assignment,
                                         int index);
  virtual int64 AssignmentPenalty(const Assignment& assignment,
                                  int index,
                                  int64 next);
  virtual bool EvaluateElementValue(const Assignment::IntContainer& container,
                                    int64 index,
                                    int* container_index,
                                    int64* penalty);
 private:
  int64 PenalizedValue(int64 i, int64 j);
  scoped_ptr<Solver::IndexEvaluator2> objective_function_;
};

BinaryGuidedLocalSearch::BinaryGuidedLocalSearch(
    Solver* const solver,
    IntVar* const objective,
    Solver::IndexEvaluator2* objective_function,
    bool maximize,
    int64 step,
    const IntVar* const* vars,
    int size,
    double penalty_factor)
    : GuidedLocalSearch(
        solver, objective, maximize, step, vars, size, penalty_factor),
      objective_function_(objective_function) {
  objective_function_->CheckIsRepeatable();
}

IntExpr* BinaryGuidedLocalSearch::MakeElementPenalty(int index) {
  return solver()->MakeElement(
      NewPermanentCallback(this,
                           &BinaryGuidedLocalSearch::PenalizedValue,
                           static_cast<int64>(index)),
      vars_[index]);
}

int64 BinaryGuidedLocalSearch::AssignmentElementPenalty(
    const Assignment& assignment,
    int index) {
  return PenalizedValue(index, assignment.Value(vars_[index]));
}

int64 BinaryGuidedLocalSearch::AssignmentPenalty(
    const Assignment& assignment,
    int index,
    int64 next) {
  return objective_function_->Run(index, next);
}

bool BinaryGuidedLocalSearch::EvaluateElementValue(
    const Assignment::IntContainer& container,
    int64 index,
    int* container_index,
    int64* penalty) {
  const IntVarElement& element = container.Element(*container_index);
  if (element.Activated()) {
    *penalty = PenalizedValue(index, element.Value());
    return true;
  }
  return false;
}

// Penalized value for (i, j) = penalty_factor_ * penalty(i, j) * cost (i, j)
int64 BinaryGuidedLocalSearch::PenalizedValue(int64 i, int64 j) {
  const Arc arc(i, j);
  const int64 penalty = penalties_->Value(arc);
  if (penalty != 0) {  // objective_function_->Run(i, j) can be costly
    const int64 penalized_value =
        penalty_factor_ * penalty * objective_function_->Run(i, j);
    if (maximize_) {
      return -penalized_value;
    } else {
      return penalized_value;
    }
  } else {
    return 0;
  }
}

class TernaryGuidedLocalSearch : public GuidedLocalSearch {
 public:
  TernaryGuidedLocalSearch(Solver* const solver,
                           IntVar* const objective,
                           Solver::IndexEvaluator3* objective_function,
                           bool maximize,
                           int64 step,
                           const IntVar* const* vars,
                           const IntVar* const* secondary_vars,
                           int size,
                           double penalty_factor);
  virtual ~TernaryGuidedLocalSearch() {}
  virtual IntExpr* MakeElementPenalty(int index);
  virtual int64 AssignmentElementPenalty(const Assignment& assignment,
                                         int index);
  virtual int64 AssignmentPenalty(const Assignment& assignment,
                                  int index,
                                  int64 next);
  virtual bool EvaluateElementValue(const Assignment::IntContainer& container,
                                    int64 index,
                                    int* container_index,
                                    int64* penalty);
 private:
  int64 PenalizedValue(int64 i, int64 j, int64 k);
  int64 GetAssignmentSecondaryValue(const Assignment::IntContainer& container,
                                    int index,
                                    int* container_index) const;

  scoped_array<IntVar*> secondary_vars_;
  scoped_ptr<Solver::IndexEvaluator3> objective_function_;
};

TernaryGuidedLocalSearch::TernaryGuidedLocalSearch(
    Solver* const solver,
    IntVar* const objective,
    Solver::IndexEvaluator3* objective_function,
    bool maximize,
    int64 step,
    const IntVar* const* vars,
    const IntVar* const* secondary_vars,
    int size,
    double penalty_factor)
    : GuidedLocalSearch(
        solver, objective, maximize, step, vars, size, penalty_factor),
      objective_function_(objective_function) {
  objective_function_->CheckIsRepeatable();
  CHECK_GE(size_, 0);
  if (size_ > 0) {
    secondary_vars_.reset(new IntVar*[size_]);
    memcpy(secondary_vars_.get(),
           secondary_vars, size_ * sizeof(*secondary_vars));
    assignment_.Add(secondary_vars_.get(), size_);
  }
}

IntExpr* TernaryGuidedLocalSearch::MakeElementPenalty(int index) {
  return solver()->MakeElement(
      NewPermanentCallback(this,
                           &TernaryGuidedLocalSearch::PenalizedValue,
                           static_cast<int64>(index)),
      vars_[index],
      secondary_vars_[index]);
}

int64 TernaryGuidedLocalSearch::AssignmentElementPenalty(
    const Assignment& assignment,
    int index) {
  return PenalizedValue(index,
                        assignment.Value(vars_[index]),
                        assignment.Value(secondary_vars_[index]));
}

int64 TernaryGuidedLocalSearch::AssignmentPenalty(
    const Assignment& assignment,
    int index,
    int64 next) {
  return objective_function_->Run(index,
                                  next,
                                  assignment.Value(secondary_vars_[index]));
}

bool TernaryGuidedLocalSearch::EvaluateElementValue(
    const Assignment::IntContainer& container,
    int64 index,
    int* container_index,
    int64* penalty) {
  const IntVarElement& element = container.Element(*container_index);
  if (element.Activated()) {
    *penalty = PenalizedValue(index,
                              element.Value(),
                              GetAssignmentSecondaryValue(container,
                                                          index,
                                                          container_index));
    return true;
  }
  return false;
}

// Penalized value for (i, j) = penalty_factor_ * penalty(i, j) * cost (i, j)
int64 TernaryGuidedLocalSearch::PenalizedValue(int64 i, int64 j, int64 k) {
  const Arc arc(i, j);
  const int64 penalty = penalties_->Value(arc);
  if (penalty != 0) {  // objective_function_->Run(i, j, k) can be costly
    const int64 penalized_value =
        penalty_factor_ * penalty * objective_function_->Run(i, j, k);
    if (maximize_) {
      return -penalized_value;
    } else {
      return penalized_value;
    }
  } else {
    return 0;
  }
}

int64 TernaryGuidedLocalSearch::GetAssignmentSecondaryValue(
    const Assignment::IntContainer& container,
    int index,
    int* container_index) const {
  const IntVar* secondary_var = secondary_vars_[index];
  int hint_index = *container_index + 1;
  if (hint_index > 0 && hint_index < container.Size()
      && secondary_var == container.Element(hint_index).Var()) {
    *container_index = hint_index;
    return container.Element(hint_index).Value();
  } else {
    return container.Element(secondary_var).Value();
  }
}

SearchMonitor* Solver::MakeGuidedLocalSearch(
    bool maximize,
    IntVar* const objective,
    ResultCallback2<int64, int64, int64>* objective_function,
    int64 step,
    const std::vector<IntVar*>& vars,
    double penalty_factor) {
  return MakeGuidedLocalSearch(maximize,
                               objective,
                               objective_function,
                               step,
                               vars.data(),
                               vars.size(),
                               penalty_factor);
}

SearchMonitor* Solver::MakeGuidedLocalSearch(
    bool maximize,
    IntVar* const objective,
    ResultCallback2<int64, int64, int64>* objective_function,
    int64 step,
    const IntVar* const* vars,
    int size,
    double penalty_factor) {
  return RevAlloc(new BinaryGuidedLocalSearch(this,
                                              objective,
                                              objective_function,
                                              maximize,
                                              step,
                                              vars,
                                              size,
                                              penalty_factor));
}

SearchMonitor* Solver::MakeGuidedLocalSearch(
    bool maximize,
    IntVar* const objective,
    ResultCallback3<int64, int64, int64, int64>* objective_function,
    int64 step,
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars,
    double penalty_factor) {
  return MakeGuidedLocalSearch(maximize,
                               objective,
                               objective_function,
                               step,
                               vars.data(),
                               secondary_vars.data(),
                               vars.size(),
                               penalty_factor);
}

SearchMonitor* Solver::MakeGuidedLocalSearch(
    bool maximize,
    IntVar* const objective,
    ResultCallback3<int64, int64, int64, int64>* objective_function,
    int64 step,
    const IntVar* const* vars,
    const IntVar* const* secondary_vars,
    int size,
    double penalty_factor) {
  return RevAlloc(new TernaryGuidedLocalSearch(this,
                                               objective,
                                               objective_function,
                                               maximize,
                                               step,
                                               vars,
                                               secondary_vars,
                                               size,
                                               penalty_factor));
}

// ---------- Search Limits ----------

// ----- Base Class -----

SearchLimit::~SearchLimit() {}

void SearchLimit::EnterSearch()  {
  crossed_ = false;
  Init();
}

void SearchLimit::BeginNextDecision(DecisionBuilder* const b) {
  PeriodicCheck();
}

void SearchLimit::RefuteDecision(Decision* const d) {
  PeriodicCheck();
}

void SearchLimit::PeriodicCheck() {
  if (crossed_ || Check()) {
    crossed_ = true;
    solver()->Fail();
  }
}

// ----- Regular Limit -----

// Usual limit based on wall_time, number of explored branches and
// number of failures in the search tree
class RegularLimit : public SearchLimit {
 public:
  RegularLimit(Solver* const s,
               int64 time,
               int64 branches,
               int64 failures,
               int64 solutions,
               bool smart_time_check,
               bool cumulative);
  virtual ~RegularLimit();
  virtual void Copy(const SearchLimit* const limit);
  virtual SearchLimit* MakeClone() const;
  virtual bool Check();
  virtual void Init();
  virtual void ExitSearch();
  void UpdateLimits(int64 time,
                    int64 branches,
                    int64 failures,
                    int64 solutions);
  int64 wall_time() { return wall_time_; }
  virtual string DebugString() const;
 private:
  bool CheckTime();

  int64 wall_time_;
  int64 wall_time_offset_;
  int64 check_count_;
  int64 next_check_;
  bool smart_time_check_;
  int64 branches_;
  int64 branches_offset_;
  int64 failures_;
  int64 failures_offset_;
  int64 solutions_;
  int64 solutions_offset_;
  // If cumulative if false, then the limit applies to each search
  // independently. If it's true, the limit applies globally to all search for
  // which this monitor is used.
  // When cumulative is true, the offset fields have two different meanings
  // depending on context:
  // - within a search, it's an offset to be subtracted from the current value
  // - outside of search, it's the amount consumed in previous searches
  bool cumulative_;
};

RegularLimit::RegularLimit(Solver* const s,
                           int64 time,
                           int64 branches,
                           int64 failures,
                           int64 solutions,
                           bool smart_time_check,
                           bool cumulative)
    : SearchLimit(s),
      wall_time_(time),
      wall_time_offset_(0),
      check_count_(0),
      next_check_(0),
      smart_time_check_(smart_time_check),
      branches_(branches),
      branches_offset_(0),
      failures_(failures),
      failures_offset_(0),
      solutions_(solutions),
      solutions_offset_(0),
      cumulative_(cumulative) {}

RegularLimit::~RegularLimit() {}

void RegularLimit::Copy(const SearchLimit* const limit) {
  const RegularLimit* const regular =
      reinterpret_cast<const RegularLimit* const>(limit);
  wall_time_ = regular->wall_time_;
  branches_ = regular->branches_;
  failures_ = regular->failures_;
  solutions_ = regular->solutions_;
  smart_time_check_ = regular->smart_time_check_;
  cumulative_ = regular->cumulative_;
}

SearchLimit* RegularLimit::MakeClone() const {
  Solver* const s = solver();
  return s->MakeLimit(wall_time_,
                      branches_,
                      failures_,
                      solutions_,
                      smart_time_check_);
}

bool RegularLimit::Check()  {
  Solver* const s = solver();
  // Warning limits might be kint64max, do not move the offset to the rhs
  return s->branches() - branches_offset_ >= branches_ ||
      s->failures() - failures_offset_ >= failures_ ||
      CheckTime() ||
      s->solutions() - solutions_offset_ >= solutions_;
}

void RegularLimit::Init() {
  Solver* const s = solver();
  branches_offset_ = s->branches();
  failures_offset_ = s->failures();
  wall_time_offset_ = s->wall_time();
  solutions_offset_ = s->solutions();
  check_count_ = 0;
  next_check_ = 0;
}

void RegularLimit::ExitSearch() {
  if (cumulative_) {
    // Reduce the limits by the amount consumed during this search
    Solver* const s = solver();
    branches_ -= s->branches() - branches_offset_;
    failures_ -= s->failures() - failures_offset_;
    wall_time_ -= s->wall_time() - wall_time_offset_;
    solutions_ -= s->solutions() - solutions_offset_;
  }
}

void RegularLimit::UpdateLimits(int64 time,
                                int64 branches,
                                int64 failures,
                                int64 solutions) {
  wall_time_ = time;
  branches_ = branches;
  failures_ = failures;
  solutions_ = solutions;
}

string RegularLimit::DebugString() const {
  return StringPrintf("RegularLimit(crossed = %i, wall_time = %"
                      GG_LL_FORMAT "d, "
                      "branches = %" GG_LL_FORMAT
                      "d, failures = %" GG_LL_FORMAT
                      "d, solutions = %" GG_LL_FORMAT
                      "d cumulative = %s",
                      crossed(), wall_time_, branches_, failures_, solutions_,
                      (cumulative_? "true":"false"));
}

bool RegularLimit::CheckTime() {
  const int64 kMaxSkip = 100;
  const int64 kCheckWarmupIterations = 100;
  ++check_count_;
  if (wall_time_ != kint64max && next_check_ <= check_count_) {
    Solver* const s = solver();
    int64 time_delta = s->wall_time() -  wall_time_offset_;
    if (smart_time_check_
        && check_count_ > kCheckWarmupIterations
        && time_delta > 0) {
      int64 approximate_calls = (wall_time_ * check_count_) / time_delta;
      next_check_ = check_count_ + std::min(kMaxSkip, approximate_calls);
    }
    return time_delta >= wall_time_;
  } else {
    return false;
  }
}


SearchLimit* Solver::MakeLimit(int64 time,
                               int64 branches,
                               int64 failures,
                               int64 solutions) {
  return MakeLimit(time, branches, failures, solutions, false);
}

SearchLimit* Solver::MakeLimit(int64 time,
                               int64 branches,
                               int64 failures,
                               int64 solutions,
                               bool smart_time_check) {
  return MakeLimit(
      time, branches, failures, solutions, smart_time_check, false);
}

SearchLimit* Solver::MakeLimit(int64 time,
                               int64 branches,
                               int64 failures,
                               int64 solutions,
                               bool smart_time_check,
                               bool cumulative) {
  return RevAlloc(new RegularLimit(this,
                                   time,
                                   branches,
                                   failures,
                                   solutions,
                                   smart_time_check,
                                   cumulative));
}

SearchLimit* Solver::MakeLimit(
    const SearchLimitProto& proto) {
  return MakeLimit(proto.time(),
                   proto.branches(),
                   proto.failures(),
                   proto.solutions(),
                   proto.smart_time_check(),
                   proto.cumulative());
}

// A limit whose Check function is the OR of two underlying limits.
class ORLimit : public SearchLimit {
 public:
  ORLimit(SearchLimit* limit_1,
          SearchLimit* limit_2)
      : SearchLimit(limit_1->solver()),
        limit_1_(limit_1),
        limit_2_(limit_2) {
    CHECK_NOTNULL(limit_1);
    CHECK_NOTNULL(limit_2);
    CHECK_EQ(limit_1->solver(), limit_2->solver())
        << "Illegal arguments: cannot combines limits that belong to different "
        << "solvers, because the reversible allocations could delete one and "
        << "not the other.";
  }

  virtual bool Check() {
    // Check being non-const, there may be side effects. So we always call both
    // checks.
    const bool check_1 = limit_1_->Check();
    const bool check_2 = limit_2_->Check();
    return check_1 || check_2;
  }

  virtual void Init() {
    limit_1_->Init();
    limit_2_->Init();
  }

  virtual void Copy(const SearchLimit* const limit) {
    LOG(FATAL) << "Not implemented.";
  }

  virtual SearchLimit* MakeClone() const {
    // Deep cloning: the underlying limits are cloned, too.
    return solver()->MakeLimit(limit_1_->MakeClone(), limit_2_->MakeClone());
  }

  virtual void EnterSearch() {
    limit_1_->EnterSearch();
    limit_2_->EnterSearch();
  }
  virtual void BeginNextDecision(DecisionBuilder* const b) {
    limit_1_->BeginNextDecision(b);
    limit_2_->BeginNextDecision(b);
  }
  virtual void PeriodicCheck() {
    limit_1_->PeriodicCheck();
    limit_2_->PeriodicCheck();
  }
  virtual void RefuteDecision(Decision* const d) {
    limit_1_->RefuteDecision(d);
    limit_2_->RefuteDecision(d);
  }
  virtual string DebugString() const {
    return StrCat("OR limit (", limit_1_->DebugString(),
                  " OR ", limit_2_->DebugString(), ")");
  }

 private:
  SearchLimit* const limit_1_;
  SearchLimit* const limit_2_;
};

SearchLimit* Solver::MakeLimit(SearchLimit* const limit_1,
                               SearchLimit* const limit_2) {
  return RevAlloc(new ORLimit(limit_1, limit_2));
}

void Solver::UpdateLimits(int64 time,
                          int64 branches,
                          int64 failures,
                          int64 solutions,
                          SearchLimit* limit) {
  reinterpret_cast<RegularLimit*>(limit)->UpdateLimits(time,
                                                branches,
                                                failures,
                                                solutions);
}

int64 Solver::GetTime(SearchLimit* limit) {
  return reinterpret_cast<RegularLimit*>(limit)->wall_time();
}

class CustomLimit : public SearchLimit {
 public:
  CustomLimit(Solver* const s, ResultCallback<bool>* limiter, bool del);
  virtual ~CustomLimit();
  virtual bool Check();
  virtual void Init();
  virtual void Copy(const SearchLimit* const limit);
  virtual SearchLimit* MakeClone() const;
 private:
  ResultCallback<bool>* limiter_;
  bool delete_;
};

CustomLimit::CustomLimit(Solver* const s,
                         ResultCallback<bool>* limiter,
                         bool del)
    : SearchLimit(s), limiter_(limiter), delete_(del) {
  limiter_->CheckIsRepeatable();
}

CustomLimit::~CustomLimit() {
  if (delete_) {
    delete limiter_;
  }
}

bool CustomLimit::Check() {
  return limiter_->Run();
}

void CustomLimit::Init() {}

void CustomLimit::Copy(const SearchLimit* const limit) {
  const CustomLimit* const custom = reinterpret_cast<const CustomLimit* const>(limit);
  CHECK(!delete_) << "Cannot copy to non-cloned custom limit";
  limiter_ = custom->limiter_;
}

SearchLimit* CustomLimit::MakeClone() const {
  Solver* const s = solver();
  return s->RevAlloc(new CustomLimit(s, limiter_, false));
}

SearchLimit* Solver::MakeCustomLimit(ResultCallback<bool>* limiter) {
  return RevAlloc(new CustomLimit(this, limiter, true));
}

// ---------- SolveOnce ----------

class SolveOnce : public DecisionBuilder {
 public:
  explicit SolveOnce(DecisionBuilder* const db) : db_(db) {
    CHECK(db != NULL);
  }

  SolveOnce(DecisionBuilder* const db,  const std::vector<SearchMonitor*>& monitors)
      : db_(db), monitors_(monitors) {
    CHECK(db != NULL);
  }

  virtual ~SolveOnce() {}

  virtual Decision* Next(Solver* s) {
    bool res = s->NestedSolve(db_, false, monitors_);
    if (!res) {
      s->Fail();
    }
    return NULL;
  }

  virtual string DebugString() const {
    return StringPrintf("SolveOnce(%s)", db_->DebugString().c_str());
  }
 private:
  DecisionBuilder* const db_;
  std::vector<SearchMonitor*> monitors_;
};

DecisionBuilder* Solver::MakeSolveOnce(DecisionBuilder* const db) {
  return RevAlloc(new SolveOnce(db));
}

DecisionBuilder* Solver::MakeSolveOnce(DecisionBuilder* const db,
                                       SearchMonitor* const monitor1) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(monitor1);
  return RevAlloc(new SolveOnce(db, monitors));
}

DecisionBuilder* Solver::MakeSolveOnce(DecisionBuilder* const db,
                                       SearchMonitor* const monitor1,
                                       SearchMonitor* const monitor2) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(monitor1);
  monitors.push_back(monitor2);
  return RevAlloc(new SolveOnce(db, monitors));
}

DecisionBuilder* Solver::MakeSolveOnce(DecisionBuilder* const db,
                                       SearchMonitor* const monitor1,
                                       SearchMonitor* const monitor2,
                                       SearchMonitor* const monitor3) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(monitor1);
  monitors.push_back(monitor2);
  monitors.push_back(monitor3);
  return RevAlloc(new SolveOnce(db, monitors));
}

DecisionBuilder* Solver::MakeSolveOnce(DecisionBuilder* const db,
                                       SearchMonitor* const monitor1,
                                       SearchMonitor* const monitor2,
                                       SearchMonitor* const monitor3,
                                       SearchMonitor* const monitor4) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(monitor1);
  monitors.push_back(monitor2);
  monitors.push_back(monitor3);
  monitors.push_back(monitor4);
  return RevAlloc(new SolveOnce(db, monitors));
}

DecisionBuilder* Solver::MakeSolveOnce(DecisionBuilder* const db,
                                       const std::vector<SearchMonitor*>& monitors) {
  return RevAlloc(new SolveOnce(db, monitors));
}

// ---------- NestedOptimize ----------

class NestedOptimize : public DecisionBuilder {
 public:
  NestedOptimize(DecisionBuilder* const db,
                 Assignment* const solution,
                 bool maximize,
                 int64 step)
      : db_(db),
        solution_(solution),
        maximize_(maximize),
        step_(step),
        collector_(NULL) {
    CHECK_NOTNULL(db);
    CHECK_NOTNULL(solution);
    CHECK(solution->HasObjective());
    AddMonitors();
  }

  NestedOptimize(DecisionBuilder* const db,
                 Assignment* const solution,
                 bool maximize,
                 int64 step,
                 const std::vector<SearchMonitor*>& monitors)
      : db_(db),
        solution_(solution),
        maximize_(maximize),
        step_(step),
        monitors_(monitors),
        collector_(NULL) {
    CHECK_NOTNULL(db);
    CHECK_NOTNULL(solution);
    CHECK(solution->HasObjective());
    AddMonitors();
  }

  void AddMonitors() {
    Solver* const solver = solution_->solver();
    collector_ = solver->MakeLastSolutionCollector(solution_);
    monitors_.push_back(collector_);
    OptimizeVar* const optimize =
        solver->MakeOptimize(maximize_, solution_->Objective(), step_);
    monitors_.push_back(optimize);
  }

  virtual Decision* Next(Solver* solver) {
    solver->NestedSolve(db_, true, monitors_);
    if (collector_->solution_count() == 0) {
      solver->Fail();
    }
    collector_->solution(0)->Restore();
    return NULL;
  }

  virtual string DebugString() const {
    return StringPrintf("NestedOptimize(db = %s, maximize = %d, step = %lld)",
                        db_->DebugString().c_str(),
                        maximize_,
                        step_);
  }
 private:
  DecisionBuilder* const db_;
  Assignment* const solution_;
  const bool maximize_;
  const int64 step_;
  std::vector<SearchMonitor*> monitors_;
  SolutionCollector* collector_;
};

DecisionBuilder* Solver::MakeNestedOptimize(DecisionBuilder* const db,
                                            Assignment* const solution,
                                            bool maximize,
                                            int64 step) {
  return RevAlloc(new NestedOptimize(db, solution, maximize, step));
}

DecisionBuilder* Solver::MakeNestedOptimize(DecisionBuilder* const db,
                                            Assignment* const solution,
                                            bool maximize,
                                            int64 step,
                                            SearchMonitor* const monitor1) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(monitor1);
  return RevAlloc(new NestedOptimize(db, solution, maximize, step, monitors));
}

DecisionBuilder* Solver::MakeNestedOptimize(DecisionBuilder* const db,
                                            Assignment* const solution,
                                            bool maximize,
                                            int64 step,
                                            SearchMonitor* const monitor1,
                                            SearchMonitor* const monitor2) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(monitor1);
  monitors.push_back(monitor2);
  return RevAlloc(new NestedOptimize(db, solution, maximize, step, monitors));
}

DecisionBuilder* Solver::MakeNestedOptimize(DecisionBuilder* const db,
                                            Assignment* const solution,
                                            bool maximize,
                                            int64 step,
                                            SearchMonitor* const monitor1,
                                            SearchMonitor* const monitor2,
                                            SearchMonitor* const monitor3) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(monitor1);
  monitors.push_back(monitor2);
  monitors.push_back(monitor3);
  return RevAlloc(new NestedOptimize(db, solution, maximize, step, monitors));
}

DecisionBuilder* Solver::MakeNestedOptimize(DecisionBuilder* const db,
                                            Assignment* const solution,
                                            bool maximize,
                                            int64 step,
                                            SearchMonitor* const monitor1,
                                            SearchMonitor* const monitor2,
                                            SearchMonitor* const monitor3,
                                            SearchMonitor* const monitor4) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(monitor1);
  monitors.push_back(monitor2);
  monitors.push_back(monitor3);
  monitors.push_back(monitor4);
  return RevAlloc(new NestedOptimize(db, solution, maximize, step, monitors));
}

DecisionBuilder* Solver::MakeNestedOptimize(
    DecisionBuilder* const db,
    Assignment* const solution,
    bool maximize,
    int64 step,
    const std::vector<SearchMonitor*>& monitors) {
  return RevAlloc(new NestedOptimize(db, solution, maximize, step, monitors));
}


// ---------- Restart ----------

namespace {
  // Luby Strategy
int64 NextLuby(int i) {
  DCHECK_GT(i, 0);
  DCHECK_LT(i, kint32max);
  int64 power;

  // let's find the least power of 2 >= (i+1).
  power = 2;
  // Cannot overflow, because bounded by kint32max + 1.
  while (power < (i + 1)) {
    power <<= 1;
  }
  if (power == i + 1) {
    return (power / 2);
  }
  return NextLuby(i - (power / 2) + 1);
}
}

class LubyRestart : public SearchMonitor {
 public:
  LubyRestart(Solver* const s, int scale_factor)
      : SearchMonitor(s),
        scale_factor_(scale_factor),
        iteration_(1),
        current_fails_(0),
        next_step_(scale_factor) {
    CHECK_GE(scale_factor, 1);
  }
  virtual ~LubyRestart() {}

  virtual void BeginFail() {
    if (++current_fails_ >= next_step_) {
      current_fails_ = 0;
      next_step_ = NextLuby(++iteration_) * scale_factor_;
      RestartCurrentSearch();
    }
  }

  virtual string DebugString() const {
    return StringPrintf("LubyRestart(%i)", scale_factor_);
  }
 private:
  const int scale_factor_;
  int iteration_;
  int64 current_fails_;
  int64 next_step_;
};

SearchMonitor* Solver::MakeLubyRestart(int scale_factor) {
  return RevAlloc(new LubyRestart(this, scale_factor));
}

// ----- Constant Restart -----

class ConstantRestart : public SearchMonitor {
 public:
  ConstantRestart(Solver* const s, int frequency)
      : SearchMonitor(s),
        frequency_(frequency),
        current_fails_(0) {
    CHECK_GE(frequency, 1);
  }
  virtual ~ConstantRestart() {}

  virtual void BeginFail() {
    if (++current_fails_ >= frequency_) {
      current_fails_ = 0;
      RestartCurrentSearch();
    }
  }

  virtual string DebugString() const {
    return StringPrintf("ConstantRestart(%i)", frequency_);
  }
 private:
  const int frequency_;
  int64 current_fails_;
};

SearchMonitor* Solver::MakeConstantRestart(int frequency) {
  return RevAlloc(new ConstantRestart(this, frequency));
}

// ---------- Symmetry Breaking ----------

// The symmetry manager maintains a list of problem symmetries.  Each
// symmetry is called on each decision and should return a term
// representing the boolean status of the symmetrical decision.
// i.e. : the decision is x == 3, the symmetrical decision is y == 5
// then the symmetry breaker should return IsEqualCst(y, 5).  Once
// this is done, upon refutation, for each symmetry breaker, the
// system adds a constraint that will forbid the symmetrical variation
// of the current explored search tree. This constraint can be expressed
// very simply just by keeping the list of current symmetrical decisions.
//
// This is called Symmetry Breaking During Search.
class SymmetryManager : public SearchMonitor {
 public:
  SymmetryManager(Solver* const s,
                  SymmetryBreaker* const * visitors,
                  int size)
      : SearchMonitor(s),
        visitors_(new SymmetryBreaker*[size]),
        clauses_(new SimpleRevFIFO<IntVar*>[size]),
        decisions_(new SimpleRevFIFO<Decision*>[size]),
        directions_(new SimpleRevFIFO<bool>[size]),  // false = left.
        size_(size) {
    CHECK_GT(size, 0);
    memcpy(visitors_.get(), visitors, size_ * sizeof(*visitors));
    for (int i = 0; i < size_; ++i) {
      CHECK(visitors_[i]->symmetry_manager() == NULL);
      visitors_[i]->set_symmetry_manager(this);
    }
  }

  virtual ~SymmetryManager() {}

  virtual void EnterSearch() {
    for (int i = 0; i < size_; ++i) {
      indices_[visitors_[i]] = i;
    }
  }

  virtual void EndNextDecision(DecisionBuilder* const db, Decision* const d) {
    if (d) {
      for (int i = 0; i < size_; ++i) {
        const void* const last = clauses_[i].Last();
        d->Accept(visitors_[i]);
        if (last != clauses_[i].Last()) {
          // Synchroneous push of decision as marker.
          decisions_[i].Push(solver(), d);
          directions_[i].Push(solver(), false);
        }
      }
    }
  }

  virtual void RefuteDecision(Decision* d) {
    for (int i = 0; i < size_; ++i) {
      if (decisions_[i].Last() != NULL && decisions_[i].LastValue() == d) {
        CheckSymmetries(i);
      }
    }
  }

  // TODO(user) : Improve speed, cache previous min and build them
  // incrementally.
  void CheckSymmetries(int index) {
    SimpleRevFIFO<IntVar*>::Iterator tmp(&clauses_[index]);
    SimpleRevFIFO<bool>::Iterator tmp_dir(&directions_[index]);
    Constraint* ct = NULL;
    {
      std::vector<IntVar*> guard;
      // keep the last entry for later, if loop doesn't exit.
      ++tmp;
      ++tmp_dir;
      while (tmp.ok()) {
        IntVar* const term = *tmp;
        if (!*tmp_dir) {
          if (term->Max() == 0) {
            // Premise is wrong. The clause will never apply.
            return;
          }
          if (term->Min() == 0) {
            DCHECK_EQ(1, term->Max());
            // Premise may be true. Adding to guard vector.
            guard.push_back(term);
          }
        }
        ++tmp;
        ++tmp_dir;
      }
      guard.push_back(clauses_[index].LastValue());
      directions_[index].SetLastValue(true);
      // Given premises: xi = ai
      // and a term y != b
      // The following is equivalent to
      // And(xi == a1) => y != b.
      ct = solver()->MakeEquality(solver()->MakeMin(guard), Zero());
    }
    DCHECK(ct != NULL);
    solver()->AddConstraint(ct);
  }

  void AddTermToClause(SymmetryBreaker* const visitor, IntVar* const term) {
    clauses_[FindOrDie(indices_, visitor)].Push(solver(), term);
  }
 private:
  scoped_array<SymmetryBreaker*> visitors_;
  scoped_array<SimpleRevFIFO<IntVar*> > clauses_;
  scoped_array<SimpleRevFIFO<Decision*> > decisions_;
  scoped_array<SimpleRevFIFO<bool> > directions_;
  int size_;
  std::map<SymmetryBreaker*, int> indices_;
};

void SymmetryBreaker::AddToClause(IntVar* const term) {
  symmetry_manager()->AddTermToClause(this, term);
}

SearchMonitor* Solver::MakeSymmetryManager(
    const std::vector<SymmetryBreaker*>& visitors) {
  return MakeSymmetryManager(visitors.data(), visitors.size());
}

SearchMonitor* Solver::MakeSymmetryManager(SymmetryBreaker* const * visitors,
                                           int size) {
  return RevAlloc(new SymmetryManager(this, visitors, size));
}

SearchMonitor* Solver::MakeSymmetryManager(SymmetryBreaker* const v1) {
  std::vector<SymmetryBreaker*> visitors;
  visitors.push_back(v1);
  return MakeSymmetryManager(visitors);
}

SearchMonitor* Solver::MakeSymmetryManager(SymmetryBreaker* const v1,
                                           SymmetryBreaker* const v2) {
  std::vector<SymmetryBreaker*> visitors;
  visitors.push_back(v1);
  visitors.push_back(v2);
  return MakeSymmetryManager(visitors);
}

SearchMonitor* Solver::MakeSymmetryManager(SymmetryBreaker* const v1,
                                           SymmetryBreaker* const v2,
                                           SymmetryBreaker* const v3) {
  std::vector<SymmetryBreaker*> visitors;
  visitors.push_back(v1);
  visitors.push_back(v2);
  visitors.push_back(v3);
  return MakeSymmetryManager(visitors);
}

SearchMonitor* Solver::MakeSymmetryManager(SymmetryBreaker* const v1,
                                           SymmetryBreaker* const v2,
                                           SymmetryBreaker* const v3,
                                           SymmetryBreaker* const v4) {
  std::vector<SymmetryBreaker*> visitors;
  visitors.push_back(v1);
  visitors.push_back(v2);
  visitors.push_back(v3);
  visitors.push_back(v4);
  return MakeSymmetryManager(visitors);
}
}  // namespace operations_research
