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

#include <limits>

#include "base/callback.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/concise_iterator.h"
#include "base/map-util.h"
#include "base/stl_util-inl.h"
#include "base/random.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/cached_log.h"

DEFINE_int32(cp_impact_divider, 10, "Divider for continuous update.");

namespace operations_research {

// Default constants for search phase parameters.
const int DefaultPhaseParameters::kDefaultNumberOfSplits = 100;
const int DefaultPhaseParameters::kDefaultHeuristicPeriod = 100;
const int DefaultPhaseParameters::kDefaultHeuristicNumFailuresLimit = 30;
const int DefaultPhaseParameters::kDefaultSeed = 0;

namespace {
// ----- DomainWatcher -----

// This class follows the domains of variables and will report the log of the
// search space of all integer variables.
class DomainWatcher {
 public:
  DomainWatcher(const IntVar* const * vars, int size, int cache_size)
      : vars_(NULL), size_(size) {
    if (size_ > 0) {
      vars_.reset(new IntVar*[size_]);
      memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    }
    cached_log_.Init(cache_size);
  }

  double LogSearchSpaceSize() {
    double result = 0.0;
    for (int index = 0; index < size_; ++index) {
      result += cached_log_.Log2(vars_[index]->Size());
    }
    return result;
  }

  double Log2(int64 size) const {
    return cached_log_.Log2(size);
  }
 private:
  scoped_array<IntVar*> vars_;
  const int size_;
  CachedLog cached_log_;
  DISALLOW_COPY_AND_ASSIGN(DomainWatcher);
};

// ----- Auxilliary decision builders to init impacts -----

// This class initialize impacts by scanning each value of the domain
// of the variable.
class InitVarImpacts : public DecisionBuilder {
 public:
  // ----- main -----
  InitVarImpacts()
      : var_(NULL),
        update_impact_callback_(NULL),
        new_start_(false),
        var_index_(0),
        value_index_(-1),
        update_impact_closure_(
            NewPermanentCallback(this, &InitVarImpacts::UpdateImpacts)),
        updater_(update_impact_closure_.get()) {
    CHECK_NOTNULL(update_impact_closure_);
  }

  virtual ~InitVarImpacts() {}

  void UpdateImpacts() {
    update_impact_callback_->Run(var_index_, var_->Min());
  }

  void Init(IntVar* const var,
            IntVarIterator* const iterator,
            int var_index) {
    var_ = var;
    iterator_ = iterator;
    var_index_ = var_index;
    new_start_ = true;
    value_index_ = 0;
  }

  virtual Decision* Next(Solver* const solver) {
    CHECK_NOTNULL(var_);
    CHECK_NOTNULL(iterator_);
    if (new_start_) {
      active_values_.clear();
      for (iterator_->Init(); iterator_->Ok(); iterator_->Next()) {
        active_values_.push_back(iterator_->Value());
      }
      new_start_ = false;
    }
    if (value_index_ == active_values_.size()) {
      return NULL;
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
        : var_(NULL),
          value_(0),
          update_impact_closure_(update_impact_closure) {
      CHECK_NOTNULL(update_impact_closure_);
    }
    virtual ~AssignCallFail() {}
    virtual void Apply(Solver* const solver) {
      CHECK_NOTNULL(var_);
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
  vector<int64> active_values_;
  int value_index_;
  scoped_ptr<Closure> update_impact_closure_;
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
        : var_(NULL),
          value_min_(0),
          value_max_(0),
          update_impact_closure_(update_impact_closure) {
      CHECK_NOTNULL(update_impact_closure_);
    }
    virtual ~AssignIntervalCallFail() {}
    virtual void Apply(Solver* const solver) {
      CHECK_NOTNULL(var_);
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
      : var_(NULL),
        update_impact_callback_(NULL),
        new_start_(false),
        var_index_(0),
        min_value_(0),
        max_value_(0),
        split_size_(split_size),
        split_index_(-1),
        update_impact_closure_(NewPermanentCallback(
            this, &InitVarImpactsWithSplits::UpdateImpacts)),
        updater_(update_impact_closure_.get()) {
    CHECK_NOTNULL(update_impact_closure_);
  }

  virtual ~InitVarImpactsWithSplits() {}

  void UpdateImpacts() {
    for (iterator_->Init(); iterator_->Ok(); iterator_->Next()) {
      update_impact_callback_->Run(var_index_, iterator_->Value());
    }
  }

  void Init(IntVar* const var,
            IntVarIterator* const iterator,
            int var_index) {
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
      return NULL;
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
  scoped_ptr<Closure> update_impact_closure_;
  AssignIntervalCallFail updater_;
};

// ----- ImpactRecorder

// This class will record the impacts of all assignment of values to
// variables. Its main output is to find the optimal pair (variable/value)
// based on default phase parameters.
class ImpactRecorder {
 public:
  static const int kLogCacheSize;
  static const double kPerfectImpact;
  static const double kFailureImpact;
  static const double kInitFailureImpact;

  ImpactRecorder(const IntVar* const * vars, int size)
      : domain_watcher_(vars, size, kLogCacheSize),
        size_(size),
        current_log_space_(0.0),
        impacts_(size_),
        original_min_(size_, 0LL),
        domain_iterators_(new IntVarIterator*[size_]) {
    CHECK_GE(size_, 0);
    if (size_ > 0) {
      vars_.reset(new IntVar*[size_]);
      memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    }
    for (int i = 0; i < size_; ++i) {
      domain_iterators_[i] = vars_[i]->MakeDomainIterator(true);
      original_min_[i] = vars_[i]->Min();
      // By default, we init impacts to 1.0 -> equivalent to failure.
      // This will be overwritten to real impact values on valid domain
      // values during the FirstRun() method.
      impacts_[i].resize(vars_[i]->Max() - vars_[i]->Min() + 1,
                         kInitFailureImpact);
    }
  }

  void RecordLogSearchSpace() {
    current_log_space_ = domain_watcher_.LogSearchSpaceSize();
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
    const double log_space = domain_watcher_.LogSearchSpaceSize();
    const double impact = kPerfectImpact - log_space / current_log_space_;
    const int64 value_index = value - original_min_[var_index];
    impacts_[var_index][value_index] = impact;
    init_count_++;
  }

  void FirstRun(Solver* const solver, int64 splits) {
    current_log_space_ = domain_watcher_.LogSearchSpaceSize();
    LOG(INFO) << "Init impacts on " << size_
              << " variables, log2(SearchSpace) = "
              << current_log_space_;
    const int64 init_time = solver->wall_time();
    InitVarImpacts without_splits;
    InitVarImpactsWithSplits with_splits(splits);
    vector<int64> removed_values;
    int64 removed_counter = 0;
    scoped_ptr<Callback2<int, int64> > update_impact_callback(
        NewPermanentCallback(this, &ImpactRecorder::InitImpact));
    without_splits.set_update_impact_callback(update_impact_callback.get());
    with_splits.set_update_impact_callback(update_impact_callback.get());

    // Loop on the variables, scan domains and initialize impacts.
    for (int var_index = 0; var_index < size_; ++var_index) {
      IntVar* const var = vars_[var_index];
      if (var->Bound()) {
        continue;
      }
      IntVarIterator* const iterator = domain_iterators_[var_index];
      DecisionBuilder* init_decision_builder = NULL;
      if (var->Max() - var->Min() < splits) {
        // The domain is small enough, we scan it completely.
        without_splits.Init(var, iterator, var_index);
        init_decision_builder = &without_splits;
      } else {
        // The domain is too big, we scan it in initialization_splits
        // intervals.
        with_splits.Init(var, iterator, var_index);
        init_decision_builder = &with_splits;
      }
      // Reset the number of impacts initialized.
      init_count_ = 0;
      // Use NestedSolve() to scan all values of one variable.
      solver->NestedSolve(init_decision_builder, true);

      // If we have not initialized all values, then they can be removed.
      // As the iterator is not stable w.r.t. deletion, we need to store
      // removed values in an intermediate vector.
      if (init_count_ != var->Size()) {
        removed_values.clear();
        for (iterator->Init(); iterator->Ok(); iterator->Next()) {
          const int64 value = iterator->Value();
          const int64 value_index = value - original_min_[var_index];
          if (impacts_[var_index][value_index] == kInitFailureImpact) {
            removed_values.push_back(value);
          }
        }
        CHECK(!removed_values.empty());
        removed_counter += removed_values.size();
        const double old_log = domain_watcher_.Log2(var->Size());
        var->RemoveValues(removed_values);
        current_log_space_ += domain_watcher_.Log2(var->Size()) - old_log;
      }
    }

    if (removed_counter) {
      LOG(INFO) << "  - time = " << solver->wall_time() - init_time
                << " ms, " << removed_counter
                << " values removed, log2(SearchSpace) = "
                << current_log_space_;
    } else {
      LOG(INFO) << "  - time = " << solver->wall_time() - init_time
                << " ms, log2(SearchSpace) = " << current_log_space_;
    }
  }

  void UpdateAfterAssignment(int var_index, int64 value) {
    CHECK_GT(current_log_space_, 0.0);
    const double log_space = domain_watcher_.LogSearchSpaceSize();
    const double impact = kPerfectImpact - log_space / current_log_space_;
    UpdateImpact(var_index, value, impact);
    current_log_space_ = log_space;
  }

  void UpdateAfterFailure(int var_index, int64 value) {
    UpdateImpact(var_index, value, kFailureImpact);
    current_log_space_ = domain_watcher_.LogSearchSpaceSize();
  }

  // This method scans the domain of one variable and returns the sum
  // of the impacts of all values in its domain, along with the value
  // with minimal impact.
  void ScanVarImpacts(int var_index,
                      int64* const best_impact_value,
                      double* const var_impacts,
                      DefaultPhaseParameters::VariableSelection var_select,
                      DefaultPhaseParameters::ValueSelection value_select) {
    CHECK_NOTNULL(best_impact_value);
    CHECK_NOTNULL(var_impacts);
    double max_impact = -std::numeric_limits<double>::max();
    double min_impact = std::numeric_limits<double>::max();
    double sum_var_impact = 0.0;
    int64 min_impact_value = -1;
    int64 max_impact_value = -1;
    IntVarIterator* const it = domain_iterators_[var_index];
    for (it->Init(); it->Ok(); it->Next()) {
      const int64 value = it->Value();
      const int64 value_index = value - original_min_[var_index];
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
 private:
  DomainWatcher domain_watcher_;
  scoped_array<IntVar*> vars_;
  const int size_;
  double current_log_space_;
  // impacts_[i][j] stores the average search space reduction when assigning
  // original_min_[i] + j to variable i.
  vector<vector<double> > impacts_;
  vector<int64> original_min_;
  scoped_array<IntVarIterator*> domain_iterators_;
  int64 init_count_;

  DISALLOW_COPY_AND_ASSIGN(ImpactRecorder);
};

const int ImpactRecorder::kLogCacheSize = 1000;
const double ImpactRecorder::kPerfectImpact = 1.0;
const double ImpactRecorder::kFailureImpact = 1.0;
const double ImpactRecorder::kInitFailureImpact = 2.0;


// ---------- ImpactDecisionBuilder ----------

// Default phase decision builder.
class ImpactDecisionBuilder : public DecisionBuilder {
 public:
  ImpactDecisionBuilder(Solver* const solver,
                        const IntVar* const* vars,
                        int size,
                        const DefaultPhaseParameters& parameters)
      : impact_recorder_(vars, size),
        size_(size),
        parameters_(parameters),
        init_done_(false),
        fail_stamp_(0),
        current_var_index_(-1),
        current_value_(0),
        heuristic_limit_(NULL),
        random_(parameters_.random_seed),
        runner_(NewPermanentCallback(this,
                                     &ImpactDecisionBuilder::RunHeuristics)),
        heuristic_branch_count_(0) {
    CHECK_GE(size_, 0);
    if (size_ > 0) {
      vars_.reset(new IntVar*[size_]);
      memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    }
    InitHeuristics(solver);
  }

  virtual ~ImpactDecisionBuilder() {
    STLDeleteElements(&heuristics_);
  }

  void InitHeuristics(Solver* const solver) {
    const int kRunOnce = 1;
    const int kRunMore = 2;
    const int kRunALot = 3;

    heuristics_.push_back(
        new HeuristicWrapper(solver,
                             vars_.get(),
                             size_,
                             Solver::CHOOSE_MIN_SIZE_LOWEST_MIN,
                             Solver::ASSIGN_MIN_VALUE,
                             "AssignMinValueToMinDomainSize",
                             kRunOnce));

    heuristics_.push_back(
        new HeuristicWrapper(solver,
                             vars_.get(),
                             size_,
                             Solver::CHOOSE_MIN_SIZE_HIGHEST_MAX,
                             Solver::ASSIGN_MAX_VALUE,
                             "AssignMaxValueToMinDomainSize",
                             kRunOnce));

    heuristics_.push_back(
        new HeuristicWrapper(solver,
                             vars_.get(),
                             size_,
                             Solver::CHOOSE_MIN_SIZE_LOWEST_MIN,
                             Solver::ASSIGN_CENTER_VALUE,
                             "AssignCenterValueToMinDomainSize",
                             kRunOnce));

    heuristics_.push_back(
        new HeuristicWrapper(solver,
                             vars_.get(),
                             size_,
                             Solver::CHOOSE_FIRST_UNBOUND,
                             Solver::ASSIGN_RANDOM_VALUE,
                             "AssignRandomValueToFirstUnbound",
                             kRunALot));

    heuristics_.push_back(
        new HeuristicWrapper(solver,
                             vars_.get(),
                             size_,
                             Solver::CHOOSE_RANDOM,
                             Solver::ASSIGN_MIN_VALUE,
                             "AssignMinValueToRandomVariable",
                             kRunMore));

    heuristics_.push_back(
        new HeuristicWrapper(solver,
                             vars_.get(),
                             size_,
                             Solver::CHOOSE_RANDOM,
                             Solver::ASSIGN_MAX_VALUE,
                             "AssignMaxValueToRandomVariable",
                             kRunMore));

    heuristics_.push_back(
        new HeuristicWrapper(solver,
                             vars_.get(),
                             size_,
                             Solver::CHOOSE_RANDOM,
                             Solver::ASSIGN_RANDOM_VALUE,
                             "AssignRandomValueToRandomVariable",
                             kRunMore));

    heuristic_limit_ =
        solver->MakeLimit(kint64max,  // time.
                          kint64max,  // branches.
                          parameters_.heuristic_num_failures_limit,  // fails.
                          kint64max);  // solutions.
  }


  // This method will do an exhaustive scan of all domains of all
  // variables to select the variable with the maximal sum of impacts
  // per value in its domain, and then select the value with the
  // minimal impact.
  bool FindVarValue(int* const var_index, int64* const value) {
    CHECK_NOTNULL(var_index);
    CHECK_NOTNULL(value);
    *var_index = -1;
    *value = 0;
    double best_var_impact = -std::numeric_limits<double>::max();
    for (int i = 0; i < size_; ++i) {
      if (!vars_[i]->Bound()) {
        int64 current_value = 0;
        double current_var_impact = 0.0;
        impact_recorder_.ScanVarImpacts(i,
                                        &current_value,
                                        &current_var_impact,
                                        parameters_.var_selection_schema,
                                        parameters_.value_selection_schema);
        if (current_var_impact > best_var_impact) {
          *var_index = i;
          *value = current_value;
          best_var_impact = current_var_impact;
        }
      }
    }
    return (*var_index != -1);
  }

  bool RunOneHeuristic(Solver* const solver, int index) {
    HeuristicWrapper* const wrapper = heuristics_[index];

    const bool result =
        solver->NestedSolve(wrapper->phase, false, heuristic_limit_);
    if (result) {
      LOG(INFO) << "Solution found by heuristic " << wrapper->name;
    }
    return result;
  }

  bool RunHeuristics(Solver* const solver) {
    if (parameters_.run_all_heuristics) {
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

  virtual Decision* Next(Solver* const s) {
    if (!init_done_) {
      impact_recorder_.FirstRun(s, parameters_.initialization_splits);
      init_done_ = true;
    }

    if (current_var_index_ == -1 && fail_stamp_ != 0) {
      // After solution or after heuristics.
      impact_recorder_.RecordLogSearchSpace();
    } else {
      if (fail_stamp_ != 0) {
        if (s->fail_stamp() == fail_stamp_) {
          impact_recorder_.UpdateAfterAssignment(current_var_index_,
                                                 current_value_);
        } else {
          impact_recorder_.UpdateAfterFailure(current_var_index_,
                                              current_value_);
        }
      }
    }
    fail_stamp_ = s->fail_stamp();

    ++heuristic_branch_count_;
    if (heuristic_branch_count_ % parameters_.heuristic_period == 0) {
      current_var_index_ = -1;
      return &runner_;
    }
    current_var_index_ = -1;
    current_value_ = 0;
    if (FindVarValue(&current_var_index_, &current_value_)) {
      return s->MakeAssignVariableValue(vars_[current_var_index_],
                                        current_value_);
    } else {
      return NULL;
    }
  }
 private:
  // ----- Heuristic wrapper -----

  struct HeuristicWrapper {
    HeuristicWrapper(Solver* const solver,
                     IntVar* const* vars,
                     int size,
                     Solver::IntVarStrategy var_strategy,
                     Solver::IntValueStrategy value_strategy,
                     const string& heuristic_name,
                     int heuristic_runs)
        : phase(solver->MakePhase(vars, size, var_strategy, value_strategy)),
          name(heuristic_name),
          runs(heuristic_runs) {}

    DecisionBuilder* const phase;
    const string name;
    const int runs;
  };

  // ----- heuristic helper ------

  class RunHeuristic : public Decision {
   public:
    explicit RunHeuristic(
        ResultCallback1<bool, Solver*>* update_impact_callback)
        : update_impact_callback_(update_impact_callback) {}
      virtual ~RunHeuristic() {}
      virtual void Apply(Solver* const solver) {
        if (!update_impact_callback_->Run(solver)) {
          solver->Fail();
        }
      }
      virtual void Refute(Solver* const solver) {}
   private:
    scoped_ptr<ResultCallback1<bool, Solver*> >  update_impact_callback_;
  };

  // ----- data members -----

  ImpactRecorder impact_recorder_;
  scoped_array<IntVar*> vars_;
  const int size_;
  DefaultPhaseParameters parameters_;
  bool init_done_;
  uint64 fail_stamp_;
  int current_var_index_;
  int64 current_value_;
  vector<HeuristicWrapper*> heuristics_;
  SearchMonitor* heuristic_limit_;
  ACMRandom random_;
  RunHeuristic runner_;
  int heuristic_branch_count_;
};
}  // namespace

// ---------- API ----------

DecisionBuilder* Solver::MakeDefaultPhase(const vector<IntVar*>& vars) {
  DefaultPhaseParameters parameters;
  return MakeDefaultPhase(vars.data(), vars.size(), parameters);
}

DecisionBuilder* Solver::MakeDefaultPhase(
    const vector<IntVar*>& vars,
    const DefaultPhaseParameters& parameters) {
  return MakeDefaultPhase(vars.data(), vars.size(), parameters);
}

DecisionBuilder* Solver::MakeDefaultPhase(
    const IntVar* const* vars,
    int size,
    const DefaultPhaseParameters& parameters) {
  return RevAlloc(new ImpactDecisionBuilder(this,
                                            vars,
                                            size,
                                            parameters));
}

DecisionBuilder* Solver::MakeDefaultPhase(const IntVar* const* vars, int size) {
  DefaultPhaseParameters parameters;
  return MakeDefaultPhase(vars, size, parameters);
}
}  // namespace operations_research
