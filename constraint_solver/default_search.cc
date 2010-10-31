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
#include "base/random.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/concise_iterator.h"
#include "base/map-util.h"
#include "base/stl_util-inl.h"
#include "base/random.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/cached_log.h"

DEFINE_int32(impact_divider, 5, "divider for continuous update");
DEFINE_int32(impact_splits, 64,
             "level of domain splitting when initing impacts");
DEFINE_int32(impact_seed, 1, "seed for impact random number generator");
DEFINE_int32(impact_heuristic_frequency, 500,
             "run heuristic every 'num' nodes");
DEFINE_int32(impact_heuristic_limit, 30,
             "fail limit when running an heuristic");

namespace operations_research {

// ---------- ImpactDecisionBuilder ----------

class ImpactDecisionBuilder : public DecisionBuilder {
 public:

  // ----- This is a complete initialization method -----
  class InitVarImpacts : public DecisionBuilder {
   public:
    // ----- helper decision -----
    class AssignCallFail : public Decision {
     public:
      AssignCallFail(Closure* const closure)
          : var_(NULL), value_(0), closure_(closure) {}
      virtual ~AssignCallFail() {}
      virtual void Apply(Solver* const s) {
        var_->SetValue(value_);
        // We call the closure on the part that cannot fail.
        closure_->Run();
        s->Fail();
      }
      virtual void Refute(Solver* const s) {}

      // Public data for easy access.
      IntVar* var_;
      int64 value_;
     private:
      Closure* const closure_;
    };

    // ----- main -----
    InitVarImpacts()
        : var_(NULL),
          callback_(NULL),
          new_start_(false),
          var_index_(0),
          value_index_(-1),
          closure_(NewPermanentCallback(this, &InitVarImpacts::UpdateImpacts)),
          updater_(closure_.get()) {}

    virtual ~InitVarImpacts() {}

    void UpdateImpacts() {
      callback_->Run(var_index_, var_->Min());
    }

    virtual Decision* Next(Solver* const s) {
      if (new_start_) {
        active_values_.clear();
        for (iterator_->Init(); iterator_->Ok(); iterator_->Next()) {
          active_values_.push_back(iterator_->Value());
        }
        value_index_ = 0;
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
    Callback2<int, int64>* callback_;
    bool new_start_;
    IntVarIterator* iterator_;
    int var_index_;
    vector<int64> active_values_;
    int value_index_;
   private:
    scoped_ptr<Closure> closure_;
    AssignCallFail updater_;
  };

  // ---- This is an initialization method based on domain splitting -----
  class InitVarImpactsWithSplits : public DecisionBuilder {
   public:
    // ----- helper decision -----
    class AssignIntervalCallFail : public Decision {
     public:
      AssignIntervalCallFail(Closure* const closure)
          : var_(NULL),
            value_min_(0),
            value_max_(0),
            closure_(closure) {}
      virtual ~AssignIntervalCallFail() {}
      virtual void Apply(Solver* const s) {
        var_->SetRange(value_min_, value_max_);
        // We call the closure on the part that cannot fail.
        closure_->Run();
        s->Fail();
      }
      virtual void Refute(Solver* const s) {}

      // Public for easy access.
      IntVar* var_;
      int64 value_min_;
      int64 value_max_;
     private:
      Closure* const closure_;
    };

    // ----- main -----

    InitVarImpactsWithSplits(int split_size)
        : var_(NULL),
          callback_(NULL),
          new_start_(false),
          var_index_(0),
          split_size_(split_size),
          split_index_(-1),
          closure_(NewPermanentCallback(
              this, &InitVarImpactsWithSplits::UpdateImpacts)),
          updater_(closure_.get()) {}

    virtual ~InitVarImpactsWithSplits() {}

    void UpdateImpacts() {
      for (iterator_->Init(); iterator_->Ok(); iterator_->Next()) {
        callback_->Run(var_index_, iterator_->Value());
      }
    }

    int64 IntervalStart(int index) {
      const int64 length = var_max_ - var_min_ + 1;
      return (var_min_ + length * index / split_index_);
    }

    virtual Decision* Next(Solver* const s) {
      if (new_start_) {
        var_min_ = var_->Min();
        var_max_ = var_->Max();
        split_index_ = 0;
        new_start_ = false;
      }
      if (split_index_ == split_size_) {
        return NULL;
      }
      split_index_++;
      updater_.var_ = var_;
      updater_.value_min_ = IntervalStart(split_index_ - 1);
      updater_.value_max_ = IntervalStart(split_index_) - 1;
      return &updater_;
    }

    // Public data (non const).
    IntVar* var_;
    Callback2<int, int64>* callback_;
    bool new_start_;
    IntVarIterator* iterator_;
    int var_index_;
    int64 var_min_;
    int64 var_max_;
    const int split_size_;
    int split_index_;
   private:
    scoped_ptr<Closure> closure_;
    AssignIntervalCallFail updater_;
  };

  // ----- heuristic helper ------

  class RunHeuristic : public Decision {
   public:
    RunHeuristic(ResultCallback1<bool, Solver*>* callback)
        : callback_(callback) {}
      virtual ~RunHeuristic() {}
      virtual void Apply(Solver* const s) {
        if (!callback_->Run(s)) {
          s->Fail();
        }
      }
      virtual void Refute(Solver* const s) {}
   private:
    ResultCallback1<bool, Solver*>* callback_;
  };

  // ----- Main method -----

  static const double kFailureImpact = 1.0;

  ImpactDecisionBuilder(const IntVar* const * vars,
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
        iterators_(new IntVarIterator*[size_]),
        init_count_(-1),
        heuristic_limit_(NULL),
        random_(FLAGS_impact_seed),
        runner_(NewPermanentCallback(this,
                                     &ImpactDecisionBuilder::RunOneHeuristic)),
        heuristic_branch_count_(0) {
    CHECK_GE(size_, 0);
    if (size_ > 0) {
      vars_.reset(new IntVar*[size_]);
      memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    }
    log_.Init(1000);
    for (int i = 0; i < size_; ++i) {
      iterators_[i] = vars_[i]->MakeDomainIterator(true);
      original_min_[i] = vars_[i]->Min();
      // By default, we init impacts to 1.0 -> equivalent to failure.
      // This will be overwritten to real impact values on valid domain
      // values during the FirstRun() method.
      impacts_[i].resize(vars_[i]->Max() - vars_[i]->Min() + 1,
                         kFailureImpact);
    }
  }

  virtual ~ImpactDecisionBuilder() {}

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
        (current_impact * (FLAGS_impact_divider - 1) + impact) /
        FLAGS_impact_divider;
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
    InitVarImpactsWithSplits dbs(FLAGS_impact_splits);
    vector<int64> removed_value;
    int64 removed_counter = 0;
    Callback2<int, int64>* const callback =
        NewPermanentCallback(this, &ImpactDecisionBuilder::InitImpact);
    db.callback_ = callback;
    dbs.callback_ = callback;

    for (int var_index = 0; var_index < size_; ++var_index) {
      IntVar* const var = vars_[var_index];
      IntVarIterator* const iterator = iterators_[var_index];
      if (var->Bound()) {
        continue;
      }
      DecisionBuilder* init_db = NULL;
      if (var->Max() - var->Min() < FLAGS_impact_splits) {
        db.var_ = var;
        db.iterator_ = iterator;
        db.new_start_ = true;
        db.var_index_ = var_index;
        init_db = &db;
      } else {
        dbs.var_ = var;
        dbs.iterator_ = iterator;
        dbs.new_start_ = true;
        dbs.var_index_ = var_index;
        init_db = &dbs;
      }
      init_count_ = 0;
      solver->NestedSolve(init_db, true);
      if (init_count_ != var->Size()) {
        removed_value.clear();
        for (iterator->Init(); iterator->Ok(); iterator->Next()) {
          const int64 value = iterator->Value();
          const int64 value_index = value - original_min_[var_index];
          if (impacts_[var_index][value_index] == kFailureImpact) {
            removed_value.push_back(value);
            removed_counter++;
          }
        }
        if (!removed_value.empty()) {
          const double old_log = log_.Log2(var->Size());
          VLOG(1) << "Var " << var_index << " has " << removed_value.size()
                  << " values removed";
          var->RemoveValues(removed_value);
          current_log_space_ += log_.Log2(var->Size()) - old_log;
        }
      }
    }
    delete callback;
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

  void ScanVarImpacts(int var_index,
                      int64* const best_impact_value,
                      double* const sum_impacts) {
    *sum_impacts = 0.0;
    double best_impact = 2.0;  // >= kFailureImpact
    *best_impact_value = -1;
    IntVarIterator* const it = iterators_[var_index];
    for (it->Init(); it->Ok(); it->Next()) {
      const int64 value = it->Value();
      const int64 value_index = value - original_min_[var_index];
      const double current_impact = impacts_[var_index][value_index];
      *sum_impacts += current_impact;
      if (current_impact < best_impact) {
        best_impact = current_impact;
        *best_impact_value = value;
      }
    }
  }

  bool FindVarValue(int* const var_index, int64* const value) {
    *var_index = -1;
    *value = 0;
    double best_sum_impact = 0.0;
    int64 current_value = 0;;
    double current_sum_impact = 0.0;
    for (int i = 0; i < size_; ++i) {
      if (!vars_[i]->Bound()) {
        ScanVarImpacts(i, &current_value, &current_sum_impact);
        if (current_sum_impact > best_sum_impact) {
          *var_index = i;
          *value = current_value;
        }
      }
    }
    return (*var_index != -1);
  }

  bool RunOneHeuristic(Solver* const solver) {
    DecisionBuilder* db = NULL;
    switch (random_.Uniform(5)) {
      case 0: {
        db = solver->MakePhase(vars_.get(),
                               size_,
                               Solver::CHOOSE_MIN_SIZE_LOWEST_MIN,
                               Solver::ASSIGN_MIN_VALUE);
        break;
      }
      case 1: {
        db = solver->MakePhase(vars_.get(),
                               size_,
                               Solver::CHOOSE_MIN_SIZE_HIGHEST_MAX,
                               Solver::ASSIGN_MAX_VALUE);
        break;
      }
      case 2: {
        db = solver->MakePhase(vars_.get(),
                               size_,
                               Solver::CHOOSE_FIRST_UNBOUND,
                               Solver::ASSIGN_RANDOM_VALUE);
        break;
      }
      case 3: {
        db = solver->MakePhase(vars_.get(),
                               size_,
                               Solver::CHOOSE_RANDOM,
                               Solver::ASSIGN_MIN_VALUE);
        break;
      }
      case 4: {
        db = solver->MakePhase(vars_.get(),
                               size_,
                               Solver::CHOOSE_RANDOM,
                               Solver::ASSIGN_CENTER_VALUE);
        break;
      }
    }
    SearchMonitor* const heuristic_limit =
        solver->MakeLimit(kint64max,  // time.
                          kint64max,  // branches.
                          FLAGS_impact_heuristic_limit, // failures.
                          kint64max); // solutions.

    const bool result = solver->NestedSolve(db, false, heuristic_limit);
    if (result) {
      LOG(INFO) << "Solution found by heuristic";
    }
    return result;
  }


  virtual Decision* Next(Solver* const s) {
    if (!init_done_) {
      FirstRun(s);
      init_done_ = true;
    }

    if (fail_stamp_ != 0) {
      if (s->fail_stamp() == fail_stamp_) {
        UpdateAfterAssignment();
      } else {
        UpdateAfterFailure();
        fail_stamp_ = s->fail_stamp();
      }
    } else {
      fail_stamp_ = s->fail_stamp();
    }
    if (++heuristic_branch_count_ % FLAGS_impact_heuristic_frequency == 0) {
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
  vector<vector<double> > impacts_;
  vector<int64> original_min_;
  bool init_done_;
  double current_log_space_;
  uint64 fail_stamp_;
  int current_var_index_;
  int64 current_value_;
  scoped_array<IntVarIterator*> iterators_;
  int64 init_count_;
  vector<DecisionBuilder*> heuristics_;
  SearchMonitor* heuristic_limit_;
  RunHeuristic runner_;
  ACMRandom random_;
  int heuristic_branch_count_;
};

// ---------- API ----------

DecisionBuilder* Solver::MakeImpactPhase(const vector<IntVar*>& vars,
                                         int64 restart_frequency) {
  return MakeImpactPhase(vars.data(), vars.size(), restart_frequency);
}

DecisionBuilder* Solver::MakeImpactPhase(const IntVar* const* vars,
                                         int size,
                                         int64 restart_frequency) {
  return RevAlloc(new ImpactDecisionBuilder(vars, size, restart_frequency));
}
}  // namespace operations_research
