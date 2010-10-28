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

namespace operations_research {

// ---------- ImpactDecisionBuilder ----------

class ImpactDecisionBuilder : public DecisionBuilder {
 public:
  class AssignAndFail : public DecisionBuilder {
   public:
    AssignAndFail() : var_(NULL), value_(0), closure_(NULL) {}
    virtual ~AssignAndFail() {}

    virtual Decision* Next(Solver* const s) {
      if (var_->Size() > 1) {
        return s->MakeAssignVariableValueOrFail(var_, value_);
      } else {
        closure_->Run();
        return NULL;
      }
    }

    IntVar* var_;
    int64 value_;
    Closure* closure_;
  };

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
        iterators_(new IntVarIterator*[size_]) {
    CHECK_GE(size_, 0);
    if (size_ > 0) {
      vars_.reset(new IntVar*[size_]);
      memcpy(vars_.get(), vars, size_ * sizeof(*vars));
    }
    log_.Init(1000);
    for (int i = 0; i < size_; ++i) {
      iterators_[i] = vars_[i]->MakeDomainIterator(true);
      original_min_[i] = vars_[i]->Min();
      impacts_[i].resize(vars_[i]->Max() - vars_[i]->Min() + 1, 0.0);
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
    const double new_impact = (current_impact * 4 + impact) / 5;
    impacts_[var_index][value_index] = new_impact;
    // LOG(INFO) << "  update: var_index = " << var_index << ", value = " << value
              // << ", impact = " << impact;
  }

  void InitImpact(int var_index, int64 value) {
    const double log_space = LogSearchSpaceSize();
    const double impact = 1.0 - log_space / current_log_space_;
    const int64 value_index = value - original_min_[var_index];
    impacts_[var_index][value_index] = impact;
    // LOG(INFO) << "  - value = " << value << ", impact = " << impact;
  }

  void InitImpactAfterFailure(int var_index, int64 value) {
    const double kFailureImpact = 1.0;
    const int64 value_index = value - original_min_[var_index];
    impacts_[var_index][value_index] = kFailureImpact;
    // LOG(INFO) << "  - value = " << value << ", impact = " << kFailureImpact;
  }

  void FirstRun(Solver* const solver) {
    LOG(INFO) << "Init impacts, time = " << solver->wall_time() << " ms";
    current_log_space_ = LogSearchSpaceSize();
    AssignAndFail db;
    for (int var_index = 0; var_index < size_; ++var_index) {
      // LOG(INFO) << "var_index = " << var_index;
      if (vars_[var_index]->Bound()) {
        continue;
      }
      db.var_ = vars_[var_index];
      IntVarIterator* const it = iterators_[var_index];
      for (it->Init(); it->Ok(); it->Next()) {
        const int64 value = it->Value();
        db.value_ = value;
        db.closure_ = NewCallback(this,
                                  &ImpactDecisionBuilder::InitImpact,
                                  var_index,
                                  value);
        if (!solver->NestedSolve(&db, true)) {
          InitImpactAfterFailure(var_index, value);
        }
      }
    }
    LOG(INFO) << "End init impacts, time = " << solver->wall_time() << " ms";
  }

  void UpdateAfterAssignment() {
    CHECK_NE(current_log_space_, 0.0);
    const double log_space = LogSearchSpaceSize();
    const double impact = 1.0 - log_space / current_log_space_;
    UpdateImpact(current_var_index_, current_value_, impact);
    current_log_space_ = log_space;
  }

  void UpdateAfterFailure() {
    const double kFailureImpact = 1.0;
    UpdateImpact(current_var_index_, current_value_, kFailureImpact);
    current_log_space_ = LogSearchSpaceSize();
  }

  void ScanVarImpacts(int var_index,
                      int64* const best_impact_value,
                      double* const sum_impacts) {
    *sum_impacts = 0.0;
    double best_impact = 2.0;  // >= 1.0
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

  bool FindVarValue(int* var_index, int64* value) {
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
    //    LOG(INFO) << "Choose var " << *var_index << " with value = " << *value;
    return (*var_index != -1);
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
