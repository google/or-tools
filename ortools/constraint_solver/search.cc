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


#include <algorithm>
#include <functional>
#include <unordered_map>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

//#include "ortools/base/casts.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/macros.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/timer.h"
#include "ortools/base/join.h"
#include "ortools/base/bitmap.h"
#include "ortools/base/map_util.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/hash.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/search_limit.pb.h"
#include "ortools/util/string_array.h"
#include "ortools/base/random.h"

DEFINE_bool(cp_use_sparse_gls_penalties, false,
            "Use sparse implementation to store Guided Local Search penalties");
DEFINE_bool(cp_log_to_vlog, false,
            "Whether search related logging should be "
            "vlog or info.");
DEFINE_int64(cp_large_domain_no_splitting_limit, 0xFFFFF,
             "Size limit to allow holes in variables from the strategy.");
namespace operations_research {

// ---------- Search Log ---------

SearchLog::SearchLog(Solver* const s, OptimizeVar* const obj, IntVar* const var,
                     std::function<std::string()> display_callback, int period)
    : SearchMonitor(s),
      period_(period),
      timer_(new WallTimer),
      var_(var),
      obj_(obj),
      display_callback_(std::move(display_callback)),
      nsol_(0),
      tick_(0LL),
      objective_min_(kint64max),
      objective_max_(kint64min),
      min_right_depth_(kint32max),
      max_depth_(0),
      sliding_min_depth_(0),
      sliding_max_depth_(0) {
  CHECK(obj == nullptr || var == nullptr)
      << "Either var or obj need to be nullptr.";
}

SearchLog::~SearchLog() {}

std::string SearchLog::DebugString() const { return "SearchLog"; }

void SearchLog::EnterSearch() {
  const std::string buffer =
      StringPrintf("Start search (%s)", MemoryUsage().c_str());
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
  const std::string buffer = StringPrintf(
      "End search (time = %" GG_LL_FORMAT "d ms, branches = %" GG_LL_FORMAT
      "d, failures = %" GG_LL_FORMAT "d, %s, speed = %" GG_LL_FORMAT
      "d branches/s)",
      ms, branches, solver()->failures(), MemoryUsage().c_str(),
      branches * 1000 / ms);
  OutputLine(buffer);
}

bool SearchLog::AtSolution() {
  Maintain();
  const int depth = solver()->SearchDepth();
  std::string obj_str = "";
  int64 current = 0;
  bool objective_updated = false;
  if (obj_ != nullptr) {
    current = obj_->Var()->Value();
    obj_str = obj_->Print();
    objective_updated = true;
  } else if (var_ != nullptr) {
    current = var_->Value();
    StringAppendF(&obj_str, "%" GG_LL_FORMAT "d, ", current);
    objective_updated = true;
  }
  if (objective_updated) {
    if (current >= objective_min_) {
      StringAppendF(&obj_str, "objective minimum = %" GG_LL_FORMAT "d, ",
                    objective_min_);
    } else {
      objective_min_ = current;
    }
    if (current <= objective_max_) {
      StringAppendF(&obj_str, "objective maximum = %" GG_LL_FORMAT "d, ",
                    objective_max_);
    } else {
      objective_max_ = current;
    }
  }
  std::string log;
  StringAppendF(&log, "Solution #%d (%stime = %" GG_LL_FORMAT
                      "d ms, branches = %" GG_LL_FORMAT
                      "d,"
                      " failures = %" GG_LL_FORMAT "d, depth = %d",
                nsol_++, obj_str.c_str(), timer_->GetInMs(),
                solver()->branches(), solver()->failures(), depth);
  if (solver()->neighbors() != 0) {
    StringAppendF(&log, ", neighbors = %" GG_LL_FORMAT
                        "d, filtered neighbors = %" GG_LL_FORMAT
                        "d,"
                        " accepted neighbors = %" GG_LL_FORMAT "d",
                  solver()->neighbors(), solver()->filtered_neighbors(),
                  solver()->accepted_neighbors());
  }
  StringAppendF(&log, ", %s", MemoryUsage().c_str());
  const int progress = solver()->TopProgressPercent();
  if (progress != SearchMonitor::kNoProgress) {
    StringAppendF(&log, ", limit = %d%%", progress);
  }
  log.append(")");
  OutputLine(log);
  if (display_callback_) {
    LOG(INFO) << display_callback_();
  }
  return false;
}

void SearchLog::BeginFail() { Maintain(); }

void SearchLog::NoMoreSolutions() {
  std::string buffer = StringPrintf("Finished search tree (time = %" GG_LL_FORMAT
                               "d ms, branches = %" GG_LL_FORMAT
                               "d,"
                               " failures = %" GG_LL_FORMAT "d",
                               timer_->GetInMs(), solver()->branches(),
                               solver()->failures());
  if (solver()->neighbors() != 0) {
    StringAppendF(&buffer, ", neighbors = %" GG_LL_FORMAT
                           "d, filtered neighbors = %" GG_LL_FORMAT
                           "d,"
                           " accepted neigbors = %" GG_LL_FORMAT "d",
                  solver()->neighbors(), solver()->filtered_neighbors(),
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
  std::string buffer = StringPrintf("%" GG_LL_FORMAT "d branches, %" GG_LL_FORMAT
                               "d ms, %" GG_LL_FORMAT "d failures",
                               solver()->branches(), timer_->GetInMs(),
                               solver()->failures());
  if (min_right_depth_ != kint32max && max_depth_ != 0) {
    const int depth = solver()->SearchDepth();
    StringAppendF(&buffer, ", tree pos=%d/%d/%d minref=%d max=%d",
                  sliding_min_depth_, depth, sliding_max_depth_,
                  min_right_depth_, max_depth_);
    sliding_min_depth_ = depth;
    sliding_max_depth_ = depth;
  }
  if (obj_ != nullptr && objective_min_ != kint64max &&
      objective_max_ != kint64min) {
    StringAppendF(&buffer, ", objective minimum = %" GG_LL_FORMAT
                           "d"
                           ", objective maximum = %" GG_LL_FORMAT "d",
                  objective_min_, objective_max_);
  }
  const int progress = solver()->TopProgressPercent();
  if (progress != SearchMonitor::kNoProgress) {
    StringAppendF(&buffer, ", limit = %d%%", progress);
  }
  OutputLine(buffer);
}

void SearchLog::Maintain() {
  const int current_depth = solver()->SearchDepth();
  sliding_min_depth_ = std::min(current_depth, sliding_min_depth_);
  sliding_max_depth_ = std::max(current_depth, sliding_max_depth_);
  max_depth_ = std::max(current_depth, max_depth_);
}

void SearchLog::BeginInitialPropagation() { tick_ = timer_->GetInMs(); }

void SearchLog::EndInitialPropagation() {
  const int64 delta = std::max(timer_->GetInMs() - tick_, 0LL);
  const std::string buffer =
      StringPrintf("Root node processed (time = %" GG_LL_FORMAT
                   "d ms, constraints = %d, %s)",
                   delta, solver()->constraints(), MemoryUsage().c_str());
  OutputLine(buffer);
}

void SearchLog::OutputLine(const std::string& line) {
  if (FLAGS_cp_log_to_vlog) {
    VLOG(1) << line;
  } else {
    LOG(INFO) << line;
  }
}

std::string SearchLog::MemoryUsage() {
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
  return RevAlloc(new SearchLog(this, nullptr, nullptr, nullptr, period));
}

SearchMonitor* Solver::MakeSearchLog(int period, IntVar* const var) {
  return RevAlloc(new SearchLog(this, nullptr, var, nullptr, period));
}

SearchMonitor* Solver::MakeSearchLog(int period,
                                     std::function<std::string()> display_callback) {
  return RevAlloc(new SearchLog(this, nullptr, nullptr,
                                std::move(display_callback), period));
}

SearchMonitor* Solver::MakeSearchLog(int period, IntVar* const var,
                                     std::function<std::string()> display_callback) {
  return RevAlloc(
      new SearchLog(this, nullptr, var, std::move(display_callback), period));
}

SearchMonitor* Solver::MakeSearchLog(int period, OptimizeVar* const obj) {
  return RevAlloc(new SearchLog(this, obj, nullptr, nullptr, period));
}

SearchMonitor* Solver::MakeSearchLog(int period, OptimizeVar* const obj,
                                     std::function<std::string()> display_callback) {
  return RevAlloc(
      new SearchLog(this, obj, nullptr, std::move(display_callback), period));
}


// ---------- Search Trace ----------
namespace {
class SearchTrace : public SearchMonitor {
 public:
  SearchTrace(Solver* const s, const std::string& prefix)
      : SearchMonitor(s), prefix_(prefix) {}
  ~SearchTrace() override {}

  void EnterSearch() override {
    LOG(INFO) << prefix_ << " EnterSearch(" << solver()->SolveDepth() << ")";
  }
  void RestartSearch() override {
    LOG(INFO) << prefix_ << " RestartSearch(" << solver()->SolveDepth() << ")";
  }
  void ExitSearch() override {
    LOG(INFO) << prefix_ << " ExitSearch(" << solver()->SolveDepth() << ")";
  }
  void BeginNextDecision(DecisionBuilder* const b) override {
    LOG(INFO) << prefix_ << " BeginNextDecision(" << b << ") ";
  }
  void EndNextDecision(DecisionBuilder* const b, Decision* const d) override {
    if (d) {
      LOG(INFO) << prefix_ << " EndNextDecision(" << b << ", " << d << ") ";
    } else {
      LOG(INFO) << prefix_ << " EndNextDecision(" << b << ") ";
    }
  }
  void ApplyDecision(Decision* const d) override {
    LOG(INFO) << prefix_ << " ApplyDecision(" << d << ") ";
  }
  void RefuteDecision(Decision* const d) override {
    LOG(INFO) << prefix_ << " RefuteDecision(" << d << ") ";
  }
  void AfterDecision(Decision* const d, bool apply) override {
    LOG(INFO) << prefix_ << " AfterDecision(" << d << ", " << apply << ") ";
  }
  void BeginFail() override {
    LOG(INFO) << prefix_ << " BeginFail(" << solver()->SearchDepth() << ")";
  }
  void EndFail() override {
    LOG(INFO) << prefix_ << " EndFail(" << solver()->SearchDepth() << ")";
  }
  void BeginInitialPropagation() override {
    LOG(INFO) << prefix_ << " BeginInitialPropagation()";
  }
  void EndInitialPropagation() override {
    LOG(INFO) << prefix_ << " EndInitialPropagation()";
  }
  bool AtSolution() override {
    LOG(INFO) << prefix_ << " AtSolution()";
    return false;
  }
  bool AcceptSolution() override {
    LOG(INFO) << prefix_ << " AcceptSolution()";
    return true;
  }
  void NoMoreSolutions() override {
    LOG(INFO) << prefix_ << " NoMoreSolutions()";
  }

  std::string DebugString() const override { return "SearchTrace"; }

 private:
  const std::string prefix_;
};
}  // namespace

SearchMonitor* Solver::MakeSearchTrace(const std::string& prefix) {
  return RevAlloc(new SearchTrace(this, prefix));
}

// ---------- Callback-based search monitors ----------
namespace {
class AtSolutionCallback : public SearchMonitor {
 public:
  AtSolutionCallback(Solver* const solver, std::function<void()> callback)
      : SearchMonitor(solver), callback_(std::move(callback)) {}
  ~AtSolutionCallback() override {}
  bool AtSolution() override;

 private:
  const std::function<void()> callback_;
};

bool AtSolutionCallback::AtSolution() {
  callback_();
  return false;
}

}  // namespace

SearchMonitor* Solver::MakeAtSolutionCallback(std::function<void()> callback) {
  return RevAlloc(new AtSolutionCallback(this, std::move(callback)));
}

namespace {
class EnterSearchCallback : public SearchMonitor {
 public:
  EnterSearchCallback(Solver* const solver, std::function<void()> callback)
      : SearchMonitor(solver), callback_(std::move(callback)) {}
  ~EnterSearchCallback() override {}
  void EnterSearch() override;

 private:
  const std::function<void()> callback_;
};

void EnterSearchCallback::EnterSearch() { callback_(); }

}  // namespace

SearchMonitor* Solver::MakeEnterSearchCallback(std::function<void()> callback) {
  return RevAlloc(new EnterSearchCallback(this, std::move(callback)));
}

namespace {
class ExitSearchCallback : public SearchMonitor {
 public:
  ExitSearchCallback(Solver* const solver, std::function<void()> callback)
      : SearchMonitor(solver), callback_(std::move(callback)) {}
  ~ExitSearchCallback() override {}
  void ExitSearch() override;

 private:
  const std::function<void()> callback_;
};

void ExitSearchCallback::ExitSearch() { callback_(); }

}  // namespace

SearchMonitor* Solver::MakeExitSearchCallback(std::function<void()> callback) {
  return RevAlloc(new ExitSearchCallback(this, std::move(callback)));
}

// ---------- Composite Decision Builder --------

namespace {
class CompositeDecisionBuilder : public DecisionBuilder {
 public:
  CompositeDecisionBuilder();
  explicit CompositeDecisionBuilder(const std::vector<DecisionBuilder*>& dbs);
  ~CompositeDecisionBuilder() override;
  void Add(DecisionBuilder* const db);
  void AppendMonitors(Solver* const solver,
                      std::vector<SearchMonitor*>* const monitors) override;
  void Accept(ModelVisitor* const visitor) const override;

 protected:
  std::vector<DecisionBuilder*> builders_;
};

CompositeDecisionBuilder::CompositeDecisionBuilder() {}

CompositeDecisionBuilder::CompositeDecisionBuilder(
    const std::vector<DecisionBuilder*>& dbs) {
  for (int i = 0; i < dbs.size(); ++i) {
    Add(dbs[i]);
  }
}

CompositeDecisionBuilder::~CompositeDecisionBuilder() {}

void CompositeDecisionBuilder::Add(DecisionBuilder* const db) {
  if (db != nullptr) {
    builders_.push_back(db);
  }
}

void CompositeDecisionBuilder::AppendMonitors(
    Solver* const solver, std::vector<SearchMonitor*>* const monitors) {
  for (DecisionBuilder* const db : builders_) {
    db->AppendMonitors(solver, monitors);
  }
}

void CompositeDecisionBuilder::Accept(ModelVisitor* const visitor) const {
  for (DecisionBuilder* const db : builders_) {
    db->Accept(visitor);
  }
}
}  // namespace

// ---------- Compose Decision Builder ----------

namespace {
class ComposeDecisionBuilder : public CompositeDecisionBuilder {
 public:
  ComposeDecisionBuilder();
  explicit ComposeDecisionBuilder(const std::vector<DecisionBuilder*>& dbs);
  ~ComposeDecisionBuilder() override;
  Decision* Next(Solver* const s) override;
  std::string DebugString() const override;

 private:
  int start_index_;
};

ComposeDecisionBuilder::ComposeDecisionBuilder() : start_index_(0) {}

ComposeDecisionBuilder::ComposeDecisionBuilder(
    const std::vector<DecisionBuilder*>& dbs)
    : CompositeDecisionBuilder(dbs), start_index_(0) {}

ComposeDecisionBuilder::~ComposeDecisionBuilder() {}

Decision* ComposeDecisionBuilder::Next(Solver* const s) {
  const int size = builders_.size();
  for (int i = start_index_; i < size; ++i) {
    Decision* d = builders_[i]->Next(s);
    if (d != nullptr) {
      s->SaveAndSetValue(&start_index_, i);
      return d;
    }
  }
  s->SaveAndSetValue(&start_index_, size);
  return nullptr;
}

std::string ComposeDecisionBuilder::DebugString() const {
  return StringPrintf("ComposeDecisionBuilder(%s)",
                      JoinDebugStringPtr(builders_, ", ").c_str());
}
}  // namespace

DecisionBuilder* Solver::Compose(DecisionBuilder* const db1,
                                 DecisionBuilder* const db2) {
  ComposeDecisionBuilder* c = RevAlloc(new ComposeDecisionBuilder());
  c->Add(db1);
  c->Add(db2);
  return c;
}

DecisionBuilder* Solver::Compose(DecisionBuilder* const db1,
                                 DecisionBuilder* const db2,
                                 DecisionBuilder* const db3) {
  ComposeDecisionBuilder* c = RevAlloc(new ComposeDecisionBuilder());
  c->Add(db1);
  c->Add(db2);
  c->Add(db3);
  return c;
}

DecisionBuilder* Solver::Compose(DecisionBuilder* const db1,
                                 DecisionBuilder* const db2,
                                 DecisionBuilder* const db3,
                                 DecisionBuilder* const db4) {
  ComposeDecisionBuilder* c = RevAlloc(new ComposeDecisionBuilder());
  c->Add(db1);
  c->Add(db2);
  c->Add(db3);
  c->Add(db4);
  return c;
}

DecisionBuilder* Solver::Compose(const std::vector<DecisionBuilder*>& dbs) {
  if (dbs.size() == 1) {
    return dbs[0];
  }
  return RevAlloc(new ComposeDecisionBuilder(dbs));
}

// ---------- ClosureDecision ---------

namespace {
class ClosureDecision : public Decision {
 public:
  ClosureDecision(Solver::Action apply, Solver::Action refute)
      : apply_(std::move(apply)), refute_(std::move(refute)) {}
  ~ClosureDecision() override {}

  void Apply(Solver* const s) override { apply_(s); }

  void Refute(Solver* const s) override { refute_(s); }

  std::string DebugString() const override { return "ClosureDecision"; }

 private:
  Solver::Action apply_;
  Solver::Action refute_;
};
}  // namespace

Decision* Solver::MakeDecision(Action apply, Action refute) {
  return RevAlloc(new ClosureDecision(std::move(apply), std::move(refute)));
}

// ---------- Try Decision Builder ----------

namespace {

class TryDecisionBuilder;

class TryDecision : public Decision {
 public:
  explicit TryDecision(TryDecisionBuilder* const try_builder);
  ~TryDecision() override;
  void Apply(Solver* const solver) override;
  void Refute(Solver* const solver) override;
  std::string DebugString() const override { return "TryDecision"; }

 private:
  TryDecisionBuilder* const try_builder_;
};

class TryDecisionBuilder : public CompositeDecisionBuilder {
 public:
  TryDecisionBuilder();
  explicit TryDecisionBuilder(const std::vector<DecisionBuilder*>& dbs);
  ~TryDecisionBuilder() override;
  Decision* Next(Solver* const s) override;
  std::string DebugString() const override;
  void AdvanceToNextBuilder(Solver* const solver);

 private:
  TryDecision try_decision_;
  int current_builder_;
  bool start_new_builder_;
};

TryDecision::TryDecision(TryDecisionBuilder* const try_builder)
    : try_builder_(try_builder) {}

TryDecision::~TryDecision() {}

void TryDecision::Apply(Solver* const solver) {}

void TryDecision::Refute(Solver* const solver) {
  try_builder_->AdvanceToNextBuilder(solver);
}

TryDecisionBuilder::TryDecisionBuilder()
    : CompositeDecisionBuilder(),
      try_decision_(this),
      current_builder_(-1),
      start_new_builder_(true) {}

TryDecisionBuilder::TryDecisionBuilder(const std::vector<DecisionBuilder*>& dbs)
    : CompositeDecisionBuilder(dbs),
      try_decision_(this),
      current_builder_(-1),
      start_new_builder_(true) {}

TryDecisionBuilder::~TryDecisionBuilder() {}

Decision* TryDecisionBuilder::Next(Solver* const solver) {
  if (current_builder_ < 0) {
    solver->SaveAndSetValue(&current_builder_, 0);
    start_new_builder_ = true;
  }
  if (start_new_builder_) {
    start_new_builder_ = false;
    return &try_decision_;
  } else {
    return builders_[current_builder_]->Next(solver);
  }
}

std::string TryDecisionBuilder::DebugString() const {
  return StringPrintf("TryDecisionBuilder(%s)",
                      JoinDebugStringPtr(builders_, ", ").c_str());
}

void TryDecisionBuilder::AdvanceToNextBuilder(Solver* const solver) {
  ++current_builder_;
  start_new_builder_ = true;
  if (current_builder_ >= builders_.size()) {
    solver->Fail();
  }
}

}  // namespace

DecisionBuilder* Solver::Try(DecisionBuilder* const db1,
                             DecisionBuilder* const db2) {
  TryDecisionBuilder* try_db = RevAlloc(new TryDecisionBuilder());
  try_db->Add(db1);
  try_db->Add(db2);
  return try_db;
}

DecisionBuilder* Solver::Try(DecisionBuilder* const db1,
                             DecisionBuilder* const db2,
                             DecisionBuilder* const db3) {
  TryDecisionBuilder* try_db = RevAlloc(new TryDecisionBuilder());
  try_db->Add(db1);
  try_db->Add(db2);
  try_db->Add(db3);
  return try_db;
}

DecisionBuilder* Solver::Try(DecisionBuilder* const db1,
                             DecisionBuilder* const db2,
                             DecisionBuilder* const db3,
                             DecisionBuilder* const db4) {
  TryDecisionBuilder* try_db = RevAlloc(new TryDecisionBuilder());
  try_db->Add(db1);
  try_db->Add(db2);
  try_db->Add(db3);
  try_db->Add(db4);
  return try_db;
}

DecisionBuilder* Solver::Try(const std::vector<DecisionBuilder*>& dbs) {
  return RevAlloc(new TryDecisionBuilder(dbs));
}

// ----------  Variable Assignments ----------

// ----- BaseAssignmentSelector -----

namespace {
class BaseVariableAssignmentSelector : public BaseObject {
 public:
  BaseVariableAssignmentSelector(Solver* solver,
                                 const std::vector<IntVar*>& vars)
      : solver_(solver),
        vars_(vars),
        first_unbound_(0),
        last_unbound_(vars.size() - 1) {}

  ~BaseVariableAssignmentSelector() override {}

  virtual int64 SelectValue(const IntVar* v, int64 id) = 0;

  // Returns -1 if no variable are suitable.
  virtual int64 ChooseVariable() = 0;

  int64 ChooseVariableWrapper() {
    int64 i;
    for (i = first_unbound_.Value(); i <= last_unbound_.Value(); ++i) {
      if (!vars_[i]->Bound()) {
        break;
      }
    }
    first_unbound_.SetValue(solver_, i);
    if (i > last_unbound_.Value()) {
      return -1;
    }
    for (i = last_unbound_.Value(); i >= first_unbound_.Value(); --i) {
      if (!vars_[i]->Bound()) {
        break;
      }
    }
    last_unbound_.SetValue(solver_, i);
    return ChooseVariable();
  }

  void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitExtension(ModelVisitor::kVariableGroupExtension);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->EndVisitExtension(ModelVisitor::kVariableGroupExtension);
  }

  const std::vector<IntVar*>& vars() const { return vars_; }

 protected:
  Solver* const solver_;
  std::vector<IntVar*> vars_;
  Rev<int64> first_unbound_;
  Rev<int64> last_unbound_;
};

// ----- Choose first unbound --

int64 ChooseFirstUnbound(Solver* solver, const std::vector<IntVar*>& vars,
                         int64 first_unbound, int64 last_unbound) {
  for (int64 i = first_unbound; i <= last_unbound; ++i) {
    if (!vars[i]->Bound()) {
      return i;
    }
  }
  return -1;
}

// ----- Choose Min Size Lowest Min -----

int64 ChooseMinSizeLowestMin(Solver* solver, const std::vector<IntVar*>& vars,
                             int64 first_unbound, int64 last_unbound) {
  uint64 best_size = kuint64max;
  int64 best_min = kint64max;
  int64 best_index = -1;
  for (int64 i = first_unbound; i <= last_unbound; ++i) {
    IntVar* const var = vars[i];
    if (!var->Bound()) {
      if (var->Size() < best_size ||
          (var->Size() == best_size && var->Min() < best_min)) {
        best_size = var->Size();
        best_min = var->Min();
        best_index = i;
      }
    }
  }
  return best_index;
}

// ----- Choose Min Size Highest Min -----

int64 ChooseMinSizeHighestMin(Solver* solver, const std::vector<IntVar*>& vars,
                              int64 first_unbound, int64 last_unbound) {
  uint64 best_size = kuint64max;
  int64 best_min = kint64min;
  int64 best_index = -1;
  for (int64 i = first_unbound; i <= last_unbound; ++i) {
    IntVar* const var = vars[i];
    if (!var->Bound()) {
      if (var->Size() < best_size ||
          (var->Size() == best_size && var->Min() > best_min)) {
        best_size = var->Size();
        best_min = var->Min();
        best_index = i;
      }
    }
  }
  return best_index;
}

// ----- Choose Min Size Lowest Max -----

int64 ChooseMinSizeLowestMax(Solver* solver, const std::vector<IntVar*>& vars,
                             int64 first_unbound, int64 last_unbound) {
  uint64 best_size = kuint64max;
  int64 best_max = kint64max;
  int64 best_index = -1;
  for (int64 i = first_unbound; i <= last_unbound; ++i) {
    IntVar* const var = vars[i];
    if (!var->Bound()) {
      if (var->Size() < best_size ||
          (var->Size() == best_size && var->Max() < best_max)) {
        best_size = var->Size();
        best_max = var->Max();
        best_index = i;
      }
    }
  }
  return best_index;
}

// ----- Choose Min Size Highest Max -----

int64 ChooseMinSizeHighestMax(Solver* solver, const std::vector<IntVar*>& vars,
                              int64 first_unbound, int64 last_unbound) {
  uint64 best_size = kuint64max;
  int64 best_max = kint64min;
  int64 best_index = -1;
  for (int64 i = first_unbound; i <= last_unbound; ++i) {
    IntVar* const var = vars[i];
    if (!var->Bound()) {
      if (var->Size() < best_size ||
          (var->Size() == best_size && var->Max() > best_max)) {
        best_size = var->Size();
        best_max = var->Max();
        best_index = i;
      }
    }
  }
  return best_index;
}

// ----- Choose Lowest Min --

int64 ChooseLowestMin(Solver* solver, const std::vector<IntVar*>& vars,
                      int64 first_unbound, int64 last_unbound) {
  int64 best_min = kint64max;
  int64 best_index = -1;
  for (int64 i = first_unbound; i <= last_unbound; ++i) {
    IntVar* const var = vars[i];
    if (!var->Bound()) {
      if (var->Min() < best_min) {
        best_min = var->Min();
        best_index = i;
      }
    }
  }
  return best_index;
}

// ----- Choose Highest Max -----

int64 ChooseHighestMax(Solver* solver, const std::vector<IntVar*>& vars,
                       int64 first_unbound, int64 last_unbound) {
  int64 best_max = kint64min;
  int64 best_index = -1;
  for (int64 i = first_unbound; i <= last_unbound; ++i) {
    IntVar* const var = vars[i];
    if (!var->Bound()) {
      if (var->Max() < best_max) {
        best_max = var->Max();
        best_index = i;
      }
    }
  }
  return best_index;
}

// ----- Choose Lowest Size --

int64 ChooseMinSize(Solver* solver, const std::vector<IntVar*>& vars,
                    int64 first_unbound, int64 last_unbound) {
  uint64 best_size = kuint64max;
  int64 best_index = -1;
  for (int64 i = first_unbound; i <= last_unbound; ++i) {
    IntVar* const var = vars[i];
    if (!var->Bound()) {
      if (var->Size() < best_size) {
        best_size = var->Size();
        best_index = i;
      }
    }
  }
  return best_index;
}

// ----- Choose Highest Size -----

int64 ChooseMaxSize(Solver* solver, const std::vector<IntVar*>& vars,
                    int64 first_unbound, int64 last_unbound) {
  uint64 best_size = 0;
  int64 best_index = -1;
  for (int64 i = first_unbound; i <= last_unbound; ++i) {
    IntVar* const var = vars[i];
    if (!var->Bound()) {
      if (var->Size() > best_size) {
        best_size = var->Size();
        best_index = i;
      }
    }
  }
  return best_index;
}

// ----- Choose Highest Regret -----

class HighestRegretSelectorOnMin : public BaseObject {
 public:
  explicit HighestRegretSelectorOnMin(const std::vector<IntVar*>& vars)
      : iterators_(vars.size()) {
    for (int64 i = 0; i < vars.size(); ++i) {
      iterators_[i] = vars[i]->MakeDomainIterator(true);
    }
  }
  ~HighestRegretSelectorOnMin() override {}
  int64 Choose(Solver* const s, const std::vector<IntVar*>& vars,
               int64 first_unbound, int64 last_unbound);
  std::string DebugString() const override { return "MaxRegretSelector"; }

  int64 ComputeRegret(IntVar* var, int64 index) const {
    DCHECK(!var->Bound());
    const int64 vmin = var->Min();
    IntVarIterator* const iterator = iterators_[index];
    iterator->Init();
    iterator->Next();
    return iterator->Value() - vmin;
  }

 private:
  std::vector<IntVarIterator*> iterators_;
};

int64 HighestRegretSelectorOnMin::Choose(Solver* const s,
                                         const std::vector<IntVar*>& vars,
                                         int64 first_unbound,
                                         int64 last_unbound) {
  int64 best_regret = 0;
  int64 index = -1;
  for (int64 i = first_unbound; i <= last_unbound; ++i) {
    IntVar* const var = vars[i];
    if (!var->Bound()) {
      const int64 regret = ComputeRegret(var, i);
      if (regret > best_regret) {
        best_regret = regret;
        index = i;
      }
    }
  }
  return index;
}

// ----- Choose random unbound --

int64 ChooseRandom(Solver* solver, const std::vector<IntVar*>& vars,
                   int64 first_unbound, int64 last_unbound) {
  const int64 span = last_unbound - first_unbound + 1;
  const int64 shift = solver->Rand32(span);
  for (int64 i = 0; i < span; ++i) {
    const int64 index = (i + shift) % span + first_unbound;
    if (!vars[index]->Bound()) {
      return index;
    }
  }
  return -1;
}

// ----- Choose min eval -----

class CheapestVarSelector : public BaseObject {
 public:
  explicit CheapestVarSelector(std::function<int64(int64)> var_evaluator)
      : var_evaluator_(std::move(var_evaluator)) {}
  ~CheapestVarSelector() override {}
  int64 Choose(Solver* const s, const std::vector<IntVar*>& vars,
               int64 first_unbound, int64 last_unbound);
  std::string DebugString() const override { return "CheapestVarSelector"; }

 private:
  std::function<int64(int64)> var_evaluator_;
};

int64 CheapestVarSelector::Choose(Solver* const s,
                                  const std::vector<IntVar*>& vars,
                                  int64 first_unbound, int64 last_unbound) {
  int64 best_eval = kint64max;
  int64 index = -1;
  for (int64 i = first_unbound; i <= last_unbound; ++i) {
    if (!vars[i]->Bound()) {
      const int64 eval = var_evaluator_(i);
      if (eval < best_eval) {
        best_eval = eval;
        index = i;
      }
    }
  }
  return index;
}

// ----- Path selector -----
// Follow path, where var[i] is represents the next of i

class PathSelector : public BaseObject {
 public:
  PathSelector() : first_(kint64max) {}
  ~PathSelector() override {}
  int64 Choose(Solver* const s, const std::vector<IntVar*>& vars,
               int64 first_unbound, int64 last_unbound);
  std::string DebugString() const override { return "ChooseNextOnPath"; }

 private:
  bool UpdateIndex(const std::vector<IntVar*>& vars, int64* index) const;
  bool FindPathStart(const std::vector<IntVar*>& vars, int64* index) const;

  Rev<int64> first_;
};

int64 PathSelector::Choose(Solver* const s, const std::vector<IntVar*>& vars,
                           int64 first_unbound, int64 last_unbound) {
  int64 index = first_.Value();
  if (!UpdateIndex(vars, &index)) {
    return -1;
  }
  int64 count = 0;
  while (vars[index]->Bound()) {
    index = vars[index]->Value();
    if (!UpdateIndex(vars, &index)) {
      return -1;
    }
    ++count;
    if (count >= vars.size() &&
        !FindPathStart(vars, &index)) {  // Cycle detected
      return -1;
    }
  }
  first_.SetValue(s, index);
  return index;
}

bool PathSelector::UpdateIndex(const std::vector<IntVar*>& vars,
                               int64* index) const {
  if (*index >= vars.size()) {
    if (!FindPathStart(vars, index)) {
      return false;
    }
  }
  return true;
}

// Select variables on a path:
//  1. Try to extend an existing route: look for an unbound variable, to which
//     some other variable points.
//  2. If no such road is found, try to find a start node of a route: look for
//     an unbound variable, to which no other variable can point.
//  3. If everything else fails, pick the first unbound variable.
bool PathSelector::FindPathStart(const std::vector<IntVar*>& vars,
                                 int64* index) const {
  // Try to extend an existing path
  for (int64 i = vars.size() - 1; i >= 0; --i) {
    if (vars[i]->Bound()) {
      const int64 next = vars[i]->Value();
      if (next < vars.size() && !vars[next]->Bound()) {
        *index = next;
        return true;
      }
    }
  }
  // Pick path start
  for (int64 i = vars.size() - 1; i >= 0; --i) {
    if (!vars[i]->Bound()) {
      bool has_possible_prev = false;
      for (int64 j = 0; j < vars.size(); ++j) {
        if (vars[j]->Contains(i)) {
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
  for (int64 i = 0; i < vars.size(); ++i) {
    if (!vars[i]->Bound()) {
      *index = i;
      return true;
    }
  }
  return false;
}

// ----- Select min -----

int64 SelectMinValue(const IntVar* v, int64 id) { return v->Min(); }

// ----- Select max -----

int64 SelectMaxValue(const IntVar* v, int64 id) { return v->Max(); }

// ----- Select random -----

int64 SelectRandomValue(const IntVar* v, int64 id) {
  const uint64 span = v->Max() - v->Min() + 1;
  if (span > FLAGS_cp_large_domain_no_splitting_limit) {
    // Do not create holes in large domains.
    return v->Min();
  }
  const uint64 size = v->Size();
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

int64 SelectCenterValue(const IntVar* v, int64 id) {
  const int64 vmin = v->Min();
  const int64 vmax = v->Max();
  if (vmax - vmin > FLAGS_cp_large_domain_no_splitting_limit) {
    // Do not create holes in large domains.
    return vmin;
  }
  const int64 mid = (vmin + vmax) / 2;
  if (v->Contains(mid)) {
    return mid;
  }
  const int64 diameter = vmax - mid;  // always greater than mid - vmix.
  for (int64 i = 1; i <= diameter; ++i) {
    if (v->Contains(mid - i)) {
      return mid - i;
    }
    if (v->Contains(mid + i)) {
      return mid + i;
    }
  }
  return 0LL;
}

// ----- Select center -----

int64 SelectSplitValue(const IntVar* v, int64 id) {
  const int64 vmin = v->Min();
  const int64 vmax = v->Max();
  const uint64 delta = vmax - vmin;
  const int64 mid = vmin + delta / 2;
  return mid;
}

// ----- Select the value yielding the cheapest "eval" for a var -----

class CheapestValueSelector : public BaseObject {
 public:
  CheapestValueSelector(std::function<int64(int64, int64)> eval,
                        std::function<int64(int64)> tie_breaker)
      : eval_(std::move(eval)), tie_breaker_(std::move(tie_breaker)) {}
  ~CheapestValueSelector() override {}
  int64 Select(const IntVar* v, int64 id);
  std::string DebugString() const override { return "CheapestValue"; }

 private:
  std::function<int64(int64, int64)> eval_;
  std::function<int64(int64)> tie_breaker_;
  std::vector<int64> cache_;
};

int64 CheapestValueSelector::Select(const IntVar* v, int64 id) {
  cache_.clear();
  int64 best = kint64max;
  std::unique_ptr<IntVarIterator> it(v->MakeDomainIterator(false));
  for (const int64 i : InitAndGetValues(it.get())) {
    int64 eval = eval_(id, i);
    if (eval < best) {
      best = eval;
      cache_.clear();
      cache_.push_back(i);
    } else if (eval == best) {
      cache_.push_back(i);
    }
  }
  DCHECK_GT(cache_.size(), 0);
  if (tie_breaker_ == nullptr || cache_.size() == 1) {
    return cache_.back();
  } else {
    return cache_[tie_breaker_(cache_.size())];
  }
}

// ----- Select the best value for the var, based on a comparator callback -----

// The comparator should be a total order, but does not have to be a strict
// ordering. If there is a tie between two values val1 and val2, i.e. if
// !comparator(var_id, val1, val2) && !comparator(var_id, val2, val1), then
// the lowest value wins.
// comparator(var_id, val1, val2) == true means than val1 should be preferred
// over val2 for variable var_id.
class BestValueByComparisonSelector : public BaseObject {
 public:
  explicit BestValueByComparisonSelector(
      Solver::VariableValueComparator comparator)
      : comparator_(std::move(comparator)) {}
  ~BestValueByComparisonSelector() override {}
  int64 Select(const IntVar* v, int64 id);
  std::string DebugString() const override {
    return "BestValueByComparisonSelector";
  }

 private:
  Solver::VariableValueComparator comparator_;
};

int64 BestValueByComparisonSelector::Select(const IntVar* v, int64 id) {
  std::unique_ptr<IntVarIterator> it(v->MakeDomainIterator(false));
  it->Init();
  DCHECK(it->Ok());  // At least one value.
  int64 best_value = it->Value();
  for (it->Next(); it->Ok(); it->Next()) {
    const int64 candidate_value = it->Value();
    if (comparator_(id, candidate_value, best_value)) {
      best_value = candidate_value;
    }
  }
  return best_value;
}

// ----- VariableAssignmentSelector -----

class VariableAssignmentSelector : public BaseVariableAssignmentSelector {
 public:
  VariableAssignmentSelector(Solver* solver, const std::vector<IntVar*>& vars,
                             Solver::VariableIndexSelector var_selector,
                             Solver::VariableValueSelector value_selector,
                             const std::string& name)
      : BaseVariableAssignmentSelector(solver, vars),
        var_selector_(std::move(var_selector)),
        value_selector_(std::move(value_selector)),
        name_(name) {}
  ~VariableAssignmentSelector() override {}
  int64 SelectValue(const IntVar* var, int64 id) override {
    return value_selector_(var, id);
  }
  int64 ChooseVariable() override {
    return var_selector_(solver_, vars_, first_unbound_.Value(),
                         last_unbound_.Value());
  }
  std::string DebugString() const override;

 private:
  Solver::VariableIndexSelector var_selector_;
  Solver::VariableValueSelector value_selector_;
  const std::string name_;
};

std::string VariableAssignmentSelector::DebugString() const {
  return StringPrintf("%s(%s)", name_.c_str(),
                      JoinDebugStringPtr(vars_, ", ").c_str());
}

// ----- Base Global Evaluator-based selector -----

class BaseEvaluatorSelector : public BaseVariableAssignmentSelector {
 public:
  BaseEvaluatorSelector(Solver* solver, const std::vector<IntVar*>& vars,
                        std::function<int64(int64, int64)> evaluator);
  ~BaseEvaluatorSelector() override {}

 protected:
  struct Element {
    Element() : var(0), value(0) {}
    Element(int64 i, int64 j) : var(i), value(j) {}
    int64 var;
    int64 value;
  };

  std::string DebugStringInternal(const std::string& name) const {
    return StringPrintf("%s(%s)", name.c_str(),
                        JoinDebugStringPtr(vars_, ", ").c_str());
  }

  std::function<int64(int64, int64)> evaluator_;
};

BaseEvaluatorSelector::BaseEvaluatorSelector(
    Solver* solver, const std::vector<IntVar*>& vars,
    std::function<int64(int64, int64)> evaluator)
    : BaseVariableAssignmentSelector(solver, vars),
      evaluator_(std::move(evaluator)) {}

// ----- Global Dynamic Evaluator-based selector -----

class DynamicEvaluatorSelector : public BaseEvaluatorSelector {
 public:
  DynamicEvaluatorSelector(Solver* solver, const std::vector<IntVar*>& vars,
                           std::function<int64(int64, int64)> evaluator,
                           std::function<int64(int64)> tie_breaker);
  ~DynamicEvaluatorSelector() override {}
  int64 SelectValue(const IntVar* var, int64 id) override;
  int64 ChooseVariable() override;
  std::string DebugString() const override;

 private:
  int64 first_;
  std::function<int64(int64)> tie_breaker_;
  std::vector<Element> cache_;
};

DynamicEvaluatorSelector::DynamicEvaluatorSelector(
    Solver* solver, const std::vector<IntVar*>& vars,
    std::function<int64(int64, int64)> evaluator,
    std::function<int64(int64)> tie_breaker)
    : BaseEvaluatorSelector(solver, vars, std::move(evaluator)),
      first_(-1),
      tie_breaker_(std::move(tie_breaker)) {}

int64 DynamicEvaluatorSelector::SelectValue(const IntVar* var, int64 id) {
  return cache_[first_].value;
}

int64 DynamicEvaluatorSelector::ChooseVariable() {
  int64 best_evaluation = kint64max;
  cache_.clear();
  for (int64 i = 0; i < vars_.size(); ++i) {
    const IntVar* const var = vars_[i];
    if (!var->Bound()) {
      std::unique_ptr<IntVarIterator> it(var->MakeDomainIterator(false));
      for (const int64 j : InitAndGetValues(it.get())) {
        const int64 value = evaluator_(i, j);
        if (value < best_evaluation) {
          best_evaluation = value;
          cache_.clear();
          cache_.push_back(Element(i, j));
        } else if (value == best_evaluation && tie_breaker_) {
          cache_.push_back(Element(i, j));
        }
      }
    }
  }

  if (cache_.empty()) {
    return -1;
  }

  if (tie_breaker_ == nullptr || cache_.size() == 1) {
    first_ = 0;
    return cache_.front().var;
  } else {
    first_ = tie_breaker_(cache_.size());
    return cache_[first_].var;
  }
}

std::string DynamicEvaluatorSelector::DebugString() const {
  return DebugStringInternal("AssignVariablesOnDynamicEvaluator");
}

// ----- Global Dynamic Evaluator-based selector -----

class StaticEvaluatorSelector : public BaseEvaluatorSelector {
 public:
  StaticEvaluatorSelector(Solver* solver, const std::vector<IntVar*>& vars,
                          const std::function<int64(int64, int64)>& evaluator);
  ~StaticEvaluatorSelector() override {}
  int64 SelectValue(const IntVar* var, int64 id) override;
  int64 ChooseVariable() override;
  std::string DebugString() const override;

 private:
  class Compare {
   public:
    explicit Compare(std::function<int64(int64, int64)> evaluator)
        : evaluator_(std::move(evaluator)) {}
    bool operator()(const Element& lhs, const Element& rhs) const {
      const int64 value_lhs = Value(lhs);
      const int64 value_rhs = Value(rhs);
      return value_lhs < value_rhs ||
             (value_lhs == value_rhs &&
              (lhs.var < rhs.var ||
               (lhs.var == rhs.var && lhs.value < rhs.value)));
    }
    int64 Value(const Element& element) const {
      return evaluator_(element.var, element.value);
    }

   private:
    std::function<int64(int64, int64)> evaluator_;
  };

  Compare comp_;
  std::vector<Element> elements_;
  int64 first_;
};

StaticEvaluatorSelector::StaticEvaluatorSelector(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::function<int64(int64, int64)>& evaluator)
    : BaseEvaluatorSelector(solver, vars, evaluator),
      comp_(evaluator),
      first_(-1) {}

int64 StaticEvaluatorSelector::SelectValue(const IntVar* var, int64 id) {
  return elements_[first_].value;
}

int64 StaticEvaluatorSelector::ChooseVariable() {
  if (first_ == -1) {  // first call to select. update assignment costs
    // Two phases: compute size then filland sort
    int64 element_size = 0;
    for (int64 i = 0; i < vars_.size(); ++i) {
      if (!vars_[i]->Bound()) {
        element_size += vars_[i]->Size();
      }
    }
    elements_.resize(element_size);
    int count = 0;
    for (int i = 0; i < vars_.size(); ++i) {
      const IntVar* const var = vars_[i];
      if (!var->Bound()) {
        std::unique_ptr<IntVarIterator> it(var->MakeDomainIterator(false));
        for (const int64 value : InitAndGetValues(it.get())) {
          elements_[count++] = Element(i, value);
        }
      }
    }
    // Sort is stable here given the tie-breaking rules in comp_.
    std::sort(elements_.begin(), elements_.end(), comp_);
    solver_->SaveAndSetValue(&first_, 0LL);
  }
  for (int64 i = first_; i < elements_.size(); ++i) {
    const Element& element = elements_[i];
    IntVar* const var = vars_[element.var];
    if (!var->Bound() && var->Contains(element.value)) {
      solver_->SaveAndSetValue(&first_, i);
      return element.var;
    }
  }
  solver_->SaveAndSetValue(&first_, static_cast<int64>(elements_.size()));
  return -1;
}

std::string StaticEvaluatorSelector::DebugString() const {
  return DebugStringInternal("AssignVariablesOnStaticEvaluator");
}

// ----- AssignOneVariableValue decision -----

class AssignOneVariableValue : public Decision {
 public:
  AssignOneVariableValue(IntVar* const v, int64 val);
  ~AssignOneVariableValue() override {}
  void Apply(Solver* const s) override;
  void Refute(Solver* const s) override;
  std::string DebugString() const override;
  void Accept(DecisionVisitor* const visitor) const override {
    visitor->VisitSetVariableValue(var_, value_);
  }

 private:
  IntVar* const var_;
  int64 value_;
};

AssignOneVariableValue::AssignOneVariableValue(IntVar* const v, int64 val)
    : var_(v), value_(val) {}

std::string AssignOneVariableValue::DebugString() const {
  return StringPrintf("[%s == %" GG_LL_FORMAT "d]", var_->DebugString().c_str(),
                      value_);
}

void AssignOneVariableValue::Apply(Solver* const s) { var_->SetValue(value_); }

void AssignOneVariableValue::Refute(Solver* const s) {
  var_->RemoveValue(value_);
}
}  // namespace

Decision* Solver::MakeAssignVariableValue(IntVar* const v, int64 val) {
  return RevAlloc(new AssignOneVariableValue(v, val));
}

// ----- AssignOneVariableValueOrFail decision -----

namespace {
class AssignOneVariableValueOrFail : public Decision {
 public:
  AssignOneVariableValueOrFail(IntVar* const v, int64 value);
  ~AssignOneVariableValueOrFail() override {}
  void Apply(Solver* const s) override;
  void Refute(Solver* const s) override;
  std::string DebugString() const override;
  void Accept(DecisionVisitor* const visitor) const override {
    visitor->VisitSetVariableValue(var_, value_);
  }

 private:
  IntVar* const var_;
  const int64 value_;
};

AssignOneVariableValueOrFail::AssignOneVariableValueOrFail(IntVar* const v,
                                                           int64 value)
    : var_(v), value_(value) {}

std::string AssignOneVariableValueOrFail::DebugString() const {
  return StringPrintf("[%s == %" GG_LL_FORMAT "d]", var_->DebugString().c_str(),
                      value_);
}

void AssignOneVariableValueOrFail::Apply(Solver* const s) {
  var_->SetValue(value_);
}

void AssignOneVariableValueOrFail::Refute(Solver* const s) { s->Fail(); }
}  // namespace

Decision* Solver::MakeAssignVariableValueOrFail(IntVar* const v, int64 value) {
  return RevAlloc(new AssignOneVariableValueOrFail(v, value));
}

// ----- AssignOneVariableValue decision -----

namespace {
class SplitOneVariable : public Decision {
 public:
  SplitOneVariable(IntVar* const v, int64 val, bool start_with_lower_half);
  ~SplitOneVariable() override {}
  void Apply(Solver* const s) override;
  void Refute(Solver* const s) override;
  std::string DebugString() const override;
  void Accept(DecisionVisitor* const visitor) const override {
    visitor->VisitSplitVariableDomain(var_, value_, start_with_lower_half_);
  }

 private:
  IntVar* const var_;
  const int64 value_;
  const bool start_with_lower_half_;
};

SplitOneVariable::SplitOneVariable(IntVar* const v, int64 val,
                                   bool start_with_lower_half)
    : var_(v), value_(val), start_with_lower_half_(start_with_lower_half) {}

std::string SplitOneVariable::DebugString() const {
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
    var_->SetMin(value_ + 1);
  }
}

void SplitOneVariable::Refute(Solver* const s) {
  if (start_with_lower_half_) {
    var_->SetMin(value_ + 1);
  } else {
    var_->SetMax(value_);
  }
}
}  // namespace

Decision* Solver::MakeSplitVariableDomain(IntVar* const v, int64 val,
                                          bool start_with_lower_half) {
  return RevAlloc(new SplitOneVariable(v, val, start_with_lower_half));
}

Decision* Solver::MakeVariableLessOrEqualValue(IntVar* const var, int64 value) {
  return MakeSplitVariableDomain(var, value, true);
}

Decision* Solver::MakeVariableGreaterOrEqualValue(IntVar* const var,
                                                  int64 value) {
  return MakeSplitVariableDomain(var, value, false);
}

// ----- AssignVariablesValues decision -----

namespace {
class AssignVariablesValues : public Decision {
 public:
  AssignVariablesValues(const std::vector<IntVar*>& vars,
                        const std::vector<int64>& values);
  ~AssignVariablesValues() override {}
  void Apply(Solver* const s) override;
  void Refute(Solver* const s) override;
  std::string DebugString() const override;
  void Accept(DecisionVisitor* const visitor) const override {
    for (int i = 0; i < vars_.size(); ++i) {
      visitor->VisitSetVariableValue(vars_[i], values_[i]);
    }
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitExtension(ModelVisitor::kVariableGroupExtension);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->EndVisitExtension(ModelVisitor::kVariableGroupExtension);
  }

 private:
  const std::vector<IntVar*> vars_;
  const std::vector<int64> values_;
};

AssignVariablesValues::AssignVariablesValues(const std::vector<IntVar*>& vars,
                                             const std::vector<int64>& values)
    : vars_(vars), values_(values) {}

std::string AssignVariablesValues::DebugString() const {
  std::string out;
  for (int i = 0; i < vars_.size(); ++i) {
    StringAppendF(&out, "[%s == %" GG_LL_FORMAT "d]",
                  vars_[i]->DebugString().c_str(), values_[i]);
  }
  return out;
}

void AssignVariablesValues::Apply(Solver* const s) {
  for (int i = 0; i < vars_.size(); ++i) {
    vars_[i]->SetValue(values_[i]);
  }
}

void AssignVariablesValues::Refute(Solver* const s) {
  std::vector<IntVar*> terms;
  for (int i = 0; i < vars_.size(); ++i) {
    IntVar* term = s->MakeBoolVar();
    s->MakeIsDifferentCstCt(vars_[i], values_[i], term);
    terms.push_back(term);
  }
  s->AddConstraint(s->MakeSumGreaterOrEqual(terms, 1));
}
}  // namespace

Decision* Solver::MakeAssignVariablesValues(const std::vector<IntVar*>& vars,
                                            const std::vector<int64>& values) {
  CHECK_EQ(vars.size(), values.size());
  return RevAlloc(new AssignVariablesValues(vars, values));
}

// ----- AssignAllVariables -----

namespace {
class BaseAssignVariables : public DecisionBuilder {
 public:
  enum Mode {
    ASSIGN,
    SPLIT_LOWER,
    SPLIT_UPPER,
  };

  BaseAssignVariables(BaseVariableAssignmentSelector* const selector, Mode mode)
      : selector_(selector), mode_(mode) {}

  ~BaseAssignVariables() override;
  Decision* Next(Solver* const s) override;
  std::string DebugString() const override;
  static BaseAssignVariables* MakePhase(
      Solver* const s, const std::vector<IntVar*>& vars,
      Solver::VariableIndexSelector var_selector,
      Solver::VariableValueSelector value_selector,
      const std::string& value_selector_name, BaseAssignVariables::Mode mode);

  static Solver::VariableIndexSelector MakeVariableSelector(
      Solver* const s, const std::vector<IntVar*>& vars,
      Solver::IntVarStrategy str) {
    switch (str) {
      case Solver::INT_VAR_DEFAULT:
      case Solver::INT_VAR_SIMPLE:
      case Solver::CHOOSE_FIRST_UNBOUND:
        return ChooseFirstUnbound;
      case Solver::CHOOSE_RANDOM:
        return ChooseRandom;
      case Solver::CHOOSE_MIN_SIZE_LOWEST_MIN:
        return ChooseMinSizeLowestMin;
      case Solver::CHOOSE_MIN_SIZE_HIGHEST_MIN:
        return ChooseMinSizeHighestMin;
      case Solver::CHOOSE_MIN_SIZE_LOWEST_MAX:
        return ChooseMinSizeLowestMax;
      case Solver::CHOOSE_MIN_SIZE_HIGHEST_MAX:
        return ChooseMinSizeHighestMax;
      case Solver::CHOOSE_LOWEST_MIN:
        return ChooseLowestMin;
      case Solver::CHOOSE_HIGHEST_MAX:
        return ChooseHighestMax;
      case Solver::CHOOSE_MIN_SIZE:
        return ChooseMinSize;
      case Solver::CHOOSE_MAX_SIZE:
        return ChooseMaxSize;
      case Solver::CHOOSE_MAX_REGRET_ON_MIN: {
        HighestRegretSelectorOnMin* const selector =
            s->RevAlloc(new HighestRegretSelectorOnMin(vars));
        return [selector](Solver* solver, const std::vector<IntVar*>& vars,
                          int first_unbound, int last_unbound) {
          return selector->Choose(solver, vars, first_unbound, last_unbound);
        };
      }
      case Solver::CHOOSE_PATH: {
        PathSelector* const selector = s->RevAlloc(new PathSelector());
        return [selector](Solver* solver, const std::vector<IntVar*>& vars,
                          int first_unbound, int last_unbound) {
          return selector->Choose(solver, vars, first_unbound, last_unbound);
        };
      }
      default:
        LOG(FATAL) << "Unknown int var strategy " << str;
        return nullptr;
    }
  }

  static Solver::VariableValueSelector MakeValueSelector(
      Solver* const s, Solver::IntValueStrategy val_str) {
    switch (val_str) {
      case Solver::INT_VALUE_DEFAULT:
      case Solver::INT_VALUE_SIMPLE:
      case Solver::ASSIGN_MIN_VALUE:
        return SelectMinValue;
      case Solver::ASSIGN_MAX_VALUE:
        return SelectMaxValue;
      case Solver::ASSIGN_RANDOM_VALUE:
        return SelectRandomValue;
      case Solver::ASSIGN_CENTER_VALUE:
        return SelectCenterValue;
      case Solver::SPLIT_LOWER_HALF:
        return SelectSplitValue;
      case Solver::SPLIT_UPPER_HALF:
        return SelectSplitValue;
      default:
        LOG(FATAL) << "Unknown int value strategy " << val_str;
        return nullptr;
    }
  }

  void Accept(ModelVisitor* const visitor) const override {
    selector_->Accept(visitor);
  }

 protected:
  BaseVariableAssignmentSelector* const selector_;
  const Mode mode_;
};

BaseAssignVariables::~BaseAssignVariables() {}

Decision* BaseAssignVariables::Next(Solver* const s) {
  const std::vector<IntVar*>& vars = selector_->vars();
  int id = selector_->ChooseVariableWrapper();
  if (id >= 0 && id < vars.size()) {
    IntVar* const var = vars[id];
    const int64 value = selector_->SelectValue(var, id);
    switch (mode_) {
      case ASSIGN:
        return s->RevAlloc(new AssignOneVariableValue(var, value));
      case SPLIT_LOWER:
        return s->RevAlloc(new SplitOneVariable(var, value, true));
      case SPLIT_UPPER:
        return s->RevAlloc(new SplitOneVariable(var, value, false));
    }
  }
  return nullptr;
}

std::string BaseAssignVariables::DebugString() const {
  return selector_->DebugString();
}

BaseAssignVariables* BaseAssignVariables::MakePhase(
    Solver* const s, const std::vector<IntVar*>& vars,
    Solver::VariableIndexSelector var_selector,
    Solver::VariableValueSelector value_selector,
    const std::string& value_selector_name, BaseAssignVariables::Mode mode) {
  BaseVariableAssignmentSelector* const selector =
      s->RevAlloc(new VariableAssignmentSelector(
          s, vars, std::move(var_selector), std::move(value_selector),
          value_selector_name));
  return s->RevAlloc(new BaseAssignVariables(selector, mode));
}

std::string ChooseVariableName(Solver::IntVarStrategy var_str) {
  switch (var_str) {
    case Solver::INT_VAR_DEFAULT:
    case Solver::INT_VAR_SIMPLE:
    case Solver::CHOOSE_FIRST_UNBOUND:
      return "ChooseFirstUnbound";
    case Solver::CHOOSE_RANDOM:
      return "ChooseRandom";
    case Solver::CHOOSE_MIN_SIZE_LOWEST_MIN:
      return "ChooseMinSizeLowestMin";
    case Solver::CHOOSE_MIN_SIZE_HIGHEST_MIN:
      return "ChooseMinSizeHighestMin";
    case Solver::CHOOSE_MIN_SIZE_LOWEST_MAX:
      return "ChooseMinSizeLowestMax";
    case Solver::CHOOSE_MIN_SIZE_HIGHEST_MAX:
      return "ChooseMinSizeHighestMax";
    case Solver::CHOOSE_LOWEST_MIN:
      return "ChooseLowestMin";
    case Solver::CHOOSE_HIGHEST_MAX:
      return "ChooseHighestMax";
    case Solver::CHOOSE_MIN_SIZE:
      return "ChooseMinSize";
    case Solver::CHOOSE_MAX_SIZE:
      return "ChooseMaxSize;";
    case Solver::CHOOSE_MAX_REGRET_ON_MIN:
      return "HighestRegretSelectorOnMin";
    case Solver::CHOOSE_PATH:
      return "PathSelector";
    default:
      LOG(FATAL) << "Unknown int var strategy " << var_str;
      return "";
  }
}

std::string SelectValueName(Solver::IntValueStrategy val_str) {
  switch (val_str) {
    case Solver::INT_VALUE_DEFAULT:
    case Solver::INT_VALUE_SIMPLE:
    case Solver::ASSIGN_MIN_VALUE:
      return "SelectMinValue";
    case Solver::ASSIGN_MAX_VALUE:
      return "SelectMaxValue";
    case Solver::ASSIGN_RANDOM_VALUE:
      return "SelectRandomValue";
    case Solver::ASSIGN_CENTER_VALUE:
      return "SelectCenterValue";
    case Solver::SPLIT_LOWER_HALF:
      return "SelectSplitValue";
    case Solver::SPLIT_UPPER_HALF:
      return "SelectSplitValue";
    default:
      LOG(FATAL) << "Unknown int value strategy " << val_str;
      return "";
  }
}

std::string BuildHeuristicsName(Solver::IntVarStrategy var_str,
                           Solver::IntValueStrategy val_str) {
  return ChooseVariableName(var_str) + "_" + SelectValueName(val_str);
}
}  // namespace

DecisionBuilder* Solver::MakePhase(IntVar* const v0,
                                   Solver::IntVarStrategy var_str,
                                   Solver::IntValueStrategy val_str) {
  std::vector<IntVar*> vars(1);
  vars[0] = v0;
  return MakePhase(vars, var_str, val_str);
}

DecisionBuilder* Solver::MakePhase(IntVar* const v0, IntVar* const v1,
                                   Solver::IntVarStrategy var_str,
                                   Solver::IntValueStrategy val_str) {
  std::vector<IntVar*> vars(2);
  vars[0] = v0;
  vars[1] = v1;
  return MakePhase(vars, var_str, val_str);
}

DecisionBuilder* Solver::MakePhase(IntVar* const v0, IntVar* const v1,
                                   IntVar* const v2,
                                   Solver::IntVarStrategy var_str,
                                   Solver::IntValueStrategy val_str) {
  std::vector<IntVar*> vars(3);
  vars[0] = v0;
  vars[1] = v1;
  vars[2] = v2;
  return MakePhase(vars, var_str, val_str);
}

DecisionBuilder* Solver::MakePhase(IntVar* const v0, IntVar* const v1,
                                   IntVar* const v2, IntVar* const v3,
                                   Solver::IntVarStrategy var_str,
                                   Solver::IntValueStrategy val_str) {
  std::vector<IntVar*> vars(4);
  vars[0] = v0;
  vars[1] = v1;
  vars[2] = v2;
  vars[3] = v3;
  return MakePhase(vars, var_str, val_str);
}

BaseAssignVariables::Mode ChooseMode(Solver::IntValueStrategy val_str) {
  BaseAssignVariables::Mode mode = BaseAssignVariables::ASSIGN;
  if (val_str == Solver::SPLIT_LOWER_HALF) {
    mode = BaseAssignVariables::SPLIT_LOWER;
  } else if (val_str == Solver::SPLIT_UPPER_HALF) {
    mode = BaseAssignVariables::SPLIT_UPPER;
  }
  return mode;
}

DecisionBuilder* Solver::MakePhase(const std::vector<IntVar*>& vars,
                                   Solver::IntVarStrategy var_str,
                                   Solver::IntValueStrategy val_str) {
  Solver::VariableIndexSelector var_selector =
      BaseAssignVariables::MakeVariableSelector(this, vars, var_str);
  Solver::VariableValueSelector value_selector =
      BaseAssignVariables::MakeValueSelector(this, val_str);
  const std::string name = BuildHeuristicsName(var_str, val_str);
  return BaseAssignVariables::MakePhase(
      this, vars, var_selector, value_selector, name, ChooseMode(val_str));
}

DecisionBuilder* Solver::MakePhase(const std::vector<IntVar*>& vars,
                                   Solver::IndexEvaluator1 var_evaluator,
                                   Solver::IntValueStrategy val_str) {
  CHECK(var_evaluator != nullptr);
  CheapestVarSelector* const var_selector =
      RevAlloc(new CheapestVarSelector(std::move(var_evaluator)));
  Solver::VariableIndexSelector choose_variable = [var_selector](
      Solver* solver, const std::vector<IntVar*>& vars, int first_unbound,
      int last_unbound) {
    return var_selector->Choose(solver, vars, first_unbound, last_unbound);
  };
  Solver::VariableValueSelector select_value =
      BaseAssignVariables::MakeValueSelector(this, val_str);
  const std::string name = "ChooseCheapestVariable_" + SelectValueName(val_str);
  return BaseAssignVariables::MakePhase(
      this, vars, choose_variable, select_value, name, ChooseMode(val_str));
}

DecisionBuilder* Solver::MakePhase(const std::vector<IntVar*>& vars,
                                   Solver::IntVarStrategy var_str,
                                   Solver::IndexEvaluator2 value_evaluator) {
  Solver::VariableIndexSelector choose_variable =
      BaseAssignVariables::MakeVariableSelector(this, vars, var_str);
  CheapestValueSelector* const value_selector =
      RevAlloc(new CheapestValueSelector(std::move(value_evaluator), nullptr));
  Solver::VariableValueSelector select_value = [value_selector](
      const IntVar* var, int64 id) { return value_selector->Select(var, id); };
  const std::string name = ChooseVariableName(var_str) + "_SelectCheapestValue";
  return BaseAssignVariables::MakePhase(this, vars, choose_variable,
                                        select_value, name,
                                        BaseAssignVariables::ASSIGN);
}

DecisionBuilder* Solver::MakePhase(
    const std::vector<IntVar*>& vars, IntVarStrategy var_str,
    VariableValueComparator var_val1_val2_comparator) {
  Solver::VariableIndexSelector choose_variable =
      BaseAssignVariables::MakeVariableSelector(this, vars, var_str);
  BestValueByComparisonSelector* const value_selector = RevAlloc(
      new BestValueByComparisonSelector(std::move(var_val1_val2_comparator)));
  Solver::VariableValueSelector select_value = [value_selector](
      const IntVar* var, int64 id) { return value_selector->Select(var, id); };
  return BaseAssignVariables::MakePhase(this, vars, choose_variable,
                                        select_value, "CheapestValue",
                                        BaseAssignVariables::ASSIGN);
}

DecisionBuilder* Solver::MakePhase(const std::vector<IntVar*>& vars,
                                   Solver::IndexEvaluator1 var_evaluator,
                                   Solver::IndexEvaluator2 value_evaluator) {
  CheapestVarSelector* const var_selector =
      RevAlloc(new CheapestVarSelector(std::move(var_evaluator)));
  Solver::VariableIndexSelector choose_variable = [var_selector](
      Solver* solver, const std::vector<IntVar*>& vars, int first_unbound,
      int last_unbound) {
    return var_selector->Choose(solver, vars, first_unbound, last_unbound);
  };
  CheapestValueSelector* value_selector =
      RevAlloc(new CheapestValueSelector(std::move(value_evaluator), nullptr));
  Solver::VariableValueSelector select_value = [value_selector](
      const IntVar* var, int64 id) { return value_selector->Select(var, id); };
  return BaseAssignVariables::MakePhase(this, vars, choose_variable,
                                        select_value, "CheapestValue",
                                        BaseAssignVariables::ASSIGN);
}

DecisionBuilder* Solver::MakePhase(const std::vector<IntVar*>& vars,
                                   Solver::IntVarStrategy var_str,
                                   Solver::IndexEvaluator2 value_evaluator,
                                   Solver::IndexEvaluator1 tie_breaker) {
  Solver::VariableIndexSelector choose_variable =
      BaseAssignVariables::MakeVariableSelector(this, vars, var_str);
  CheapestValueSelector* value_selector = RevAlloc(new CheapestValueSelector(
      std::move(value_evaluator), std::move(tie_breaker)));
  Solver::VariableValueSelector select_value = [value_selector](
      const IntVar* var, int64 id) { return value_selector->Select(var, id); };
  return BaseAssignVariables::MakePhase(this, vars, choose_variable,
                                        select_value, "CheapestValue",
                                        BaseAssignVariables::ASSIGN);
}

DecisionBuilder* Solver::MakePhase(const std::vector<IntVar*>& vars,
                                   Solver::IndexEvaluator1 var_evaluator,
                                   Solver::IndexEvaluator2 value_evaluator,
                                   Solver::IndexEvaluator1 tie_breaker) {
  CheapestVarSelector* const var_selector =
      RevAlloc(new CheapestVarSelector(std::move(var_evaluator)));
  Solver::VariableIndexSelector choose_variable = [var_selector](
      Solver* solver, const std::vector<IntVar*>& vars, int first_unbound,
      int last_unbound) {
    return var_selector->Choose(solver, vars, first_unbound, last_unbound);
  };
  CheapestValueSelector* value_selector = RevAlloc(new CheapestValueSelector(
      std::move(value_evaluator), std::move(tie_breaker)));
  Solver::VariableValueSelector select_value = [value_selector](
      const IntVar* var, int64 id) { return value_selector->Select(var, id); };
  return BaseAssignVariables::MakePhase(this, vars, choose_variable,
                                        select_value, "CheapestValue",
                                        BaseAssignVariables::ASSIGN);
}

DecisionBuilder* Solver::MakePhase(const std::vector<IntVar*>& vars,
                                   Solver::IndexEvaluator2 eval,
                                   Solver::EvaluatorStrategy str) {
  return MakePhase(vars, std::move(eval), nullptr, str);
}

DecisionBuilder* Solver::MakePhase(const std::vector<IntVar*>& vars,
                                   Solver::IndexEvaluator2 eval,
                                   Solver::IndexEvaluator1 tie_breaker,
                                   Solver::EvaluatorStrategy str) {
  BaseVariableAssignmentSelector* selector = nullptr;
  switch (str) {
    case Solver::CHOOSE_STATIC_GLOBAL_BEST: {
      // TODO(user): support tie breaker
      selector = RevAlloc(new StaticEvaluatorSelector(this, vars, eval));
      break;
    }
    case Solver::CHOOSE_DYNAMIC_GLOBAL_BEST: {
      selector = RevAlloc(new DynamicEvaluatorSelector(this, vars, eval,
                                                       std::move(tie_breaker)));
      break;
    }
  }
  return RevAlloc(
      new BaseAssignVariables(selector, BaseAssignVariables::ASSIGN));
}

// ----- AssignAllVariablesFromAssignment decision builder -----

namespace {
class AssignVariablesFromAssignment : public DecisionBuilder {
 public:
  AssignVariablesFromAssignment(const Assignment* const assignment,
                                DecisionBuilder* const db,
                                const std::vector<IntVar*>& vars)
      : assignment_(assignment), db_(db), vars_(vars), iter_(0) {}

  ~AssignVariablesFromAssignment() override {}

  Decision* Next(Solver* const s) override {
    if (iter_ < vars_.size()) {
      IntVar* const var = vars_[iter_++];
      return s->RevAlloc(
          new AssignOneVariableValue(var, assignment_->Value(var)));
    } else {
      return db_->Next(s);
    }
  }

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitExtension(ModelVisitor::kVariableGroupExtension);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->EndVisitExtension(ModelVisitor::kVariableGroupExtension);
  }

 private:
  const Assignment* const assignment_;
  DecisionBuilder* const db_;
  const std::vector<IntVar*> vars_;
  int iter_;
};
}  // namespace

DecisionBuilder* Solver::MakeDecisionBuilderFromAssignment(
    Assignment* const assignment, DecisionBuilder* const db,
    const std::vector<IntVar*>& vars) {
  return RevAlloc(new AssignVariablesFromAssignment(assignment, db, vars));
}

// ---------- Solution Collectors -----------

// ----- Base Class -----

SolutionCollector::SolutionCollector(Solver* const s, const Assignment* const a)
    : SearchMonitor(s),
      prototype_(a == nullptr ? nullptr : new Assignment(a)) {}

SolutionCollector::SolutionCollector(Solver* const s)
    : SearchMonitor(s), prototype_(new Assignment(s)) {}

SolutionCollector::~SolutionCollector() {
  STLDeleteElements(&solutions_);
  STLDeleteElements(&recycle_solutions_);
}

void SolutionCollector::Add(IntVar* const var) {
  if (prototype_ != nullptr) {
    prototype_->Add(var);
  }
}

void SolutionCollector::Add(const std::vector<IntVar*>& vars) {
  if (prototype_ != nullptr) {
    prototype_->Add(vars);
  }
}

void SolutionCollector::Add(IntervalVar* const var) {
  if (prototype_ != nullptr) {
    prototype_->Add(var);
  }
}

void SolutionCollector::Add(const std::vector<IntervalVar*>& vars) {
  if (prototype_ != nullptr) {
    prototype_->Add(vars);
  }
}

void SolutionCollector::Add(SequenceVar* const var) {
  if (prototype_ != nullptr) {
    prototype_->Add(var);
  }
}

void SolutionCollector::Add(const std::vector<SequenceVar*>& vars) {
  if (prototype_ != nullptr) {
    prototype_->Add(vars);
  }
}

void SolutionCollector::AddObjective(IntVar* const objective) {
  if (prototype_ != nullptr && objective != nullptr) {
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
  Assignment* new_sol = nullptr;
  if (prototype_ != nullptr) {
    if (!recycle_solutions_.empty()) {
      new_sol = recycle_solutions_.back();
      DCHECK(new_sol != nullptr);
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
  if (new_sol != nullptr) {
    objective_values_.push_back(new_sol->ObjectiveValue());
  } else {
    objective_values_.push_back(0);
  }
}

void SolutionCollector::PopSolution() {
  if (!solutions_.empty()) {
    Assignment* popped = solutions_.back();
    solutions_.pop_back();
    if (popped != nullptr) {
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

int SolutionCollector::solution_count() const { return solutions_.size(); }

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

const std::vector<int>& SolutionCollector::ForwardSequence(
    int n, SequenceVar* const v) const {
  return solutions_[n]->ForwardSequence(v);
}

const std::vector<int>& SolutionCollector::BackwardSequence(
    int n, SequenceVar* const v) const {
  return solutions_[n]->BackwardSequence(v);
}

const std::vector<int>& SolutionCollector::Unperformed(
    int n, SequenceVar* const v) const {
  return solutions_[n]->Unperformed(v);
}

namespace {
// ----- First Solution Collector -----

// Collect first solution, useful when looking satisfaction problems
class FirstSolutionCollector : public SolutionCollector {
 public:
  FirstSolutionCollector(Solver* const s, const Assignment* const a);
  explicit FirstSolutionCollector(Solver* const s);
  ~FirstSolutionCollector() override;
  void EnterSearch() override;
  bool AtSolution() override;
  std::string DebugString() const override;

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

std::string FirstSolutionCollector::DebugString() const {
  if (prototype_ == nullptr) {
    return "FirstSolutionCollector()";
  } else {
    return "FirstSolutionCollector(" + prototype_->DebugString() + ")";
  }
}
}  // namespace

SolutionCollector* Solver::MakeFirstSolutionCollector(
    const Assignment* const a) {
  return RevAlloc(new FirstSolutionCollector(this, a));
}

SolutionCollector* Solver::MakeFirstSolutionCollector() {
  return RevAlloc(new FirstSolutionCollector(this));
}

// ----- Last Solution Collector -----

// Collect last solution, useful when optimizing
namespace {
class LastSolutionCollector : public SolutionCollector {
 public:
  LastSolutionCollector(Solver* const s, const Assignment* const a);
  explicit LastSolutionCollector(Solver* const s);
  ~LastSolutionCollector() override;
  bool AtSolution() override;
  std::string DebugString() const override;
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

std::string LastSolutionCollector::DebugString() const {
  if (prototype_ == nullptr) {
    return "LastSolutionCollector()";
  } else {
    return "LastSolutionCollector(" + prototype_->DebugString() + ")";
  }
}
}  // namespace

SolutionCollector* Solver::MakeLastSolutionCollector(
    const Assignment* const a) {
  return RevAlloc(new LastSolutionCollector(this, a));
}

SolutionCollector* Solver::MakeLastSolutionCollector() {
  return RevAlloc(new LastSolutionCollector(this));
}

// ----- Best Solution Collector -----

namespace {
class BestValueSolutionCollector : public SolutionCollector {
 public:
  BestValueSolutionCollector(Solver* const s, const Assignment* const a,
                             bool maximize);
  BestValueSolutionCollector(Solver* const s, bool maximize);
  ~BestValueSolutionCollector() override {}
  void EnterSearch() override;
  bool AtSolution() override;
  std::string DebugString() const override;

 public:
  const bool maximize_;
  int64 best_;
};

BestValueSolutionCollector::BestValueSolutionCollector(
    Solver* const s, const Assignment* const a, bool maximize)
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
  if (prototype_ != nullptr) {
    const IntVar* objective = prototype_->Objective();
    if (objective != nullptr) {
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

std::string BestValueSolutionCollector::DebugString() const {
  if (prototype_ == nullptr) {
    return "BestValueSolutionCollector()";
  } else {
    return "BestValueSolutionCollector(" + prototype_->DebugString() + ")";
  }
}
}  // namespace

SolutionCollector* Solver::MakeBestValueSolutionCollector(
    const Assignment* const a, bool maximize) {
  return RevAlloc(new BestValueSolutionCollector(this, a, maximize));
}

SolutionCollector* Solver::MakeBestValueSolutionCollector(bool maximize) {
  return RevAlloc(new BestValueSolutionCollector(this, maximize));
}

// ----- All Solution Collector -----

// collect all solutions
namespace {
class AllSolutionCollector : public SolutionCollector {
 public:
  AllSolutionCollector(Solver* const s, const Assignment* const a);
  explicit AllSolutionCollector(Solver* const s);
  ~AllSolutionCollector() override;
  bool AtSolution() override;
  std::string DebugString() const override;
};

AllSolutionCollector::AllSolutionCollector(Solver* const s,
                                           const Assignment* const a)
    : SolutionCollector(s, a) {}

AllSolutionCollector::AllSolutionCollector(Solver* const s)
    : SolutionCollector(s) {}

AllSolutionCollector::~AllSolutionCollector() {}

bool AllSolutionCollector::AtSolution() {
  PushSolution();
  return true;
}

std::string AllSolutionCollector::DebugString() const {
  if (prototype_ == nullptr) {
    return "AllSolutionCollector()";
  } else {
    return "AllSolutionCollector(" + prototype_->DebugString() + ")";
  }
}
}  // namespace

SolutionCollector* Solver::MakeAllSolutionCollector(const Assignment* const a) {
  return RevAlloc(new AllSolutionCollector(this, a));
}

SolutionCollector* Solver::MakeAllSolutionCollector() {
  return RevAlloc(new AllSolutionCollector(this));
}

// ---------- Objective Management ----------

OptimizeVar::OptimizeVar(Solver* const s, bool maximize, IntVar* const a,
                         int64 step)
    : SearchMonitor(s),
      var_(a),
      step_(step),
      best_(kint64max),
      maximize_(maximize),
      found_initial_solution_(false) {
  CHECK_GT(step, 0);
}

OptimizeVar::~OptimizeVar() {}

void OptimizeVar::EnterSearch() {
  found_initial_solution_ = false;
  if (maximize_) {
    best_ = kint64min;
  } else {
    best_ = kint64max;
  }
}

void OptimizeVar::BeginNextDecision(DecisionBuilder* const db) {
  if (solver()->SearchDepth() == 0) {  // after a restart.
    ApplyBound();
  }
}

void OptimizeVar::ApplyBound() {
  if (found_initial_solution_) {
    if (maximize_) {
      var_->SetMin(best_ + step_);
    } else {
      var_->SetMax(best_ - step_);
    }
  }
}

void OptimizeVar::RefuteDecision(Decision* const d) { ApplyBound(); }

bool OptimizeVar::AcceptSolution() {
  const int64 val = var_->Value();
  if (!found_initial_solution_) {
    return true;
  } else {
    // This code should never return false in sequential mode because
    // ApplyBound should have been called before. In parallel, this is
    // no longer true. That is why we keep it there, just in case.
    return (maximize_ && val > best_) || (!maximize_ && val < best_);
  }
}

bool OptimizeVar::AtSolution() {
  int64 val = var_->Value();
  if (maximize_) {
    CHECK(!found_initial_solution_ || val > best_);
    best_ = val;
  } else {
    CHECK(!found_initial_solution_ || val < best_);
    best_ = val;
  }
  found_initial_solution_ = true;
  return true;
}

std::string OptimizeVar::Print() const {
  return StringPrintf("objective value = %" GG_LL_FORMAT "d, ", var_->Value());
}

std::string OptimizeVar::DebugString() const {
  std::string out;
  if (maximize_) {
    out = "MaximizeVar(";
  } else {
    out = "MinimizeVar(";
  }
  StringAppendF(&out,
                "%s, step = %" GG_LL_FORMAT "d, best = %" GG_LL_FORMAT "d)",
                var_->DebugString().c_str(), step_, best_);
  return out;
}

void OptimizeVar::Accept(ModelVisitor* const visitor) const {
  visitor->BeginVisitExtension(ModelVisitor::kObjectiveExtension);
  visitor->VisitIntegerArgument(ModelVisitor::kMaximizeArgument, maximize_);
  visitor->VisitIntegerArgument(ModelVisitor::kStepArgument, step_);
  visitor->VisitIntegerExpressionArgument(ModelVisitor::kExpressionArgument,
                                          var_);
  visitor->EndVisitExtension(ModelVisitor::kObjectiveExtension);
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

namespace {
class WeightedOptimizeVar : public OptimizeVar {
 public:
  WeightedOptimizeVar(Solver* solver, bool maximize,
                      const std::vector<IntVar*>& sub_objectives,
                      const std::vector<int64>& weights, int64 step)
      : OptimizeVar(solver, maximize,
                    solver->MakeScalProd(sub_objectives, weights)->Var(), step),
        sub_objectives_(sub_objectives),
        weights_(weights) {
    CHECK_EQ(sub_objectives.size(), weights.size());
  }

  ~WeightedOptimizeVar() override {}
  std::string Print() const override;

 private:
  const std::vector<IntVar*> sub_objectives_;
  const std::vector<int64> weights_;

  DISALLOW_COPY_AND_ASSIGN(WeightedOptimizeVar);
};

std::string WeightedOptimizeVar::Print() const {
  std::string result(OptimizeVar::Print());
  result.append("\nWeighted Objective:\n");
  for (int i = 0; i < sub_objectives_.size(); ++i) {
    StringAppendF(&result, "Variable %s,\tvalue %lld,\tweight %lld\n",
                  sub_objectives_[i]->name().c_str(),
                  sub_objectives_[i]->Value(), weights_[i]);
  }
  return result;
}
}  // namespace

OptimizeVar* Solver::MakeWeightedOptimize(
    bool maximize, const std::vector<IntVar*>& sub_objectives,
    const std::vector<int64>& weights, int64 step) {
  return RevAlloc(
      new WeightedOptimizeVar(this, maximize, sub_objectives, weights, step));
}

OptimizeVar* Solver::MakeWeightedMinimize(
    const std::vector<IntVar*>& sub_objectives,
    const std::vector<int64>& weights, int64 step) {
  return RevAlloc(
      new WeightedOptimizeVar(this, false, sub_objectives, weights, step));
}

OptimizeVar* Solver::MakeWeightedMaximize(
    const std::vector<IntVar*>& sub_objectives,
    const std::vector<int64>& weights, int64 step) {
  return RevAlloc(
      new WeightedOptimizeVar(this, true, sub_objectives, weights, step));
}

OptimizeVar* Solver::MakeWeightedOptimize(
    bool maximize, const std::vector<IntVar*>& sub_objectives,
    const std::vector<int>& weights, int64 step) {
  return MakeWeightedOptimize(maximize, sub_objectives, ToInt64Vector(weights),
                              step);
}

OptimizeVar* Solver::MakeWeightedMinimize(
    const std::vector<IntVar*>& sub_objectives, const std::vector<int>& weights,
    int64 step) {
  return MakeWeightedMinimize(sub_objectives, ToInt64Vector(weights), step);
}

OptimizeVar* Solver::MakeWeightedMaximize(
    const std::vector<IntVar*>& sub_objectives, const std::vector<int>& weights,
    int64 step) {
  return MakeWeightedMaximize(sub_objectives, ToInt64Vector(weights), step);
}

// ---------- Metaheuristics ---------

namespace {
class Metaheuristic : public SearchMonitor {
 public:
  Metaheuristic(Solver* const solver, bool maximize, IntVar* objective,
                int64 step);
  ~Metaheuristic() override {}

  bool AtSolution() override;
  void EnterSearch() override;
  void RefuteDecision(Decision* const d) override;

 protected:
  IntVar* const objective_;
  int64 step_;
  int64 current_;
  int64 best_;
  bool maximize_;
};

Metaheuristic::Metaheuristic(Solver* const solver, bool maximize,
                             IntVar* objective, int64 step)
    : SearchMonitor(solver),
      objective_(objective),
      step_(step),
      current_(kint64max),
      best_(kint64max),
      maximize_(maximize) {}

bool Metaheuristic::AtSolution() {
  current_ = objective_->Value();
  if (maximize_) {
    best_ = std::max(current_, best_);
  } else {
    best_ = std::min(current_, best_);
  }
  return true;
}

void Metaheuristic::EnterSearch() {
  if (maximize_) {
    best_ = objective_->Min();
    current_ = kint64min;
  } else {
    best_ = objective_->Max();
    current_ = kint64max;
  }
}

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
  TabuSearch(Solver* const s, bool maximize, IntVar* objective, int64 step,
             const std::vector<IntVar*>& vars, int64 keep_tenure,
             int64 forbid_tenure, double tabu_factor);
  ~TabuSearch() override {}
  void EnterSearch() override;
  void ApplyDecision(Decision* d) override;
  bool AtSolution() override;
  bool LocalOptimum() override;
  void AcceptNeighbor() override;
  std::string DebugString() const override { return "Tabu Search"; }

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

  const std::vector<IntVar*> vars_;
  Assignment assignment_;
  int64 last_;
  TabuList keep_tabu_list_;
  int64 keep_tenure_;
  TabuList forbid_tabu_list_;
  int64 forbid_tenure_;
  double tabu_factor_;
  int64 stamp_;
  bool found_initial_solution_;

  DISALLOW_COPY_AND_ASSIGN(TabuSearch);
};

TabuSearch::TabuSearch(Solver* const s, bool maximize, IntVar* objective,
                       int64 step, const std::vector<IntVar*>& vars,
                       int64 keep_tenure, int64 forbid_tenure,
                       double tabu_factor)
    : Metaheuristic(s, maximize, objective, step),
      vars_(vars),
      assignment_(s),
      last_(kint64max),
      keep_tenure_(keep_tenure),
      forbid_tenure_(forbid_tenure),
      tabu_factor_(tabu_factor),
      stamp_(0),
      found_initial_solution_(false) {
  assignment_.Add(vars_);
}

void TabuSearch::EnterSearch() {
  Metaheuristic::EnterSearch();
  found_initial_solution_ = false;
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
    s->AddConstraint(
        s->MakeIsGreaterOrEqualCstCt(objective_, best_ + step_, aspiration));
  } else {
    s->AddConstraint(
        s->MakeIsLessOrEqualCstCt(objective_, best_ - step_, aspiration));
  }

  // Tabu criterion
  // A variable in the "keep" list must keep its value, a variable in the
  // "forbid" list must not take its value in the list. The tabu criterion is
  // softened by the tabu factor which gives the number of violations to
  // the tabu criterion which is tolerated; a factor of 1 means no violations
  // allowed, a factor of 0 means all violations allowed.
  std::vector<IntVar*> tabu_vars;
  for (const VarValue& vv : keep_tabu_list_) {
    IntVar* const tabu_var = s->MakeBoolVar();
    Constraint* const keep_cst =
        s->MakeIsEqualCstCt(vv.var_, vv.value_, tabu_var);
    s->AddConstraint(keep_cst);
    tabu_vars.push_back(tabu_var);
  }
  for (const VarValue& vv : forbid_tabu_list_) {
    IntVar* tabu_var = s->MakeBoolVar();
    Constraint* const forbid_cst =
        s->MakeIsDifferentCstCt(vv.var_, vv.value_, tabu_var);
    s->AddConstraint(forbid_cst);
    tabu_vars.push_back(tabu_var);
  }

  if (!tabu_vars.empty()) {
    IntVar* const tabu = s->MakeBoolVar();
    s->AddConstraint(s->MakeIsGreaterOrEqualCstCt(
        s->MakeSum(tabu_vars)->Var(), tabu_vars.size() * tabu_factor_, tabu));
    s->AddConstraint(s->MakeGreaterOrEqual(s->MakeSum(aspiration, tabu), 1LL));
  }

  // Go downhill to the next local optimum
  if (maximize_) {
    const int64 bound = (current_ > kint64min) ? current_ + step_ : current_;
    s->AddConstraint(s->MakeGreaterOrEqual(objective_, bound));
  } else {
    const int64 bound = (current_ < kint64max) ? current_ - step_ : current_;
    s->AddConstraint(s->MakeLessOrEqual(objective_, bound));
  }

  // Avoid cost plateau's which lead to tabu cycles
  if (found_initial_solution_) {
    s->AddConstraint(s->MakeNonEquality(objective_, last_));
  }
}

bool TabuSearch::AtSolution() {
  if (!Metaheuristic::AtSolution()) {
    return false;
  }
  found_initial_solution_ = true;
  last_ = current_;

  // New solution found: add new assignments to tabu lists; this is only
  // done after the first local optimum (stamp_ != 0)
  if (0 != stamp_) {
    for (int i = 0; i < vars_.size(); ++i) {
      IntVar* const var = vars_[i];
      const int64 old_value = assignment_.Value(var);
      const int64 new_value = var->Value();
      if (old_value != new_value) {
        if (keep_tenure_ > 0) {
          VarValue keep_value(var, new_value, stamp_);
          keep_tabu_list_.push_front(keep_value);
        }
        if (forbid_tenure_ > 0) {
          VarValue forbid_value(var, old_value, stamp_);
          forbid_tabu_list_.push_front(forbid_value);
        }
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
  return found_initial_solution_;
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
}  // namespace

SearchMonitor* Solver::MakeTabuSearch(bool maximize, IntVar* const v,
                                      int64 step,
                                      const std::vector<IntVar*>& vars,
                                      int64 keep_tenure, int64 forbid_tenure,
                                      double tabu_factor) {
  return RevAlloc(new TabuSearch(this, maximize, v, step, vars, keep_tenure,
                                 forbid_tenure, tabu_factor));
}

SearchMonitor* Solver::MakeObjectiveTabuSearch(bool maximize, IntVar* const v,
                                               int64 step,
                                               int64 forbid_tenure) {
  const std::vector<IntVar*>& vars = {v};
  return RevAlloc(
      new TabuSearch(this, maximize, v, step, vars, 0, forbid_tenure, 1));
}

// ---------- Simulated Annealing ----------

namespace {
class SimulatedAnnealing : public Metaheuristic {
 public:
  SimulatedAnnealing(Solver* const s, bool maximize, IntVar* objective,
                     int64 step, int64 initial_temperature);
  ~SimulatedAnnealing() override {}
  void EnterSearch() override;
  void ApplyDecision(Decision* d) override;
  bool AtSolution() override;
  bool LocalOptimum() override;
  void AcceptNeighbor() override;
  std::string DebugString() const override { return "Simulated Annealing"; }

 private:
  float Temperature() const;

  const int64 temperature0_;
  int64 iteration_;
  ACMRandom rand_;
  bool found_initial_solution_;

  DISALLOW_COPY_AND_ASSIGN(SimulatedAnnealing);
};

SimulatedAnnealing::SimulatedAnnealing(Solver* const s, bool maximize,
                                       IntVar* objective, int64 step,
                                       int64 initial_temperature)
    : Metaheuristic(s, maximize, objective, step),
      temperature0_(initial_temperature),
      iteration_(0),
      rand_(654),
      found_initial_solution_(false) {}

void SimulatedAnnealing::EnterSearch() {
  Metaheuristic::EnterSearch();
  found_initial_solution_ = false;
}

void SimulatedAnnealing::ApplyDecision(Decision* const d) {
  Solver* const s = solver();
  if (d == s->balancing_decision()) {
    return;
  }
  #if defined(_MSC_VER) || defined(__ANDROID__)
     const int64  energy_bound = Temperature() * log(rand_.RndFloat()) /
     log(2.0L);
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
  if (!Metaheuristic::AtSolution()) {
    return false;
  }
  found_initial_solution_ = true;
  return true;
}

bool SimulatedAnnealing::LocalOptimum() {
  if (maximize_) {
    current_ = kint64min;
  } else {
    current_ = kint64max;
  }
  ++iteration_;
  return found_initial_solution_ && Temperature() > 0;
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
}  // namespace

SearchMonitor* Solver::MakeSimulatedAnnealing(bool maximize, IntVar* const v,
                                              int64 step,
                                              int64 initial_temperature) {
  return RevAlloc(
      new SimulatedAnnealing(this, maximize, v, step, initial_temperature));
}

// ---------- Guided Local Search ----------

typedef std::pair<int64, int64> Arc;

namespace {
// Base GLS penalties abstract class. Maintains the penalty frequency for each
// (variable, value) arc.
class GuidedLocalSearchPenalties {
 public:
  virtual ~GuidedLocalSearchPenalties() {}
  virtual bool HasValues() const = 0;
  virtual void Increment(const Arc& arc) = 0;
  virtual int64 Value(const Arc& arc) const = 0;
  virtual void Reset() = 0;
};

// Dense GLS penalties implementation using a matrix to store penalties.
class GuidedLocalSearchPenaltiesTable : public GuidedLocalSearchPenalties {
 public:
  explicit GuidedLocalSearchPenaltiesTable(int size);
  ~GuidedLocalSearchPenaltiesTable() override {}
  bool HasValues() const override { return has_values_; }
  void Increment(const Arc& arc) override;
  int64 Value(const Arc& arc) const override;
  void Reset() override;

 private:
  std::vector<std::vector<int64> > penalties_;
  bool has_values_;
};

GuidedLocalSearchPenaltiesTable::GuidedLocalSearchPenaltiesTable(int size)
    : penalties_(size), has_values_(false) {}

void GuidedLocalSearchPenaltiesTable::Increment(const Arc& arc) {
  std::vector<int64>& first_penalties = penalties_[arc.first];
  const int64 second = arc.second;
  if (second >= first_penalties.size()) {
    first_penalties.resize(second + 1, 0LL);
  }
  ++first_penalties[second];
  has_values_ = true;
}

void GuidedLocalSearchPenaltiesTable::Reset() {
  has_values_ = false;
  for (int i = 0; i < penalties_.size(); ++i) {
    penalties_[i].clear();
  }
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
  ~GuidedLocalSearchPenaltiesMap() override {}
  bool HasValues() const override { return (!penalties_.empty()); }
  void Increment(const Arc& arc) override;
  int64 Value(const Arc& arc) const override;
  void Reset() override;

 private:
  Bitmap penalized_;
  std::unordered_map<Arc, int64> penalties_;
};

GuidedLocalSearchPenaltiesMap::GuidedLocalSearchPenaltiesMap(int size)
    : penalized_(size, false) {}

void GuidedLocalSearchPenaltiesMap::Increment(const Arc& arc) {
  ++penalties_[arc];
  penalized_.Set(arc.first, true);
}

void GuidedLocalSearchPenaltiesMap::Reset() {
  penalties_.clear();
  penalized_.Clear();
}

int64 GuidedLocalSearchPenaltiesMap::Value(const Arc& arc) const {
  if (penalized_.Get(arc.first)) {
    return FindWithDefault(penalties_, arc, 0LL);
  }
  return 0LL;
}

class GuidedLocalSearch : public Metaheuristic {
 public:
  GuidedLocalSearch(Solver* const s, IntVar* objective, bool maximize,
                    int64 step, const std::vector<IntVar*>& vars,
                    double penalty_factor);
  ~GuidedLocalSearch() override {}
  bool AcceptDelta(Assignment* delta, Assignment* deltadelta) override;
  void ApplyDecision(Decision* d) override;
  bool AtSolution() override;
  void EnterSearch() override;
  bool LocalOptimum() override;
  virtual int64 AssignmentElementPenalty(const Assignment& assignment,
                                         int index) = 0;
  virtual int64 AssignmentPenalty(const Assignment& assignment, int index,
                                  int64 next) = 0;
  virtual bool EvaluateElementValue(const Assignment::IntContainer& container,
                                    int64 index, int* container_index,
                                    int64* penalty) = 0;
  virtual IntExpr* MakeElementPenalty(int index) = 0;
  std::string DebugString() const override { return "Guided Local Search"; }

 protected:
  struct Comparator {
    bool operator()(const std::pair<Arc, double>& i,
                    const std::pair<Arc, double>& j) {
      return i.second > j.second;
    }
  };

  int64 Evaluate(const Assignment* delta, int64 current_penalty,
                 const int64* const out_values, bool cache_delta_values);

  IntVar* penalized_objective_;
  Assignment assignment_;
  int64 assignment_penalized_value_;
  int64 old_penalized_value_;
  const std::vector<IntVar*> vars_;
  std::unordered_map<const IntVar*, int64> indices_;
  const double penalty_factor_;
  std::unique_ptr<GuidedLocalSearchPenalties> penalties_;
  std::unique_ptr<int64[]> current_penalized_values_;
  std::unique_ptr<int64[]> delta_cache_;
  bool incremental_;
};

GuidedLocalSearch::GuidedLocalSearch(Solver* const s, IntVar* objective,
                                     bool maximize, int64 step,
                                     const std::vector<IntVar*>& vars,
                                     double penalty_factor)
    : Metaheuristic(s, maximize, objective, step),
      penalized_objective_(nullptr),
      assignment_(s),
      assignment_penalized_value_(0),
      old_penalized_value_(0),
      vars_(vars),
      penalty_factor_(penalty_factor),
      incremental_(false) {
  if (!vars.empty()) {
    // TODO(user): Remove scoped_array.
    assignment_.Add(vars_);
    current_penalized_values_.reset(new int64[vars_.size()]);
    delta_cache_.reset(new int64[vars_.size()]);
    memset(current_penalized_values_.get(), 0,
           vars_.size() * sizeof(*current_penalized_values_.get()));
  }
  for (int i = 0; i < vars_.size(); ++i) {
    indices_[vars_[i]] = i;
  }
  if (FLAGS_cp_use_sparse_gls_penalties) {
    penalties_.reset(new GuidedLocalSearchPenaltiesMap(vars_.size()));
  } else {
    penalties_.reset(new GuidedLocalSearchPenaltiesTable(vars_.size()));
  }
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
    for (int i = 0; i < vars_.size(); ++i) {
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
      IntExpr* min_pen_exp =
          solver()->MakeDifference(current_ + step_, penalized_objective_);
      IntVar* min_exp = solver()->MakeMin(min_pen_exp, best_ + step_)->Var();
      solver()->AddConstraint(
          solver()->MakeGreaterOrEqual(objective_, min_exp));
    } else {
      IntExpr* max_pen_exp =
          solver()->MakeDifference(current_ - step_, penalized_objective_);
      IntVar* max_exp = solver()->MakeMax(max_pen_exp, best_ - step_)->Var();
      solver()->AddConstraint(solver()->MakeLessOrEqual(objective_, max_exp));
    }
  } else {
    penalized_objective_ = nullptr;
    if (maximize_) {
      const int64 bound = (current_ > kint64min) ? current_ + step_ : current_;
      objective_->SetMin(bound);
    } else {
      const int64 bound = (current_ < kint64max) ? current_ - step_ : current_;
      objective_->SetMax(bound);
    }
  }
}

bool GuidedLocalSearch::AtSolution() {
  if (!Metaheuristic::AtSolution()) {
    return false;
  }
  if (penalized_objective_ != nullptr) {  // In case no move has been found
    current_ += penalized_objective_->Value();
  }
  assignment_.Store();
  return true;
}

void GuidedLocalSearch::EnterSearch() {
  Metaheuristic::EnterSearch();
  penalized_objective_ = nullptr;
  assignment_penalized_value_ = 0;
  old_penalized_value_ = 0;
  memset(current_penalized_values_.get(), 0,
         vars_.size() * sizeof(*current_penalized_values_.get()));
  penalties_->Reset();
}

// GLS filtering; compute the penalized value corresponding to the delta and
// modify objective bound accordingly.
bool GuidedLocalSearch::AcceptDelta(Assignment* delta, Assignment* deltadelta) {
  if ((delta != nullptr || deltadelta != nullptr) && penalties_->HasValues()) {
    int64 penalty = 0;
    if (!deltadelta->Empty()) {
      if (!incremental_) {
        penalty = Evaluate(delta, assignment_penalized_value_,
                           current_penalized_values_.get(), true);
      } else {
        penalty = Evaluate(deltadelta, old_penalized_value_, delta_cache_.get(),
                           true);
      }
      incremental_ = true;
    } else {
      if (incremental_) {
        for (int i = 0; i < vars_.size(); ++i) {
          delta_cache_[i] = current_penalized_values_[i];
        }
        old_penalized_value_ = assignment_penalized_value_;
      }
      incremental_ = false;
      penalty = Evaluate(delta, assignment_penalized_value_,
                         current_penalized_values_.get(), false);
    }
    old_penalized_value_ = penalty;
    if (!delta->HasObjective()) {
      delta->AddObjective(objective_);
    }
    if (delta->Objective() == objective_) {
      if (maximize_) {
        delta->SetObjectiveMin(
            std::max(std::min(current_ + step_ - penalty, best_ + step_),
                     delta->ObjectiveMin()));
      } else {
        delta->SetObjectiveMax(
            std::min(std::max(current_ - step_ - penalty, best_ - step_),
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
    IntVar* var = new_element.Var();
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
  std::vector<std::pair<Arc, double> > utility(vars_.size());
  for (int i = 0; i < vars_.size(); ++i) {
    if (!assignment_.Bound(vars_[i])) {
      // Never synced with a solution, problem infeasible.
      return false;
    }
    const int64 var_value = assignment_.Value(vars_[i]);
    const int64 value =
        (var_value != i) ? AssignmentPenalty(assignment_, i, var_value) : 0;
    const Arc arc(i, var_value);
    const int64 penalty = penalties_->Value(arc);
    utility[i] = std::pair<Arc, double>(arc, value / (penalty + 1.0));
  }
  Comparator comparator;
  std::stable_sort(utility.begin(), utility.end(), comparator);
  int64 utility_value = utility[0].second;
  penalties_->Increment(utility[0].first);
  for (int i = 1; i < utility.size() && utility_value == utility[i].second;
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
  BinaryGuidedLocalSearch(Solver* const solver, IntVar* const objective,
                          std::function<int64(int64, int64)> objective_function,
                          bool maximize, int64 step,
                          const std::vector<IntVar*>& vars,
                          double penalty_factor);
  ~BinaryGuidedLocalSearch() override {}
  IntExpr* MakeElementPenalty(int index) override;
  int64 AssignmentElementPenalty(const Assignment& assignment,
                                 int index) override;
  int64 AssignmentPenalty(const Assignment& assignment, int index,
                          int64 next) override;
  bool EvaluateElementValue(const Assignment::IntContainer& container,
                            int64 index, int* container_index,
                            int64* penalty) override;

 private:
  int64 PenalizedValue(int64 i, int64 j);
  std::function<int64(int64, int64)> objective_function_;
};

BinaryGuidedLocalSearch::BinaryGuidedLocalSearch(
    Solver* const solver, IntVar* const objective,
    std::function<int64(int64, int64)> objective_function, bool maximize,
    int64 step, const std::vector<IntVar*>& vars, double penalty_factor)
    : GuidedLocalSearch(solver, objective, maximize, step, vars,
                        penalty_factor),
      objective_function_(std::move(objective_function)) {}

IntExpr* BinaryGuidedLocalSearch::MakeElementPenalty(int index) {
  return solver()->MakeElement(
      [this, index](int64 i) { return PenalizedValue(index, i); },
      vars_[index]);
}

int64 BinaryGuidedLocalSearch::AssignmentElementPenalty(
    const Assignment& assignment, int index) {
  return PenalizedValue(index, assignment.Value(vars_[index]));
}

int64 BinaryGuidedLocalSearch::AssignmentPenalty(const Assignment& assignment,
                                                 int index, int64 next) {
  return objective_function_(index, next);
}

bool BinaryGuidedLocalSearch::EvaluateElementValue(
    const Assignment::IntContainer& container, int64 index,
    int* container_index, int64* penalty) {
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
        penalty_factor_ * penalty * objective_function_(i, j);
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
  TernaryGuidedLocalSearch(
      Solver* const solver, IntVar* const objective,
      std::function<int64(int64, int64, int64)> objective_function,
      bool maximize, int64 step, const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars, double penalty_factor);
  ~TernaryGuidedLocalSearch() override {}
  IntExpr* MakeElementPenalty(int index) override;
  int64 AssignmentElementPenalty(const Assignment& assignment,
                                 int index) override;
  int64 AssignmentPenalty(const Assignment& assignment, int index,
                          int64 next) override;
  bool EvaluateElementValue(const Assignment::IntContainer& container,
                            int64 index, int* container_index,
                            int64* penalty) override;

 private:
  int64 PenalizedValue(int64 i, int64 j, int64 k);
  int64 GetAssignmentSecondaryValue(const Assignment::IntContainer& container,
                                    int index, int* container_index) const;

  const std::vector<IntVar*> secondary_vars_;
  std::function<int64(int64, int64, int64)> objective_function_;
};

TernaryGuidedLocalSearch::TernaryGuidedLocalSearch(
    Solver* const solver, IntVar* const objective,
    std::function<int64(int64, int64, int64)> objective_function, bool maximize,
    int64 step, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars, double penalty_factor)
    : GuidedLocalSearch(solver, objective, maximize, step, vars,
                        penalty_factor),
      secondary_vars_(secondary_vars),
      objective_function_(std::move(objective_function)) {
  if (!secondary_vars.empty()) {
    assignment_.Add(secondary_vars);
  }
}

IntExpr* TernaryGuidedLocalSearch::MakeElementPenalty(int index) {
  return solver()->MakeElement(
      [this, index](int64 i, int64 j) { return PenalizedValue(index, i, j); },
      vars_[index], secondary_vars_[index]);
}

int64 TernaryGuidedLocalSearch::AssignmentElementPenalty(
    const Assignment& assignment, int index) {
  return PenalizedValue(index, assignment.Value(vars_[index]),
                        assignment.Value(secondary_vars_[index]));
}

int64 TernaryGuidedLocalSearch::AssignmentPenalty(const Assignment& assignment,
                                                  int index, int64 next) {
  return objective_function_(index, next,
                             assignment.Value(secondary_vars_[index]));
}

bool TernaryGuidedLocalSearch::EvaluateElementValue(
    const Assignment::IntContainer& container, int64 index,
    int* container_index, int64* penalty) {
  const IntVarElement& element = container.Element(*container_index);
  if (element.Activated()) {
    *penalty = PenalizedValue(
        index, element.Value(),
        GetAssignmentSecondaryValue(container, index, container_index));
    return true;
  }
  return false;
}

// Penalized value for (i, j) = penalty_factor_ * penalty(i, j) * cost (i, j)
int64 TernaryGuidedLocalSearch::PenalizedValue(int64 i, int64 j, int64 k) {
  const Arc arc(i, j);
  const int64 penalty = penalties_->Value(arc);
  if (penalty != 0) {  // objective_function_(i, j, k) can be costly
    const int64 penalized_value =
        penalty_factor_ * penalty * objective_function_(i, j, k);
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
    const Assignment::IntContainer& container, int index,
    int* container_index) const {
  const IntVar* secondary_var = secondary_vars_[index];
  int hint_index = *container_index + 1;
  if (hint_index > 0 && hint_index < container.Size() &&
      secondary_var == container.Element(hint_index).Var()) {
    *container_index = hint_index;
    return container.Element(hint_index).Value();
  } else {
    return container.Element(secondary_var).Value();
  }
}
}  // namespace

SearchMonitor* Solver::MakeGuidedLocalSearch(
    bool maximize, IntVar* const objective,
    Solver::IndexEvaluator2 objective_function, int64 step,
    const std::vector<IntVar*>& vars, double penalty_factor) {
  return RevAlloc(new BinaryGuidedLocalSearch(
      this, objective, std::move(objective_function), maximize, step, vars,
      penalty_factor));
}

SearchMonitor* Solver::MakeGuidedLocalSearch(
    bool maximize, IntVar* const objective,
    Solver::IndexEvaluator3 objective_function, int64 step,
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars, double penalty_factor) {
  return RevAlloc(new TernaryGuidedLocalSearch(
      this, objective, std::move(objective_function), maximize, step, vars,
      secondary_vars, penalty_factor));
}

// ---------- Search Limits ----------

// ----- Base Class -----

SearchLimit::~SearchLimit() {}

void SearchLimit::EnterSearch() {
  crossed_ = false;
  Init();
}

void SearchLimit::BeginNextDecision(DecisionBuilder* const b) {
  PeriodicCheck();
  TopPeriodicCheck();
}

void SearchLimit::RefuteDecision(Decision* const d) {
  PeriodicCheck();
  TopPeriodicCheck();
}

void SearchLimit::PeriodicCheck() {
  if (crossed_ || Check()) {
    crossed_ = true;
    solver()->Fail();
  }
}

void SearchLimit::TopPeriodicCheck() {
  if (solver()->TopLevelSearch() != solver()->ActiveSearch()) {
    solver()->TopPeriodicCheck();
  }
}

namespace {
// ----- Regular Limit -----

// Usual limit based on wall_time, number of explored branches and
// number of failures in the search tree
class RegularLimit : public SearchLimit {
 public:
  RegularLimit(Solver* const s, int64 time, int64 branches, int64 failures,
               int64 solutions, bool smart_time_check, bool cumulative);
  ~RegularLimit() override;
  void Copy(const SearchLimit* const limit) override;
  SearchLimit* MakeClone() const override;
  bool Check() override;
  void Init() override;
  void ExitSearch() override;
  void UpdateLimits(int64 time, int64 branches, int64 failures,
                    int64 solutions);
  int64 wall_time() { return wall_time_; }
  int ProgressPercent() override;
  std::string DebugString() const override;

  void Accept(ModelVisitor* const visitor) const override {
    visitor->BeginVisitExtension(ModelVisitor::kSearchLimitExtension);
    visitor->VisitIntegerArgument(ModelVisitor::kTimeLimitArgument, wall_time_);
    visitor->VisitIntegerArgument(ModelVisitor::kBranchesLimitArgument,
                                  branches_);
    visitor->VisitIntegerArgument(ModelVisitor::kFailuresLimitArgument,
                                  failures_);
    visitor->VisitIntegerArgument(ModelVisitor::kSolutionLimitArgument,
                                  solutions_);
    visitor->VisitIntegerArgument(ModelVisitor::kSmartTimeCheckArgument,
                                  smart_time_check_);
    visitor->VisitIntegerArgument(ModelVisitor::kCumulativeArgument,
                                  cumulative_);
    visitor->EndVisitExtension(ModelVisitor::kObjectiveExtension);
  }

 private:
  bool CheckTime();
  int64 TimeDelta();
  static int64 GetPercent(int64 value, int64 offset, int64 total) {
    return (total > 0 && total < kint64max) ? 100 * (value - offset) / total
                                            : -1;
  }

  int64 wall_time_;
  int64 wall_time_offset_;
  int64 last_time_delta_;
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

RegularLimit::RegularLimit(Solver* const s, int64 time, int64 branches,
                           int64 failures, int64 solutions,
                           bool smart_time_check, bool cumulative)
    : SearchLimit(s),
      wall_time_(time),
      wall_time_offset_(0),
      last_time_delta_(-1),
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
  return s->MakeLimit(wall_time_, branches_, failures_, solutions_,
                      smart_time_check_);
}

bool RegularLimit::Check() {
  Solver* const s = solver();
  // Warning limits might be kint64max, do not move the offset to the rhs
  return s->branches() - branches_offset_ >= branches_ ||
         s->failures() - failures_offset_ >= failures_ || CheckTime() ||
         s->solutions() - solutions_offset_ >= solutions_;
}

int RegularLimit::ProgressPercent() {
  Solver* const s = solver();
  int64 progress = GetPercent(s->branches(), branches_offset_, branches_);
  progress = std::max(progress,
                      GetPercent(s->failures(), failures_offset_, failures_));
  progress = std::max(
      progress, GetPercent(s->solutions(), solutions_offset_, solutions_));
  if (wall_time_ < kint64max) {
    progress = std::max(progress, (100 * TimeDelta()) / wall_time_);
  }
  return progress;
}

void RegularLimit::Init() {
  Solver* const s = solver();
  branches_offset_ = s->branches();
  failures_offset_ = s->failures();
  wall_time_offset_ = s->wall_time();
  last_time_delta_ = -1;
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

void RegularLimit::UpdateLimits(int64 time, int64 branches, int64 failures,
                                int64 solutions) {
  wall_time_ = time;
  branches_ = branches;
  failures_ = failures;
  solutions_ = solutions;
}

std::string RegularLimit::DebugString() const {
  return StringPrintf("RegularLimit(crossed = %i, wall_time = %" GG_LL_FORMAT
                      "d, "
                      "branches = %" GG_LL_FORMAT "d, failures = %" GG_LL_FORMAT
                      "d, solutions = %" GG_LL_FORMAT "d cumulative = %s",
                      crossed(), wall_time_, branches_, failures_, solutions_,
                      (cumulative_ ? "true" : "false"));
}

bool RegularLimit::CheckTime() { return TimeDelta() >= wall_time_; }

int64 RegularLimit::TimeDelta() {
  const int64 kMaxSkip = 100;
  const int64 kCheckWarmupIterations = 100;
  ++check_count_;
  if (wall_time_ != kint64max && next_check_ <= check_count_) {
    Solver* const s = solver();
    int64 time_delta = s->wall_time() - wall_time_offset_;
    if (smart_time_check_ && check_count_ > kCheckWarmupIterations &&
        time_delta > 0) {
      int64 approximate_calls = (wall_time_ * check_count_) / time_delta;
      next_check_ = check_count_ + std::min(kMaxSkip, approximate_calls);
    }
    last_time_delta_ = time_delta;
  }
  return last_time_delta_;
}
}  // namespace

SearchLimit* Solver::MakeTimeLimit(int64 time) {
  return MakeLimit(time, kint64max, kint64max, kint64max);
}

SearchLimit* Solver::MakeBranchesLimit(int64 branches) {
  return MakeLimit(kint64max, branches, kint64max, kint64max);
}

SearchLimit* Solver::MakeFailuresLimit(int64 failures) {
  return MakeLimit(kint64max, kint64max, failures, kint64max);
}

SearchLimit* Solver::MakeSolutionsLimit(int64 solutions) {
  return MakeLimit(kint64max, kint64max, kint64max, solutions);
}

SearchLimit* Solver::MakeLimit(int64 time, int64 branches, int64 failures,
                               int64 solutions) {
  return MakeLimit(time, branches, failures, solutions, false);
}

SearchLimit* Solver::MakeLimit(int64 time, int64 branches, int64 failures,
                               int64 solutions, bool smart_time_check) {
  return MakeLimit(time, branches, failures, solutions, smart_time_check,
                   false);
}

SearchLimit* Solver::MakeLimit(int64 time, int64 branches, int64 failures,
                               int64 solutions, bool smart_time_check,
                               bool cumulative) {
  return RevAlloc(new RegularLimit(this, time, branches, failures, solutions,
                                   smart_time_check, cumulative));
}

SearchLimit* Solver::MakeLimit(const SearchLimitParameters& proto) {
  return MakeLimit(proto.time(), proto.branches(), proto.failures(),
                   proto.solutions(), proto.smart_time_check(),
                   proto.cumulative());
}

SearchLimitParameters Solver::MakeDefaultSearchLimitParameters() const {
  SearchLimitParameters proto;
  proto.set_time(kint64max);
  proto.set_branches(kint64max);
  proto.set_failures(kint64max);
  proto.set_solutions(kint64max);
  proto.set_smart_time_check(false);
  proto.set_cumulative(false);
  return proto;
}

// A limit whose Check function is the OR of two underlying limits.
namespace {
class ORLimit : public SearchLimit {
 public:
  ORLimit(SearchLimit* limit_1, SearchLimit* limit_2)
      : SearchLimit(limit_1->solver()), limit_1_(limit_1), limit_2_(limit_2) {
    CHECK(limit_1 != nullptr);
    CHECK(limit_2 != nullptr);
    CHECK_EQ(limit_1->solver(), limit_2->solver())
        << "Illegal arguments: cannot combines limits that belong to different "
        << "solvers, because the reversible allocations could delete one and "
        << "not the other.";
  }

  bool Check() override {
    // Check being non-const, there may be side effects. So we always call both
    // checks.
    const bool check_1 = limit_1_->Check();
    const bool check_2 = limit_2_->Check();
    return check_1 || check_2;
  }

  void Init() override {
    limit_1_->Init();
    limit_2_->Init();
  }

  void Copy(const SearchLimit* const limit) override {
    LOG(FATAL) << "Not implemented.";
  }

  SearchLimit* MakeClone() const override {
    // Deep cloning: the underlying limits are cloned, too.
    return solver()->MakeLimit(limit_1_->MakeClone(), limit_2_->MakeClone());
  }

  void EnterSearch() override {
    limit_1_->EnterSearch();
    limit_2_->EnterSearch();
  }
  void BeginNextDecision(DecisionBuilder* const b) override {
    limit_1_->BeginNextDecision(b);
    limit_2_->BeginNextDecision(b);
  }
  void PeriodicCheck() override {
    limit_1_->PeriodicCheck();
    limit_2_->PeriodicCheck();
  }
  void RefuteDecision(Decision* const d) override {
    limit_1_->RefuteDecision(d);
    limit_2_->RefuteDecision(d);
  }
  std::string DebugString() const override {
    return StrCat("OR limit (", limit_1_->DebugString(), " OR ",
                        limit_2_->DebugString(), ")");
  }

 private:
  SearchLimit* const limit_1_;
  SearchLimit* const limit_2_;
};
}  // namespace

SearchLimit* Solver::MakeLimit(SearchLimit* const limit_1,
                               SearchLimit* const limit_2) {
  return RevAlloc(new ORLimit(limit_1, limit_2));
}

void Solver::UpdateLimits(int64 time, int64 branches, int64 failures,
                          int64 solutions, SearchLimit* limit) {
  reinterpret_cast<RegularLimit*>(limit)->UpdateLimits(time, branches, failures,
                                                       solutions);
}

int64 Solver::GetTime(SearchLimit* limit) {
  return reinterpret_cast<RegularLimit*>(limit)->wall_time();
}

namespace {
class CustomLimit : public SearchLimit {
 public:
  CustomLimit(Solver* const s, std::function<bool()> limiter);
  bool Check() override;
  void Init() override;
  void Copy(const SearchLimit* const limit) override;
  SearchLimit* MakeClone() const override;

 private:
  std::function<bool()> limiter_;
};

CustomLimit::CustomLimit(Solver* const s, std::function<bool()> limiter)
    : SearchLimit(s), limiter_(std::move(limiter)) {}

bool CustomLimit::Check() {
  if (limiter_) return limiter_();
  return false;
}

void CustomLimit::Init() {}

void CustomLimit::Copy(const SearchLimit* const limit) {
  const CustomLimit* const custom =
      reinterpret_cast<const CustomLimit* const>(limit);
  limiter_ = custom->limiter_;
}

SearchLimit* CustomLimit::MakeClone() const {
  return solver()->RevAlloc(new CustomLimit(solver(), limiter_));
}
}  // namespace

SearchLimit* Solver::MakeCustomLimit(std::function<bool()> limiter) {
  return RevAlloc(new CustomLimit(this, std::move(limiter)));
}

// ---------- SolveOnce ----------

namespace {
class SolveOnce : public DecisionBuilder {
 public:
  explicit SolveOnce(DecisionBuilder* const db) : db_(db) {
    CHECK(db != nullptr);
  }

  SolveOnce(DecisionBuilder* const db,
            const std::vector<SearchMonitor*>& monitors)
      : db_(db), monitors_(monitors) {
    CHECK(db != nullptr);
  }

  ~SolveOnce() override {}

  Decision* Next(Solver* s) override {
    bool res = s->SolveAndCommit(db_, monitors_);
    if (!res) {
      s->Fail();
    }
    return nullptr;
  }

  std::string DebugString() const override {
    return StringPrintf("SolveOnce(%s)", db_->DebugString().c_str());
  }

  void Accept(ModelVisitor* const visitor) const override {
    db_->Accept(visitor);
  }

 private:
  DecisionBuilder* const db_;
  std::vector<SearchMonitor*> monitors_;
};
}  // namespace

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

DecisionBuilder* Solver::MakeSolveOnce(
    DecisionBuilder* const db, const std::vector<SearchMonitor*>& monitors) {
  return RevAlloc(new SolveOnce(db, monitors));
}

// ---------- NestedOptimize ----------

namespace {
class NestedOptimize : public DecisionBuilder {
 public:
  NestedOptimize(DecisionBuilder* const db, Assignment* const solution,
                 bool maximize, int64 step)
      : db_(db),
        solution_(solution),
        maximize_(maximize),
        step_(step),
        collector_(nullptr) {
    CHECK(db != nullptr);
    CHECK(solution != nullptr);
    CHECK(solution->HasObjective());
    AddMonitors();
  }

  NestedOptimize(DecisionBuilder* const db, Assignment* const solution,
                 bool maximize, int64 step,
                 const std::vector<SearchMonitor*>& monitors)
      : db_(db),
        solution_(solution),
        maximize_(maximize),
        step_(step),
        monitors_(monitors),
        collector_(nullptr) {
    CHECK(db != nullptr);
    CHECK(solution != nullptr);
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

  Decision* Next(Solver* solver) override {
    solver->Solve(db_, monitors_);
    if (collector_->solution_count() == 0) {
      solver->Fail();
    }
    collector_->solution(0)->Restore();
    return nullptr;
  }

  std::string DebugString() const override {
    return StringPrintf("NestedOptimize(db = %s, maximize = %d, step = %lld)",
                        db_->DebugString().c_str(), maximize_, step_);
  }

  void Accept(ModelVisitor* const visitor) const override {
    db_->Accept(visitor);
  }

 private:
  DecisionBuilder* const db_;
  Assignment* const solution_;
  const bool maximize_;
  const int64 step_;
  std::vector<SearchMonitor*> monitors_;
  SolutionCollector* collector_;
};
}  // namespace

DecisionBuilder* Solver::MakeNestedOptimize(DecisionBuilder* const db,
                                            Assignment* const solution,
                                            bool maximize, int64 step) {
  return RevAlloc(new NestedOptimize(db, solution, maximize, step));
}

DecisionBuilder* Solver::MakeNestedOptimize(DecisionBuilder* const db,
                                            Assignment* const solution,
                                            bool maximize, int64 step,
                                            SearchMonitor* const monitor1) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(monitor1);
  return RevAlloc(new NestedOptimize(db, solution, maximize, step, monitors));
}

DecisionBuilder* Solver::MakeNestedOptimize(DecisionBuilder* const db,
                                            Assignment* const solution,
                                            bool maximize, int64 step,
                                            SearchMonitor* const monitor1,
                                            SearchMonitor* const monitor2) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(monitor1);
  monitors.push_back(monitor2);
  return RevAlloc(new NestedOptimize(db, solution, maximize, step, monitors));
}

DecisionBuilder* Solver::MakeNestedOptimize(DecisionBuilder* const db,
                                            Assignment* const solution,
                                            bool maximize, int64 step,
                                            SearchMonitor* const monitor1,
                                            SearchMonitor* const monitor2,
                                            SearchMonitor* const monitor3) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(monitor1);
  monitors.push_back(monitor2);
  monitors.push_back(monitor3);
  return RevAlloc(new NestedOptimize(db, solution, maximize, step, monitors));
}

DecisionBuilder* Solver::MakeNestedOptimize(
    DecisionBuilder* const db, Assignment* const solution, bool maximize,
    int64 step, SearchMonitor* const monitor1, SearchMonitor* const monitor2,
    SearchMonitor* const monitor3, SearchMonitor* const monitor4) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(monitor1);
  monitors.push_back(monitor2);
  monitors.push_back(monitor3);
  monitors.push_back(monitor4);
  return RevAlloc(new NestedOptimize(db, solution, maximize, step, monitors));
}

DecisionBuilder* Solver::MakeNestedOptimize(
    DecisionBuilder* const db, Assignment* const solution, bool maximize,
    int64 step, const std::vector<SearchMonitor*>& monitors) {
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

  ~LubyRestart() override {}

  void BeginFail() override {
    if (++current_fails_ >= next_step_) {
      current_fails_ = 0;
      next_step_ = NextLuby(++iteration_) * scale_factor_;
      solver()->RestartCurrentSearch();
    }
  }

  std::string DebugString() const override {
    return StringPrintf("LubyRestart(%i)", scale_factor_);
  }

 private:
  const int scale_factor_;
  int iteration_;
  int64 current_fails_;
  int64 next_step_;
};
}  // namespace

SearchMonitor* Solver::MakeLubyRestart(int scale_factor) {
  return RevAlloc(new LubyRestart(this, scale_factor));
}

// ----- Constant Restart -----

namespace {
class ConstantRestart : public SearchMonitor {
 public:
  ConstantRestart(Solver* const s, int frequency)
      : SearchMonitor(s), frequency_(frequency), current_fails_(0) {
    CHECK_GE(frequency, 1);
  }

  ~ConstantRestart() override {}

  void BeginFail() override {
    if (++current_fails_ >= frequency_) {
      current_fails_ = 0;
      solver()->RestartCurrentSearch();
    }
  }

  std::string DebugString() const override {
    return StringPrintf("ConstantRestart(%i)", frequency_);
  }

 private:
  const int frequency_;
  int64 current_fails_;
};
}  // namespace

SearchMonitor* Solver::MakeConstantRestart(int frequency) {
  return RevAlloc(new ConstantRestart(this, frequency));
}

// ---------- Symmetry Breaking ----------

// The symmetry manager maintains a list of problem symmetries.  Each
// symmetry is called on each decision and should return a term
// representing the boolean status of the symmetrical decision.
// e.g. : the decision is x == 3, the symmetrical decision is y == 5
// then the symmetry breaker should use
// AddIntegerVariableEqualValueClause(y, 5).  Once this is done, upon
// refutation, for each symmetry breaker, the system adds a constraint
// that will forbid the symmetrical variation of the current explored
// search tree. This constraint can be expressed very simply just by
// keeping the list of current symmetrical decisions.
//
// This is called Symmetry Breaking During Search (Ian Gent, Barbara
// Smith, ECAI 2000).
// http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.42.3788&rep=rep1&type=pdf
//
class SymmetryManager : public SearchMonitor {
 public:
  SymmetryManager(Solver* const s,
                  const std::vector<SymmetryBreaker*>& visitors)
      : SearchMonitor(s),
        visitors_(visitors),
        clauses_(visitors.size()),
        decisions_(visitors.size()),
        directions_(visitors.size()) {  // false = left.
    for (int i = 0; i < visitors_.size(); ++i) {
      visitors_[i]->set_symmetry_manager_and_index(this, i);
    }
  }

  ~SymmetryManager() override {}

  void EndNextDecision(DecisionBuilder* const db, Decision* const d) override {
    if (d) {
      for (int i = 0; i < visitors_.size(); ++i) {
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

  void RefuteDecision(Decision* d) override {
    for (int i = 0; i < visitors_.size(); ++i) {
      if (decisions_[i].Last() != nullptr && decisions_[i].LastValue() == d) {
        CheckSymmetries(i);
      }
    }
  }

  // TODO(user) : Improve speed, cache previous min and build them
  // incrementally.
  void CheckSymmetries(int index) {
    SimpleRevFIFO<IntVar*>::Iterator tmp(&clauses_[index]);
    SimpleRevFIFO<bool>::Iterator tmp_dir(&directions_[index]);
    Constraint* ct = nullptr;
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
    DCHECK(ct != nullptr);
    solver()->AddConstraint(ct);
  }

  void AddTermToClause(SymmetryBreaker* const visitor, IntVar* const term) {
    clauses_[visitor->index_in_symmetry_manager()].Push(solver(), term);
  }

  std::string DebugString() const override { return "SymmetryManager"; }

 private:
  const std::vector<SymmetryBreaker*> visitors_;
  std::vector<SimpleRevFIFO<IntVar*> > clauses_;
  std::vector<SimpleRevFIFO<Decision*> > decisions_;
  std::vector<SimpleRevFIFO<bool> > directions_;
};

// ----- Symmetry Breaker -----

void SymmetryBreaker::AddIntegerVariableEqualValueClause(IntVar* const var,
                                                         int64 value) {
  CHECK(var != nullptr);
  Solver* const solver = var->solver();
  IntVar* const term = solver->MakeIsEqualCstVar(var, value);
  symmetry_manager()->AddTermToClause(this, term);
}

void SymmetryBreaker::AddIntegerVariableGreaterOrEqualValueClause(
    IntVar* const var, int64 value) {
  CHECK(var != nullptr);
  Solver* const solver = var->solver();
  IntVar* const term = solver->MakeIsGreaterOrEqualCstVar(var, value);
  symmetry_manager()->AddTermToClause(this, term);
}

void SymmetryBreaker::AddIntegerVariableLessOrEqualValueClause(
    IntVar* const var, int64 value) {
  CHECK(var != nullptr);
  Solver* const solver = var->solver();
  IntVar* const term = solver->MakeIsLessOrEqualCstVar(var, value);
  symmetry_manager()->AddTermToClause(this, term);
}

// ----- API -----

SearchMonitor* Solver::MakeSymmetryManager(
    const std::vector<SymmetryBreaker*>& visitors) {
  return RevAlloc(new SymmetryManager(this, visitors));
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
