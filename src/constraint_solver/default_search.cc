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

#include <cstddef>
#include "base/hash.h"
#include <limits>
#include "base/unique_ptr.h"
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"

#include "base/stl_util.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/cached_log.h"
#include "util/string_array.h"
#include "base/random.h"

DEFINE_int32(cp_impact_divider, 10, "Divider for continuous update.");

namespace operations_research {

// Default constants for search phase parameters.
const int DefaultPhaseParameters::kDefaultNumberOfSplits = 100;
const int DefaultPhaseParameters::kDefaultHeuristicPeriod = 100;
const int DefaultPhaseParameters::kDefaultHeuristicNumFailuresLimit = 30;
const int DefaultPhaseParameters::kDefaultSeed = 0;
const double DefaultPhaseParameters::kDefaultRestartLogSize = -1.0;
const bool DefaultPhaseParameters::kDefaultUseNoGoods = true;

class NoGoodManager;

namespace {
// ----- DomainWatcher -----

// This class follows the domains of variables and will report the log of the
// search space of all integer variables.
class DomainWatcher {
 public:
  DomainWatcher(const std::vector<IntVar*>& vars, int cache_size) : vars_(vars) {
    cached_log_.Init(cache_size);
  }

  double LogSearchSpaceSize() {
    double result = 0.0;
    for (int index = 0; index < vars_.size(); ++index) {
      result += cached_log_.Log2(vars_[index]->Size());
    }
    return result;
  }

  double Log2(int64 size) const { return cached_log_.Log2(size); }

 private:
  std::vector<IntVar*> vars_;
  CachedLog cached_log_;
  DISALLOW_COPY_AND_ASSIGN(DomainWatcher);
};

// ---------- FindVar decision visitor ---------

class FindVar : public DecisionVisitor {
 public:
  FindVar() : var_(nullptr), value_(0), valid_(false) {}

  virtual ~FindVar() {}

  virtual void VisitSetVariableValue(IntVar* const var, int64 value) {
    var_ = var;
    value_ = value;
    valid_ = true;
  }

  virtual void VisitSplitVariableDomain(IntVar* const var, int64 value,
                                        bool start_with_lower_half) {
    valid_ = false;
  }

  virtual void VisitScheduleOrPostpone(IntervalVar* const var, int64 est) {
    valid_ = false;
  }

  virtual void VisitTryRankFirst(SequenceVar* const sequence, int index) {
    valid_ = false;
  }

  virtual void VisitTryRankLast(SequenceVar* const sequence, int index) {
    valid_ = false;
  }

  virtual void VisitUnknownDecision() { valid_ = false; }

  // Indicates whether var() and value() can be called.
  bool valid() { return valid_; }

  // Returns the current variable.
  IntVar* const var() const {
    CHECK(valid_);
    return var_;
  }

  // Returns the value of the current variable.
  int64 value() {
    CHECK(valid_);
    return value_;
  }

  virtual std::string DebugString() const { return "FindVar decision visitor"; }

 private:
  IntVar* var_;
  int64 value_;
  bool valid_;
};

// ----- Auxilliary decision builders to init impacts -----

// This class initialize impacts by scanning each value of the domain
// of the variable.
class InitVarImpacts : public DecisionBuilder {
 public:
  // ----- main -----
  InitVarImpacts()
      : var_(nullptr),
        update_impact_callback_(nullptr),
        new_start_(false),
        var_index_(0),
        value_index_(-1),
        update_impact_closure_(
            NewPermanentCallback(this, &InitVarImpacts::UpdateImpacts)),
        updater_(update_impact_closure_.get()) {
    CHECK(update_impact_closure_ != nullptr);
  }

  virtual ~InitVarImpacts() {}

  void UpdateImpacts() {
    // the Min is always the value we just set.
    update_impact_callback_->Run(var_index_, var_->Min());
  }

  void Init(IntVar* const var, IntVarIterator* const iterator, int var_index) {
    var_ = var;
    iterator_ = iterator;
    var_index_ = var_index;
    new_start_ = true;
    value_index_ = 0;
  }

  virtual Decision* Next(Solver* const solver) {
    CHECK(var_ != nullptr);
    CHECK(iterator_ != nullptr);
    if (new_start_) {
      active_values_.clear();
      for (const int64 value : InitAndGetValues(iterator_)) {
        active_values_.push_back(value);
      }
      new_start_ = false;
    }
    if (value_index_ == active_values_.size()) {
      return nullptr;
    }
    updater_.var_ = var_;
    updater_.value_ = active_values_[value_index_];
    value_index_++;
    return &updater_;
  }

  void set_update_impact_callback(Callback2<int, int64>* const callback) {
    update_impact_callback_ = callback;
  }

 private:
  // ----- helper decision -----
  class AssignCallFail : public Decision {
   public:
    explicit AssignCallFail(Closure* const update_impact_closure)
        : var_(nullptr),
          value_(0),
          update_impact_closure_(update_impact_closure) {
      CHECK(update_impact_closure_ != nullptr);
    }
    virtual ~AssignCallFail() {}
    virtual void Apply(Solver* const solver) {
      CHECK(var_ != nullptr);
      var_->SetValue(value_);
      // We call the closure on the part that cannot fail.
      update_impact_closure_->Run();
      solver->Fail();
    }
    virtual void Refute(Solver* const solver) {}
    // Public data for easy access.
    IntVar* var_;
    int64 value_;

   private:
    Closure* const update_impact_closure_;
    DISALLOW_COPY_AND_ASSIGN(AssignCallFail);
  };

  IntVar* var_;
  Callback2<int, int64>* update_impact_callback_;
  bool new_start_;
  IntVarIterator* iterator_;
  int var_index_;
  std::vector<int64> active_values_;
  int value_index_;
  const std::unique_ptr<Closure> update_impact_closure_;
  AssignCallFail updater_;
};

// This class initialize impacts by scanning at most 'split_size'
// intervals on the domain of the variable.

class InitVarImpactsWithSplits : public DecisionBuilder {
 public:
  // ----- helper decision -----
  class AssignIntervalCallFail : public Decision {
   public:
    explicit AssignIntervalCallFail(Closure* const update_impact_closure)
        : var_(nullptr),
          value_min_(0),
          value_max_(0),
          update_impact_closure_(update_impact_closure) {
      CHECK(update_impact_closure_ != nullptr);
    }
    virtual ~AssignIntervalCallFail() {}
    virtual void Apply(Solver* const solver) {
      CHECK(var_ != nullptr);
      var_->SetRange(value_min_, value_max_);
      // We call the closure on the part that cannot fail.
      update_impact_closure_->Run();
      solver->Fail();
    }
    virtual void Refute(Solver* const solver) {}

    // Public for easy access.
    IntVar* var_;
    int64 value_min_;
    int64 value_max_;

   private:
    Closure* const update_impact_closure_;
    DISALLOW_COPY_AND_ASSIGN(AssignIntervalCallFail);
  };

  // ----- main -----

  explicit InitVarImpactsWithSplits(int split_size)
      : var_(nullptr),
        update_impact_callback_(nullptr),
        new_start_(false),
        var_index_(0),
        min_value_(0),
        max_value_(0),
        split_size_(split_size),
        split_index_(-1),
        update_impact_closure_(NewPermanentCallback(
            this, &InitVarImpactsWithSplits::UpdateImpacts)),
        updater_(update_impact_closure_.get()) {
    CHECK(update_impact_closure_ != nullptr);
  }

  virtual ~InitVarImpactsWithSplits() {}

  void UpdateImpacts() {
    for (const int64 value : InitAndGetValues(iterator_)) {
      update_impact_callback_->Run(var_index_, value);
    }
  }

  void Init(IntVar* const var, IntVarIterator* const iterator, int var_index) {
    var_ = var;
    iterator_ = iterator;
    var_index_ = var_index;
    new_start_ = true;
    split_index_ = 0;
  }

  int64 IntervalStart(int index) const {
    const int64 length = max_value_ - min_value_ + 1;
    return (min_value_ + length * index / split_size_);
  }

  virtual Decision* Next(Solver* const solver) {
    if (new_start_) {
      min_value_ = var_->Min();
      max_value_ = var_->Max();
      new_start_ = false;
    }
    if (split_index_ == split_size_) {
      return nullptr;
    }
    updater_.var_ = var_;
    updater_.value_min_ = IntervalStart(split_index_);
    split_index_++;
    if (split_index_ == split_size_) {
      updater_.value_max_ = max_value_;
    } else {
      updater_.value_max_ = IntervalStart(split_index_) - 1;
    }
    return &updater_;
  }

  void set_update_impact_callback(Callback2<int, int64>* const callback) {
    update_impact_callback_ = callback;
  }

 private:
  IntVar* var_;
  Callback2<int, int64>* update_impact_callback_;
  bool new_start_;
  IntVarIterator* iterator_;
  int var_index_;
  int64 min_value_;
  int64 max_value_;
  const int split_size_;
  int split_index_;
  std::unique_ptr<Closure> update_impact_closure_;
  AssignIntervalCallFail updater_;
};

// ----- ImpactRecorder

// This class will record the impacts of all assignment of values to
// variables. Its main output is to find the optimal pair (variable/value)
// based on default phase parameters.
class ImpactRecorder : public SearchMonitor {
 public:
  static const int kLogCacheSize;
  static const double kPerfectImpact;
  static const double kFailureImpact;
  static const double kInitFailureImpact;
  static const int kUninitializedVarIndex;

  ImpactRecorder(Solver* const solver, DomainWatcher* const domain_watcher,
                 const std::vector<IntVar*>& vars,
                 DefaultPhaseParameters::DisplayLevel display_level)
      : SearchMonitor(solver),
        domain_watcher_(domain_watcher),
        vars_(vars),
        size_(vars.size()),
        current_log_space_(0.0),
        impacts_(size_),
        original_min_(size_, 0LL),
        domain_iterators_(new IntVarIterator* [size_]),
        display_level_(display_level),
        current_var_(kUninitializedVarIndex),
        current_value_(0),
        init_done_(false) {
    for (int i = 0; i < size_; ++i) {
      domain_iterators_[i] = vars_[i]->MakeDomainIterator(true);
      var_map_[vars_[i]] = i;
    }
  }

  virtual void ApplyDecision(Decision* const d) {
    if (!init_done_) {
      return;
    }
    d->Accept(&find_var_);
    if (find_var_.valid()) {
      current_var_ = var_map_[find_var_.var()];
      current_value_ = find_var_.value();
      current_log_space_ = domain_watcher_->LogSearchSpaceSize();
    } else {
      current_var_ = kUninitializedVarIndex;
      current_value_ = 0;
    }
  }

  virtual void AfterDecision(Decision* const d, bool apply) {
    if (init_done_ && current_var_ != kUninitializedVarIndex) {
      if (current_log_space_ > 0.0) {
        const double log_space = domain_watcher_->LogSearchSpaceSize();
        if (apply) {
          const double impact = kPerfectImpact - log_space / current_log_space_;
          UpdateImpact(current_var_, current_value_, impact);
          current_var_ = kUninitializedVarIndex;
          current_value_ = 0;
        }
        current_log_space_ = log_space;
      }
    }
  }

  virtual void BeginFail() {
    if (init_done_ && current_var_ != kUninitializedVarIndex) {
      UpdateImpact(current_var_, current_value_, kFailureImpact);
      current_var_ = kUninitializedVarIndex;
      current_value_ = 0;
    }
  }

  void ResetAllImpacts() {
    for (int i = 0; i < size_; ++i) {
      original_min_[i] = vars_[i]->Min();
      // By default, we init impacts to 2.0 -> equivalent to failure.
      // This will be overwritten to real impact values on valid domain
      // values during the FirstRun() method.
      impacts_[i]
          .resize(vars_[i]->Max() - vars_[i]->Min() + 1, kInitFailureImpact);
    }

    for (int i = 0; i < size_; ++i) {
      for (int j = 0; j < impacts_[i].size(); ++j) {
        impacts_[i][j] = kInitFailureImpact;
      }
    }
  }

  void UpdateImpact(int var_index, int64 value, double impact) {
    const int64 value_index = value - original_min_[var_index];
    const double current_impact = impacts_[var_index][value_index];
    const double new_impact =
        (current_impact * (FLAGS_cp_impact_divider - 1) + impact) /
        FLAGS_cp_impact_divider;
    impacts_[var_index][value_index] = new_impact;
  }

  void InitImpact(int var_index, int64 value) {
    const double log_space = domain_watcher_->LogSearchSpaceSize();
    const double impact = kPerfectImpact - log_space / current_log_space_;
    const int64 value_index = value - original_min_[var_index];
    DCHECK_LT(var_index, size_);
    DCHECK_LT(value_index, impacts_[var_index].size());
    impacts_[var_index][value_index] = impact;
    init_count_++;
  }

  void FirstRun(int64 splits) {
    Solver* const s = solver();
    current_log_space_ = domain_watcher_->LogSearchSpaceSize();
    if (display_level_ != DefaultPhaseParameters::NONE) {
      LOG(INFO) << "  - initial log2(SearchSpace) = " << current_log_space_;
    }
    const int64 init_time = s->wall_time();
    ResetAllImpacts();
    int64 removed_counter = 0;
    FirstRunVariableContainers* container =
        s->RevAlloc(new FirstRunVariableContainers(this, splits));
    // Loop on the variables, scan domains and initialize impacts.
    for (int var_index = 0; var_index < size_; ++var_index) {
      IntVar* const var = vars_[var_index];
      if (var->Bound()) {
        continue;
      }
      IntVarIterator* const iterator = domain_iterators_[var_index];
      DecisionBuilder* init_decision_builder = nullptr;
      if (var->Max() - var->Min() < splits) {
        // The domain is small enough, we scan it completely.
        container->without_split()->set_update_impact_callback(
            container->update_impact_callback());
        container->without_split()->Init(var, iterator, var_index);
        init_decision_builder = container->without_split();
      } else {
        // The domain is too big, we scan it in initialization_splits
        // intervals.
        container->with_splits()->set_update_impact_callback(
            container->update_impact_callback());
        container->with_splits()->Init(var, iterator, var_index);
        init_decision_builder = container->with_splits();
      }
      // Reset the number of impacts initialized.
      init_count_ = 0;
      // Use Solve() to scan all values of one variable.
      s->Solve(init_decision_builder);

      // If we have not initialized all values, then they can be removed.
      // As the iterator is not stable w.r.t. deletion, we need to store
      // removed values in an intermediate vector.
      if (init_count_ != var->Size()) {
        container->ClearRemovedValues();
        for (const int64 value : InitAndGetValues(iterator)) {
          const int64 value_index = value - original_min_[var_index];
          if (impacts_[var_index][value_index] == kInitFailureImpact) {
            container->PushBackRemovedValue(value);
          }
        }
        CHECK(container->HasRemovedValues()) << var->DebugString();
        removed_counter += container->NumRemovedValues();
        const double old_log = domain_watcher_->Log2(var->Size());
        var->RemoveValues(container->removed_values());
        current_log_space_ += domain_watcher_->Log2(var->Size()) - old_log;
      }
    }
    if (display_level_ != DefaultPhaseParameters::NONE) {
      if (removed_counter) {
        LOG(INFO) << "  - init done, time = " << s->wall_time() - init_time
                  << " ms, " << removed_counter
                  << " values removed, log2(SearchSpace) = "
                  << current_log_space_;
      } else {
        LOG(INFO) << "  - init done, time = " << s->wall_time() - init_time
                  << " ms";
      }
    }
    s->SaveAndSetValue(&init_done_, true);
  }

  // This method scans the domain of one variable and returns the sum
  // of the impacts of all values in its domain, along with the value
  // with minimal impact.
  void ScanVarImpacts(int var_index, int64* const best_impact_value,
                      double* const var_impacts,
                      DefaultPhaseParameters::VariableSelection var_select,
                      DefaultPhaseParameters::ValueSelection value_select) {
    CHECK(best_impact_value != nullptr);
    CHECK(var_impacts != nullptr);
    double max_impact = -std::numeric_limits<double>::max();
    double min_impact = std::numeric_limits<double>::max();
    double sum_var_impact = 0.0;
    int64 min_impact_value = -1;
    int64 max_impact_value = -1;
    for (const int64 value : InitAndGetValues(domain_iterators_[var_index])) {
      const int64 value_index = value - original_min_[var_index];
      DCHECK_LT(var_index, size_);
      DCHECK_LT(value_index, impacts_[var_index].size());
      const double current_impact = impacts_[var_index][value_index];
      sum_var_impact += current_impact;
      if (current_impact > max_impact) {
        max_impact = current_impact;
        max_impact_value = value;
      }
      if (current_impact < min_impact) {
        min_impact = current_impact;
        min_impact_value = value;
      }
    }

    switch (var_select) {
      case DefaultPhaseParameters::CHOOSE_MAX_AVERAGE_IMPACT: {
        *var_impacts = sum_var_impact / vars_[var_index]->Size();
        break;
      }
      case DefaultPhaseParameters::CHOOSE_MAX_VALUE_IMPACT: {
        *var_impacts = max_impact;
        break;
      }
      default: {
        *var_impacts = sum_var_impact;
        break;
      }
    }

    switch (value_select) {
      case DefaultPhaseParameters::SELECT_MIN_IMPACT: {
        *best_impact_value = min_impact_value;
        break;
      }
      case DefaultPhaseParameters::SELECT_MAX_IMPACT: {
        *best_impact_value = max_impact_value;
        break;
      }
    }
  }

  virtual std::string DebugString() const { return "ImpactRecorder"; }

 private:
  // A container for the variables needed in FirstRun that is reversibly
  // allocable.
  class FirstRunVariableContainers : public BaseObject {
   public:
    FirstRunVariableContainers(ImpactRecorder* impact_recorder, int64 splits)
        : update_impact_callback_(NewPermanentCallback(
              impact_recorder, &ImpactRecorder::InitImpact)),
          removed_values_(),
          without_splits_(),
          with_splits_(splits) {}
    Callback2<int, int64>* update_impact_callback() {
      return update_impact_callback_.get();
    }
    void PushBackRemovedValue(int64 value) { removed_values_.push_back(value); }
    bool HasRemovedValues() const { return !removed_values_.empty(); }
    void ClearRemovedValues() { removed_values_.clear(); }
    size_t NumRemovedValues() const { return removed_values_.size(); }
    const std::vector<int64>& removed_values() const { return removed_values_; }
    InitVarImpacts* without_split() { return &without_splits_; }
    InitVarImpactsWithSplits* with_splits() { return &with_splits_; }

    virtual std::string DebugString() const { return "FirstRunVariableContainers"; }

   private:
    std::unique_ptr<Callback2<int, int64> > update_impact_callback_;
    std::vector<int64> removed_values_;
    InitVarImpacts without_splits_;
    InitVarImpactsWithSplits with_splits_;
  };

  DomainWatcher* const domain_watcher_;
  std::vector<IntVar*> vars_;
  const int size_;
  double current_log_space_;
  // impacts_[i][j] stores the average search space reduction when assigning
  // original_min_[i] + j to variable i.
  std::vector<std::vector<double> > impacts_;
  std::vector<int64> original_min_;
  std::unique_ptr<IntVarIterator * []> domain_iterators_;
  int64 init_count_;
  const DefaultPhaseParameters::DisplayLevel display_level_;
  int current_var_;
  int64 current_value_;
  FindVar find_var_;
  hash_map<const IntVar*, int> var_map_;
  bool init_done_;

  DISALLOW_COPY_AND_ASSIGN(ImpactRecorder);
};

const int ImpactRecorder::kLogCacheSize = 1000;
const double ImpactRecorder::kPerfectImpact = 1.0;
const double ImpactRecorder::kFailureImpact = 1.0;
const double ImpactRecorder::kInitFailureImpact = 2.0;
const int ImpactRecorder::kUninitializedVarIndex = -1;

// ----- Restart -----

int64 ComputeBranchRestart(int64 log) {
  if (log <= 0 || log > 63) {
    return kint64max;
  }
  return GG_ULONGLONG(1) << log;
}

// This structure stores 'var[index] (left?==:!=) value'.
class ChoiceInfo {
 public:
  ChoiceInfo() : value_(0), var_(nullptr), left_(false) {}

  ChoiceInfo(IntVar* const var, int64 value, bool left)
      : value_(value), var_(var), left_(left) {}

  std::string DebugString() const {
    return StringPrintf("%s %s %lld", var_->name().c_str(),
                        (left_ ? "==" : "!="), value_);
  }

  IntVar* var() const { return var_; }

  bool left() const { return left_; }

  int64 value() const { return value_; }

  void set_left(bool left) { left_ = left; }

 private:
  int64 value_;
  IntVar* var_;
  bool left_;
};

// Hook on the search to check restart before the refutation of a decision.
class RestartMonitor : public SearchMonitor {
 public:
  RestartMonitor(Solver* const solver, DefaultPhaseParameters parameters,
                 DomainWatcher* const domain_watcher)
      : SearchMonitor(solver),
        parameters_(parameters),
        domain_watcher_(domain_watcher),
        min_log_search_space_(std::numeric_limits<double>::infinity()),
        no_good_manager_(parameters_.restart_log_size >= 0 &&
                                 parameters_.use_no_goods
                             ? solver->MakeNoGoodManager()
                             : nullptr),
        branches_between_restarts_(0),
        min_restart_period_(ComputeBranchRestart(parameters_.restart_log_size)),
        maximum_restart_depth_(kint64max),
        num_restarts_(0) {}

  virtual ~RestartMonitor() {}

  virtual void ApplyDecision(Decision* const d) {
    Solver* const s = solver();
    branches_between_restarts_++;
    d->Accept(&find_var_);
    if (find_var_.valid()) {
      choices_.Push(solver(),
                    ChoiceInfo(find_var_.var(), find_var_.value(), true));
      if (parameters_.display_level == DefaultPhaseParameters::VERBOSE) {
        VLOG(2) << "adding no good = " << choices_.Last()->DebugString()
                << " at depth " << s->SearchDepth();
      }
    }
  }

  virtual void RefuteDecision(Decision* const d) {
    CHECK(d != nullptr);
    Solver* const s = solver();
    branches_between_restarts_++;
    d->Accept(&find_var_);
    if (find_var_.valid()) {
      choices_.Push(solver(),
                    ChoiceInfo(find_var_.var(), find_var_.value(), false));
      if (parameters_.display_level == DefaultPhaseParameters::VERBOSE) {
        VLOG(2) << "adding no good = " << choices_.Last()->DebugString()
                << " at depth " << s->SearchDepth();
      }
      if (ChecksRestartOnRefute(s)) {
        if (AddsNoGood(s)) {
          if (parameters_.display_level == DefaultPhaseParameters::VERBOSE) {
            VLOG(2) << "restarting";
          }
          num_restarts_++;
          RestartCurrentSearch();
          s->Fail();
        }
      }
    }
  }

  virtual void ExitSearch() {
    if (parameters_.display_level != DefaultPhaseParameters::NONE &&
        no_good_manager_ != nullptr) {
      LOG(INFO) << "Default search has generated "
                << no_good_manager_->NoGoodCount() << " no goods, and "
                << num_restarts_ << " restarts";
    }
  }

  virtual bool AtSolution() {
    if (parameters_.display_level == DefaultPhaseParameters::VERBOSE) {
      VLOG(2) << "Found a solution after the following decisions:";
      for (SimpleRevFIFO<ChoiceInfo>::Iterator it(&choices_); it.ok(); ++it) {
        VLOG(2) << "  " << (*it).DebugString();
      }
    }
    return false;
  }

  virtual void BeginFail() {
    if (parameters_.display_level == DefaultPhaseParameters::VERBOSE) {
      VLOG(2) << "-- Failure";
    }
  }

  void Install() {
    SearchMonitor::Install();
    if (no_good_manager_ != nullptr) {
      no_good_manager_->Install();
    }
  }

  virtual std::string DebugString() const { return "RestartMonitor"; }

 private:
  // Called before applying the refutation of the decision.  This
  // method must decide if we need to restart or not.  The main
  // decision is based on the restart_log_size and the current search
  // space size. If, the current search space size is greater than the
  // min search space size encountered since the last restart +
  // restart_log_size, this means that we have just finished a search
  // tree of size at least restart_log_size.  If the search tree is
  // very sparse, then we may have visited close to restart_log_size
  // nodes since the last restart (instead of the maximum of
  // 2^restart_log_size). To fight this degenerate case, we also count
  // the number of branches explored since the last restart and decide
  // to postpone restart until we finish a sub-search tree and have
  // visited enough branches (2^restart_log_size). To enforce this,
  // when we postpone a restart, we store the current search depth -1
  // in maximum_restart_depth_ and will restart as soon as we reach a
  // node above the current one, with enough visited branches.
  bool ChecksRestartOnRefute(Solver* const solver) {
    // We do nothing if restart_log_size is < 0.
    if (parameters_.restart_log_size >= 0) {
      const int search_depth = solver->SearchDepth();
      const double log_search_space_size =
          domain_watcher_->LogSearchSpaceSize();
      if (min_log_search_space_ > log_search_space_size) {
        min_log_search_space_ = log_search_space_size;
      }

      // Some verbose display.
      if (parameters_.display_level == DefaultPhaseParameters::VERBOSE) {
        VLOG(2) << "search_depth = " << search_depth
                << ", branches between restarts = "
                << branches_between_restarts_
                << ", log_search_space_size = " << log_search_space_size
                << ", min_log_search_space = " << min_log_search_space_;
      }
      bool all_rights = true;
      for (SimpleRevFIFO<ChoiceInfo>::Iterator it(&choices_); it.ok(); ++it) {
        if ((*it).left()) {
          all_rights = false;
          break;
        }
      }
      if (all_rights) {
        if (parameters_.display_level == DefaultPhaseParameters::VERBOSE) {
          VLOG(2) << "  - finished a left subtree, forcing a nogood";
        }
        return true;
      }
      if (search_depth > maximum_restart_depth_ || search_depth == 0) {
        // We are deeper than maximum_restart_depth_, we should not restart
        // because we have not finished a sub-tree of sufficient size.
        return false;
      }
      // We may restart either because of the search space criteria,
      // or the search depth is less than maximum_restart_depth_.
      if (min_log_search_space_ + parameters_.restart_log_size <
              log_search_space_size ||
          (search_depth <= maximum_restart_depth_ &&
           maximum_restart_depth_ != kint64max)) {
        // If we have not visited enough branches, we postpone the
        // restart and force to check at least at the parent of the
        // current search node.
        if (branches_between_restarts_ < min_restart_period_) {
          maximum_restart_depth_ = search_depth - 1;
          if (parameters_.display_level == DefaultPhaseParameters::VERBOSE) {
            VLOG(2) << "Postpone restarting until depth <= "
                    << maximum_restart_depth_
                    << ", visited nodes = " << branches_between_restarts_
                    << " / " << min_restart_period_;
          }
          return false;
        }
        return true;
      }
    }
    return false;
  }

  // Performs the restart. It resets various counters and adds a
  // non-reversible nogood if need be. It returns true if we should
  // restart after having added the nogood.
  bool AddsNoGood(Solver* const solver) {
    min_log_search_space_ = std::numeric_limits<double>::infinity();
    branches_between_restarts_ = 0;
    maximum_restart_depth_ = kint64max;
    // Creates nogood.
    if (parameters_.use_no_goods) {
      bool all_rights = true;
      for (SimpleRevFIFO<ChoiceInfo>::Iterator it(&choices_); it.ok(); ++it) {
        const ChoiceInfo& choice = *it;
        if (choice.left()) {
          all_rights = false;
        }
      }
      DCHECK(no_good_manager_ != nullptr);

      // Reverse the last no good if need be. If we have finished the
      // apply branch, then the subtree below the left branch is
      // completely explored. The choice list contains the refute
      // branch. This one should be reverted.
      //
      // It can also happen that we are failing in the apply
      // branch. In that case, we do not need to revert it.
      DCHECK(!choices_.Last()->left());
      const bool last_left = choices_.Last()->left();
      choices_.MutableLast()->set_left(true);
      NoGood* const nogood = no_good_manager_->MakeNoGood();

      // if the nogood contains both x == 3 and x != 4, we can simplify
      // to keep only x == 3.
      hash_set<const IntVar*> positive_variable;
      for (SimpleRevFIFO<ChoiceInfo>::Iterator it(&choices_); it.ok(); ++it) {
        const ChoiceInfo& choice = *it;
        if (choice.left()) {
          positive_variable.insert(choice.var());
        }
      }
      // We fill the nogood structure.
      for (SimpleRevFIFO<ChoiceInfo>::Iterator it(&choices_); it.ok(); ++it) {
        const ChoiceInfo& choice = *it;
        IntVar* const var = choice.var();
        const int64 value = choice.value();
        if (choice.left()) {
          nogood->AddIntegerVariableEqualValueTerm(var, value);
        } else if (!ContainsKey(positive_variable, choice.var())) {
          nogood->AddIntegerVariableNotEqualValueTerm(var, value);
        }
      }
      if (parameters_.display_level == DefaultPhaseParameters::VERBOSE) {
        VLOG(2) << "Adding no good no " << no_good_manager_->NoGoodCount()
                << ": " << nogood->DebugString();
      }
      // Adds the nogood to the nogood manager.
      no_good_manager_->AddNoGood(nogood);
      // Revert the modification on the last.
      choices_.MutableLast()->set_left(last_left);
      // If the nogood is only right branches, there is no need to
      // restart as we would end up in the same place.
      return !all_rights;
    }
    return true;
  }

  const DefaultPhaseParameters parameters_;
  DomainWatcher* const domain_watcher_;
  double min_log_search_space_;
  NoGoodManager* no_good_manager_;
  int64 branches_between_restarts_;
  const int64 min_restart_period_;
  int64 maximum_restart_depth_;
  SimpleRevFIFO<ChoiceInfo> choices_;
  FindVar find_var_;
  int num_restarts_;
};

// ---------- Heuristics ----------

class RunHeuristicsAsDives : public Decision {
 public:
  RunHeuristicsAsDives(Solver* const solver, const std::vector<IntVar*>& vars,
                       DefaultPhaseParameters::DisplayLevel level,
                       bool run_all_heuristics, int random_seed,
                       int heuristic_period, int heuristic_num_failures_limit)
      : heuristic_limit_(nullptr),
        display_level_(level),
        run_all_heuristics_(run_all_heuristics),
        random_(random_seed),
        heuristic_period_(heuristic_period),
        heuristic_branch_count_(0) {
    Init(solver, vars, heuristic_num_failures_limit);
  }

  virtual ~RunHeuristicsAsDives() { STLDeleteElements(&heuristics_); }

  virtual void Apply(Solver* const solver) {
    if (!RunAllHeuristics(solver)) {
      solver->Fail();
    }
  }

  virtual void Refute(Solver* const solver) {}

  bool ShouldRun() {
    if (heuristic_period_ <= 0) {
      return false;
    }
    ++heuristic_branch_count_;
    return heuristic_branch_count_ % heuristic_period_ == 0;
  }

  bool RunOneHeuristic(Solver* const solver, int index) {
    HeuristicWrapper* const wrapper = heuristics_[index];

    const bool result =
        solver->SolveAndCommit(wrapper->phase, heuristic_limit_);
    if (result && display_level_ != DefaultPhaseParameters::NONE) {
      LOG(INFO) << "  --- solution found by heuristic " << wrapper->name
                << " --- ";
    }
    return result;
  }

  bool RunAllHeuristics(Solver* const solver) {
    if (run_all_heuristics_) {
      for (int index = 0; index < heuristics_.size(); ++index) {
        for (int run = 0; run < heuristics_[index]->runs; ++run) {
          if (RunOneHeuristic(solver, index)) {
            return true;
          }
        }
      }
      return false;
    } else {
      const int index = random_.Uniform(heuristics_.size());
      return RunOneHeuristic(solver, index);
    }
  }

  int Rand32(int size) { return random_.Next() % size; }

  void Init(Solver* const solver, const std::vector<IntVar*>& vars,
            int heuristic_num_failures_limit) {
    const int kRunOnce = 1;
    const int kRunMore = 2;
    const int kRunALot = 3;

    heuristics_.push_back(new HeuristicWrapper(
        solver, vars, Solver::CHOOSE_MIN_SIZE_LOWEST_MIN,
        Solver::ASSIGN_MIN_VALUE, "AssignMinValueToMinDomainSize", kRunOnce));

    heuristics_.push_back(new HeuristicWrapper(
        solver, vars, Solver::CHOOSE_MIN_SIZE_HIGHEST_MAX,
        Solver::ASSIGN_MAX_VALUE, "AssignMaxValueToMinDomainSize", kRunOnce));

    heuristics_.push_back(
        new HeuristicWrapper(solver, vars, Solver::CHOOSE_MIN_SIZE_LOWEST_MIN,
                             Solver::ASSIGN_CENTER_VALUE,
                             "AssignCenterValueToMinDomainSize", kRunOnce));

    heuristics_.push_back(new HeuristicWrapper(
        solver, vars, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_RANDOM_VALUE,
        "AssignRandomValueToFirstUnbound", kRunALot));

    heuristics_.push_back(new HeuristicWrapper(
        solver, vars, Solver::CHOOSE_RANDOM, Solver::ASSIGN_MIN_VALUE,
        "AssignMinValueToRandomVariable", kRunMore));

    heuristics_.push_back(new HeuristicWrapper(
        solver, vars, Solver::CHOOSE_RANDOM, Solver::ASSIGN_MAX_VALUE,
        "AssignMaxValueToRandomVariable", kRunMore));

    heuristics_.push_back(new HeuristicWrapper(
        solver, vars, Solver::CHOOSE_RANDOM, Solver::ASSIGN_RANDOM_VALUE,
        "AssignRandomValueToRandomVariable", kRunMore));

    heuristic_limit_ =
        solver->MakeLimit(kint64max,                     // time.
                          kint64max,                     // branches.
                          heuristic_num_failures_limit,  // fails.
                          kint64max);                    // solutions.
  }

 private:
  // This class wraps one heuristic with extra information: name and
  // number of runs.
  struct HeuristicWrapper {
    HeuristicWrapper(Solver* const solver, const std::vector<IntVar*>& vars,
                     Solver::IntVarStrategy var_strategy,
                     Solver::IntValueStrategy value_strategy,
                     const std::string& heuristic_name, int heuristic_runs)
        : phase(solver->MakePhase(vars, var_strategy, value_strategy)),
          name(heuristic_name),
          runs(heuristic_runs) {}

    // The decision builder we are going to use in this dive.
    DecisionBuilder* const phase;
    // A name for logging purposes.
    const std::string name;
    // How many times we will run this particular heuristic in case the
    // parameter run_all_heuristics is true. This is useful for random
    // heuristics where it makes sense to run them more than once.
    const int runs;
  };

  std::vector<HeuristicWrapper*> heuristics_;
  SearchMonitor* heuristic_limit_;
  DefaultPhaseParameters::DisplayLevel display_level_;
  bool run_all_heuristics_;
  ACMRandom random_;
  const int heuristic_period_;
  int heuristic_branch_count_;
};

// ---------- DefaultIntegerSearch ----------

// Default phase decision builder.
class DefaultIntegerSearch : public DecisionBuilder {
 public:
  static const double kSmallSearchSpaceLimit;

  DefaultIntegerSearch(Solver* const solver, const std::vector<IntVar*>& vars,
                       const DefaultPhaseParameters& parameters)
      : vars_(vars),
        parameters_(parameters),
        domain_watcher_(vars, ImpactRecorder::kLogCacheSize),
        impact_recorder_(solver, &domain_watcher_, vars,
                         parameters.display_level),
        heuristics_(solver, vars_, parameters_.display_level,
                    parameters_.run_all_heuristics, parameters_.random_seed,
                    parameters_.heuristic_period,
                    parameters_.heuristic_num_failures_limit),
        restart_monitor_(solver, parameters_, &domain_watcher_),
        init_done_(false) {}

  virtual ~DefaultIntegerSearch() {}

  virtual Decision* Next(Solver* const solver) {
    CheckInit(solver);

    if (heuristics_.ShouldRun()) {
      return &heuristics_;
    }

    return parameters_.decision_builder != nullptr
               ? parameters_.decision_builder->Next(solver)
               : ImpactNext(solver);
  }

  virtual void AppendMonitors(Solver* const solver,
                              std::vector<SearchMonitor*>* const extras) {
    CHECK(solver != nullptr);
    CHECK(extras != nullptr);
    if (parameters_.decision_builder == nullptr) {
      extras->push_back(&impact_recorder_);
    }
    if (parameters_.restart_log_size >= 0) {
      extras->push_back(&restart_monitor_);
    }
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitExtension(ModelVisitor::kVariableGroupExtension);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               vars_);
    visitor->EndVisitExtension(ModelVisitor::kVariableGroupExtension);
  }

  virtual std::string DebugString() const {
    std::string out = "DefaultIntegerSearch(";

    if (parameters_.decision_builder == nullptr) {
      out.append("Impact Based Search, ");
    } else {
      out.append(parameters_.decision_builder->DebugString());
      out.append(", ");
    }
    out.append(JoinDebugStringPtr(vars_, ", "));
    out.append(")");
    return out;
  }

 private:
  void CheckInit(Solver* const solver) {
    if (init_done_) {
      return;
    }
    if (parameters_.decision_builder == nullptr) {
      // Decide if we are doing impacts, no if one variable is too big.
      for (int i = 0; i < vars_.size(); ++i) {
        if (vars_[i]->Max() - vars_[i]->Min() > 0xFFFFFF) {
          if (parameters_.display_level == DefaultPhaseParameters::VERBOSE) {
            LOG(INFO) << "Domains are too large, switching to simple "
                      << "heuristics";
          }
          solver->SaveValue(
              reinterpret_cast<void**>(&parameters_.decision_builder));
          parameters_.decision_builder =
              solver->MakePhase(vars_, Solver::CHOOSE_MIN_SIZE_LOWEST_MIN,
                                Solver::ASSIGN_MIN_VALUE);
          solver->SaveAndSetValue(&init_done_, true);
          return;
        }
      }
      // No if the search space is too small.
      if (domain_watcher_.LogSearchSpaceSize() < kSmallSearchSpaceLimit) {
        if (parameters_.display_level == DefaultPhaseParameters::VERBOSE) {
          LOG(INFO) << "Search space is too small, switching to simple "
                    << "heuristics";
        }
        solver->SaveValue(
            reinterpret_cast<void**>(&parameters_.decision_builder));
        parameters_.decision_builder = solver->MakePhase(
            vars_, Solver::CHOOSE_FIRST_UNBOUND, Solver::ASSIGN_MIN_VALUE);
        solver->SaveAndSetValue(&init_done_, true);
        return;
      }

      if (parameters_.display_level != DefaultPhaseParameters::NONE) {
        LOG(INFO) << "Init impact based search phase on " << vars_.size()
                  << " variables, initialization splits = "
                  << parameters_.initialization_splits
                  << ", heuristic_period = " << parameters_.heuristic_period
                  << ", run_all_heuristics = " << parameters_.run_all_heuristics
                  << ", restart_log_size = " << parameters_.restart_log_size;
      }
      // Init the impacts.
      impact_recorder_.FirstRun(parameters_.initialization_splits);
    }
    if (parameters_.persistent_impact) {
      init_done_ = true;
    } else {
      solver->SaveAndSetValue(&init_done_, true);
    }
  }

  // This method will do an exhaustive scan of all domains of all
  // variables to select the variable with the maximal sum of impacts
  // per value in its domain, and then select the value with the
  // minimal impact.
  Decision* ImpactNext(Solver* const solver) {
    IntVar* var = nullptr;
    int64 value = 0;
    double best_var_impact = -std::numeric_limits<double>::max();
    for (int i = 0; i < vars_.size(); ++i) {
      if (!vars_[i]->Bound()) {
        int64 current_value = 0;
        double current_var_impact = 0.0;
        impact_recorder_.ScanVarImpacts(i, &current_value, &current_var_impact,
                                        parameters_.var_selection_schema,
                                        parameters_.value_selection_schema);
        if (current_var_impact > best_var_impact) {
          var = vars_[i];
          value = current_value;
          best_var_impact = current_var_impact;
        }
      }
    }
    if (var == nullptr) {
      return nullptr;
    } else {
      return solver->MakeAssignVariableValue(var, value);
    }
  }

  // ----- data members -----

  std::vector<IntVar*> vars_;
  DefaultPhaseParameters parameters_;
  DomainWatcher domain_watcher_;
  ImpactRecorder impact_recorder_;
  RunHeuristicsAsDives heuristics_;
  RestartMonitor restart_monitor_;
  bool init_done_;
};

const double DefaultIntegerSearch::kSmallSearchSpaceLimit = 10.0;
}  // namespace

// ---------- API ----------

DecisionBuilder* Solver::MakeDefaultPhase(const std::vector<IntVar*>& vars) {
  DefaultPhaseParameters parameters;
  return MakeDefaultPhase(vars, parameters);
}

DecisionBuilder* Solver::MakeDefaultPhase(
    const std::vector<IntVar*>& vars, const DefaultPhaseParameters& parameters) {
  return RevAlloc(new DefaultIntegerSearch(this, vars, parameters));
}
}  // namespace operations_research
