// Copyright 2010-2025 Google LLC
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
#include <cstdint>
#include <functional>
#include <limits>
#include <list>
#include <memory>
#include <queue>
#include <random>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/flags/flag.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "ortools/base/bitmap.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/map_util.h"
#include "ortools/base/timer.h"
#include "ortools/base/types.h"
#include "ortools/constraint_solver/assignment.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/constraint_solver/constraint_solveri.h"
#include "ortools/constraint_solver/search_limit.pb.h"
#include "ortools/util/bitset.h"
#include "ortools/util/saturated_arithmetic.h"
#include "ortools/util/stats.h"
#include "ortools/util/string_array.h"

ABSL_FLAG(bool, cp_use_sparse_gls_penalties, false,
          "Use sparse implementation to store Guided Local Search penalties");
ABSL_FLAG(bool, cp_log_to_vlog, false,
          "Whether search related logging should be "
          "vlog or info.");
ABSL_FLAG(int64_t, cp_large_domain_no_splitting_limit, 0xFFFFF,
          "Size limit to allow holes in variables from the strategy.");
namespace operations_research {

// ---------- Search Log ---------

SearchLog::SearchLog(Solver* solver, std::vector<IntVar*> vars,
                     std::string vars_name, std::vector<double> scaling_factors,
                     std::vector<double> offsets,
                     std::function<std::string()> display_callback,
                     bool display_on_new_solutions_only, int period)
    : SearchMonitor(solver),
      period_(period),
      timer_(new WallTimer),
      vars_(std::move(vars)),
      vars_name_(std::move(vars_name)),
      scaling_factors_(std::move(scaling_factors)),
      offsets_(std::move(offsets)),
      display_callback_(std::move(display_callback)),
      display_on_new_solutions_only_(display_on_new_solutions_only),
      nsol_(0),
      objective_min_(vars_.size(), std::numeric_limits<int64_t>::max()),
      objective_max_(vars_.size(), std::numeric_limits<int64_t>::min()),
      min_right_depth_(std::numeric_limits<int32_t>::max()),
      max_depth_(0),
      sliding_min_depth_(0),
      sliding_max_depth_(0) {}

SearchLog::~SearchLog() {}

std::string SearchLog::DebugString() const { return "SearchLog"; }

void SearchLog::EnterSearch() {
  const std::string buffer =
      (!display_on_new_solutions_only_ && display_callback_ != nullptr)
          ? absl::StrFormat("Start search (%s, %s)", MemoryUsage(),
                            display_callback_())
          : absl::StrFormat("Start search (%s)", MemoryUsage());
  OutputLine(buffer);
  timer_->Restart();
  min_right_depth_ = std::numeric_limits<int32_t>::max();
  neighbors_offset_ = solver()->accepted_neighbors();
  nsol_ = 0;
  last_objective_value_.clear();
  last_objective_timestamp_ = timer_->GetDuration();
}

void SearchLog::ExitSearch() {
  const int64_t branches = solver()->branches();
  int64_t ms = absl::ToInt64Milliseconds(timer_->GetDuration());
  if (ms == 0) {
    ms = 1;
  }
  const std::string buffer = absl::StrFormat(
      "End search (time = %d ms, branches = %d, failures = %d, %s, speed = %d "
      "branches/s)",
      ms, branches, solver()->failures(), MemoryUsage(), branches * 1000 / ms);
  OutputLine(buffer);
}

bool SearchLog::AtSolution() {
  Maintain();
  const int depth = solver()->SearchDepth();
  std::string obj_str = "";
  std::vector<int64_t> current;
  bool objective_updated = false;
  const auto scaled_str = [this](absl::Span<const int64_t> values) {
    std::vector<std::string> value_strings(values.size());
    for (int i = 0; i < values.size(); ++i) {
      if (scaling_factors_[i] != 1.0 || offsets_[i] != 0.0) {
        value_strings[i] =
            absl::StrFormat("%d (%.8lf)", values[i],
                            scaling_factors_[i] * (values[i] + offsets_[i]));
      } else {
        value_strings[i] = absl::StrCat(values[i]);
      }
    }
    return absl::StrJoin(value_strings, ", ");
  };
  bool all_vars_bound = !vars_.empty();
  for (IntVar* var : vars_) {
    all_vars_bound &= var->Bound();
  }
  if (all_vars_bound) {
    for (IntVar* var : vars_) {
      current.push_back(var->Value());
      objective_updated = true;
    }
    absl::StrAppend(&obj_str,
                    vars_name_.empty() ? "" : absl::StrCat(vars_name_, " = "),
                    scaled_str(current), ", ");
  } else {
    current.push_back(solver()->GetOrCreateLocalSearchState()->ObjectiveMin());
    absl::StrAppend(&obj_str, "objective = ", scaled_str(current), ", ");
    objective_updated = true;
  }
  const absl::Duration now = timer_->GetDuration();
  if (objective_updated) {
    if (!last_objective_value_.empty()) {
      const int64_t elapsed_ms =
          absl::ToInt64Milliseconds(now - last_objective_timestamp_);
      for (int i = 0; i < current.size(); ++i) {
        const double improvement_rate =
            100.0 * 1000.0 * (current[i] - last_objective_value_[i]) /
            std::max<int64_t>(1, last_objective_value_[i] * elapsed_ms);
        absl::StrAppend(&obj_str, "improvement rate = ", improvement_rate,
                        "%/s, ");
        last_objective_value_[i] = current[i];
      }
    } else {
      last_objective_value_ = current;
    }
    last_objective_timestamp_ = now;
    if (!objective_min_.empty() &&
        std::lexicographical_compare(objective_min_.begin(),
                                     objective_min_.end(), current.begin(),
                                     current.end())) {
      absl::StrAppend(&obj_str,
                      vars_name_.empty() ? "" : absl::StrCat(vars_name_, " "),
                      "minimum = ", scaled_str(objective_min_), ", ");

    } else {
      objective_min_ = current;
    }
    if (!objective_max_.empty() &&
        std::lexicographical_compare(current.begin(), current.end(),
                                     objective_max_.begin(),
                                     objective_max_.end())) {
      absl::StrAppend(&obj_str,
                      vars_name_.empty() ? "" : absl::StrCat(vars_name_, " "),
                      "maximum = ", scaled_str(objective_max_), ", ");
    } else {
      objective_max_ = current;
    }
  }
  std::string log;
  absl::StrAppendFormat(&log,
                        "Solution #%d (%stime = %d ms, branches = %d,"
                        " failures = %d, depth = %d",
                        nsol_++, obj_str, absl::ToInt64Milliseconds(now),
                        solver()->branches(), solver()->failures(), depth);
  if (!solver()->SearchContext().empty()) {
    absl::StrAppendFormat(&log, ", %s", solver()->SearchContext());
  }
  if (solver()->accepted_neighbors() != neighbors_offset_) {
    absl::StrAppendFormat(&log,
                          ", neighbors = %d, filtered neighbors = %d,"
                          " accepted neighbors = %d",
                          solver()->neighbors(), solver()->filtered_neighbors(),
                          solver()->accepted_neighbors());
  }
  absl::StrAppendFormat(&log, ", %s", MemoryUsage());
  const int progress = solver()->TopProgressPercent();
  if (progress != SearchMonitor::kNoProgress) {
    absl::StrAppendFormat(&log, ", limit = %d%%", progress);
  }
  if (display_callback_) {
    absl::StrAppendFormat(&log, ", %s", display_callback_());
  }
  log.append(")");
  OutputLine(log);
  return false;
}

void SearchLog::AcceptUncheckedNeighbor() { AtSolution(); }

void SearchLog::BeginFail() { Maintain(); }

void SearchLog::NoMoreSolutions() {
  std::string buffer = absl::StrFormat(
      "Finished search tree (time = %d ms, branches = %d,"
      " failures = %d",
      absl::ToInt64Milliseconds(timer_->GetDuration()), solver()->branches(),
      solver()->failures());
  if (solver()->neighbors() != 0) {
    absl::StrAppendFormat(&buffer,
                          ", neighbors = %d, filtered neighbors = %d,"
                          " accepted neigbors = %d",
                          solver()->neighbors(), solver()->filtered_neighbors(),
                          solver()->accepted_neighbors());
  }
  absl::StrAppendFormat(&buffer, ", %s", MemoryUsage());
  if (!display_on_new_solutions_only_ && display_callback_) {
    absl::StrAppendFormat(&buffer, ", %s", display_callback_());
  }
  buffer.append(")");
  OutputLine(buffer);
}

void SearchLog::ApplyDecision(Decision* const) {
  Maintain();
  const int64_t b = solver()->branches();
  if (b % period_ == 0 && b > 0) {
    OutputDecision();
  }
}

void SearchLog::RefuteDecision(Decision* const decision) {
  min_right_depth_ = std::min(min_right_depth_, solver()->SearchDepth());
  ApplyDecision(decision);
}

void SearchLog::OutputDecision() {
  std::string buffer = absl::StrFormat(
      "%d branches, %d ms, %d failures", solver()->branches(),
      absl::ToInt64Milliseconds(timer_->GetDuration()), solver()->failures());
  if (min_right_depth_ != std::numeric_limits<int32_t>::max() &&
      max_depth_ != 0) {
    const int depth = solver()->SearchDepth();
    absl::StrAppendFormat(&buffer, ", tree pos=%d/%d/%d minref=%d max=%d",
                          sliding_min_depth_, depth, sliding_max_depth_,
                          min_right_depth_, max_depth_);
    sliding_min_depth_ = depth;
    sliding_max_depth_ = depth;
  }
  if (!objective_min_.empty() &&
      objective_min_[0] != std::numeric_limits<int64_t>::max() &&
      objective_max_[0] != std::numeric_limits<int64_t>::min()) {
    const std::string name =
        vars_name_.empty() ? "" : absl::StrCat(" ", vars_name_);
    absl::StrAppendFormat(&buffer,
                          ",%s minimum = %d"
                          ",%s maximum = %d",
                          name, objective_min_[0], name, objective_max_[0]);
  }
  const int progress = solver()->TopProgressPercent();
  if (progress != SearchMonitor::kNoProgress) {
    absl::StrAppendFormat(&buffer, ", limit = %d%%", progress);
  }
  OutputLine(buffer);
}

void SearchLog::Maintain() {
  const int current_depth = solver()->SearchDepth();
  sliding_min_depth_ = std::min(current_depth, sliding_min_depth_);
  sliding_max_depth_ = std::max(current_depth, sliding_max_depth_);
  max_depth_ = std::max(current_depth, max_depth_);
}

void SearchLog::BeginInitialPropagation() { tick_ = timer_->GetDuration(); }

void SearchLog::EndInitialPropagation() {
  const int64_t delta = std::max<int64_t>(
      absl::ToInt64Milliseconds(timer_->GetDuration() - tick_), 0);
  const std::string buffer = absl::StrFormat(
      "Root node processed (time = %d ms, constraints = %d, %s)", delta,
      solver()->constraints(), MemoryUsage());
  OutputLine(buffer);
}

void SearchLog::OutputLine(const std::string& line) {
  if (absl::GetFlag(FLAGS_cp_log_to_vlog)) {
    VLOG(1) << line;
  } else {
    LOG(INFO) << line;
  }
}

std::string SearchLog::MemoryUsage() {
  return absl::StrCat("memory used = ", operations_research::MemoryUsage());
}

SearchMonitor* Solver::MakeSearchLog(int branch_period) {
  return MakeSearchLog(branch_period, std::vector<IntVar*>{}, nullptr);
}

SearchMonitor* Solver::MakeSearchLog(int branch_period, IntVar* var) {
  return MakeSearchLog(branch_period, std::vector<IntVar*>{var}, nullptr);
}

SearchMonitor* Solver::MakeSearchLog(
    int branch_period, std::function<std::string()> display_callback) {
  return MakeSearchLog(branch_period, std::vector<IntVar*>{},
                       std::move(display_callback));
}

SearchMonitor* Solver::MakeSearchLog(
    int branch_period, IntVar* var,
    std::function<std::string()> display_callback) {
  return MakeSearchLog(branch_period, std::vector<IntVar*>{var},
                       std::move(display_callback));
}

SearchMonitor* Solver::MakeSearchLog(
    int branch_period, std::vector<IntVar*> vars,
    std::function<std::string()> display_callback) {
  return RevAlloc(new SearchLog(this, std::move(vars), "", {1.0}, {0.0},
                                std::move(display_callback), true,
                                branch_period));
}

SearchMonitor* Solver::MakeSearchLog(int branch_period, OptimizeVar* opt_var) {
  return MakeSearchLog(branch_period, opt_var, nullptr);
}

SearchMonitor* Solver::MakeSearchLog(
    int branch_period, OptimizeVar* opt_var,
    std::function<std::string()> display_callback) {
  std::vector<IntVar*> vars = opt_var->objective_vars();
  std::vector<double> scaling_factors(vars.size(), 1.0);
  std::vector<double> offsets(vars.size(), 0.0);
  return RevAlloc(new SearchLog(
      this, std::move(vars), opt_var->Name(), std::move(scaling_factors),
      std::move(offsets), std::move(display_callback),
      /*display_on_new_solutions_only=*/true, branch_period));
}

SearchMonitor* Solver::MakeSearchLog(SearchLogParameters parameters) {
  DCHECK(parameters.objective == nullptr || parameters.variables.empty())
      << "Either variables are empty or objective is nullptr.";
  std::vector<IntVar*> vars = parameters.objective != nullptr
                                  ? parameters.objective->objective_vars()
                                  : std::move(parameters.variables);
  std::vector<double> scaling_factors = std::move(parameters.scaling_factors);
  scaling_factors.resize(vars.size(), 1.0);
  std::vector<double> offsets = std::move(parameters.offsets);
  offsets.resize(vars.size(), 0.0);
  return RevAlloc(new SearchLog(
      this, std::move(vars), "", std::move(scaling_factors), std::move(offsets),
      std::move(parameters.display_callback),
      parameters.display_on_new_solutions_only, parameters.branch_period));
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
  void Install() override;

 private:
  const std::function<void()> callback_;
};

bool AtSolutionCallback::AtSolution() {
  callback_();
  return false;
}

void AtSolutionCallback::Install() {
  ListenToEvent(Solver::MonitorEvent::kAtSolution);
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
  void Install() override;

 private:
  const std::function<void()> callback_;
};

void EnterSearchCallback::EnterSearch() { callback_(); }

void EnterSearchCallback::Install() {
  ListenToEvent(Solver::MonitorEvent::kEnterSearch);
}

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
  void Install() override;

 private:
  const std::function<void()> callback_;
};

void ExitSearchCallback::ExitSearch() { callback_(); }

void ExitSearchCallback::Install() {
  ListenToEvent(Solver::MonitorEvent::kExitSearch);
}

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
  void Add(DecisionBuilder* db);
  void AppendMonitors(Solver* solver,
                      std::vector<SearchMonitor*>* monitors) override;
  void Accept(ModelVisitor* visitor) const override;

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
  Decision* Next(Solver* s) override;
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
  return absl::StrFormat("ComposeDecisionBuilder(%s)",
                         JoinDebugStringPtr(builders_, ", "));
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
  explicit TryDecision(TryDecisionBuilder* try_builder);
  ~TryDecision() override;
  void Apply(Solver* solver) override;
  void Refute(Solver* solver) override;
  std::string DebugString() const override { return "TryDecision"; }

 private:
  TryDecisionBuilder* const try_builder_;
};

class TryDecisionBuilder : public CompositeDecisionBuilder {
 public:
  TryDecisionBuilder();
  explicit TryDecisionBuilder(const std::vector<DecisionBuilder*>& dbs);
  ~TryDecisionBuilder() override;
  Decision* Next(Solver* solver) override;
  std::string DebugString() const override;
  void AdvanceToNextBuilder(Solver* solver);

 private:
  TryDecision try_decision_;
  int current_builder_;
  bool start_new_builder_;
};

TryDecision::TryDecision(TryDecisionBuilder* const try_builder)
    : try_builder_(try_builder) {}

TryDecision::~TryDecision() {}

void TryDecision::Apply(Solver* const) {}

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
  return absl::StrFormat("TryDecisionBuilder(%s)",
                         JoinDebugStringPtr(builders_, ", "));
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

  virtual int64_t SelectValue(const IntVar* v, int64_t id) = 0;

  // Returns -1 if no variable are suitable.
  virtual int64_t ChooseVariable() = 0;

  int64_t ChooseVariableWrapper() {
    int64_t i;
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
  Rev<int64_t> first_unbound_;
  Rev<int64_t> last_unbound_;
};

// ----- Choose first unbound --

int64_t ChooseFirstUnbound(Solver*, const std::vector<IntVar*>& vars,
                           int64_t first_unbound, int64_t last_unbound) {
  for (int64_t i = first_unbound; i <= last_unbound; ++i) {
    if (!vars[i]->Bound()) {
      return i;
    }
  }
  return -1;
}

// ----- Choose Min Size Lowest Min -----

int64_t ChooseMinSizeLowestMin(Solver*, const std::vector<IntVar*>& vars,
                               int64_t first_unbound, int64_t last_unbound) {
  uint64_t best_size = std::numeric_limits<uint64_t>::max();
  int64_t best_min = std::numeric_limits<int64_t>::max();
  int64_t best_index = -1;
  for (int64_t i = first_unbound; i <= last_unbound; ++i) {
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

int64_t ChooseMinSizeHighestMin(Solver*, const std::vector<IntVar*>& vars,
                                int64_t first_unbound, int64_t last_unbound) {
  uint64_t best_size = std::numeric_limits<uint64_t>::max();
  int64_t best_min = std::numeric_limits<int64_t>::min();
  int64_t best_index = -1;
  for (int64_t i = first_unbound; i <= last_unbound; ++i) {
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

int64_t ChooseMinSizeLowestMax(Solver*, const std::vector<IntVar*>& vars,
                               int64_t first_unbound, int64_t last_unbound) {
  uint64_t best_size = std::numeric_limits<uint64_t>::max();
  int64_t best_max = std::numeric_limits<int64_t>::max();
  int64_t best_index = -1;
  for (int64_t i = first_unbound; i <= last_unbound; ++i) {
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

int64_t ChooseMinSizeHighestMax(Solver*, const std::vector<IntVar*>& vars,
                                int64_t first_unbound, int64_t last_unbound) {
  uint64_t best_size = std::numeric_limits<uint64_t>::max();
  int64_t best_max = std::numeric_limits<int64_t>::min();
  int64_t best_index = -1;
  for (int64_t i = first_unbound; i <= last_unbound; ++i) {
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

int64_t ChooseLowestMin(Solver*, const std::vector<IntVar*>& vars,
                        int64_t first_unbound, int64_t last_unbound) {
  int64_t best_min = std::numeric_limits<int64_t>::max();
  int64_t best_index = -1;
  for (int64_t i = first_unbound; i <= last_unbound; ++i) {
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

int64_t ChooseHighestMax(Solver*, const std::vector<IntVar*>& vars,
                         int64_t first_unbound, int64_t last_unbound) {
  int64_t best_max = std::numeric_limits<int64_t>::min();
  int64_t best_index = -1;
  for (int64_t i = first_unbound; i <= last_unbound; ++i) {
    IntVar* const var = vars[i];
    if (!var->Bound()) {
      if (var->Max() > best_max) {
        best_max = var->Max();
        best_index = i;
      }
    }
  }
  return best_index;
}

// ----- Choose Lowest Size --

int64_t ChooseMinSize(Solver*, const std::vector<IntVar*>& vars,
                      int64_t first_unbound, int64_t last_unbound) {
  uint64_t best_size = std::numeric_limits<uint64_t>::max();
  int64_t best_index = -1;
  for (int64_t i = first_unbound; i <= last_unbound; ++i) {
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

int64_t ChooseMaxSize(Solver*, const std::vector<IntVar*>& vars,
                      int64_t first_unbound, int64_t last_unbound) {
  uint64_t best_size = 0;
  int64_t best_index = -1;
  for (int64_t i = first_unbound; i <= last_unbound; ++i) {
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
    for (int64_t i = 0; i < vars.size(); ++i) {
      iterators_[i] = vars[i]->MakeDomainIterator(true);
    }
  }
  ~HighestRegretSelectorOnMin() override {};
  int64_t Choose(Solver* s, const std::vector<IntVar*>& vars,
                 int64_t first_unbound, int64_t last_unbound);
  std::string DebugString() const override { return "MaxRegretSelector"; }

  int64_t ComputeRegret(IntVar* var, int64_t index) const {
    DCHECK(!var->Bound());
    const int64_t vmin = var->Min();
    IntVarIterator* const iterator = iterators_[index];
    iterator->Init();
    iterator->Next();
    return iterator->Value() - vmin;
  }

 private:
  std::vector<IntVarIterator*> iterators_;
};

int64_t HighestRegretSelectorOnMin::Choose(Solver* const,
                                           const std::vector<IntVar*>& vars,
                                           int64_t first_unbound,
                                           int64_t last_unbound) {
  int64_t best_regret = 0;
  int64_t index = -1;
  for (int64_t i = first_unbound; i <= last_unbound; ++i) {
    IntVar* const var = vars[i];
    if (!var->Bound()) {
      const int64_t regret = ComputeRegret(var, i);
      if (regret > best_regret) {
        best_regret = regret;
        index = i;
      }
    }
  }
  return index;
}

// ----- Choose random unbound --

int64_t ChooseRandom(Solver* solver, const std::vector<IntVar*>& vars,
                     int64_t first_unbound, int64_t last_unbound) {
  const int64_t span = last_unbound - first_unbound + 1;
  const int64_t shift = solver->Rand32(span);
  for (int64_t i = 0; i < span; ++i) {
    const int64_t index = (i + shift) % span + first_unbound;
    if (!vars[index]->Bound()) {
      return index;
    }
  }
  return -1;
}

// ----- Choose min eval -----

class CheapestVarSelector : public BaseObject {
 public:
  explicit CheapestVarSelector(std::function<int64_t(int64_t)> var_evaluator)
      : var_evaluator_(std::move(var_evaluator)) {}
  ~CheapestVarSelector() override {};
  int64_t Choose(Solver* s, const std::vector<IntVar*>& vars,
                 int64_t first_unbound, int64_t last_unbound);
  std::string DebugString() const override { return "CheapestVarSelector"; }

 private:
  std::function<int64_t(int64_t)> var_evaluator_;
};

int64_t CheapestVarSelector::Choose(Solver* const,
                                    const std::vector<IntVar*>& vars,
                                    int64_t first_unbound,
                                    int64_t last_unbound) {
  int64_t best_eval = std::numeric_limits<int64_t>::max();
  int64_t index = -1;
  for (int64_t i = first_unbound; i <= last_unbound; ++i) {
    if (!vars[i]->Bound()) {
      const int64_t eval = var_evaluator_(i);
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
  PathSelector() : first_(std::numeric_limits<int64_t>::max()) {}
  ~PathSelector() override {};
  int64_t Choose(Solver* s, const std::vector<IntVar*>& vars);
  std::string DebugString() const override { return "ChooseNextOnPath"; }

 private:
  bool UpdateIndex(const std::vector<IntVar*>& vars, int64_t* index) const;
  bool FindPathStart(const std::vector<IntVar*>& vars, int64_t* index) const;

  Rev<int64_t> first_;
};

int64_t PathSelector::Choose(Solver* const s,
                             const std::vector<IntVar*>& vars) {
  int64_t index = first_.Value();
  if (!UpdateIndex(vars, &index)) {
    return -1;
  }
  int64_t count = 0;
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
                               int64_t* index) const {
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
                                 int64_t* index) const {
  // Try to extend an existing path
  for (int64_t i = vars.size() - 1; i >= 0; --i) {
    if (vars[i]->Bound()) {
      const int64_t next = vars[i]->Value();
      if (next < vars.size() && !vars[next]->Bound()) {
        *index = next;
        return true;
      }
    }
  }
  // Pick path start
  for (int64_t i = vars.size() - 1; i >= 0; --i) {
    if (!vars[i]->Bound()) {
      bool has_possible_prev = false;
      for (int64_t j = 0; j < vars.size(); ++j) {
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
  for (int64_t i = 0; i < vars.size(); ++i) {
    if (!vars[i]->Bound()) {
      *index = i;
      return true;
    }
  }
  return false;
}

// ----- Select min -----

int64_t SelectMinValue(const IntVar* v, int64_t) { return v->Min(); }

// ----- Select max -----

int64_t SelectMaxValue(const IntVar* v, int64_t) { return v->Max(); }

// ----- Select random -----

int64_t SelectRandomValue(const IntVar* v, int64_t) {
  const uint64_t span = v->Max() - v->Min() + 1;
  if (span > absl::GetFlag(FLAGS_cp_large_domain_no_splitting_limit)) {
    // Do not create holes in large domains.
    return v->Min();
  }
  const uint64_t size = v->Size();
  Solver* const s = v->solver();
  if (size > span / 4) {  // Dense enough, we can try to find the
    // value randomly.
    for (;;) {
      const int64_t value = v->Min() + s->Rand64(span);
      if (v->Contains(value)) {
        return value;
      }
    }
  } else {  // Not dense enough, we will count.
    int64_t index = s->Rand64(size);
    if (index <= size / 2) {
      for (int64_t i = v->Min(); i <= v->Max(); ++i) {
        if (v->Contains(i)) {
          if (--index == 0) {
            return i;
          }
        }
      }
      CHECK_LE(index, 0);
    } else {
      for (int64_t i = v->Max(); i > v->Min(); --i) {
        if (v->Contains(i)) {
          if (--index == 0) {
            return i;
          }
        }
      }
      CHECK_LE(index, 0);
    }
  }
  return 0;
}

// ----- Select center -----

int64_t SelectCenterValue(const IntVar* v, int64_t) {
  const int64_t vmin = v->Min();
  const int64_t vmax = v->Max();
  if (vmax - vmin > absl::GetFlag(FLAGS_cp_large_domain_no_splitting_limit)) {
    // Do not create holes in large domains.
    return vmin;
  }
  const int64_t mid = (vmin + vmax) / 2;
  if (v->Contains(mid)) {
    return mid;
  }
  const int64_t diameter = vmax - mid;  // always greater than mid - vmix.
  for (int64_t i = 1; i <= diameter; ++i) {
    if (v->Contains(mid - i)) {
      return mid - i;
    }
    if (v->Contains(mid + i)) {
      return mid + i;
    }
  }
  return 0;
}

// ----- Select center -----

int64_t SelectSplitValue(const IntVar* v, int64_t) {
  const int64_t vmin = v->Min();
  const int64_t vmax = v->Max();
  const uint64_t delta = vmax - vmin;
  const int64_t mid = vmin + delta / 2;
  return mid;
}

// ----- Select the value yielding the cheapest "eval" for a var -----

class CheapestValueSelector : public BaseObject {
 public:
  CheapestValueSelector(std::function<int64_t(int64_t, int64_t)> eval,
                        std::function<int64_t(int64_t)> tie_breaker)
      : eval_(std::move(eval)), tie_breaker_(std::move(tie_breaker)) {}
  ~CheapestValueSelector() override {}
  int64_t Select(const IntVar* v, int64_t id);
  std::string DebugString() const override { return "CheapestValue"; }

 private:
  std::function<int64_t(int64_t, int64_t)> eval_;
  std::function<int64_t(int64_t)> tie_breaker_;
  std::vector<int64_t> cache_;
};

int64_t CheapestValueSelector::Select(const IntVar* v, int64_t id) {
  cache_.clear();
  int64_t best = std::numeric_limits<int64_t>::max();
  std::unique_ptr<IntVarIterator> it(v->MakeDomainIterator(false));
  for (const int64_t i : InitAndGetValues(it.get())) {
    int64_t eval = eval_(id, i);
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
  int64_t Select(const IntVar* v, int64_t id);
  std::string DebugString() const override {
    return "BestValueByComparisonSelector";
  }

 private:
  Solver::VariableValueComparator comparator_;
};

int64_t BestValueByComparisonSelector::Select(const IntVar* v, int64_t id) {
  std::unique_ptr<IntVarIterator> it(v->MakeDomainIterator(false));
  it->Init();
  DCHECK(it->Ok());  // At least one value.
  int64_t best_value = it->Value();
  for (it->Next(); it->Ok(); it->Next()) {
    const int64_t candidate_value = it->Value();
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
  int64_t SelectValue(const IntVar* var, int64_t id) override {
    return value_selector_(var, id);
  }
  int64_t ChooseVariable() override {
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
  return absl::StrFormat("%s(%s)", name_, JoinDebugStringPtr(vars_, ", "));
}

// ----- Base Global Evaluator-based selector -----

class BaseEvaluatorSelector : public BaseVariableAssignmentSelector {
 public:
  BaseEvaluatorSelector(Solver* solver, const std::vector<IntVar*>& vars,
                        std::function<int64_t(int64_t, int64_t)> evaluator);
  ~BaseEvaluatorSelector() override {}

 protected:
  struct Element {
    Element() : var(0), value(0) {}
    Element(int64_t i, int64_t j) : var(i), value(j) {}
    int64_t var;
    int64_t value;
  };

  std::string DebugStringInternal(absl::string_view name) const {
    return absl::StrFormat("%s(%s)", name, JoinDebugStringPtr(vars_, ", "));
  }

  std::function<int64_t(int64_t, int64_t)> evaluator_;
};

BaseEvaluatorSelector::BaseEvaluatorSelector(
    Solver* solver, const std::vector<IntVar*>& vars,
    std::function<int64_t(int64_t, int64_t)> evaluator)
    : BaseVariableAssignmentSelector(solver, vars),
      evaluator_(std::move(evaluator)) {}

// ----- Global Dynamic Evaluator-based selector -----

class DynamicEvaluatorSelector : public BaseEvaluatorSelector {
 public:
  DynamicEvaluatorSelector(Solver* solver, const std::vector<IntVar*>& vars,
                           std::function<int64_t(int64_t, int64_t)> evaluator,
                           std::function<int64_t(int64_t)> tie_breaker);
  ~DynamicEvaluatorSelector() override {}
  int64_t SelectValue(const IntVar* var, int64_t id) override;
  int64_t ChooseVariable() override;
  std::string DebugString() const override;

 private:
  int64_t first_;
  std::function<int64_t(int64_t)> tie_breaker_;
  std::vector<Element> cache_;
};

DynamicEvaluatorSelector::DynamicEvaluatorSelector(
    Solver* solver, const std::vector<IntVar*>& vars,
    std::function<int64_t(int64_t, int64_t)> evaluator,
    std::function<int64_t(int64_t)> tie_breaker)
    : BaseEvaluatorSelector(solver, vars, std::move(evaluator)),
      first_(-1),
      tie_breaker_(std::move(tie_breaker)) {}

int64_t DynamicEvaluatorSelector::SelectValue(const IntVar*, int64_t) {
  return cache_[first_].value;
}

int64_t DynamicEvaluatorSelector::ChooseVariable() {
  int64_t best_evaluation = std::numeric_limits<int64_t>::max();
  cache_.clear();
  for (int64_t i = 0; i < vars_.size(); ++i) {
    const IntVar* const var = vars_[i];
    if (!var->Bound()) {
      std::unique_ptr<IntVarIterator> it(var->MakeDomainIterator(false));
      for (const int64_t j : InitAndGetValues(it.get())) {
        const int64_t value = evaluator_(i, j);
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
  StaticEvaluatorSelector(
      Solver* solver, const std::vector<IntVar*>& vars,
      const std::function<int64_t(int64_t, int64_t)>& evaluator);
  ~StaticEvaluatorSelector() override {}
  int64_t SelectValue(const IntVar* var, int64_t id) override;
  int64_t ChooseVariable() override;
  std::string DebugString() const override;

 private:
  class Compare {
   public:
    explicit Compare(std::function<int64_t(int64_t, int64_t)> evaluator)
        : evaluator_(std::move(evaluator)) {}
    bool operator()(const Element& lhs, const Element& rhs) const {
      const int64_t value_lhs = Value(lhs);
      const int64_t value_rhs = Value(rhs);
      return value_lhs < value_rhs ||
             (value_lhs == value_rhs &&
              (lhs.var < rhs.var ||
               (lhs.var == rhs.var && lhs.value < rhs.value)));
    }
    int64_t Value(const Element& element) const {
      return evaluator_(element.var, element.value);
    }

   private:
    std::function<int64_t(int64_t, int64_t)> evaluator_;
  };

  Compare comp_;
  std::vector<Element> elements_;
  int64_t first_;
};

StaticEvaluatorSelector::StaticEvaluatorSelector(
    Solver* solver, const std::vector<IntVar*>& vars,
    const std::function<int64_t(int64_t, int64_t)>& evaluator)
    : BaseEvaluatorSelector(solver, vars, evaluator),
      comp_(evaluator),
      first_(-1) {}

int64_t StaticEvaluatorSelector::SelectValue(const IntVar*, int64_t) {
  return elements_[first_].value;
}

int64_t StaticEvaluatorSelector::ChooseVariable() {
  if (first_ == -1) {  // first call to select. update assignment costs
    // Two phases: compute size then fill and sort
    int64_t element_size = 0;
    for (int64_t i = 0; i < vars_.size(); ++i) {
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
        for (const int64_t value : InitAndGetValues(it.get())) {
          elements_[count++] = Element(i, value);
        }
      }
    }
    // Sort is stable here given the tie-breaking rules in comp_.
    std::sort(elements_.begin(), elements_.end(), comp_);
    solver_->SaveAndSetValue<int64_t>(&first_, 0);
  }
  for (int64_t i = first_; i < elements_.size(); ++i) {
    const Element& element = elements_[i];
    IntVar* const var = vars_[element.var];
    if (!var->Bound() && var->Contains(element.value)) {
      solver_->SaveAndSetValue(&first_, i);
      return element.var;
    }
  }
  solver_->SaveAndSetValue(&first_, static_cast<int64_t>(elements_.size()));
  return -1;
}

std::string StaticEvaluatorSelector::DebugString() const {
  return DebugStringInternal("AssignVariablesOnStaticEvaluator");
}

// ----- AssignOneVariableValue decision -----

class AssignOneVariableValue : public Decision {
 public:
  AssignOneVariableValue(IntVar* v, int64_t val);
  ~AssignOneVariableValue() override {}
  void Apply(Solver* s) override;
  void Refute(Solver* s) override;
  std::string DebugString() const override;
  void Accept(DecisionVisitor* const visitor) const override {
    visitor->VisitSetVariableValue(var_, value_);
  }

 private:
  IntVar* const var_;
  int64_t value_;
};

AssignOneVariableValue::AssignOneVariableValue(IntVar* const v, int64_t val)
    : var_(v), value_(val) {}

std::string AssignOneVariableValue::DebugString() const {
  return absl::StrFormat("[%s == %d] or [%s != %d]", var_->DebugString(),
                         value_, var_->DebugString(), value_);
}

void AssignOneVariableValue::Apply(Solver* const) { var_->SetValue(value_); }

void AssignOneVariableValue::Refute(Solver* const) {
  var_->RemoveValue(value_);
}
}  // namespace

Decision* Solver::MakeAssignVariableValue(IntVar* const var, int64_t val) {
  return RevAlloc(new AssignOneVariableValue(var, val));
}

// ----- AssignOneVariableValueOrFail decision -----

namespace {
class AssignOneVariableValueOrFail : public Decision {
 public:
  AssignOneVariableValueOrFail(IntVar* v, int64_t value);
  ~AssignOneVariableValueOrFail() override {}
  void Apply(Solver* s) override;
  void Refute(Solver* s) override;
  std::string DebugString() const override;
  void Accept(DecisionVisitor* const visitor) const override {
    visitor->VisitSetVariableValue(var_, value_);
  }

 private:
  IntVar* const var_;
  const int64_t value_;
};

AssignOneVariableValueOrFail::AssignOneVariableValueOrFail(IntVar* const v,
                                                           int64_t value)
    : var_(v), value_(value) {}

std::string AssignOneVariableValueOrFail::DebugString() const {
  return absl::StrFormat("[%s == %d] or fail", var_->DebugString(), value_);
}

void AssignOneVariableValueOrFail::Apply(Solver* const) {
  var_->SetValue(value_);
}

void AssignOneVariableValueOrFail::Refute(Solver* const s) { s->Fail(); }
}  // namespace

Decision* Solver::MakeAssignVariableValueOrFail(IntVar* const var,
                                                int64_t value) {
  return RevAlloc(new AssignOneVariableValueOrFail(var, value));
}

// ----- AssignOneVariableValueOrDoNothing decision -----

namespace {
class AssignOneVariableValueDoNothing : public Decision {
 public:
  AssignOneVariableValueDoNothing(IntVar* const v, int64_t value)
      : var_(v), value_(value) {}
  ~AssignOneVariableValueDoNothing() override {}
  void Apply(Solver* const) override { var_->SetValue(value_); }
  void Refute(Solver* const) override {}
  std::string DebugString() const override {
    return absl::StrFormat("[%s == %d] or []", var_->DebugString(), value_);
  }
  void Accept(DecisionVisitor* const visitor) const override {
    visitor->VisitSetVariableValue(var_, value_);
  }

 private:
  IntVar* const var_;
  const int64_t value_;
};

}  // namespace

Decision* Solver::MakeAssignVariableValueOrDoNothing(IntVar* const var,
                                                     int64_t value) {
  return RevAlloc(new AssignOneVariableValueDoNothing(var, value));
}

// ----- AssignOneVariableValue decision -----

namespace {
class SplitOneVariable : public Decision {
 public:
  SplitOneVariable(IntVar* v, int64_t val, bool start_with_lower_half);
  ~SplitOneVariable() override {}
  void Apply(Solver* s) override;
  void Refute(Solver* s) override;
  std::string DebugString() const override;
  void Accept(DecisionVisitor* const visitor) const override {
    visitor->VisitSplitVariableDomain(var_, value_, start_with_lower_half_);
  }

 private:
  IntVar* const var_;
  const int64_t value_;
  const bool start_with_lower_half_;
};

SplitOneVariable::SplitOneVariable(IntVar* const v, int64_t val,
                                   bool start_with_lower_half)
    : var_(v), value_(val), start_with_lower_half_(start_with_lower_half) {}

std::string SplitOneVariable::DebugString() const {
  if (start_with_lower_half_) {
    return absl::StrFormat("[%s <= %d]", var_->DebugString(), value_);
  } else {
    return absl::StrFormat("[%s >= %d]", var_->DebugString(), value_);
  }
}

void SplitOneVariable::Apply(Solver* const) {
  if (start_with_lower_half_) {
    var_->SetMax(value_);
  } else {
    var_->SetMin(value_ + 1);
  }
}

void SplitOneVariable::Refute(Solver* const) {
  if (start_with_lower_half_) {
    var_->SetMin(value_ + 1);
  } else {
    var_->SetMax(value_);
  }
}
}  // namespace

Decision* Solver::MakeSplitVariableDomain(IntVar* const var, int64_t val,
                                          bool start_with_lower_half) {
  return RevAlloc(new SplitOneVariable(var, val, start_with_lower_half));
}

Decision* Solver::MakeVariableLessOrEqualValue(IntVar* const var,
                                               int64_t value) {
  return MakeSplitVariableDomain(var, value, true);
}

Decision* Solver::MakeVariableGreaterOrEqualValue(IntVar* const var,
                                                  int64_t value) {
  return MakeSplitVariableDomain(var, value, false);
}

// ----- AssignVariablesValues decision -----

namespace {
class AssignVariablesValues : public Decision {
 public:
  // Selects what this Decision does on the Refute() branch:
  // - kForbidAssignment: adds a constraint that forbids the assignment.
  // - kDoNothing: does nothing.
  // - kFail: fails.
  enum class RefutationBehavior { kForbidAssignment, kDoNothing, kFail };
  AssignVariablesValues(
      const std::vector<IntVar*>& vars, const std::vector<int64_t>& values,
      RefutationBehavior refutation = RefutationBehavior::kForbidAssignment);
  ~AssignVariablesValues() override {}
  void Apply(Solver* s) override;
  void Refute(Solver* s) override;
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
  const std::vector<int64_t> values_;
  const RefutationBehavior refutation_;
};

AssignVariablesValues::AssignVariablesValues(const std::vector<IntVar*>& vars,
                                             const std::vector<int64_t>& values,
                                             RefutationBehavior refutation)
    : vars_(vars), values_(values), refutation_(refutation) {}

std::string AssignVariablesValues::DebugString() const {
  std::string out;
  if (vars_.empty()) out += "do nothing";
  for (int i = 0; i < vars_.size(); ++i) {
    absl::StrAppendFormat(&out, "[%s == %d]", vars_[i]->DebugString(),
                          values_[i]);
  }
  switch (refutation_) {
    case RefutationBehavior::kForbidAssignment:
      out += " or forbid assignment";
      break;
    case RefutationBehavior::kDoNothing:
      out += " or do nothing";
      break;
    case RefutationBehavior::kFail:
      out += " or fail";
      break;
  }
  return out;
}

void AssignVariablesValues::Apply(Solver* const) {
  if (vars_.empty()) return;
  vars_[0]->FreezeQueue();
  for (int i = 0; i < vars_.size(); ++i) {
    vars_[i]->SetValue(values_[i]);
  }
  vars_[0]->UnfreezeQueue();
}

void AssignVariablesValues::Refute(Solver* const s) {
  switch (refutation_) {
    case RefutationBehavior::kForbidAssignment: {
      std::vector<IntVar*> terms;
      for (int i = 0; i < vars_.size(); ++i) {
        IntVar* term = s->MakeBoolVar();
        s->AddConstraint(s->MakeIsDifferentCstCt(vars_[i], values_[i], term));
        terms.push_back(term);
      }
      s->AddConstraint(s->MakeSumGreaterOrEqual(terms, 1));
      break;
    }
    case RefutationBehavior::kDoNothing: {
      break;
    }
    case RefutationBehavior::kFail: {
      s->Fail();
      break;
    }
  }
}
}  // namespace

Decision* Solver::MakeAssignVariablesValues(
    const std::vector<IntVar*>& vars, const std::vector<int64_t>& values) {
  CHECK_EQ(vars.size(), values.size());
  return RevAlloc(new AssignVariablesValues(
      vars, values,
      AssignVariablesValues::RefutationBehavior::kForbidAssignment));
}

Decision* Solver::MakeAssignVariablesValuesOrDoNothing(
    const std::vector<IntVar*>& vars, const std::vector<int64_t>& values) {
  CHECK_EQ(vars.size(), values.size());
  return RevAlloc(new AssignVariablesValues(
      vars, values, AssignVariablesValues::RefutationBehavior::kDoNothing));
}

Decision* Solver::MakeAssignVariablesValuesOrFail(
    const std::vector<IntVar*>& vars, const std::vector<int64_t>& values) {
  CHECK_EQ(vars.size(), values.size());
  return RevAlloc(new AssignVariablesValues(
      vars, values, AssignVariablesValues::RefutationBehavior::kFail));
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
  Decision* Next(Solver* s) override;
  std::string DebugString() const override;
  static BaseAssignVariables* MakePhase(
      Solver* s, const std::vector<IntVar*>& vars,
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
        return [selector](Solver* solver, const std::vector<IntVar*>& vars, int,
                          int) { return selector->Choose(solver, vars); };
      }
      default:
        LOG(FATAL) << "Unknown int var strategy " << str;
        return nullptr;
    }
  }

  static Solver::VariableValueSelector MakeValueSelector(
      Solver* const, Solver::IntValueStrategy val_str) {
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
    const int64_t value = selector_->SelectValue(var, id);
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
  return BaseAssignVariables::MakePhase(
      this, vars, /*var_selector=*/
      BaseAssignVariables::MakeVariableSelector(this, vars, var_str),
      /*value_selector=*/BaseAssignVariables::MakeValueSelector(this, val_str),
      /*value_selector_name=*/BuildHeuristicsName(var_str, val_str),
      ChooseMode(val_str));
}

DecisionBuilder* Solver::MakePhase(const std::vector<IntVar*>& vars,
                                   Solver::IndexEvaluator1 var_evaluator,
                                   Solver::IntValueStrategy val_str) {
  CHECK(var_evaluator != nullptr);
  CheapestVarSelector* const var_selector =
      RevAlloc(new CheapestVarSelector(std::move(var_evaluator)));
  return BaseAssignVariables::MakePhase(
      this, vars,
      /*var_selector=*/
      [var_selector](Solver* solver, const std::vector<IntVar*>& vars,
                     int first_unbound, int last_unbound) {
        return var_selector->Choose(solver, vars, first_unbound, last_unbound);
      },
      /*value_selector=*/BaseAssignVariables::MakeValueSelector(this, val_str),
      /*value_selector_name=*/"ChooseCheapestVariable_" +
          SelectValueName(val_str),
      ChooseMode(val_str));
}

DecisionBuilder* Solver::MakePhase(const std::vector<IntVar*>& vars,
                                   Solver::IntVarStrategy var_str,
                                   Solver::IndexEvaluator2 value_evaluator) {
  CheapestValueSelector* const value_selector =
      RevAlloc(new CheapestValueSelector(std::move(value_evaluator), nullptr));
  return BaseAssignVariables::MakePhase(
      this, vars,
      /*var_selector=*/
      BaseAssignVariables::MakeVariableSelector(this, vars, var_str),
      /*value_selector=*/
      [value_selector](const IntVar* var, int64_t id) {
        return value_selector->Select(var, id);
      },
      /*value_selector_name=*/ChooseVariableName(var_str) +
          "_SelectCheapestValue",
      BaseAssignVariables::ASSIGN);
}

DecisionBuilder* Solver::MakePhase(
    const std::vector<IntVar*>& vars, IntVarStrategy var_str,
    VariableValueComparator var_val1_val2_comparator) {
  BestValueByComparisonSelector* const value_selector = RevAlloc(
      new BestValueByComparisonSelector(std::move(var_val1_val2_comparator)));
  return BaseAssignVariables::MakePhase(
      this, vars, /*var_selector=*/
      BaseAssignVariables::MakeVariableSelector(this, vars, var_str),
      /*value_selector=*/
      [value_selector](const IntVar* var, int64_t id) {
        return value_selector->Select(var, id);
      },
      /*value_selector_name=*/"CheapestValue", BaseAssignVariables::ASSIGN);
}

DecisionBuilder* Solver::MakePhase(const std::vector<IntVar*>& vars,
                                   Solver::IndexEvaluator1 var_evaluator,
                                   Solver::IndexEvaluator2 value_evaluator) {
  CheapestVarSelector* const var_selector =
      RevAlloc(new CheapestVarSelector(std::move(var_evaluator)));
  CheapestValueSelector* value_selector =
      RevAlloc(new CheapestValueSelector(std::move(value_evaluator), nullptr));
  return BaseAssignVariables::MakePhase(
      this, vars, /*var_selector=*/
      [var_selector](Solver* solver, const std::vector<IntVar*>& vars,
                     int first_unbound, int last_unbound) {
        return var_selector->Choose(solver, vars, first_unbound, last_unbound);
      },
      /*value_selector=*/
      [value_selector](const IntVar* var, int64_t id) {
        return value_selector->Select(var, id);
      },
      /*value_selector_name=*/"CheapestValue", BaseAssignVariables::ASSIGN);
}

DecisionBuilder* Solver::MakePhase(const std::vector<IntVar*>& vars,
                                   Solver::IntVarStrategy var_str,
                                   Solver::IndexEvaluator2 value_evaluator,
                                   Solver::IndexEvaluator1 tie_breaker) {
  CheapestValueSelector* value_selector = RevAlloc(new CheapestValueSelector(
      std::move(value_evaluator), std::move(tie_breaker)));
  return BaseAssignVariables::MakePhase(
      this, vars, /*var_selector=*/
      BaseAssignVariables::MakeVariableSelector(this, vars, var_str),
      /*value_selector=*/
      [value_selector](const IntVar* var, int64_t id) {
        return value_selector->Select(var, id);
      },
      /*value_selector_name=*/"CheapestValue", BaseAssignVariables::ASSIGN);
}

DecisionBuilder* Solver::MakePhase(const std::vector<IntVar*>& vars,
                                   Solver::IndexEvaluator1 var_evaluator,
                                   Solver::IndexEvaluator2 value_evaluator,
                                   Solver::IndexEvaluator1 tie_breaker) {
  CheapestVarSelector* const var_selector =
      RevAlloc(new CheapestVarSelector(std::move(var_evaluator)));
  CheapestValueSelector* value_selector = RevAlloc(new CheapestValueSelector(
      std::move(value_evaluator), std::move(tie_breaker)));
  return BaseAssignVariables::MakePhase(
      this, vars, /*var_selector=*/
      [var_selector](Solver* solver, const std::vector<IntVar*>& vars,
                     int first_unbound, int last_unbound) {
        return var_selector->Choose(solver, vars, first_unbound, last_unbound);
      },
      /*value_selector=*/
      [value_selector](const IntVar* var, int64_t id) {
        return value_selector->Select(var, id);
      },
      /*value_selector_name=*/"CheapestValue", BaseAssignVariables::ASSIGN);
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
      selector =
          RevAlloc(new StaticEvaluatorSelector(this, vars, std::move(eval)));
      break;
    }
    case Solver::CHOOSE_DYNAMIC_GLOBAL_BEST: {
      selector = RevAlloc(new DynamicEvaluatorSelector(
          this, vars, std::move(eval), std::move(tie_breaker)));
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

SolutionCollector::SolutionCollector(Solver* solver,
                                     const Assignment* assignment)
    : SearchMonitor(solver),
      prototype_(assignment == nullptr ? nullptr : new Assignment(assignment)) {
}

SolutionCollector::SolutionCollector(Solver* solver)
    : SearchMonitor(solver), prototype_(new Assignment(solver)) {}

SolutionCollector::~SolutionCollector() {}

int64_t SolutionCollector::SolutionData::ObjectiveValue() const {
  return solution != nullptr ? solution->ObjectiveValue() : 0;
}

int64_t SolutionCollector::SolutionData::ObjectiveValueFromIndex(
    int index) const {
  return solution != nullptr ? solution->ObjectiveValueFromIndex(index) : 0;
}

bool SolutionCollector::SolutionData::operator<(
    const SolutionData& other) const {
  const auto fields = std::tie(solution, time, branches, failures);
  const auto other_fields =
      std::tie(other.solution, other.time, other.branches, other.failures);
  if (fields != other_fields) return fields < other_fields;
  if (solution == nullptr) {
    DCHECK_EQ(other.solution, nullptr);
    return false;
  }
  for (int i = 0; i < solution->NumObjectives(); ++i) {
    const int64_t value = solution->ObjectiveValueFromIndex(i);
    const int64_t other_value = other.solution->ObjectiveValueFromIndex(i);
    if (value != other_value) return value < other_value;
  }
  return false;
}

void SolutionCollector::Install() {
  ListenToEvent(Solver::MonitorEvent::kEnterSearch);
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

void SolutionCollector::AddObjectives(const std::vector<IntVar*>& objectives) {
  if (prototype_ != nullptr) {
    prototype_->AddObjectives(objectives);
  }
}

void SolutionCollector::EnterSearch() {
  solution_data_.clear();
  recycle_solutions_.clear();
}

void SolutionCollector::PushSolution() {
  Push(BuildSolutionDataForCurrentState());
}

void SolutionCollector::PopSolution() {
  if (!solution_data_.empty()) {
    FreeSolution(solution_data_.back().solution);
    solution_data_.pop_back();
  }
}

SolutionCollector::SolutionData
SolutionCollector::BuildSolutionDataForCurrentState() {
  Assignment* solution = nullptr;
  if (prototype_ != nullptr) {
    if (!recycle_solutions_.empty()) {
      solution = recycle_solutions_.back();
      DCHECK(solution != nullptr);
      recycle_solutions_.pop_back();
    } else {
      solution_pool_.push_back(std::make_unique<Assignment>(prototype_.get()));
      solution = solution_pool_.back().get();
    }
    solution->Store();
  }
  SolutionData data;
  data.solution = solution;
  data.time = solver()->wall_time();
  data.branches = solver()->branches();
  data.failures = solver()->failures();
  return data;
}

void SolutionCollector::FreeSolution(Assignment* solution) {
  if (solution != nullptr) {
    recycle_solutions_.push_back(solution);
  }
}

void SolutionCollector::check_index(int n) const {
  CHECK_GE(n, 0) << "wrong index in solution getter";
  CHECK_LT(n, solution_data_.size()) << "wrong index in solution getter";
}

Assignment* SolutionCollector::solution(int n) const {
  check_index(n);
  return solution_data_[n].solution;
}

Assignment* SolutionCollector::last_solution_or_null() const {
  return solution_data_.empty() ? nullptr : solution_data_.back().solution;
}

int SolutionCollector::solution_count() const { return solution_data_.size(); }

bool SolutionCollector::has_solution() const { return !solution_data_.empty(); }

int64_t SolutionCollector::wall_time(int n) const {
  check_index(n);
  return solution_data_[n].time;
}

int64_t SolutionCollector::branches(int n) const {
  check_index(n);
  return solution_data_[n].branches;
}

int64_t SolutionCollector::failures(int n) const {
  check_index(n);
  return solution_data_[n].failures;
}

int64_t SolutionCollector::objective_value(int n) const {
  check_index(n);
  return solution_data_[n].ObjectiveValue();
}

int64_t SolutionCollector::ObjectiveValueFromIndex(int n, int index) const {
  check_index(n);
  return solution_data_[n].ObjectiveValueFromIndex(index);
}

int64_t SolutionCollector::Value(int n, IntVar* var) const {
  return solution(n)->Value(var);
}

int64_t SolutionCollector::StartValue(int n, IntervalVar* var) const {
  return solution(n)->StartValue(var);
}

int64_t SolutionCollector::DurationValue(int n, IntervalVar* var) const {
  return solution(n)->DurationValue(var);
}

int64_t SolutionCollector::EndValue(int n, IntervalVar* var) const {
  return solution(n)->EndValue(var);
}

int64_t SolutionCollector::PerformedValue(int n, IntervalVar* var) const {
  return solution(n)->PerformedValue(var);
}

const std::vector<int>& SolutionCollector::ForwardSequence(
    int n, SequenceVar* var) const {
  return solution(n)->ForwardSequence(var);
}

const std::vector<int>& SolutionCollector::BackwardSequence(
    int n, SequenceVar* var) const {
  return solution(n)->BackwardSequence(var);
}

const std::vector<int>& SolutionCollector::Unperformed(int n,
                                                       SequenceVar* var) const {
  return solution(n)->Unperformed(var);
}

namespace {
// ----- First Solution Collector -----

// Collect first solution, useful when looking satisfaction problems
class FirstSolutionCollector : public SolutionCollector {
 public:
  FirstSolutionCollector(Solver* s, const Assignment* a);
  explicit FirstSolutionCollector(Solver* s);
  ~FirstSolutionCollector() override;
  void EnterSearch() override;
  bool AtSolution() override;
  void Install() override;
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

void FirstSolutionCollector::Install() {
  SolutionCollector::Install();
  ListenToEvent(Solver::MonitorEvent::kAtSolution);
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
    const Assignment* const assignment) {
  return RevAlloc(new FirstSolutionCollector(this, assignment));
}

SolutionCollector* Solver::MakeFirstSolutionCollector() {
  return RevAlloc(new FirstSolutionCollector(this));
}

// ----- Last Solution Collector -----

// Collect last solution, useful when optimizing
namespace {
class LastSolutionCollector : public SolutionCollector {
 public:
  LastSolutionCollector(Solver* s, const Assignment* a);
  explicit LastSolutionCollector(Solver* s);
  ~LastSolutionCollector() override;
  bool AtSolution() override;
  void Install() override;
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

void LastSolutionCollector::Install() {
  SolutionCollector::Install();
  ListenToEvent(Solver::MonitorEvent::kAtSolution);
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
    const Assignment* const assignment) {
  return RevAlloc(new LastSolutionCollector(this, assignment));
}

SolutionCollector* Solver::MakeLastSolutionCollector() {
  return RevAlloc(new LastSolutionCollector(this));
}

// ----- Best Solution Collector -----

namespace {
class BestValueSolutionCollector : public SolutionCollector {
 public:
  BestValueSolutionCollector(Solver* solver, const Assignment* assignment,
                             std::vector<bool> maximize);
  BestValueSolutionCollector(Solver* solver, std::vector<bool> maximize);
  ~BestValueSolutionCollector() override {}
  void EnterSearch() override;
  bool AtSolution() override;
  void Install() override;
  std::string DebugString() const override;

 private:
  void ResetBestObjective() {
    for (int i = 0; i < maximize_.size(); ++i) {
      best_[i] = maximize_[i] ? std::numeric_limits<int64_t>::min()
                              : std::numeric_limits<int64_t>::max();
    }
  }

  const std::vector<bool> maximize_;
  std::vector<int64_t> best_;
};

BestValueSolutionCollector::BestValueSolutionCollector(
    Solver* solver, const Assignment* assignment, std::vector<bool> maximize)
    : SolutionCollector(solver, assignment),
      maximize_(std::move(maximize)),
      best_(maximize_.size()) {
  ResetBestObjective();
}

BestValueSolutionCollector::BestValueSolutionCollector(
    Solver* solver, std::vector<bool> maximize)
    : SolutionCollector(solver),
      maximize_(std::move(maximize)),
      best_(maximize_.size()) {
  ResetBestObjective();
}

void BestValueSolutionCollector::EnterSearch() {
  SolutionCollector::EnterSearch();
  ResetBestObjective();
}

bool BestValueSolutionCollector::AtSolution() {
  if (prototype_ != nullptr && prototype_->HasObjective()) {
    const int size = std::min(prototype_->NumObjectives(),
                              static_cast<int>(maximize_.size()));
    // We could use std::lexicographical_compare here but this would force us to
    // create a vector of objectives.
    bool is_improvement = false;
    for (int i = 0; i < size; ++i) {
      const IntVar* objective = prototype_->ObjectiveFromIndex(i);
      const int64_t objective_value =
          maximize_[i] ? CapOpp(objective->Max()) : objective->Min();
      if (objective_value < best_[i]) {
        is_improvement = true;
        break;
      } else if (objective_value > best_[i]) {
        break;
      }
    }
    if (solution_count() == 0 || is_improvement) {
      PopSolution();
      PushSolution();
      for (int i = 0; i < size; ++i) {
        best_[i] = maximize_[i]
                       ? CapOpp(prototype_->ObjectiveFromIndex(i)->Max())
                       : prototype_->ObjectiveFromIndex(i)->Min();
      }
    }
  }
  return true;
}

void BestValueSolutionCollector::Install() {
  SolutionCollector::Install();
  ListenToEvent(Solver::MonitorEvent::kAtSolution);
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
    const Assignment* const assignment, bool maximize) {
  return RevAlloc(new BestValueSolutionCollector(this, assignment, {maximize}));
}

SolutionCollector* Solver::MakeBestLexicographicValueSolutionCollector(
    const Assignment* assignment, std::vector<bool> maximize) {
  return RevAlloc(
      new BestValueSolutionCollector(this, assignment, std::move(maximize)));
}

SolutionCollector* Solver::MakeBestValueSolutionCollector(bool maximize) {
  return RevAlloc(new BestValueSolutionCollector(this, {maximize}));
}

SolutionCollector* Solver::MakeBestLexicographicValueSolutionCollector(
    std::vector<bool> maximize) {
  return RevAlloc(new BestValueSolutionCollector(this, std::move(maximize)));
}

// ----- N Best Solution Collector -----

namespace {
class NBestValueSolutionCollector : public SolutionCollector {
 public:
  NBestValueSolutionCollector(Solver* solver, const Assignment* assignment,
                              int solution_count, std::vector<bool> maximize);
  NBestValueSolutionCollector(Solver* solver, int solution_count,
                              std::vector<bool> maximize);
  ~NBestValueSolutionCollector() override { Clear(); }
  void EnterSearch() override;
  void ExitSearch() override;
  bool AtSolution() override;
  void Install() override;
  std::string DebugString() const override;

 private:
  void Clear();

  const std::vector<bool> maximize_;
  std::priority_queue<std::pair<std::vector<int64_t>, SolutionData>>
      solutions_pq_;
  const int solution_count_;
};

NBestValueSolutionCollector::NBestValueSolutionCollector(
    Solver* solver, const Assignment* assignment, int solution_count,
    std::vector<bool> maximize)
    : SolutionCollector(solver, assignment),
      maximize_(std::move(maximize)),
      solution_count_(solution_count) {}

NBestValueSolutionCollector::NBestValueSolutionCollector(
    Solver* solver, int solution_count, std::vector<bool> maximize)
    : SolutionCollector(solver),
      maximize_(std::move(maximize)),
      solution_count_(solution_count) {}

void NBestValueSolutionCollector::EnterSearch() {
  SolutionCollector::EnterSearch();
  // TODO(user): Remove this when fast local search works with
  // multiple solutions collected.
  if (solution_count_ > 1) {
    solver()->SetUseFastLocalSearch(false);
  }
  Clear();
}

void NBestValueSolutionCollector::ExitSearch() {
  while (!solutions_pq_.empty()) {
    Push(solutions_pq_.top().second);
    solutions_pq_.pop();
  }
}

bool NBestValueSolutionCollector::AtSolution() {
  if (prototype_ != nullptr && prototype_->HasObjective()) {
    const int size = std::min(prototype_->NumObjectives(),
                              static_cast<int>(maximize_.size()));
    std::vector<int64_t> objective_values(size);
    for (int i = 0; i < size; ++i) {
      objective_values[i] =
          maximize_[i] ? CapOpp(prototype_->ObjectiveFromIndex(i)->Max())
                       : prototype_->ObjectiveFromIndex(i)->Min();
    }
    if (solutions_pq_.size() < solution_count_) {
      solutions_pq_.push(
          {std::move(objective_values), BuildSolutionDataForCurrentState()});
    } else if (!solutions_pq_.empty()) {
      const auto& [top_obj_value, top_sol_data] = solutions_pq_.top();
      if (std::lexicographical_compare(
              objective_values.begin(), objective_values.end(),
              top_obj_value.begin(), top_obj_value.end())) {
        FreeSolution(top_sol_data.solution);
        solutions_pq_.pop();
        solutions_pq_.push(
            {std::move(objective_values), BuildSolutionDataForCurrentState()});
      }
    }
  }
  return true;
}

void NBestValueSolutionCollector::Install() {
  SolutionCollector::Install();
  ListenToEvent(Solver::MonitorEvent::kExitSearch);
  ListenToEvent(Solver::MonitorEvent::kAtSolution);
}

std::string NBestValueSolutionCollector::DebugString() const {
  if (prototype_ == nullptr) {
    return "NBestValueSolutionCollector()";
  } else {
    return "NBestValueSolutionCollector(" + prototype_->DebugString() + ")";
  }
}

void NBestValueSolutionCollector::Clear() {
  while (!solutions_pq_.empty()) {
    delete solutions_pq_.top().second.solution;
    solutions_pq_.pop();
  }
}

}  // namespace

SolutionCollector* Solver::MakeNBestValueSolutionCollector(
    const Assignment* assignment, int solution_count, bool maximize) {
  if (solution_count == 1) {
    return MakeBestValueSolutionCollector(assignment, maximize);
  }
  return RevAlloc(new NBestValueSolutionCollector(this, assignment,
                                                  solution_count, {maximize}));
}

SolutionCollector* Solver::MakeNBestValueSolutionCollector(int solution_count,
                                                           bool maximize) {
  if (solution_count == 1) {
    return MakeBestValueSolutionCollector(maximize);
  }
  return RevAlloc(
      new NBestValueSolutionCollector(this, solution_count, {maximize}));
}

SolutionCollector* Solver::MakeNBestLexicographicValueSolutionCollector(
    const Assignment* assignment, int solution_count,
    std::vector<bool> maximize) {
  if (solution_count == 1) {
    return MakeBestLexicographicValueSolutionCollector(assignment,
                                                       std::move(maximize));
  }
  return RevAlloc(new NBestValueSolutionCollector(
      this, assignment, solution_count, std::move(maximize)));
}

SolutionCollector* Solver::MakeNBestLexicographicValueSolutionCollector(
    int solution_count, std::vector<bool> maximize) {
  if (solution_count == 1) {
    return MakeBestLexicographicValueSolutionCollector(std::move(maximize));
  }
  return RevAlloc(new NBestValueSolutionCollector(this, solution_count,
                                                  std::move(maximize)));
}
// ----- All Solution Collector -----

// collect all solutions
namespace {
class AllSolutionCollector : public SolutionCollector {
 public:
  AllSolutionCollector(Solver* s, const Assignment* a);
  explicit AllSolutionCollector(Solver* s);
  ~AllSolutionCollector() override;
  bool AtSolution() override;
  void Install() override;
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

void AllSolutionCollector::Install() {
  SolutionCollector::Install();
  ListenToEvent(Solver::MonitorEvent::kAtSolution);
}

std::string AllSolutionCollector::DebugString() const {
  if (prototype_ == nullptr) {
    return "AllSolutionCollector()";
  } else {
    return "AllSolutionCollector(" + prototype_->DebugString() + ")";
  }
}
}  // namespace

SolutionCollector* Solver::MakeAllSolutionCollector(
    const Assignment* const assignment) {
  return RevAlloc(new AllSolutionCollector(this, assignment));
}

SolutionCollector* Solver::MakeAllSolutionCollector() {
  return RevAlloc(new AllSolutionCollector(this));
}

// ---------- Objective Management ----------

class RoundRobinCompoundObjectiveMonitor : public BaseObjectiveMonitor {
 public:
  RoundRobinCompoundObjectiveMonitor(
      std::vector<BaseObjectiveMonitor*> monitors,
      int num_max_local_optima_before_metaheuristic_switch)
      : BaseObjectiveMonitor(monitors[0]->solver()),
        monitors_(std::move(monitors)),
        enabled_monitors_(monitors_.size(), true),
        local_optimum_limit_(num_max_local_optima_before_metaheuristic_switch) {
  }
  void EnterSearch() override {
    active_monitor_ = 0;
    num_local_optimum_ = 0;
    enabled_monitors_.assign(monitors_.size(), true);
    for (auto& monitor : monitors_) {
      monitor->set_active(monitor == monitors_[active_monitor_]);
      monitor->EnterSearch();
    }
  }
  void ApplyDecision(Decision* d) override {
    monitors_[active_monitor_]->ApplyDecision(d);
  }
  void AcceptNeighbor() override {
    monitors_[active_monitor_]->AcceptNeighbor();
  }
  bool AtSolution() override {
    bool ok = true;
    for (auto& monitor : monitors_) {
      ok &= monitor->AtSolution();
    }
    return ok;
  }
  bool AcceptDelta(Assignment* delta, Assignment* deltadelta) override {
    return monitors_[active_monitor_]->AcceptDelta(delta, deltadelta);
  }
  void BeginNextDecision(DecisionBuilder* db) override {
    monitors_[active_monitor_]->BeginNextDecision(db);
  }
  void RefuteDecision(Decision* d) override {
    monitors_[active_monitor_]->RefuteDecision(d);
  }
  bool AcceptSolution() override {
    return monitors_[active_monitor_]->AcceptSolution();
  }
  bool AtLocalOptimum() override {
    const bool ok = monitors_[active_monitor_]->AtLocalOptimum();
    if (!ok) {
      enabled_monitors_[active_monitor_] = false;
    }
    if (++num_local_optimum_ >= local_optimum_limit_ || !ok) {
      monitors_[active_monitor_]->set_active(false);
      int next_active_monitor = (active_monitor_ + 1) % monitors_.size();
      while (!enabled_monitors_[next_active_monitor]) {
        if (next_active_monitor == active_monitor_) return false;
        next_active_monitor = (active_monitor_ + 1) % monitors_.size();
      }
      active_monitor_ = next_active_monitor;
      monitors_[active_monitor_]->set_active(true);
      num_local_optimum_ = 0;
      VLOG(2) << "Switching to monitor " << active_monitor_ << " "
              << monitors_[active_monitor_]->DebugString();
    }
    return true;
  }
  IntVar* ObjectiveVar(int index) const override {
    return monitors_[active_monitor_]->ObjectiveVar(index);
  }
  IntVar* MinimizationVar(int index) const override {
    return monitors_[active_monitor_]->MinimizationVar(index);
  }
  int64_t Step(int index) const override {
    return monitors_[active_monitor_]->Step(index);
  }
  bool Maximize(int index) const override {
    return monitors_[active_monitor_]->Maximize(index);
  }
  int64_t BestValue(int index) const override {
    return monitors_[active_monitor_]->BestValue(index);
  }
  int Size() const override { return monitors_[active_monitor_]->Size(); }
  std::string DebugString() const override {
    return monitors_[active_monitor_]->DebugString();
  }
  void Accept(ModelVisitor* visitor) const override {
    // TODO(user): properly implement this.
    for (auto& monitor : monitors_) {
      monitor->Accept(visitor);
    }
  }

 private:
  const std::vector<BaseObjectiveMonitor*> monitors_;
  std::vector<bool> enabled_monitors_;
  int active_monitor_ = 0;
  int num_local_optimum_ = 0;
  const int local_optimum_limit_;
};

BaseObjectiveMonitor* Solver::MakeRoundRobinCompoundObjectiveMonitor(
    std::vector<BaseObjectiveMonitor*> monitors,
    int num_max_local_optima_before_metaheuristic_switch) {
  return RevAlloc(new RoundRobinCompoundObjectiveMonitor(
      std::move(monitors), num_max_local_optima_before_metaheuristic_switch));
}

ObjectiveMonitor::ObjectiveMonitor(Solver* solver,
                                   const std::vector<bool>& maximize,
                                   std::vector<IntVar*> vars,
                                   std::vector<int64_t> steps)
    : BaseObjectiveMonitor(solver),
      found_initial_solution_(false),
      objective_vars_(std::move(vars)),
      minimization_vars_(objective_vars_),
      upper_bounds_(Size() + 1, nullptr),
      steps_(std::move(steps)),
      best_values_(Size(), std::numeric_limits<int64_t>::max()),
      current_values_(Size(), std::numeric_limits<int64_t>::max()) {
  DCHECK_GT(Size(), 0);
  DCHECK_EQ(objective_vars_.size(), steps_.size());
  DCHECK_EQ(objective_vars_.size(), maximize.size());
  DCHECK(std::all_of(steps_.begin(), steps_.end(),
                     [](int64_t step) { return step > 0; }));
  for (int i = 0; i < Size(); ++i) {
    if (maximize[i]) {
      minimization_vars_[i] = solver->MakeOpposite(objective_vars_[i])->Var();
    }
  }
  // Necessary to enforce strict lexical less-than constraint.
  minimization_vars_.push_back(solver->MakeIntConst(1));
  upper_bounds_.back() = solver->MakeIntConst(0);
  steps_.push_back(1);
  // TODO(user): Remove optimization direction from solver or expose it for
  // each OptimizeVar variable. Note that Solver::optimization_direction() is
  // not used anywhere, only passed as information for the user. Direction set
  // based on highest level as of 02/2023.
  solver->set_optimization_direction(maximize[0] ? Solver::MAXIMIZATION
                                                 : Solver::MINIMIZATION);
}

void ObjectiveMonitor::EnterSearch() {
  found_initial_solution_ = false;
  best_values_.assign(Size(), std::numeric_limits<int64_t>::max());
  current_values_ = best_values_;
  solver()->SetUseFastLocalSearch(true);
}

bool ObjectiveMonitor::AtSolution() {
  for (int i = 0; i < Size(); ++i) {
    if (VLOG_IS_ON(2) && !ObjectiveVar(i)->Bound()) {
      VLOG(2) << "Variable not bound: " << ObjectiveVar(i)->DebugString()
              << ".";
    }
    current_values_[i] = MinimizationVar(i)->Max();
  }
  if (std::lexicographical_compare(current_values_.begin(),
                                   current_values_.end(), best_values_.begin(),
                                   best_values_.end())) {
    best_values_ = current_values_;
  }
  found_initial_solution_ = true;
  return true;
}

bool ObjectiveMonitor::AcceptDelta(Assignment* delta, Assignment*) {
  if (delta == nullptr) return true;
  const bool delta_has_objective = delta->HasObjective();
  if (!delta_has_objective) {
    delta->AddObjectives(objective_vars());
  }
  const Assignment* const local_search_state =
      solver()->GetOrCreateLocalSearchState();
  for (int i = 0; i < Size(); ++i) {
    if (delta->ObjectiveFromIndex(i) == ObjectiveVar(i)) {
      if (Maximize(i)) {
        int64_t obj_min = ObjectiveVar(i)->Min();
        if (delta_has_objective) {
          obj_min = std::max(obj_min, delta->ObjectiveMinFromIndex(i));
        }
        if (solver()->UseFastLocalSearch() &&
            i < local_search_state->NumObjectives()) {
          obj_min = std::max(
              obj_min,
              CapAdd(local_search_state->ObjectiveMinFromIndex(i), Step(i)));
        }
        delta->SetObjectiveMinFromIndex(i, obj_min);
      } else {
        int64_t obj_max = ObjectiveVar(i)->Max();
        if (delta_has_objective) {
          obj_max = std::min(obj_max, delta->ObjectiveMaxFromIndex(i));
        }
        if (solver()->UseFastLocalSearch() &&
            i < local_search_state->NumObjectives()) {
          obj_max = std::min(
              obj_max,
              CapSub(local_search_state->ObjectiveMaxFromIndex(i), Step(i)));
        }
        delta->SetObjectiveMaxFromIndex(i, obj_max);
      }
    }
  }
  return true;
}

void ObjectiveMonitor::Accept(ModelVisitor* const visitor) const {
  visitor->BeginVisitExtension(ModelVisitor::kObjectiveExtension);
  visitor->VisitIntegerArrayArgument(ModelVisitor::kStepArgument, steps_);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kExpressionArgument,
                                             objective_vars_);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kExpressionArgument,
                                             minimization_vars_);
  visitor->EndVisitExtension(ModelVisitor::kObjectiveExtension);
}

bool ObjectiveMonitor::CurrentInternalValuesAreConstraining() const {
  int num_values_at_max = 0;
  for (int i = 0; i < Size(); ++i) {
    if (CurrentInternalValue(i) < std::numeric_limits<int64_t>::max()) {
      DCHECK_EQ(num_values_at_max, 0);
    } else {
      ++num_values_at_max;
    }
  }
  DCHECK(num_values_at_max == 0 || num_values_at_max == Size());
  return num_values_at_max < Size();
}

OptimizeVar::OptimizeVar(Solver* solver, bool maximize, IntVar* var,
                         int64_t step)
    : OptimizeVar(solver, std::vector<bool>{maximize},
                  std::vector<IntVar*>{var}, {step}) {}

OptimizeVar::OptimizeVar(Solver* solver, const std::vector<bool>& maximize,
                         std::vector<IntVar*> vars, std::vector<int64_t> steps)
    : ObjectiveMonitor(solver, maximize, std::move(vars), std::move(steps)) {}

void OptimizeVar::BeginNextDecision(DecisionBuilder*) {
  if (solver()->SearchDepth() == 0) {  // after a restart.
    ApplyBound();
  }
}

void OptimizeVar::ApplyBound() {
  if (found_initial_solution_) {
    MakeMinimizationVarsLessOrEqualWithSteps(
        [this](int i) { return CurrentInternalValue(i); });
  }
}

namespace {
class ApplyBoundDecisionBuilder : public DecisionBuilder {
 public:
  explicit ApplyBoundDecisionBuilder(OptimizeVar* optimize_var)
      : optimize_var_(optimize_var) {}
  ~ApplyBoundDecisionBuilder() override = default;
  Decision* Next(Solver*) override {
    optimize_var_->ApplyBound();
    return nullptr;
  }

 private:
  OptimizeVar* optimize_var_;
};
}  // namespace

void OptimizeVar::RefuteDecision(Decision*) {
  if (!solver()->SolveAndCommit(
          solver()->RevAlloc(new ApplyBoundDecisionBuilder(this)))) {
    if (on_optimal_found_) {
      // TODO(user): Support multiple objectives.
      const int64_t value = CurrentInternalValue(0);
      on_optimal_found_(objective_vars()[0] == minimization_vars()[0]
                            ? value
                            : CapOpp(value));
    }
    solver()->Fail();
  }
}

bool OptimizeVar::AcceptSolution() {
  if (!found_initial_solution_ || !is_active()) {
    return true;
  } else {
    // This code should never return false in sequential mode because
    // ApplyBound should have been called before. In parallel, this is
    // no longer true. That is why we keep it there, just in case.
    for (int i = 0; i < Size(); ++i) {
      IntVar* const minimization_var = MinimizationVar(i);
      // In unchecked mode, variables are unbound and the solution should be
      // accepted.
      if (!minimization_var->Bound()) return true;
      const int64_t value = minimization_var->Value();
      if (value == CurrentInternalValue(i)) continue;
      return value < CurrentInternalValue(i);
    }
    return false;
  }
}

bool OptimizeVar::AtSolution() {
  DCHECK(AcceptSolution());
  return ObjectiveMonitor::AtSolution();
}

std::string OptimizeVar::Name() const { return "objective"; }

std::string OptimizeVar::DebugString() const {
  std::string out;
  for (int i = 0; i < Size(); ++i) {
    absl::StrAppendFormat(
        &out, "%s%s(%s, step = %d, best = %d)", i == 0 ? "" : "; ",
        Maximize(i) ? "MaximizeVar" : "MinimizeVar",
        ObjectiveVar(i)->DebugString(), Step(i), BestValue(i));
  }
  return out;
}

OptimizeVar* Solver::MakeMinimize(IntVar* const v, int64_t step) {
  return RevAlloc(new OptimizeVar(this, false, v, step));
}

OptimizeVar* Solver::MakeMaximize(IntVar* const v, int64_t step) {
  return RevAlloc(new OptimizeVar(this, true, v, step));
}

OptimizeVar* Solver::MakeOptimize(bool maximize, IntVar* const v,
                                  int64_t step) {
  return RevAlloc(new OptimizeVar(this, maximize, v, step));
}

OptimizeVar* Solver::MakeLexicographicOptimize(std::vector<bool> maximize,
                                               std::vector<IntVar*> variables,
                                               std::vector<int64_t> steps) {
  return RevAlloc(new OptimizeVar(this, std::move(maximize),
                                  std::move(variables), std::move(steps)));
}

namespace {
class WeightedOptimizeVar : public OptimizeVar {
 public:
  WeightedOptimizeVar(Solver* solver, bool maximize,
                      const std::vector<IntVar*>& sub_objectives,
                      const std::vector<int64_t>& weights, int64_t step)
      : OptimizeVar(solver, maximize,
                    solver->MakeScalProd(sub_objectives, weights)->Var(), step),
        sub_objectives_(sub_objectives),
        weights_(weights) {
    CHECK_EQ(sub_objectives.size(), weights.size());
  }

  ~WeightedOptimizeVar() override {}
  std::string Name() const override;

 private:
  const std::vector<IntVar*> sub_objectives_;
  const std::vector<int64_t> weights_;
};

std::string WeightedOptimizeVar::Name() const { return "weighted objective"; }
}  // namespace

OptimizeVar* Solver::MakeWeightedOptimize(
    bool maximize, const std::vector<IntVar*>& sub_objectives,
    const std::vector<int64_t>& weights, int64_t step) {
  return RevAlloc(
      new WeightedOptimizeVar(this, maximize, sub_objectives, weights, step));
}

OptimizeVar* Solver::MakeWeightedMinimize(
    const std::vector<IntVar*>& sub_objectives,
    const std::vector<int64_t>& weights, int64_t step) {
  return RevAlloc(
      new WeightedOptimizeVar(this, false, sub_objectives, weights, step));
}

OptimizeVar* Solver::MakeWeightedMaximize(
    const std::vector<IntVar*>& sub_objectives,
    const std::vector<int64_t>& weights, int64_t step) {
  return RevAlloc(
      new WeightedOptimizeVar(this, true, sub_objectives, weights, step));
}

OptimizeVar* Solver::MakeWeightedOptimize(
    bool maximize, const std::vector<IntVar*>& sub_objectives,
    const std::vector<int>& weights, int64_t step) {
  return MakeWeightedOptimize(maximize, sub_objectives, ToInt64Vector(weights),
                              step);
}

OptimizeVar* Solver::MakeWeightedMinimize(
    const std::vector<IntVar*>& sub_objectives, const std::vector<int>& weights,
    int64_t step) {
  return MakeWeightedMinimize(sub_objectives, ToInt64Vector(weights), step);
}

OptimizeVar* Solver::MakeWeightedMaximize(
    const std::vector<IntVar*>& sub_objectives, const std::vector<int>& weights,
    int64_t step) {
  return MakeWeightedMaximize(sub_objectives, ToInt64Vector(weights), step);
}

// ---------- Metaheuristics ---------

namespace {
class Metaheuristic : public ObjectiveMonitor {
 public:
  Metaheuristic(Solver* solver, const std::vector<bool>& maximize,
                std::vector<IntVar*> objectives, std::vector<int64_t> steps);
  ~Metaheuristic() override {}

  void EnterSearch() override;
  void RefuteDecision(Decision* d) override;
};

Metaheuristic::Metaheuristic(Solver* solver, const std::vector<bool>& maximize,
                             std::vector<IntVar*> objectives,
                             std::vector<int64_t> steps)
    : ObjectiveMonitor(solver, maximize, std::move(objectives),
                       std::move(steps)) {}

void Metaheuristic::EnterSearch() {
  ObjectiveMonitor::EnterSearch();
  // TODO(user): Remove this when fast local search works with
  // metaheuristics.
  solver()->SetUseFastLocalSearch(false);
}

void Metaheuristic::RefuteDecision(Decision*) {
  for (int i = 0; i < Size(); ++i) {
    const int64_t objective_value = MinimizationVar(i)->Min();
    if (objective_value > BestInternalValue(i)) break;
    if (objective_value <= CapSub(BestInternalValue(i), Step(i))) return;
  }
  solver()->Fail();
}

// ---------- Tabu Search ----------

class TabuSearch : public Metaheuristic {
 public:
  TabuSearch(Solver* solver, const std::vector<bool>& maximize,
             std::vector<IntVar*> objectives, std::vector<int64_t> steps,
             const std::vector<IntVar*>& vars, int64_t keep_tenure,
             int64_t forbid_tenure, double tabu_factor);
  ~TabuSearch() override {}
  void EnterSearch() override;
  void ApplyDecision(Decision* d) override;
  bool AtSolution() override;
  bool AcceptSolution() override;
  bool AtLocalOptimum() override;
  bool AcceptDelta(Assignment* delta, Assignment* deltadelta) override;
  void AcceptNeighbor() override;
  void BeginNextDecision(DecisionBuilder* const) override {
    if (stop_search_) solver()->Fail();
  }
  void RefuteDecision(Decision* const d) override {
    Metaheuristic::RefuteDecision(d);
    if (stop_search_) solver()->Fail();
  }
  std::string DebugString() const override { return "Tabu Search"; }

 protected:
  struct VarValue {
    int var_index;
    int64_t value;
    int64_t stamp;
  };
  typedef std::list<VarValue> TabuList;

  virtual std::vector<IntVar*> CreateTabuVars();
  const TabuList& forbid_tabu_list() { return forbid_tabu_list_; }
  IntVar* vars(int index) const { return vars_[index]; }

 private:
  void AgeList(int64_t tenure, TabuList* list);
  void AgeLists();
  int64_t TabuLimit() const {
    return (synced_keep_tabu_list_.size() + synced_forbid_tabu_list_.size()) *
           tabu_factor_;
  }

  const std::vector<IntVar*> vars_;
  Assignment::IntContainer assignment_container_;
  bool has_stored_assignment_ = false;
  std::vector<int64_t> last_values_;
  TabuList keep_tabu_list_;
  TabuList synced_keep_tabu_list_;
  int64_t keep_tenure_;
  TabuList forbid_tabu_list_;
  TabuList synced_forbid_tabu_list_;
  int64_t forbid_tenure_;
  double tabu_factor_;
  int64_t stamp_;
  int64_t solution_count_ = 0;
  bool stop_search_ = false;
  std::vector<int64_t> delta_values_;
  SparseBitset<> delta_vars_;
  std::vector<int> var_index_to_index_;
};

TabuSearch::TabuSearch(Solver* solver, const std::vector<bool>& maximize,
                       std::vector<IntVar*> objectives,
                       std::vector<int64_t> steps,
                       const std::vector<IntVar*>& vars, int64_t keep_tenure,
                       int64_t forbid_tenure, double tabu_factor)
    : Metaheuristic(solver, maximize, std::move(objectives), std::move(steps)),
      vars_(vars),
      last_values_(Size(), std::numeric_limits<int64_t>::max()),
      keep_tenure_(keep_tenure),
      forbid_tenure_(forbid_tenure),
      tabu_factor_(tabu_factor),
      stamp_(0),
      delta_values_(vars.size(), 0),
      delta_vars_(vars.size()) {
  for (int index = 0; index < vars_.size(); ++index) {
    assignment_container_.FastAdd(vars_[index]);
    DCHECK_EQ(vars_[index], assignment_container_.Element(index).Var());
    const int var_index = vars_[index]->index();
    if (var_index >= var_index_to_index_.size()) {
      var_index_to_index_.resize(var_index + 1, -1);
    }
    var_index_to_index_[var_index] = index;
  }
}

void TabuSearch::EnterSearch() {
  Metaheuristic::EnterSearch();
  solver()->SetUseFastLocalSearch(true);
  stamp_ = 0;
  has_stored_assignment_ = false;
  solution_count_ = 0;
  stop_search_ = false;
}

void TabuSearch::ApplyDecision(Decision* const d) {
  Solver* const s = solver();
  if (d == s->balancing_decision()) {
    return;
  }

  synced_keep_tabu_list_ = keep_tabu_list_;
  synced_forbid_tabu_list_ = forbid_tabu_list_;
  Constraint* tabu_ct = nullptr;
  {
    // Creating vectors in a scope to make sure they get deleted before
    // adding the tabu constraint which could fail and lead to a leak.
    const std::vector<IntVar*> tabu_vars = CreateTabuVars();
    if (!tabu_vars.empty()) {
      IntVar* tabu_var = s->MakeIsGreaterOrEqualCstVar(
          s->MakeSum(tabu_vars)->Var(), TabuLimit());
      // Aspiration criterion
      // Accept a neighbor if it improves the best solution found so far.
      IntVar* aspiration = MakeMinimizationVarsLessOrEqualWithStepsStatus(
          [this](int i) { return BestInternalValue(i); });
      tabu_ct = s->MakeSumGreaterOrEqual({aspiration, tabu_var}, int64_t{1});
    }
  }
  if (tabu_ct != nullptr) s->AddConstraint(tabu_ct);

  // Go downhill to the next local optimum
  if (CurrentInternalValuesAreConstraining()) {
    MakeMinimizationVarsLessOrEqualWithSteps(
        [this](int i) { return CurrentInternalValue(i); });
  }
}

bool TabuSearch::AcceptSolution() {
  // Avoid cost plateaus which lead to tabu cycles.
  if (found_initial_solution_) {
    for (int i = 0; i < Size(); ++i) {
      if (last_values_[i] != MinimizationVar(i)->Min()) {
        return true;
      }
    }
    return false;
  }
  return true;
}

std::vector<IntVar*> TabuSearch::CreateTabuVars() {
  Solver* const s = solver();

  // Tabu criterion
  // A variable in the "keep" list must keep its value, a variable in the
  // "forbid" list must not take its value in the list. The tabu criterion is
  // softened by the tabu factor which gives the number of violations to
  // the tabu criterion which is tolerated; a factor of 1 means no violations
  // allowed, a factor of 0 means all violations allowed.
  std::vector<IntVar*> tabu_vars;
  for (const auto [var_index, value, unused_stamp] : keep_tabu_list_) {
    tabu_vars.push_back(s->MakeIsEqualCstVar(vars(var_index), value));
  }
  for (const auto [var_index, value, unused_stamp] : forbid_tabu_list_) {
    tabu_vars.push_back(s->MakeIsDifferentCstVar(vars(var_index), value));
  }
  return tabu_vars;
}

bool TabuSearch::AtSolution() {
  ++solution_count_;
  if (!ObjectiveMonitor::AtSolution()) {
    return false;
  }
  for (int i = 0; i < Size(); ++i) {
    last_values_[i] = CurrentInternalValue(i);
  }

  // New solution found: add new assignments to tabu lists; this is only
  // done after the first local optimum (stamp_ != 0)
  if (0 != stamp_ && has_stored_assignment_) {
    for (int index = 0; index < vars_.size(); ++index) {
      IntVar* var = vars(index);
      const int64_t old_value = assignment_container_.Element(index).Value();
      const int64_t new_value = var->Value();
      if (old_value != new_value) {
        if (keep_tenure_ > 0) {
          keep_tabu_list_.push_front({index, new_value, stamp_});
        }
        if (forbid_tenure_ > 0) {
          forbid_tabu_list_.push_front({index, old_value, stamp_});
        }
      }
    }
  }
  assignment_container_.Store();
  has_stored_assignment_ = true;

  return true;
}

bool TabuSearch::AtLocalOptimum() {
  solver()->SetUseFastLocalSearch(false);
  // If no solution has been accepted since the last local optimum, and no tabu
  // lists are active, stop the search.
  if (stamp_ > 0 && solution_count_ == 0 && keep_tabu_list_.empty() &&
      forbid_tabu_list_.empty()) {
    stop_search_ = true;
  }
  solution_count_ = 0;
  AgeLists();
  for (int i = 0; i < Size(); ++i) {
    SetCurrentInternalValue(i, std::numeric_limits<int64_t>::max());
  }
  return found_initial_solution_;
}

bool TabuSearch::AcceptDelta(Assignment* delta, Assignment* deltadelta) {
  if (delta == nullptr) return true;
  if (!Metaheuristic::AcceptDelta(delta, deltadelta)) return false;
  if (synced_keep_tabu_list_.empty() && synced_forbid_tabu_list_.empty()) {
    return true;
  }
  const Assignment::IntContainer& delta_container = delta->IntVarContainer();
  // Detect LNS, bail out quickly in this case without filtering.
  for (const IntVarElement& element : delta_container.elements()) {
    if (!element.Bound()) return true;
  }
  delta_vars_.ResetAllToFalse();
  for (const IntVarElement& element : delta_container.elements()) {
    const int var_index = element.Var()->index();
    if (var_index >= var_index_to_index_.size()) continue;
    const int index = var_index_to_index_[var_index];
    if (index == -1) continue;
    delta_values_[index] = element.Value();
    delta_vars_.Set(index);
  }
  int num_respected = 0;
  auto get_value = [this](int var_index) {
    return delta_vars_[var_index]
               ? delta_values_[var_index]
               : assignment_container_.Element(var_index).Value();
  };
  const int64_t tabu_limit = TabuLimit();
  for (const auto [var_index, value, unused_stamp] : synced_keep_tabu_list_) {
    if (get_value(var_index) == value) {
      if (++num_respected >= tabu_limit) return true;
    }
  }
  for (const auto [var_index, value, unused_stamp] : synced_forbid_tabu_list_) {
    if (get_value(var_index) != value) {
      if (++num_respected >= tabu_limit) return true;
    }
  }
  if (num_respected >= tabu_limit) return true;
  // Aspiration
  // TODO(user): Add proper support for lex-objectives with steps.
  if (Size() == 1) {
    if (Maximize(0)) {
      delta->SetObjectiveMinFromIndex(0, CapAdd(BestInternalValue(0), Step(0)));
    } else {
      delta->SetObjectiveMaxFromIndex(0, CapSub(BestInternalValue(0), Step(0)));
    }
  } else {
    for (int i = 0; i < Size(); ++i) {
      if (Maximize(i)) {
        delta->SetObjectiveMinFromIndex(i, BestInternalValue(i));
      } else {
        delta->SetObjectiveMaxFromIndex(i, BestInternalValue(i));
      }
    }
  }
  // TODO(user): Add support for plateau removal.
  return true;
}

void TabuSearch::AcceptNeighbor() {
  if (0 != stamp_) {
    AgeLists();
  }
}

void TabuSearch::AgeList(int64_t tenure, TabuList* list) {
  while (!list->empty() && list->back().stamp < stamp_ - tenure) {
    list->pop_back();
  }
}

void TabuSearch::AgeLists() {
  AgeList(keep_tenure_, &keep_tabu_list_);
  AgeList(forbid_tenure_, &forbid_tabu_list_);
  ++stamp_;
}

class GenericTabuSearch : public TabuSearch {
 public:
  GenericTabuSearch(Solver* solver, bool maximize, IntVar* objective,
                    int64_t step, const std::vector<IntVar*>& vars,
                    int64_t forbid_tenure)
      : TabuSearch(solver, {maximize}, {objective}, {step}, vars, 0,
                   forbid_tenure, 1) {}
  std::string DebugString() const override { return "Generic Tabu Search"; }

 protected:
  std::vector<IntVar*> CreateTabuVars() override;
};

std::vector<IntVar*> GenericTabuSearch::CreateTabuVars() {
  Solver* const s = solver();

  // Tabu criterion
  // At least one element of the forbid_tabu_list must change value.
  std::vector<IntVar*> forbid_values;
  for (const auto [var_index, value, unused_stamp] : forbid_tabu_list()) {
    forbid_values.push_back(s->MakeIsDifferentCstVar(vars(var_index), value));
  }
  std::vector<IntVar*> tabu_vars;
  if (!forbid_values.empty()) {
    tabu_vars.push_back(s->MakeIsGreaterCstVar(s->MakeSum(forbid_values), 0));
  }
  return tabu_vars;
}

}  // namespace

ObjectiveMonitor* Solver::MakeTabuSearch(bool maximize, IntVar* objective,
                                         int64_t step,
                                         const std::vector<IntVar*>& vars,
                                         int64_t keep_tenure,
                                         int64_t forbid_tenure,
                                         double tabu_factor) {
  return RevAlloc(new TabuSearch(this, {maximize}, {objective}, {step}, vars,
                                 keep_tenure, forbid_tenure, tabu_factor));
}

ObjectiveMonitor* Solver::MakeLexicographicTabuSearch(
    const std::vector<bool>& maximize, std::vector<IntVar*> objectives,
    std::vector<int64_t> steps, const std::vector<IntVar*>& vars,
    int64_t keep_tenure, int64_t forbid_tenure, double tabu_factor) {
  return RevAlloc(new TabuSearch(this, maximize, std::move(objectives),
                                 std::move(steps), vars, keep_tenure,
                                 forbid_tenure, tabu_factor));
}

ObjectiveMonitor* Solver::MakeGenericTabuSearch(
    bool maximize, IntVar* const v, int64_t step,
    const std::vector<IntVar*>& tabu_vars, int64_t forbid_tenure) {
  return RevAlloc(
      new GenericTabuSearch(this, maximize, v, step, tabu_vars, forbid_tenure));
}

// ---------- Simulated Annealing ----------

namespace {
class SimulatedAnnealing : public Metaheuristic {
 public:
  SimulatedAnnealing(Solver* solver, const std::vector<bool>& maximize,
                     std::vector<IntVar*> objectives,
                     std::vector<int64_t> steps,
                     std::vector<int64_t> initial_temperatures);
  ~SimulatedAnnealing() override {}
  void ApplyDecision(Decision* d) override;
  bool AtLocalOptimum() override;
  void AcceptNeighbor() override;
  std::string DebugString() const override { return "Simulated Annealing"; }

 private:
  double Temperature(int index) const {
    return iteration_ > 0
               ? (1.0 * temperature0_[index]) / iteration_  // Cauchy annealing
               : 0;
  }

  const std::vector<int64_t> temperature0_;
  int64_t iteration_;
  std::mt19937 rand_;
};

SimulatedAnnealing::SimulatedAnnealing(
    Solver* solver, const std::vector<bool>& maximize,
    std::vector<IntVar*> objectives, std::vector<int64_t> steps,
    std::vector<int64_t> initial_temperatures)
    : Metaheuristic(solver, maximize, std::move(objectives), std::move(steps)),
      temperature0_(std::move(initial_temperatures)),
      iteration_(0),
      rand_(CpRandomSeed()) {}

// As a reminder, if s is the current solution, s' the new solution, s' will be
// accepted iff:
// 1) cost(s') ≤ cost(s) - step
// or
// 2) P(cost(s) - step, cost(s'), T) ≥ random(0, 1),
//    where P(e, e', T) = exp(-(e' - e) / T).
// 2) is equivalent to exp(-(e' - e) / T) ≥ random(0, 1)
// or -(e' - e) / T ≥ log(random(0, 1))
// or e' - e ≤ -log(random(0, 1)) * T
// or e' ≤ e - log(random(0, 1)) * T.
// 2) can therefore be expressed as:
// cost(s') ≤ cost(s) - step - log(random(0, 1) * T.
// Note that if 1) is true, 2) will be true too as exp(-(e' - e) / T) ≥ 1.
void SimulatedAnnealing::ApplyDecision(Decision* const d) {
  Solver* const s = solver();
  if (d == s->balancing_decision()) {
    return;
  }
  if (CurrentInternalValuesAreConstraining()) {
    MakeMinimizationVarsLessOrEqualWithSteps([this](int i) {
      const double rand_double = absl::Uniform<double>(rand_, 0.0, 1.0);
#if defined(_MSC_VER) || defined(__ANDROID__)
      const double rand_log2_double = log(rand_double) / log(2.0L);
#else
      const double rand_log2_double = log2(rand_double);
#endif
      const int64_t energy_bound = Temperature(i) * rand_log2_double;
      // energy_bound is negative, since we want to allow higher bounds it's
      // subtracted from the current bound.
      return CapSub(CurrentInternalValue(i), energy_bound);
    });
  }
}

bool SimulatedAnnealing::AtLocalOptimum() {
  for (int i = 0; i < Size(); ++i) {
    SetCurrentInternalValue(i, std::numeric_limits<int64_t>::max());
  }
  ++iteration_;
  if (!found_initial_solution_) return false;
  for (int i = 0; i < Size(); ++i) {
    if (Temperature(i) <= 0) return false;
  }
  return true;
}

void SimulatedAnnealing::AcceptNeighbor() {
  if (iteration_ > 0) {
    ++iteration_;
  }
}
}  // namespace

ObjectiveMonitor* Solver::MakeSimulatedAnnealing(bool maximize, IntVar* const v,
                                                 int64_t step,
                                                 int64_t initial_temperature) {
  return RevAlloc(new SimulatedAnnealing(this, {maximize}, {v}, {step},
                                         {initial_temperature}));
}

ObjectiveMonitor* Solver::MakeLexicographicSimulatedAnnealing(
    const std::vector<bool>& maximize, std::vector<IntVar*> vars,
    std::vector<int64_t> steps, std::vector<int64_t> initial_temperatures) {
  return RevAlloc(new SimulatedAnnealing(this, maximize, std::move(vars),
                                         std::move(steps),
                                         std::move(initial_temperatures)));
}

// ---------- Guided Local Search ----------

namespace {
// GLS penalty management classes. Maintains the penalty frequency for each
// (variable, value) pair.

// Dense GLS penalties implementation using a matrix to store penalties.
class GuidedLocalSearchPenaltiesTable {
 public:
  struct VarValue {
    int64_t var;
    int64_t value;
  };
  explicit GuidedLocalSearchPenaltiesTable(int num_vars);
  bool HasPenalties() const { return has_values_; }
  void IncrementPenalty(const VarValue& var_value);
  int64_t GetPenalty(const VarValue& var_value) const;
  void Reset();

 private:
  std::vector<std::vector<int64_t>> penalties_;
  bool has_values_;
};

GuidedLocalSearchPenaltiesTable::GuidedLocalSearchPenaltiesTable(int num_vars)
    : penalties_(num_vars), has_values_(false) {}

void GuidedLocalSearchPenaltiesTable::IncrementPenalty(
    const VarValue& var_value) {
  std::vector<int64_t>& var_penalties = penalties_[var_value.var];
  const int64_t value = var_value.value;
  if (value >= var_penalties.size()) {
    var_penalties.resize(value + 1, 0);
  }
  ++var_penalties[value];
  has_values_ = true;
}

void GuidedLocalSearchPenaltiesTable::Reset() {
  has_values_ = false;
  for (int i = 0; i < penalties_.size(); ++i) {
    penalties_[i].clear();
  }
}

int64_t GuidedLocalSearchPenaltiesTable::GetPenalty(
    const VarValue& var_value) const {
  const std::vector<int64_t>& var_penalties = penalties_[var_value.var];
  const int64_t value = var_value.value;
  return (value >= var_penalties.size()) ? 0 : var_penalties[value];
}

// Sparse GLS penalties implementation using hash_map to store penalties.
class GuidedLocalSearchPenaltiesMap {
 public:
  struct VarValue {
    int64_t var;
    int64_t value;

    friend bool operator==(const VarValue& lhs, const VarValue& rhs) {
      return lhs.var == rhs.var && lhs.value == rhs.value;
    }
    template <typename H>
    friend H AbslHashValue(H h, const VarValue& var_value) {
      return H::combine(std::move(h), var_value.var, var_value.value);
    }
  };
  explicit GuidedLocalSearchPenaltiesMap(int num_vars);
  bool HasPenalties() const { return (!penalties_.empty()); }
  void IncrementPenalty(const VarValue& var_value);
  int64_t GetPenalty(const VarValue& var_value) const;
  void Reset();

 private:
  Bitmap penalized_;
  absl::flat_hash_map<VarValue, int64_t> penalties_;
};

GuidedLocalSearchPenaltiesMap::GuidedLocalSearchPenaltiesMap(int num_vars)
    : penalized_(num_vars, false) {}

void GuidedLocalSearchPenaltiesMap::IncrementPenalty(
    const VarValue& var_value) {
  ++penalties_[var_value];
  penalized_.Set(var_value.var, true);
}

void GuidedLocalSearchPenaltiesMap::Reset() {
  penalties_.clear();
  penalized_.Clear();
}

int64_t GuidedLocalSearchPenaltiesMap::GetPenalty(
    const VarValue& var_value) const {
  return (penalized_.Get(var_value.var))
             ? gtl::FindWithDefault(penalties_, var_value)
             : 0;
}

template <typename P>
class GuidedLocalSearch : public Metaheuristic {
 public:
  GuidedLocalSearch(
      Solver* solver, IntVar* objective, bool maximize, int64_t step,
      const std::vector<IntVar*>& vars, double penalty_factor,
      std::function<std::vector<std::pair<int64_t, int64_t>>(int64_t, int64_t)>
          get_equivalent_pairs,
      bool reset_penalties_on_new_best_solution);
  ~GuidedLocalSearch() override {}
  bool AcceptDelta(Assignment* delta, Assignment* deltadelta) override;
  void ApplyDecision(Decision* d) override;
  bool AtSolution() override;
  void EnterSearch() override;
  bool AtLocalOptimum() override;
  virtual int64_t AssignmentElementPenalty(int index) const = 0;
  virtual int64_t AssignmentPenalty(int64_t var, int64_t value) const = 0;
  virtual int64_t Evaluate(const Assignment* delta, int64_t current_penalty,
                           bool incremental) = 0;
  virtual IntExpr* MakeElementPenalty(int index) = 0;
  std::string DebugString() const override { return "Guided Local Search"; }

 protected:
  // Array which keeps track of modifications done. This allows to effectively
  // revert or commit modifications.
  // TODO(user): Expose this in a utility file.
  template <typename T, typename IndexType = int64_t>
  class DirtyArray {
   public:
    explicit DirtyArray(IndexType size)
        : base_data_(size), modified_data_(size), touched_(size) {}
    // Sets a value in the array. This value will be reverted if Revert() is
    // called.
    void Set(IndexType i, const T& value) {
      modified_data_[i] = value;
      touched_.Set(i);
    }
    // Same as Set() but modifies all values of the array.
    void SetAll(const T& value) {
      for (IndexType i = 0; i < modified_data_.size(); ++i) {
        Set(i, value);
      }
    }
    // Returns the modified value in the array.
    T Get(IndexType i) const { return modified_data_[i]; }
    // Commits all modifications done to the array, effectively copying all
    // modifications to the base values.
    void Commit() {
      for (const IndexType index : touched_.PositionsSetAtLeastOnce()) {
        base_data_[index] = modified_data_[index];
      }
      touched_.ResetAllToFalse();
    }
    // Reverts all modified values in the array.
    void Revert() {
      for (const IndexType index : touched_.PositionsSetAtLeastOnce()) {
        modified_data_[index] = base_data_[index];
      }
      touched_.ResetAllToFalse();
    }
    // Returns the number of values modified since the last call to Commit or
    // Revert.
    int NumSetValues() const {
      return touched_.NumberOfSetCallsWithDifferentArguments();
    }

   private:
    std::vector<T> base_data_;
    std::vector<T> modified_data_;
    SparseBitset<IndexType> touched_;
  };

  int64_t GetValue(int64_t index) const {
    return assignment_.Element(index).Value();
  }
  IntVar* GetVar(int64_t index) const {
    return assignment_.Element(index).Var();
  }
  void AddVars(const std::vector<IntVar*>& vars);
  int NumPrimaryVars() const { return num_vars_; }
  int GetLocalIndexFromVar(IntVar* var) const {
    const int var_index = var->index();
    return (var_index < var_index_to_local_index_.size())
               ? var_index_to_local_index_[var_index]
               : -1;
  }
  void ResetPenalties();

  IntVar* penalized_objective_;
  Assignment::IntContainer assignment_;
  int64_t assignment_penalized_value_;
  int64_t old_penalized_value_;
  const int num_vars_;
  std::vector<int> var_index_to_local_index_;
  const double penalty_factor_;
  P penalties_;
  DirtyArray<int64_t> penalized_values_;
  bool incremental_;
  std::function<std::vector<std::pair<int64_t, int64_t>>(int64_t, int64_t)>
      get_equivalent_pairs_;
  const bool reset_penalties_on_new_best_solution_;
};

template <typename P>
GuidedLocalSearch<P>::GuidedLocalSearch(
    Solver* solver, IntVar* objective, bool maximize, int64_t step,
    const std::vector<IntVar*>& vars, double penalty_factor,
    std::function<std::vector<std::pair<int64_t, int64_t>>(int64_t, int64_t)>
        get_equivalent_pairs,
    bool reset_penalties_on_new_best_solution)
    : Metaheuristic(solver, {maximize}, {objective}, {step}),
      penalized_objective_(nullptr),
      assignment_penalized_value_(0),
      old_penalized_value_(0),
      num_vars_(vars.size()),
      penalty_factor_(penalty_factor),
      penalties_(vars.size()),
      penalized_values_(vars.size()),
      incremental_(false),
      get_equivalent_pairs_(std::move(get_equivalent_pairs)),
      reset_penalties_on_new_best_solution_(
          reset_penalties_on_new_best_solution) {
  AddVars(vars);
}

template <typename P>
void GuidedLocalSearch<P>::AddVars(const std::vector<IntVar*>& vars) {
  const int offset = assignment_.Size();
  if (vars.empty()) return;
  assignment_.Resize(offset + vars.size());
  for (int i = 0; i < vars.size(); ++i) {
    assignment_.AddAtPosition(vars[i], offset + i);
  }
  const int max_var_index =
      (*std::max_element(vars.begin(), vars.end(), [](IntVar* a, IntVar* b) {
        return a->index() < b->index();
      }))->index();
  if (max_var_index >= var_index_to_local_index_.size()) {
    var_index_to_local_index_.resize(max_var_index + 1, -1);
  }
  for (int i = 0; i < vars.size(); ++i) {
    var_index_to_local_index_[vars[i]->index()] = offset + i;
  }
}

// Add the following constraint (includes aspiration criterion):
// if minimizing,
//      objective =< Max(current penalized cost - penalized_objective - step,
//                       best solution cost - step)
// if maximizing,
//      objective >= Min(current penalized cost - penalized_objective + step,
//                       best solution cost + step)
template <typename P>
void GuidedLocalSearch<P>::ApplyDecision(Decision* const d) {
  if (d == solver()->balancing_decision()) {
    return;
  }
  assignment_penalized_value_ = 0;
  if (penalties_.HasPenalties()) {
    // Computing sum of penalties expression.
    // Scope needed to avoid potential leak of elements.
    {
      std::vector<IntVar*> elements;
      for (int i = 0; i < num_vars_; ++i) {
        elements.push_back(MakeElementPenalty(i)->Var());
        const int64_t penalty = AssignmentElementPenalty(i);
        penalized_values_.Set(i, penalty);
        assignment_penalized_value_ =
            CapAdd(assignment_penalized_value_, penalty);
      }
      solver()->SaveAndSetValue(
          reinterpret_cast<void**>(&penalized_objective_),
          reinterpret_cast<void*>(solver()->MakeSum(elements)->Var()));
    }
    penalized_values_.Commit();
    old_penalized_value_ = assignment_penalized_value_;
    incremental_ = false;
    IntExpr* max_pen_exp = solver()->MakeDifference(
        CapSub(CurrentInternalValue(0), Step(0)), penalized_objective_);
    IntVar* max_exp =
        solver()
            ->MakeMax(max_pen_exp, CapSub(BestInternalValue(0), Step(0)))
            ->Var();
    solver()->AddConstraint(
        solver()->MakeLessOrEqual(MinimizationVar(0), max_exp));
  } else {
    penalized_objective_ = nullptr;
    const int64_t bound =
        (CurrentInternalValue(0) < std::numeric_limits<int64_t>::max())
            ? CapSub(CurrentInternalValue(0), Step(0))
            : CurrentInternalValue(0);
    MinimizationVar(0)->SetMax(bound);
  }
}

template <typename P>
void GuidedLocalSearch<P>::ResetPenalties() {
  assignment_penalized_value_ = 0;
  old_penalized_value_ = 0;
  penalized_values_.SetAll(0);
  penalized_values_.Commit();
  penalties_.Reset();
}

template <typename P>
bool GuidedLocalSearch<P>::AtSolution() {
  const int64_t old_best = BestInternalValue(0);
  if (!ObjectiveMonitor::AtSolution()) {
    return false;
  }
  if (penalized_objective_ != nullptr && penalized_objective_->Bound()) {
    // If the value of the best solution has changed (aka a new best solution
    // has been found), triggering a reset on the penalties to start fresh.
    // The immediate consequence is a greedy dive towards a local minimum,
    // followed by a new penalization phase.
    if (reset_penalties_on_new_best_solution_ &&
        old_best != BestInternalValue(0)) {
      ResetPenalties();
      DCHECK_EQ(CurrentInternalValue(0), BestInternalValue(0));
    } else {
      // A penalized move has been found.
      SetCurrentInternalValue(
          0, CapAdd(CurrentInternalValue(0), penalized_objective_->Value()));
    }
  }
  assignment_.Store();
  return true;
}

template <typename P>
void GuidedLocalSearch<P>::EnterSearch() {
  Metaheuristic::EnterSearch();
  solver()->SetUseFastLocalSearch(true);
  penalized_objective_ = nullptr;
  ResetPenalties();
}

// GLS filtering; compute the penalized value corresponding to the delta and
// modify objective bound accordingly.
template <typename P>
bool GuidedLocalSearch<P>::AcceptDelta(Assignment* delta,
                                       Assignment* deltadelta) {
  if (delta == nullptr && deltadelta == nullptr) return true;
  if (!penalties_.HasPenalties()) {
    return Metaheuristic::AcceptDelta(delta, deltadelta);
  }
  int64_t penalty = 0;
  if (!deltadelta->Empty()) {
    if (!incremental_) {
      DCHECK_EQ(penalized_values_.NumSetValues(), 0);
      penalty = Evaluate(delta, assignment_penalized_value_, true);
    } else {
      penalty = Evaluate(deltadelta, old_penalized_value_, true);
    }
    incremental_ = true;
  } else {
    if (incremental_) {
      penalized_values_.Revert();
    }
    incremental_ = false;
    DCHECK_EQ(penalized_values_.NumSetValues(), 0);
    penalty = Evaluate(delta, assignment_penalized_value_, false);
  }
  old_penalized_value_ = penalty;
  if (!delta->HasObjective()) {
    delta->AddObjective(ObjectiveVar(0));
  }
  if (delta->Objective() == ObjectiveVar(0)) {
    const int64_t bound =
        std::max(CapSub(CapSub(CurrentInternalValue(0), Step(0)), penalty),
                 CapSub(BestInternalValue(0), Step(0)));
    if (Maximize(0)) {
      delta->SetObjectiveMin(std::max(CapOpp(bound), delta->ObjectiveMin()));
    } else {
      delta->SetObjectiveMax(std::min(bound, delta->ObjectiveMax()));
    }
  }
  return true;
}

// Penalize (var, value) pairs of maximum utility, with
// utility(var, value) = cost(var, value) / (1 + penalty(var, value))
template <typename P>
bool GuidedLocalSearch<P>::AtLocalOptimum() {
  solver()->SetUseFastLocalSearch(false);
  std::vector<double> utilities(num_vars_);
  double max_utility = -std::numeric_limits<double>::infinity();
  for (int var = 0; var < num_vars_; ++var) {
    const IntVarElement& element = assignment_.Element(var);
    if (!element.Bound()) {
      // Never synced with a solution, problem infeasible.
      return false;
    }
    const int64_t value = element.Value();
    // The fact that we do not penalize loops is influenced by vehicle routing.
    // Assuming a cost of 0 in that case.
    const int64_t cost = (value != var) ? AssignmentPenalty(var, value) : 0;
    const double utility = cost / (penalties_.GetPenalty({var, value}) + 1.0);
    utilities[var] = utility;
    if (utility > max_utility) max_utility = utility;
  }
  for (int var = 0; var < num_vars_; ++var) {
    if (utilities[var] == max_utility) {
      const IntVarElement& element = assignment_.Element(var);
      DCHECK(element.Bound());
      const int64_t value = element.Value();
      if (get_equivalent_pairs_ == nullptr) {
        penalties_.IncrementPenalty({var, value});
      } else {
        for (const auto [other_var, other_value] :
             get_equivalent_pairs_(var, value)) {
          penalties_.IncrementPenalty({other_var, other_value});
        }
      }
    }
  }
  SetCurrentInternalValue(0, std::numeric_limits<int64_t>::max());
  return true;
}

template <typename P>
class BinaryGuidedLocalSearch : public GuidedLocalSearch<P> {
 public:
  BinaryGuidedLocalSearch(
      Solver* solver, IntVar* objective,
      std::function<int64_t(int64_t, int64_t)> objective_function,
      bool maximize, int64_t step, const std::vector<IntVar*>& vars,
      double penalty_factor,
      std::function<std::vector<std::pair<int64_t, int64_t>>(int64_t, int64_t)>
          get_equivalent_pairs,
      bool reset_penalties_on_new_best_solution);
  ~BinaryGuidedLocalSearch() override {}
  IntExpr* MakeElementPenalty(int index) override;
  int64_t AssignmentElementPenalty(int index) const override;
  int64_t AssignmentPenalty(int64_t var, int64_t value) const override;
  int64_t Evaluate(const Assignment* delta, int64_t current_penalty,
                   bool incremental) override;

 private:
  int64_t PenalizedValue(int64_t i, int64_t j) const;
  std::function<int64_t(int64_t, int64_t)> objective_function_;
};

template <typename P>
BinaryGuidedLocalSearch<P>::BinaryGuidedLocalSearch(
    Solver* const solver, IntVar* const objective,
    std::function<int64_t(int64_t, int64_t)> objective_function, bool maximize,
    int64_t step, const std::vector<IntVar*>& vars, double penalty_factor,
    std::function<std::vector<std::pair<int64_t, int64_t>>(int64_t, int64_t)>
        get_equivalent_pairs,
    bool reset_penalties_on_new_best_solution)
    : GuidedLocalSearch<P>(solver, objective, maximize, step, vars,
                           penalty_factor, std::move(get_equivalent_pairs),
                           reset_penalties_on_new_best_solution),
      objective_function_(std::move(objective_function)) {
  solver->SetGuidedLocalSearchPenaltyCallback(
      [this](int64_t i, int64_t j, int64_t) { return PenalizedValue(i, j); });
}

template <typename P>
IntExpr* BinaryGuidedLocalSearch<P>::MakeElementPenalty(int index) {
  return this->solver()->MakeElement(
      [this, index](int64_t i) { return PenalizedValue(index, i); },
      this->GetVar(index));
}

template <typename P>
int64_t BinaryGuidedLocalSearch<P>::AssignmentElementPenalty(int index) const {
  return PenalizedValue(index, this->GetValue(index));
}

template <typename P>
int64_t BinaryGuidedLocalSearch<P>::AssignmentPenalty(int64_t var,
                                                      int64_t value) const {
  return objective_function_(var, value);
}

template <typename P>
int64_t BinaryGuidedLocalSearch<P>::Evaluate(const Assignment* delta,
                                             int64_t current_penalty,
                                             bool incremental) {
  int64_t penalty = current_penalty;
  const Assignment::IntContainer& container = delta->IntVarContainer();
  for (const IntVarElement& new_element : container.elements()) {
    const int index = this->GetLocalIndexFromVar(new_element.Var());
    if (index == -1) continue;
    penalty = CapSub(penalty, this->penalized_values_.Get(index));
    if (new_element.Activated()) {
      const int64_t new_penalty = PenalizedValue(index, new_element.Value());
      penalty = CapAdd(penalty, new_penalty);
      if (incremental) {
        this->penalized_values_.Set(index, new_penalty);
      }
    }
  }
  return penalty;
}

// Penalized value for (i, j) = penalty_factor_ * penalty(i, j) * cost (i, j)
template <typename P>
int64_t BinaryGuidedLocalSearch<P>::PenalizedValue(int64_t i, int64_t j) const {
  const int64_t penalty = this->penalties_.GetPenalty({i, j});
  // Calls to objective_function_(i, j) can be costly.
  if (penalty == 0) return 0;
  const double penalized_value_fp =
      this->penalty_factor_ * penalty * objective_function_(i, j);
  const int64_t penalized_value =
      (penalized_value_fp <= std::numeric_limits<int64_t>::max())
          ? static_cast<int64_t>(penalized_value_fp)
          : std::numeric_limits<int64_t>::max();
  return penalized_value;
}

template <typename P>
class TernaryGuidedLocalSearch : public GuidedLocalSearch<P> {
 public:
  TernaryGuidedLocalSearch(
      Solver* solver, IntVar* objective,
      std::function<int64_t(int64_t, int64_t, int64_t)> objective_function,
      bool maximize, int64_t step, const std::vector<IntVar*>& vars,
      const std::vector<IntVar*>& secondary_vars, double penalty_factor,
      std::function<std::vector<std::pair<int64_t, int64_t>>(int64_t, int64_t)>
          get_equivalent_pairs,
      bool reset_penalties_on_new_best_solution);
  ~TernaryGuidedLocalSearch() override {}
  IntExpr* MakeElementPenalty(int index) override;
  int64_t AssignmentElementPenalty(int index) const override;
  int64_t AssignmentPenalty(int64_t var, int64_t value) const override;
  int64_t Evaluate(const Assignment* delta, int64_t current_penalty,
                   bool incremental) override;

 private:
  int64_t PenalizedValue(int64_t i, int64_t j, int64_t k) const;

  std::function<int64_t(int64_t, int64_t, int64_t)> objective_function_;
  std::vector<int> secondary_values_;
};

template <typename P>
TernaryGuidedLocalSearch<P>::TernaryGuidedLocalSearch(
    Solver* const solver, IntVar* const objective,
    std::function<int64_t(int64_t, int64_t, int64_t)> objective_function,
    bool maximize, int64_t step, const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars, double penalty_factor,
    std::function<std::vector<std::pair<int64_t, int64_t>>(int64_t, int64_t)>
        get_equivalent_pairs,
    bool reset_penalties_on_new_best_solution)
    : GuidedLocalSearch<P>(solver, objective, maximize, step, vars,
                           penalty_factor, std::move(get_equivalent_pairs),
                           reset_penalties_on_new_best_solution),
      objective_function_(std::move(objective_function)),
      secondary_values_(this->NumPrimaryVars(), -1) {
  this->AddVars(secondary_vars);
  solver->SetGuidedLocalSearchPenaltyCallback(
      [this](int64_t i, int64_t j, int64_t k) {
        return PenalizedValue(i, j, k);
      });
}

template <typename P>
IntExpr* TernaryGuidedLocalSearch<P>::MakeElementPenalty(int index) {
  Solver* const solver = this->solver();
  IntVar* var = solver->MakeIntVar(0, kint64max);
  solver->AddConstraint(solver->MakeLightElement(
      [this, index](int64_t j, int64_t k) {
        return PenalizedValue(index, j, k);
      },
      var, this->GetVar(index), this->GetVar(this->NumPrimaryVars() + index)));
  return var;
}

template <typename P>
int64_t TernaryGuidedLocalSearch<P>::AssignmentElementPenalty(int index) const {
  return PenalizedValue(index, this->GetValue(index),
                        this->GetValue(this->NumPrimaryVars() + index));
}

template <typename P>
int64_t TernaryGuidedLocalSearch<P>::AssignmentPenalty(int64_t var,
                                                       int64_t value) const {
  return objective_function_(var, value,
                             this->GetValue(this->NumPrimaryVars() + var));
}

template <typename P>
int64_t TernaryGuidedLocalSearch<P>::Evaluate(const Assignment* delta,
                                              int64_t current_penalty,
                                              bool incremental) {
  int64_t penalty = current_penalty;
  const Assignment::IntContainer& container = delta->IntVarContainer();
  // Collect values for each secondary variable, matching them with their
  // corresponding primary variable. Making sure all secondary values are -1 if
  // unset.
  for (const IntVarElement& new_element : container.elements()) {
    const int index = this->GetLocalIndexFromVar(new_element.Var());
    if (index != -1 && index < this->NumPrimaryVars()) {  // primary variable
      secondary_values_[index] = -1;
    }
  }
  for (const IntVarElement& new_element : container.elements()) {
    const int index = this->GetLocalIndexFromVar(new_element.Var());
    if (!new_element.Activated()) continue;
    if (index != -1 && index >= this->NumPrimaryVars()) {  // secondary variable
      secondary_values_[index - this->NumPrimaryVars()] = new_element.Value();
    }
  }
  for (const IntVarElement& new_element : container.elements()) {
    const int index = this->GetLocalIndexFromVar(new_element.Var());
    // Only process primary variables.
    if (index == -1 || index >= this->NumPrimaryVars()) {
      continue;
    }
    penalty = CapSub(penalty, this->penalized_values_.Get(index));
    // Performed and active.
    if (new_element.Activated() && secondary_values_[index] != -1) {
      const int64_t new_penalty =
          PenalizedValue(index, new_element.Value(), secondary_values_[index]);
      penalty = CapAdd(penalty, new_penalty);
      if (incremental) {
        this->penalized_values_.Set(index, new_penalty);
      }
    }
  }
  return penalty;
}

// Penalized value for (i, j) = penalty_factor_ * penalty(i, j) * cost (i, j, k)
template <typename P>
int64_t TernaryGuidedLocalSearch<P>::PenalizedValue(int64_t i, int64_t j,
                                                    int64_t k) const {
  const int64_t penalty = this->penalties_.GetPenalty({i, j});
  // Calls to objective_function_(i, j, k) can be costly.
  if (penalty == 0) return 0;
  const double penalized_value_fp =
      this->penalty_factor_ * penalty * objective_function_(i, j, k);
  const int64_t penalized_value =
      (penalized_value_fp < std::numeric_limits<int64_t>::max())
          ? static_cast<int64_t>(penalized_value_fp)
          : std::numeric_limits<int64_t>::max();
  return penalized_value;
}
}  // namespace

ObjectiveMonitor* Solver::MakeGuidedLocalSearch(
    bool maximize, IntVar* const objective,
    Solver::IndexEvaluator2 objective_function, int64_t step,
    const std::vector<IntVar*>& vars, double penalty_factor,
    std::function<std::vector<std::pair<int64_t, int64_t>>(int64_t, int64_t)>
        get_equivalent_pairs,
    bool reset_penalties_on_new_best_solution) {
  if (absl::GetFlag(FLAGS_cp_use_sparse_gls_penalties)) {
    return RevAlloc(new BinaryGuidedLocalSearch<GuidedLocalSearchPenaltiesMap>(
        this, objective, std::move(objective_function), maximize, step, vars,
        penalty_factor, std::move(get_equivalent_pairs),
        reset_penalties_on_new_best_solution));
  } else {
    return RevAlloc(
        new BinaryGuidedLocalSearch<GuidedLocalSearchPenaltiesTable>(
            this, objective, std::move(objective_function), maximize, step,
            vars, penalty_factor, std::move(get_equivalent_pairs),
            reset_penalties_on_new_best_solution));
  }
}

ObjectiveMonitor* Solver::MakeGuidedLocalSearch(
    bool maximize, IntVar* const objective,
    Solver::IndexEvaluator3 objective_function, int64_t step,
    const std::vector<IntVar*>& vars,
    const std::vector<IntVar*>& secondary_vars, double penalty_factor,
    std::function<std::vector<std::pair<int64_t, int64_t>>(int64_t, int64_t)>
        get_equivalent_pairs,
    bool reset_penalties_on_new_best_solution) {
  if (absl::GetFlag(FLAGS_cp_use_sparse_gls_penalties)) {
    return RevAlloc(new TernaryGuidedLocalSearch<GuidedLocalSearchPenaltiesMap>(
        this, objective, std::move(objective_function), maximize, step, vars,
        secondary_vars, penalty_factor, std::move(get_equivalent_pairs),
        reset_penalties_on_new_best_solution));
  } else {
    return RevAlloc(
        new TernaryGuidedLocalSearch<GuidedLocalSearchPenaltiesTable>(
            this, objective, std::move(objective_function), maximize, step,
            vars, secondary_vars, penalty_factor,
            std::move(get_equivalent_pairs),
            reset_penalties_on_new_best_solution));
  }
}

// ---------- Search Limits ----------

// ----- Base Class -----

SearchLimit::~SearchLimit() {}

void SearchLimit::Install() {
  ListenToEvent(Solver::MonitorEvent::kEnterSearch);
  ListenToEvent(Solver::MonitorEvent::kBeginNextDecision);
  ListenToEvent(Solver::MonitorEvent::kPeriodicCheck);
  ListenToEvent(Solver::MonitorEvent::kRefuteDecision);
}

void SearchLimit::EnterSearch() {
  crossed_ = false;
  Init();
}

void SearchLimit::BeginNextDecision(DecisionBuilder* const) {
  PeriodicCheck();
  TopPeriodicCheck();
}

void SearchLimit::RefuteDecision(Decision* const) {
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

// ----- Regular Limit -----

RegularLimit::RegularLimit(Solver* const s, absl::Duration time,
                           int64_t branches, int64_t failures,
                           int64_t solutions, bool smart_time_check,
                           bool cumulative)
    : SearchLimit(s),
      duration_limit_(time),
      solver_time_at_limit_start_(s->Now()),
      last_time_elapsed_(absl::ZeroDuration()),
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

void RegularLimit::Install() {
  SearchLimit::Install();
  ListenToEvent(Solver::MonitorEvent::kExitSearch);
  ListenToEvent(Solver::MonitorEvent::kIsUncheckedSolutionLimitReached);
  ListenToEvent(Solver::MonitorEvent::kProgressPercent);
  ListenToEvent(Solver::MonitorEvent::kAccept);
}

void RegularLimit::Copy(const SearchLimit* const limit) {
  const RegularLimit* const regular =
      reinterpret_cast<const RegularLimit* const>(limit);
  duration_limit_ = regular->duration_limit_;
  branches_ = regular->branches_;
  failures_ = regular->failures_;
  solutions_ = regular->solutions_;
  smart_time_check_ = regular->smart_time_check_;
  cumulative_ = regular->cumulative_;
}

SearchLimit* RegularLimit::MakeClone() const { return MakeIdenticalClone(); }

RegularLimit* RegularLimit::MakeIdenticalClone() const {
  Solver* const s = solver();
  return s->MakeLimit(wall_time(), branches_, failures_, solutions_,
                      smart_time_check_);
}

bool RegularLimit::CheckWithOffset(absl::Duration offset) {
  Solver* const s = solver();
  // Warning limits might be kint64max, do not move the offset to the rhs
  return s->branches() - branches_offset_ >= branches_ ||
         s->failures() - failures_offset_ >= failures_ || CheckTime(offset) ||
         s->solutions() - solutions_offset_ >= solutions_;
}

int RegularLimit::ProgressPercent() {
  Solver* const s = solver();
  int64_t progress = GetPercent(s->branches(), branches_offset_, branches_);
  progress = std::max(progress,
                      GetPercent(s->failures(), failures_offset_, failures_));
  progress = std::max(
      progress, GetPercent(s->solutions(), solutions_offset_, solutions_));
  if (duration_limit() != absl::InfiniteDuration()) {
    progress = std::max(progress, (100 * TimeElapsed()) / duration_limit());
  }
  return progress;
}

void RegularLimit::Init() {
  Solver* const s = solver();
  branches_offset_ = s->branches();
  failures_offset_ = s->failures();
  solver_time_at_limit_start_ = s->Now();
  last_time_elapsed_ = absl::ZeroDuration();
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
    duration_limit_ -= s->Now() - solver_time_at_limit_start_;
    solutions_ -= s->solutions() - solutions_offset_;
  }
}

void RegularLimit::UpdateLimits(absl::Duration time, int64_t branches,
                                int64_t failures, int64_t solutions) {
  Init();
  duration_limit_ = time;
  branches_ = branches;
  failures_ = failures;
  solutions_ = solutions;
}

bool RegularLimit::IsUncheckedSolutionLimitReached() {
  Solver* const s = solver();
  return s->solutions() + s->unchecked_solutions() - solutions_offset_ >=
         solutions_;
}

std::string RegularLimit::DebugString() const {
  return absl::StrFormat(
      "RegularLimit(crossed = %i, duration_limit = %s, "
      "branches = %d, failures = %d, solutions = %d cumulative = %s",
      crossed(), absl::FormatDuration(duration_limit()), branches_, failures_,
      solutions_, (cumulative_ ? "true" : "false"));
}

void RegularLimit::Accept(ModelVisitor* const visitor) const {
  visitor->BeginVisitExtension(ModelVisitor::kSearchLimitExtension);
  visitor->VisitIntegerArgument(ModelVisitor::kTimeLimitArgument, wall_time());
  visitor->VisitIntegerArgument(ModelVisitor::kBranchesLimitArgument,
                                branches_);
  visitor->VisitIntegerArgument(ModelVisitor::kFailuresLimitArgument,
                                failures_);
  visitor->VisitIntegerArgument(ModelVisitor::kSolutionLimitArgument,
                                solutions_);
  visitor->VisitIntegerArgument(ModelVisitor::kSmartTimeCheckArgument,
                                smart_time_check_);
  visitor->VisitIntegerArgument(ModelVisitor::kCumulativeArgument, cumulative_);
  visitor->EndVisitExtension(ModelVisitor::kSearchLimitExtension);
}

bool RegularLimit::CheckTime(absl::Duration offset) {
  return TimeElapsed() >= duration_limit() - offset;
}

absl::Duration RegularLimit::TimeElapsed() {
  const int64_t kMaxSkip = 100;
  const int64_t kCheckWarmupIterations = 100;
  ++check_count_;
  if (duration_limit() != absl::InfiniteDuration() &&
      next_check_ <= check_count_) {
    Solver* const s = solver();
    absl::Duration elapsed = s->Now() - solver_time_at_limit_start_;
    if (smart_time_check_ && check_count_ > kCheckWarmupIterations &&
        elapsed > absl::ZeroDuration()) {
      const int64_t estimated_check_count_at_limit = MathUtil::FastInt64Round(
          check_count_ * absl::FDivDuration(duration_limit_, elapsed));
      next_check_ =
          std::min(check_count_ + kMaxSkip, estimated_check_count_at_limit);
    }
    last_time_elapsed_ = elapsed;
  }
  return last_time_elapsed_;
}

RegularLimit* Solver::MakeTimeLimit(absl::Duration time) {
  return MakeLimit(time, std::numeric_limits<int64_t>::max(),
                   std::numeric_limits<int64_t>::max(),
                   std::numeric_limits<int64_t>::max(),
                   /*smart_time_check=*/false, /*cumulative=*/false);
}

RegularLimit* Solver::MakeBranchesLimit(int64_t branches) {
  return MakeLimit(absl::InfiniteDuration(), branches,
                   std::numeric_limits<int64_t>::max(),
                   std::numeric_limits<int64_t>::max(),
                   /*smart_time_check=*/false, /*cumulative=*/false);
}

RegularLimit* Solver::MakeFailuresLimit(int64_t failures) {
  return MakeLimit(absl::InfiniteDuration(),
                   std::numeric_limits<int64_t>::max(), failures,
                   std::numeric_limits<int64_t>::max(),
                   /*smart_time_check=*/false, /*cumulative=*/false);
}

RegularLimit* Solver::MakeSolutionsLimit(int64_t solutions) {
  return MakeLimit(absl::InfiniteDuration(),
                   std::numeric_limits<int64_t>::max(),
                   std::numeric_limits<int64_t>::max(), solutions,
                   /*smart_time_check=*/false, /*cumulative=*/false);
}

RegularLimit* Solver::MakeLimit(int64_t time, int64_t branches,
                                int64_t failures, int64_t solutions,
                                bool smart_time_check, bool cumulative) {
  return MakeLimit(absl::Milliseconds(time), branches, failures, solutions,
                   smart_time_check, cumulative);
}

RegularLimit* Solver::MakeLimit(absl::Duration time, int64_t branches,
                                int64_t failures, int64_t solutions,
                                bool smart_time_check, bool cumulative) {
  return RevAlloc(new RegularLimit(this, time, branches, failures, solutions,
                                   smart_time_check, cumulative));
}

RegularLimit* Solver::MakeLimit(const RegularLimitParameters& proto) {
  return MakeLimit(proto.time() == std::numeric_limits<int64_t>::max()
                       ? absl::InfiniteDuration()
                       : absl::Milliseconds(proto.time()),
                   proto.branches(), proto.failures(), proto.solutions(),
                   proto.smart_time_check(), proto.cumulative());
}

RegularLimitParameters Solver::MakeDefaultRegularLimitParameters() const {
  RegularLimitParameters proto;
  proto.set_time(std::numeric_limits<int64_t>::max());
  proto.set_branches(std::numeric_limits<int64_t>::max());
  proto.set_failures(std::numeric_limits<int64_t>::max());
  proto.set_solutions(std::numeric_limits<int64_t>::max());
  proto.set_smart_time_check(false);
  proto.set_cumulative(false);
  return proto;
}

// ----- Improvement Search Limit -----

ImprovementSearchLimit::ImprovementSearchLimit(
    Solver* solver, IntVar* objective_var, bool maximize,
    double objective_scaling_factor, double objective_offset,
    double improvement_rate_coefficient,
    int improvement_rate_solutions_distance)
    : ImprovementSearchLimit(solver, std::vector<IntVar*>{objective_var},
                             std::vector<bool>{maximize},
                             std::vector<double>{objective_scaling_factor},
                             std::vector<double>{objective_offset},
                             improvement_rate_coefficient,
                             improvement_rate_solutions_distance) {}

ImprovementSearchLimit::ImprovementSearchLimit(
    Solver* solver, std::vector<IntVar*> objective_vars,
    std::vector<bool> maximize, std::vector<double> objective_scaling_factors,
    std::vector<double> objective_offsets, double improvement_rate_coefficient,
    int improvement_rate_solutions_distance)
    : SearchLimit(solver),
      objective_vars_(std::move(objective_vars)),
      maximize_(std::move(maximize)),
      objective_scaling_factors_(std::move(objective_scaling_factors)),
      objective_offsets_(std::move(objective_offsets)),
      improvement_rate_coefficient_(improvement_rate_coefficient),
      improvement_rate_solutions_distance_(improvement_rate_solutions_distance),
      best_objectives_(objective_vars_.size()),
      improvements_(objective_vars_.size()),
      thresholds_(objective_vars_.size(),
                  std::numeric_limits<double>::infinity()) {
  Init();
}

ImprovementSearchLimit::~ImprovementSearchLimit() {}

void ImprovementSearchLimit::Install() {
  SearchLimit::Install();
  ListenToEvent(Solver::MonitorEvent::kAtSolution);
}

void ImprovementSearchLimit::Init() {
  for (int i = 0; i < objective_vars_.size(); ++i) {
    best_objectives_[i] = std::numeric_limits<double>::infinity();
    improvements_[i].clear();
    thresholds_[i] = std::numeric_limits<double>::infinity();
  }
  objective_updated_ = false;
  gradient_stage_ = true;
}

void ImprovementSearchLimit::Copy(const SearchLimit* const limit) {
  const ImprovementSearchLimit* const improv =
      reinterpret_cast<const ImprovementSearchLimit* const>(limit);
  objective_vars_ = improv->objective_vars_;
  maximize_ = improv->maximize_;
  objective_scaling_factors_ = improv->objective_scaling_factors_;
  objective_offsets_ = improv->objective_offsets_;
  improvement_rate_coefficient_ = improv->improvement_rate_coefficient_;
  improvement_rate_solutions_distance_ =
      improv->improvement_rate_solutions_distance_;
  improvements_ = improv->improvements_;
  thresholds_ = improv->thresholds_;
  best_objectives_ = improv->best_objectives_;
  objective_updated_ = improv->objective_updated_;
  gradient_stage_ = improv->gradient_stage_;
}

SearchLimit* ImprovementSearchLimit::MakeClone() const {
  return solver()->RevAlloc(new ImprovementSearchLimit(
      solver(), objective_vars_, maximize_, objective_scaling_factors_,
      objective_offsets_, improvement_rate_coefficient_,
      improvement_rate_solutions_distance_));
}

bool ImprovementSearchLimit::CheckWithOffset(absl::Duration) {
  if (!objective_updated_) {
    return false;
  }
  objective_updated_ = false;

  std::vector<double> improvement_rates(improvements_.size());
  for (int i = 0; i < improvements_.size(); ++i) {
    if (improvements_[i].size() <= improvement_rate_solutions_distance_) {
      return false;
    }

    const auto [cur_obj, cur_neighbors] = improvements_[i].back();
    const auto [prev_obj, prev_neighbors] = improvements_[i].front();
    DCHECK_GT(cur_neighbors, prev_neighbors);
    improvement_rates[i] =
        (prev_obj - cur_obj) / (cur_neighbors - prev_neighbors);
    if (gradient_stage_) continue;
    const double scaled_improvement_rate =
        improvement_rate_coefficient_ * improvement_rates[i];
    if (scaled_improvement_rate < thresholds_[i]) {
      return true;
    } else if (scaled_improvement_rate > thresholds_[i]) {
      return false;
    }
  }
  if (gradient_stage_ && std::lexicographical_compare(
                             improvement_rates.begin(), improvement_rates.end(),
                             thresholds_.begin(), thresholds_.end())) {
    thresholds_ = std::move(improvement_rates);
  }
  return false;
}

bool ImprovementSearchLimit::AtSolution() {
  std::vector<double> scaled_new_objectives(objective_vars_.size());
  for (int i = 0; i < objective_vars_.size(); ++i) {
    const int64_t new_objective =
        objective_vars_[i] != nullptr && objective_vars_[i]->Bound()
            ? objective_vars_[i]->Min()
            : (maximize_[i] ? solver()
                                  ->GetOrCreateLocalSearchState()
                                  ->ObjectiveMaxFromIndex(i)
                            : solver()
                                  ->GetOrCreateLocalSearchState()
                                  ->ObjectiveMinFromIndex(i));
    // To simplify, we'll consider minimization only in the rest of the code,
    // which requires taking the opposite of the objective value if maximizing.
    scaled_new_objectives[i] = (maximize_[i] ? -objective_scaling_factors_[i]
                                             : objective_scaling_factors_[i]) *
                               (new_objective + objective_offsets_[i]);
  }
  const bool is_improvement = std::lexicographical_compare(
      scaled_new_objectives.begin(), scaled_new_objectives.end(),
      best_objectives_.begin(), best_objectives_.end());
  if (gradient_stage_ && !is_improvement) {
    gradient_stage_ = false;
    // In case we haven't got enough solutions during the first stage, the
    // limit never stops the search.
    for (int i = 0; i < objective_vars_.size(); ++i) {
      if (thresholds_[i] == std::numeric_limits<double>::infinity()) {
        thresholds_[i] = -1;
      }
    }
  }

  if (is_improvement) {
    objective_updated_ = true;
    for (int i = 0; i < objective_vars_.size(); ++i) {
      improvements_[i].push_back(
          std::make_pair(scaled_new_objectives[i], solver()->neighbors()));
      // We need to have 'improvement_rate_solutions_distance_' + 1 element in
      // the 'improvements_', so the distance between improvements is
      // 'improvement_rate_solutions_distance_'.
      if (improvements_[i].size() - 1 > improvement_rate_solutions_distance_) {
        improvements_[i].pop_front();
      }
      DCHECK_LE(improvements_[i].size() - 1,
                improvement_rate_solutions_distance_);
    }
    best_objectives_ = std::move(scaled_new_objectives);
  }
  return true;
}

ImprovementSearchLimit* Solver::MakeImprovementLimit(
    IntVar* objective_var, bool maximize, double objective_scaling_factor,
    double objective_offset, double improvement_rate_coefficient,
    int improvement_rate_solutions_distance) {
  return RevAlloc(new ImprovementSearchLimit(
      this, objective_var, maximize, objective_scaling_factor, objective_offset,
      improvement_rate_coefficient, improvement_rate_solutions_distance));
}

ImprovementSearchLimit* Solver::MakeLexicographicImprovementLimit(
    std::vector<IntVar*> objective_vars, std::vector<bool> maximize,
    std::vector<double> objective_scaling_factors,
    std::vector<double> objective_offsets, double improvement_rate_coefficient,
    int improvement_rate_solutions_distance) {
  return RevAlloc(new ImprovementSearchLimit(
      this, std::move(objective_vars), std::move(maximize),
      std::move(objective_scaling_factors), std::move(objective_offsets),
      improvement_rate_coefficient, improvement_rate_solutions_distance));
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

  bool CheckWithOffset(absl::Duration offset) override {
    // Check being non-const, there may be side effects. So we always call both
    // checks.
    const bool check_1 = limit_1_->CheckWithOffset(offset);
    const bool check_2 = limit_2_->CheckWithOffset(offset);
    return check_1 || check_2;
  }

  void Init() override {
    limit_1_->Init();
    limit_2_->Init();
  }

  void Copy(const SearchLimit* const) override {
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
    return absl::StrCat("OR limit (", limit_1_->DebugString(), " OR ",
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

namespace {
class CustomLimit : public SearchLimit {
 public:
  CustomLimit(Solver* s, std::function<bool()> limiter);
  bool CheckWithOffset(absl::Duration offset) override;
  void Init() override;
  void Copy(const SearchLimit* limit) override;
  SearchLimit* MakeClone() const override;

 private:
  std::function<bool()> limiter_;
};

CustomLimit::CustomLimit(Solver* const s, std::function<bool()> limiter)
    : SearchLimit(s), limiter_(std::move(limiter)) {}

bool CustomLimit::CheckWithOffset(absl::Duration) {
  // TODO(user): Consider the offset in limiter_.
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
    return absl::StrFormat("SolveOnce(%s)", db_->DebugString());
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
                 bool maximize, int64_t step)
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
                 bool maximize, int64_t step,
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
    return absl::StrFormat("NestedOptimize(db = %s, maximize = %d, step = %d)",
                           db_->DebugString(), maximize_, step_);
  }

  void Accept(ModelVisitor* const visitor) const override {
    db_->Accept(visitor);
  }

 private:
  DecisionBuilder* const db_;
  Assignment* const solution_;
  const bool maximize_;
  const int64_t step_;
  std::vector<SearchMonitor*> monitors_;
  SolutionCollector* collector_;
};
}  // namespace

DecisionBuilder* Solver::MakeNestedOptimize(DecisionBuilder* const db,
                                            Assignment* const solution,
                                            bool maximize, int64_t step) {
  return RevAlloc(new NestedOptimize(db, solution, maximize, step));
}

DecisionBuilder* Solver::MakeNestedOptimize(DecisionBuilder* const db,
                                            Assignment* const solution,
                                            bool maximize, int64_t step,
                                            SearchMonitor* const monitor1) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(monitor1);
  return RevAlloc(new NestedOptimize(db, solution, maximize, step, monitors));
}

DecisionBuilder* Solver::MakeNestedOptimize(DecisionBuilder* const db,
                                            Assignment* const solution,
                                            bool maximize, int64_t step,
                                            SearchMonitor* const monitor1,
                                            SearchMonitor* const monitor2) {
  std::vector<SearchMonitor*> monitors;
  monitors.push_back(monitor1);
  monitors.push_back(monitor2);
  return RevAlloc(new NestedOptimize(db, solution, maximize, step, monitors));
}

DecisionBuilder* Solver::MakeNestedOptimize(DecisionBuilder* const db,
                                            Assignment* const solution,
                                            bool maximize, int64_t step,
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
    int64_t step, SearchMonitor* const monitor1, SearchMonitor* const monitor2,
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
    int64_t step, const std::vector<SearchMonitor*>& monitors) {
  return RevAlloc(new NestedOptimize(db, solution, maximize, step, monitors));
}

// ---------- Restart ----------

namespace {
// Luby Strategy
int64_t NextLuby(int i) {
  DCHECK_GT(i, 0);
  DCHECK_LT(i, std::numeric_limits<int32_t>::max());
  int64_t power;

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

  void Install() override { ListenToEvent(Solver::MonitorEvent::kBeginFail); }

  std::string DebugString() const override {
    return absl::StrFormat("LubyRestart(%i)", scale_factor_);
  }

 private:
  const int scale_factor_;
  int iteration_;
  int64_t current_fails_;
  int64_t next_step_;
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

  void Install() override { ListenToEvent(Solver::MonitorEvent::kBeginFail); }

  std::string DebugString() const override {
    return absl::StrFormat("ConstantRestart(%i)", frequency_);
  }

 private:
  const int frequency_;
  int64_t current_fails_;
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

  void EndNextDecision(DecisionBuilder* const, Decision* const d) override {
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
  std::vector<SimpleRevFIFO<IntVar*>> clauses_;
  std::vector<SimpleRevFIFO<Decision*>> decisions_;
  std::vector<SimpleRevFIFO<bool>> directions_;
};

// ----- Symmetry Breaker -----

void SymmetryBreaker::AddIntegerVariableEqualValueClause(IntVar* const var,
                                                         int64_t value) {
  CHECK(var != nullptr);
  Solver* const solver = var->solver();
  IntVar* const term = solver->MakeIsEqualCstVar(var, value);
  symmetry_manager()->AddTermToClause(this, term);
}

void SymmetryBreaker::AddIntegerVariableGreaterOrEqualValueClause(
    IntVar* const var, int64_t value) {
  CHECK(var != nullptr);
  Solver* const solver = var->solver();
  IntVar* const term = solver->MakeIsGreaterOrEqualCstVar(var, value);
  symmetry_manager()->AddTermToClause(this, term);
}

void SymmetryBreaker::AddIntegerVariableLessOrEqualValueClause(
    IntVar* const var, int64_t value) {
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
