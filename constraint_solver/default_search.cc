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

DEFINE_int32(cp_impact_divider, 5, "Divider for continuous update.");
DEFINE_int32(cp_impact_splits, 64,
             "Level of domain splitting when initializing impacts.");
DEFINE_int32(cp_impact_seed, 1, "Seed for impact random number generator.");
DEFINE_int32(cp_impact_heuristic_frequency, 500,
             "Fun heuristic every 'num' nodes.");
DEFINE_int32(cp_impact_heuristic_limit, 30,
             "Fail limit when running an heuristic.");
DEFINE_bool(cp_impact_run_all_heuristics, false,
            "Run all heuristics instead of a random one.");

namespace operations_research {

// Useful constants.
namespace {
const int kLogCacheSize = 1000;
const double kFailureImpact = 1.0;

// ---------- ImpactDecisionBuilder ----------

class ImpactDecisionBuilder : public DecisionBuilder {
 public:

  // ----- This is a complete initialization method -----
  class InitVarImpacts : public DecisionBuilder {
   public:
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
    };

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

    // Public data (non const).
    IntVar* var_;
    Callback2<int, int64>* update_impact_callback_;
    bool new_start_;
    IntVarIterator* iterator_;
    int var_index_;
    vector<int64> active_values_;
    int value_index_;
   private:
    scoped_ptr<Closure> update_impact_closure_;
    AssignCallFail updater_;
  };

  // ---- This is an initialization method based on domain splitting -----
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
    };

    // ----- main -----

    explicit InitVarImpactsWithSplits(int split_size)
        : var_(NULL),
          update_impact_callback_(NULL),
          new_start_(false),
          var_index_(0),
          var_min_(0),
          var_max_(0),
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
      const int64 length = var_max_ - var_min_ + 1;
      return (var_min_ + length * index / split_size_);
    }

    virtual Decision* Next(Solver* const solver) {
      if (new_start_) {
        var_min_ = var_->Min();
        var_max_ = var_->Max();
        new_start_ = false;
      }
      if (split_index_ == split_size_) {
        return NULL;
      }
      updater_.var_ = var_;
      updater_.value_min_ = IntervalStart(split_index_);
      split_index_++;
      if (split_index_ == split_size_) {
        updater_.value_max_ = var_max_;
      } else {
        updater_.value_max_ = IntervalStart(split_index_) - 1;
      }
      return &updater_;
    }

    // Public data (non const).
    IntVar* var_;
    Callback2<int, int64>* update_impact_callback_;
    bool new_start_;
    IntVarIterator* iterator_;
    int var_index_;
    int64 var_min_;
    int64 var_max_;
    const int split_size_;
    int split_index_;
   private:
    scoped_ptr<Closure> update_impact_closure_;
    AssignIntervalCallFail updater_;
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

  // ----- Main method -----

  ImpactDecisionBuilder(Solver* const solver,
                        const IntVar* const* vars,
                        int size,
                        int64 restart_frequency)
      : size_(size),
        restart_frequency_(restart_frequency),
        impacts_(size_),
        original_min_(size_, 0LL),
        init_done_(false),
        current_log_space_(0.0),
        fail_stamp_(0),
        current_var_index_(-1),
        current_value_(0),
        domain_iterators_(new IntVarIterator*[size_]),
        init_count_(-1),
        heuristic_limit_(NULL),
        random_(FLAGS_cp_impact_seed),
        runner_(NewPermanentCallback(this,
                                     &ImpactDecisionBuilder::RunHeuristics)),
        heuristic_branch_count_(0) {
    CHECK_GE(size_, 0);
    if (size_ > 0) {
      vars_.reset(new IntVar*[size_]);
      memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    }
    log_.Init(kLogCacheSize);
    for (int i = 0; i < size_; ++i) {
      domain_iterators_[i] = vars_[i]->MakeDomainIterator(true);
      original_min_[i] = vars_[i]->Min();
      // By default, we init impacts to 1.0 -> equivalent to failure.
      // This will be overwritten to real impact values on valid domain
      // values during the FirstRun() method.
      impacts_[i].resize(vars_[i]->Max() - vars_[i]->Min() + 1,
                         kFailureImpact);
    }
    InitHeuristics(solver);
  }

  virtual ~ImpactDecisionBuilder() {}

  void InitHeuristics(Solver* const solver) {
    DecisionBuilder* db = NULL;
    string heuristic_name;

    db = solver->MakePhase(vars_.get(),
                           size_,
                           Solver::CHOOSE_MIN_SIZE_LOWEST_MIN,
                           Solver::ASSIGN_MIN_VALUE);
    heuristic_name = "AssignMinValueToMinDomainSize";
    heuristics_.push_back(db);
    heuristics_names_.push_back(heuristic_name);

    db = solver->MakePhase(vars_.get(),
                           size_,
                           Solver::CHOOSE_MIN_SIZE_HIGHEST_MAX,
                           Solver::ASSIGN_MAX_VALUE);
    heuristic_name = "AssignMaxValueToMinDomainSize";
    heuristics_.push_back(db);
    heuristics_names_.push_back(heuristic_name);

    db = solver->MakePhase(vars_.get(),
                           size_,
                           Solver::CHOOSE_FIRST_UNBOUND,
                           Solver::ASSIGN_RANDOM_VALUE);
    heuristic_name = "AssignRandomValueToFirstUnbound";
    heuristics_.push_back(db);
    heuristics_names_.push_back(heuristic_name);

    db = solver->MakePhase(vars_.get(),
                           size_,
                           Solver::CHOOSE_RANDOM,
                           Solver::ASSIGN_MIN_VALUE);
    heuristic_name = "AssignMinValueToRandomVariable";
    heuristics_.push_back(db);
    heuristics_names_.push_back(heuristic_name);

    db = solver->MakePhase(vars_.get(),
                           size_,
                           Solver::CHOOSE_RANDOM,
                           Solver::ASSIGN_CENTER_VALUE);
    heuristic_name = "AssignCenterValueToRandomVariable";
    heuristics_.push_back(db);
    heuristics_names_.push_back(heuristic_name);

    CHECK_EQ(heuristics_names_.size(), heuristics_.size());

    heuristic_limit_ =
        solver->MakeLimit(kint64max,  // time.
                          kint64max,  // branches.
                          FLAGS_cp_impact_heuristic_limit,  // failures.
                          kint64max);  // solutions.
  }

  double LogSearchSpaceSize() {
    double result = 0.0;
    for (int index = 0; index < size_; ++index) {
      result += log_.Log2(vars_[index]->Size());
    }
    return result;
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
    const double log_space = LogSearchSpaceSize();
    const double impact = kFailureImpact - log_space / current_log_space_;
    const int64 value_index = value - original_min_[var_index];
    impacts_[var_index][value_index] = impact;
    if (impact != kFailureImpact) {
      init_count_++;
    }
  }

  void InitImpactAfterFailure(int var_index, int64 value) {
    const int64 value_index = value - original_min_[var_index];
    impacts_[var_index][value_index] = kFailureImpact;
  }

  void FirstRun(Solver* const solver) {
    LOG(INFO) << "Init impacts on " << size_ << " variables";
    const int64 init_time = solver->wall_time();
    current_log_space_ = LogSearchSpaceSize();
    InitVarImpacts db;
    InitVarImpactsWithSplits dbs(FLAGS_cp_impact_splits);
    vector<int64> removed_values;
    int64 removed_counter = 0;
    scoped_ptr<Callback2<int, int64> > update_impact_callback(
        NewPermanentCallback(this, &ImpactDecisionBuilder::InitImpact));
    db.update_impact_callback_ = update_impact_callback.get();
    dbs.update_impact_callback_ = update_impact_callback.get();

    // Loop on the variables, scan domains and initialize impacts.
    for (int var_index = 0; var_index < size_; ++var_index) {
      IntVar* const var = vars_[var_index];
      if (var->Bound()) {
        continue;
      }
      IntVarIterator* const iterator = domain_iterators_[var_index];
      DecisionBuilder* init_db = NULL;
      if (var->Max() - var->Min() < FLAGS_cp_impact_splits) {
        // The domain is small enough, we scan it completely.
        db.Init(var, iterator, var_index);
        init_db = &db;
      } else {
        // The domain is too big, we scan it in FLAGS_cp_impact_splits
        // intervals.
        dbs.Init(var, iterator, var_index);
        init_db = &dbs;
      }
      // Reset the number of impacts initialized.
      init_count_ = 0;
      // Use NestedSolve() to scan all values of one variable.
      solver->NestedSolve(init_db, true);
      // If we have not initialized all values, then they can be removed.
      // As the iterator is not stable w.r.t. deletion, we need to store
      // removed values in an intermediate vector.
      if (init_count_ != var->Size()) {
        removed_values.clear();
        for (iterator->Init(); iterator->Ok(); iterator->Next()) {
          const int64 value = iterator->Value();
          const int64 value_index = value - original_min_[var_index];
          if (impacts_[var_index][value_index] == kFailureImpact) {
            removed_values.push_back(value);
          }
        }
        CHECK(!removed_values.empty());
        removed_counter += removed_values.size();
        const double old_log = log_.Log2(var->Size());
        VLOG(1) << "Var " << var_index << " has " << removed_values.size()
                << " values removed";
        var->RemoveValues(removed_values);
        current_log_space_ += log_.Log2(var->Size()) - old_log;
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

  void UpdateAfterAssignment() {
    CHECK_GT(current_log_space_, 0.0);
    const double log_space = LogSearchSpaceSize();
    const double impact = kFailureImpact - log_space / current_log_space_;
    UpdateImpact(current_var_index_, current_value_, impact);
    current_log_space_ = log_space;
  }

  void UpdateAfterFailure() {
    UpdateImpact(current_var_index_, current_value_, kFailureImpact);
    current_log_space_ = LogSearchSpaceSize();
  }

  // This method scans the domain of one variable and returns the sum
  // of the impacts of all values in its domain, along with the value
  // with minimal impact.
  void ScanVarImpacts(int var_index,
                      int64* const min_impact_value,
                      double* const sum_impacts) {
    CHECK_NOTNULL(min_impact_value);
    CHECK_NOTNULL(sum_impacts);
    *sum_impacts = 0.0;
    double best_impact = kFailureImpact + 1.0;  // >= kFailureImpact
    *min_impact_value = -1;
    IntVarIterator* const it = domain_iterators_[var_index];
    for (it->Init(); it->Ok(); it->Next()) {
      const int64 value = it->Value();
      const int64 value_index = value - original_min_[var_index];
      const double current_impact = impacts_[var_index][value_index];
      *sum_impacts += current_impact;
      if (current_impact < best_impact) {
        best_impact = current_impact;
        *min_impact_value = value;
      }
    }
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
    double best_sum_impact = 0.0;
    for (int i = 0; i < size_; ++i) {
      if (!vars_[i]->Bound()) {
        int64 current_value = 0;
        double current_sum_impact = 0.0;
        ScanVarImpacts(i, &current_value, &current_sum_impact);
        if (current_sum_impact > best_sum_impact) {
          *var_index = i;
          *value = current_value;
          best_sum_impact = current_sum_impact;
        }
      }
    }
    return (*var_index != -1);
  }

  bool RunOneHeuristic(Solver* const solver, int index) {
    DecisionBuilder* const db = heuristics_[index];
    const string heuristic_name = heuristics_names_[index];

    const bool result = solver->NestedSolve(db, false, heuristic_limit_);
    if (result) {
      LOG(INFO) << "Solution found by heuristic " << heuristic_name;
    }
    return result;
  }

  bool RunHeuristics(Solver* const solver) {
    if (FLAGS_cp_impact_run_all_heuristics) {
      for (int index = 0; index < heuristics_.size(); ++index) {
        if (RunOneHeuristic(solver, index)) {
          return true;
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
      FirstRun(s);
      init_done_ = true;
    }

    if (current_var_index_ == -1 && fail_stamp_ != 0) {
      // After solution.
      current_log_space_ = LogSearchSpaceSize();
    } else {
      if (fail_stamp_ != 0) {
        if (s->fail_stamp() == fail_stamp_) {
          UpdateAfterAssignment();
        } else {
          UpdateAfterFailure();
        }
      }
    }
    fail_stamp_ = s->fail_stamp();

    ++heuristic_branch_count_;
    if (heuristic_branch_count_ % FLAGS_cp_impact_heuristic_frequency == 0) {
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
  scoped_array<IntVar*> vars_;
  const int size_;
  const int64 restart_frequency_;
  CachedLog log_;
  // impacts_[i][j] stores the average search space reduction when assigning
  // original_min_[i] + j to variable i.
  vector<vector<double> > impacts_;
  vector<int64> original_min_;
  bool init_done_;
  double current_log_space_;
  uint64 fail_stamp_;
  int current_var_index_;
  int64 current_value_;
  scoped_array<IntVarIterator*> domain_iterators_;
  int64 init_count_;
  vector<DecisionBuilder*> heuristics_;
  vector<string> heuristics_names_;
  SearchMonitor* heuristic_limit_;
  ACMRandom random_;
  RunHeuristic runner_;
  int heuristic_branch_count_;
};
}  // namespace

// ---------- API ----------

DecisionBuilder* Solver::MakeImpactPhase(const vector<IntVar*>& vars,
                                         int64 restart_frequency) {
  return MakeImpactPhase(vars.data(), vars.size(), restart_frequency);
}

DecisionBuilder* Solver::MakeImpactPhase(const IntVar* const* vars,
                                         int size,
                                         int64 restart_frequency) {
  return RevAlloc(new ImpactDecisionBuilder(this,
                                            vars,
                                            size,
                                            restart_frequency));
}
}  // namespace operations_research
