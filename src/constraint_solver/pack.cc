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

//  Packing constraints

#include <stddef.h>
#include <string.h>
#include <algorithm>
#include "base/unique_ptr.h"
#include <string>
#include <utility>
#include <vector>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/stringprintf.h"
#include "base/concise_iterator.h"
#include "constraint_solver/constraint_solver.h"
#include "constraint_solver/constraint_solveri.h"
#include "util/string_array.h"

namespace operations_research {

// ---------- Dimension ----------

class Dimension : public BaseObject {
 public:
  explicit Dimension(Solver* const s, Pack* const pack)
      : solver_(s), pack_(pack) {}
  virtual ~Dimension() {}

  virtual void Post() = 0;
  virtual void InitialPropagate(int bin_index, const std::vector<int>& forced,
                                const std::vector<int>& undecided) = 0;
  virtual void InitialPropagateUnassigned(const std::vector<int>& assigned,
                                          const std::vector<int>& unassigned) = 0;
  virtual void EndInitialPropagate() = 0;
  virtual void Propagate(int bin_index, const std::vector<int>& forced,
                         const std::vector<int>& removed) = 0;
  virtual void PropagateUnassigned(const std::vector<int>& assigned,
                                   const std::vector<int>& unassigned) = 0;
  virtual void EndPropagate() = 0;
  virtual string DebugString() const { return "Dimension"; }
  virtual void Accept(ModelVisitor* const visitor) const = 0;

  Solver* solver() const { return solver_; }

  bool IsUndecided(int var_index, int bin_index) const {
    return pack_->IsUndecided(var_index, bin_index);
  }

  bool IsPossible(int var_index, int bin_index) const {
    return pack_->IsPossible(var_index, bin_index);
  }

  IntVar* AssignVar(int var_index, int bin_index) const {
    return pack_->AssignVar(var_index, bin_index);
  }

  void SetImpossible(int var_index, int bin_index) {
    pack_->SetImpossible(var_index, bin_index);
  }

  void Assign(int var_index, int bin_index) {
    pack_->Assign(var_index, bin_index);
  }

  bool IsAssignedStatusKnown(int var_index) const {
    return pack_->IsAssignedStatusKnown(var_index);
  }

  void SetAssigned(int var_index) { pack_->SetAssigned(var_index); }

  void SetUnassigned(int var_index) { pack_->SetUnassigned(var_index); }

  void RemoveAllPossibleFromBin(int bin_index) {
    pack_->RemoveAllPossibleFromBin(bin_index);
  }

  void AssignAllPossibleToBin(int bin_index) {
    pack_->AssignAllPossibleToBin(bin_index);
  }

  void AssignFirstPossibleToBin(int bin_index) {
    pack_->AssignFirstPossibleToBin(bin_index);
  }

  void AssignAllRemainingItems() { pack_->AssignAllRemainingItems(); }

  void UnassignAllRemainingItems() { pack_->UnassignAllRemainingItems(); }

 private:
  Solver* const solver_;
  Pack* const pack_;
};

// ----- Pack -----

Pack::Pack(Solver* const s, const std::vector<IntVar*>& vars, int number_of_bins)
    : Constraint(s),
      vars_(vars),
      bins_(number_of_bins),
      unprocessed_(new RevBitMatrix(bins_ + 1, vars_.size())),
      forced_(bins_ + 1),
      removed_(bins_ + 1),
      holes_(vars_.size()),
      stamp_(GG_ULONGLONG(0)),
      demon_(nullptr),
      in_process_(false) {
  for (int i = 0; i < vars_.size(); ++i) {
    holes_[i] = vars_[i]->MakeHoleIterator(true);
  }
}

Pack::~Pack() {}

void Pack::Post() {
  for (int i = 0; i < vars_.size(); ++i) {
    IntVar* const var = vars_[i];
    if (!var->Bound()) {
      Demon* const d = MakeConstraintDemon1(solver(), this, &Pack::OneDomain,
                                            "OneDomain", i);
      var->WhenDomain(d);
    }
  }
  for (int i = 0; i < dims_.size(); ++i) {
    dims_[i]->Post();
  }
  demon_ = solver()->RegisterDemon(MakeDelayedConstraintDemon0(
      solver(), this, &Pack::Propagate, "Propagate"));
}

void Pack::ClearAll() {
  for (int bin_index = 0; bin_index <= bins_; ++bin_index) {
    forced_[bin_index].clear();
    removed_[bin_index].clear();
  }
  to_set_.clear();
  to_unset_.clear();
  in_process_ = false;
  stamp_ = solver()->fail_stamp();
}

void Pack::PropagateDelayed() {
  for (int i = 0; i < to_set_.size(); ++i) {
    vars_[to_set_[i].first]->SetValue(to_set_[i].second);
  }
  for (int i = 0; i < to_unset_.size(); ++i) {
    vars_[to_unset_[i].first]->RemoveValue(to_unset_[i].second);
  }
}

// A reversibly-allocable container for the data needed in
// Pack::InitialPropagate()
namespace {
class InitialPropagateData : public BaseObject {
 public:
  explicit InitialPropagateData(size_t num_bins)
      : BaseObject(), undecided_(num_bins) {}
  void PushAssigned(int index) { assigned_.push_back(index); }
  void PushUnassigned(int index) { unassigned_.push_back(index); }
  void PushUndecided(int bin, int index) {
    undecided_.at(bin).push_back(index);
  }
  const std::vector<int>& undecided(int bin) const { return undecided_.at(bin); }
  const std::vector<int>& assigned() const { return assigned_; }
  const std::vector<int>& unassigned() const { return unassigned_; }

  virtual string DebugString() const { return "InitialPropagateData"; }

 private:
  std::vector<std::vector<int>> undecided_;
  std::vector<int> unassigned_;
  std::vector<int> assigned_;
};
}  // namespace

void Pack::InitialPropagate() {
  const bool need_context = solver()->InstrumentsVariables();
  ClearAll();
  Solver* const s = solver();
  in_process_ = true;
  InitialPropagateData* data = s->RevAlloc(new InitialPropagateData(bins_));
  for (int var_index = 0; var_index < vars_.size(); ++var_index) {
    IntVar* const var = vars_[var_index];
    var->SetRange(0, bins_);  // bins_ -> item is not assigned to a bin.
    if (var->Bound()) {
      const int64 value = var->Min();
      if (value < bins_) {
        forced_[value].push_back(var_index);
        data->PushAssigned(var_index);
      } else {
        data->PushUnassigned(var_index);
      }
    } else {
      DCHECK_GT(bins_, var->Min())
          << "The variable maximum should be at most " << bins_
          << ", and it should be unbound, so its min is expected to be less "
          << "than " << bins_;
      if (var->Max() < bins_) {
        data->PushAssigned(var_index);
      }
      std::unique_ptr<IntVarIterator> it(var->MakeDomainIterator(false));
      for (it->Init(); it->Ok(); it->Next()) {
        const int64 value = it->Value();
        if (value >= 0 && value <= bins_) {
          unprocessed_->SetToOne(s, value, var_index);
          if (value != bins_) {
            data->PushUndecided(value, var_index);
          }
        }
      }
    }
  }
  for (int bin_index = 0; bin_index < bins_; ++bin_index) {
    if (need_context) {
      solver()->GetPropagationMonitor()->PushContext(StringPrintf(
          "Pack(bin %d, forced = [%s], undecided = [%s])", bin_index,
          IntVectorToString(forced_[bin_index], ", ").c_str(),
          IntVectorToString(data->undecided(bin_index), ", ").c_str()));
    }

    for (int dim_index = 0; dim_index < dims_.size(); ++dim_index) {
      if (need_context) {
        solver()->GetPropagationMonitor()->PushContext(
            StringPrintf("InitialProgateDimension(%s)",
                         dims_[dim_index]->DebugString().c_str()));
      }
      dims_[dim_index]->InitialPropagate(bin_index, forced_[bin_index],
                                         data->undecided(bin_index));
      if (need_context) {
        solver()->GetPropagationMonitor()->PopContext();
      }
    }
    if (need_context) {
      solver()->GetPropagationMonitor()->PopContext();
    }
  }
  if (need_context) {
    solver()->GetPropagationMonitor()->PushContext(
        StringPrintf("Pack(assigned = [%s], unassigned = [%s])",
                     IntVectorToString(data->assigned(), ", ").c_str(),
                     IntVectorToString(data->unassigned(), ", ").c_str()));
  }
  for (int dim_index = 0; dim_index < dims_.size(); ++dim_index) {
    if (need_context) {
      solver()->GetPropagationMonitor()->PushContext(
          StringPrintf("InitialProgateDimension(%s)",
                       dims_[dim_index]->DebugString().c_str()));
    }
    dims_[dim_index]
        ->InitialPropagateUnassigned(data->assigned(), data->unassigned());
    dims_[dim_index]->EndInitialPropagate();
    if (need_context) {
      solver()->GetPropagationMonitor()->PopContext();
    }
  }
  if (need_context) {
    solver()->GetPropagationMonitor()->PopContext();
  }

  PropagateDelayed();
  ClearAll();
}

void Pack::Propagate() {
  const bool need_context = solver()->InstrumentsVariables();
  in_process_ = true;
  DCHECK_EQ(stamp_, solver()->fail_stamp());
  for (int bin_index = 0; bin_index < bins_; ++bin_index) {
    if (!removed_[bin_index].empty() || !forced_[bin_index].empty()) {
      if (need_context) {
        solver()->GetPropagationMonitor()->PushContext(StringPrintf(
            "Pack(bin %d, forced = [%s], removed = [%s])", bin_index,
            IntVectorToString(forced_[bin_index], ", ").c_str(),
            IntVectorToString(removed_[bin_index], ", ").c_str()));
      }

      for (int dim_index = 0; dim_index < dims_.size(); ++dim_index) {
        if (need_context) {
          solver()->GetPropagationMonitor()->PushContext(StringPrintf(
              "ProgateDimension(%s)", dims_[dim_index]->DebugString().c_str()));
        }
        dims_[dim_index]
            ->Propagate(bin_index, forced_[bin_index], removed_[bin_index]);
        if (need_context) {
          solver()->GetPropagationMonitor()->PopContext();
        }
      }
      if (need_context) {
        solver()->GetPropagationMonitor()->PopContext();
      }
    }
  }
  if (!removed_[bins_].empty() || !forced_[bins_].empty()) {
    if (need_context) {
      solver()->GetPropagationMonitor()->PushContext(
          StringPrintf("Pack(removed = [%s], forced = [%s])",
                       IntVectorToString(removed_[bins_], ", ").c_str(),
                       IntVectorToString(forced_[bins_], ", ").c_str()));
    }

    for (int dim_index = 0; dim_index < dims_.size(); ++dim_index) {
      if (need_context) {
        solver()->GetPropagationMonitor()->PushContext(StringPrintf(
            "ProgateDimension(%s)", dims_[dim_index]->DebugString().c_str()));
      }
      dims_[dim_index]->PropagateUnassigned(removed_[bins_], forced_[bins_]);
      if (need_context) {
        solver()->GetPropagationMonitor()->PopContext();
      }
    }
    if (need_context) {
      solver()->GetPropagationMonitor()->PopContext();
    }
  }
  for (int dim_index = 0; dim_index < dims_.size(); ++dim_index) {
    dims_[dim_index]->EndPropagate();
  }

  PropagateDelayed();
  ClearAll();
}

void Pack::OneDomain(int var_index) {
  // TODO(user): We know var ranges from 0 to bins_. There are lots
  // of simplifications possible.
  Solver* const s = solver();
  const uint64 current_stamp = s->fail_stamp();
  if (stamp_ < current_stamp) {
    stamp_ = current_stamp;
    ClearAll();
  }
  IntVar* const var = vars_[var_index];
  bool bound = var->Bound();
  const int64 oldmin = var->OldMin();
  const int64 oldmax = var->OldMax();
  const int64 vmin = var->Min();
  const int64 vmax = var->Max();
  for (int64 value = std::max(oldmin, 0LL); value < std::min(vmin, bins_ + 1LL);
       ++value) {
    if (unprocessed_->IsSet(value, var_index)) {
      unprocessed_->SetToZero(s, value, var_index);
      removed_[value].push_back(var_index);
    }
  }
  if (!bound) {
    IntVarIterator* const holes = holes_[var_index];
    for (holes->Init(); holes->Ok(); holes->Next()) {
      const int64 value = holes->Value();
      if (value >= std::max(0LL, vmin) &&
          value <= std::min(static_cast<int64>(bins_), vmax)) {
        DCHECK(unprocessed_->IsSet(value, var_index));
        unprocessed_->SetToZero(s, value, var_index);
        removed_[value].push_back(var_index);
      }
    }
  }
  for (int64 value = std::max(vmax + 1, 0LL);
       value <= std::min(oldmax, static_cast<int64>(bins_)); ++value) {
    if (unprocessed_->IsSet(value, var_index)) {
      unprocessed_->SetToZero(s, value, var_index);
      removed_[value].push_back(var_index);
    }
  }
  if (bound) {
    unprocessed_->SetToZero(s, var->Min(), var_index);
    forced_[var->Min()].push_back(var_index);
  }
  EnqueueDelayedDemon(demon_);
}

string Pack::DebugString() const {
  string result = "Pack([";
  for (int i = 0; i < vars_.size(); ++i) {
    result += vars_[i]->DebugString() + " ";
  }
  result += "], dimensions = [";
  for (int i = 0; i < dims_.size(); ++i) {
    result += dims_[i]->DebugString() + " ";
  }
  StringAppendF(&result, "], bins = %d)", bins_);
  return result;
}

void Pack::Accept(ModelVisitor* const visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kPack, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_);
  visitor->VisitIntegerArgument(ModelVisitor::kSizeArgument, bins_);
  for (int i = 0; i < dims_.size(); ++i) {
    dims_[i]->Accept(visitor);
  }
  visitor->EndVisitConstraint(ModelVisitor::kPack, this);
}

bool Pack::IsUndecided(int var_index, int bin_index) const {
  return unprocessed_->IsSet(bin_index, var_index);
}

void Pack::SetImpossible(int var_index, int bin_index) {
  if (IsInProcess()) {
    to_unset_.push_back(std::make_pair(var_index, bin_index));
  } else {
    vars_[var_index]->RemoveValue(bin_index);
  }
}

void Pack::Assign(int var_index, int bin_index) {
  if (IsInProcess()) {
    to_set_.push_back(std::make_pair(var_index, bin_index));
  } else {
    vars_[var_index]->SetValue(bin_index);
  }
}

bool Pack::IsAssignedStatusKnown(int var_index) const {
  return !unprocessed_->IsSet(bins_, var_index);
}

bool Pack::IsPossible(int var_index, int bin_index) const {
  return vars_[var_index]->Contains(bin_index);
}

IntVar* Pack::AssignVar(int var_index, int bin_index) const {
  return solver()->MakeIsEqualCstVar(vars_[var_index], bin_index);
}

void Pack::SetAssigned(int var_index) {
  if (IsInProcess()) {
    to_unset_.push_back(std::make_pair(var_index, bins_));
  } else {
    vars_[var_index]->RemoveValue(bins_);
  }
}

void Pack::SetUnassigned(int var_index) {
  if (IsInProcess()) {
    to_set_.push_back(std::make_pair(var_index, bins_));
  } else {
    vars_[var_index]->SetValue(bins_);
  }
}

bool Pack::IsInProcess() const {
  return in_process_ && (solver()->fail_stamp() == stamp_);
}

void Pack::RemoveAllPossibleFromBin(int bin_index) {
  int var_index = unprocessed_->GetFirstBit(bin_index, 0);
  while (var_index != -1 && var_index < vars_.size()) {
    SetImpossible(var_index, bin_index);
    var_index = var_index == vars_.size() - 1
                    ? -1
                    : unprocessed_->GetFirstBit(bin_index, var_index + 1);
  }
}

void Pack::AssignAllPossibleToBin(int bin_index) {
  int var_index = unprocessed_->GetFirstBit(bin_index, 0);
  while (var_index != -1 && var_index < vars_.size()) {
    Assign(var_index, bin_index);
    var_index = var_index == vars_.size() - 1
                    ? -1
                    : unprocessed_->GetFirstBit(bin_index, var_index + 1);
  }
}

void Pack::AssignFirstPossibleToBin(int bin_index) {
  int var_index = unprocessed_->GetFirstBit(bin_index, 0);
  if (var_index != -1 && var_index < vars_.size()) {
    Assign(var_index, bin_index);
  }
}

void Pack::AssignAllRemainingItems() {
  int var_index = unprocessed_->GetFirstBit(bins_, 0);
  while (var_index != -1 && var_index < vars_.size()) {
    SetAssigned(var_index);
    var_index = var_index == vars_.size() - 1 ? -1 : unprocessed_->GetFirstBit(
                                                         bins_, var_index + 1);
  }
}

void Pack::UnassignAllRemainingItems() {
  int var_index = unprocessed_->GetFirstBit(bins_, 0);
  while (var_index != -1 && var_index < vars_.size()) {
    SetUnassigned(var_index);
    var_index = var_index == vars_.size() - 1 ? -1 : unprocessed_->GetFirstBit(
                                                         bins_, var_index + 1);
  }
}

// ----- Dimension -----

// ----- Class Dimension Less Than Constant -----

namespace {
struct WeightContainer {
  int index;
  int64 weight;
  WeightContainer(int i, int64 w) : index(i), weight(w) {}
  bool operator<(const WeightContainer& c) const { return (weight < c.weight); }
};

void SortWeightVector(std::vector<int>* const indices,
                      std::vector<WeightContainer>* const to_sort) {
  std::sort(to_sort->begin(), to_sort->end());
  for (int index = 0; index < to_sort->size(); ++index) {
    (*indices)[index] = (*to_sort)[index].index;
  }
  indices->resize(to_sort->size());
}

void SortIndexByWeight(std::vector<int>* const indices,
                       const std::vector<int64>& weights) {
  std::vector<WeightContainer> to_sort;
  for (int index = 0; index < indices->size(); ++index) {
    if (weights[index] != 0) {
      to_sort.push_back(WeightContainer((*indices)[index], weights[index]));
    }
  }
  SortWeightVector(indices, &to_sort);
}

void SortIndexByWeight(std::vector<int>* const indices,
                       Pack::ItemUsageEvaluator* weights) {
  std::vector<WeightContainer> to_sort;
  for (int index = 0; index < indices->size(); ++index) {
    const int w = weights->Run(index);
    if (w != 0) {
      to_sort.push_back(WeightContainer((*indices)[index], w));
    }
  }
  SortWeightVector(indices, &to_sort);
}

void SortIndexByWeight(std::vector<int>* const indices,
                       Pack::ItemUsagePerBinEvaluator* weights, int bin_index) {
  std::vector<WeightContainer> to_sort;
  for (int index = 0; index < indices->size(); ++index) {
    const int w = weights->Run(index, bin_index);
    if (w != 0) {
      to_sort.push_back(WeightContainer((*indices)[index], w));
    }
  }
  SortWeightVector(indices, &to_sort);
}

class DimensionLessThanConstant : public Dimension {
 public:
  DimensionLessThanConstant(Solver* const s, Pack* const p,
                            const std::vector<int64>& weights,
                            const std::vector<int64>& upper_bounds)
      : Dimension(s, p),
        vars_count_(weights.size()),
        weights_(weights),
        bins_count_(upper_bounds.size()),
        upper_bounds_(upper_bounds),
        first_unbound_backward_vector_(bins_count_, 0),
        sum_of_bound_variables_vector_(bins_count_, 0LL),
        ranked_(vars_count_) {
    for (int i = 0; i < vars_count_; ++i) {
      ranked_[i] = i;
    }
    SortIndexByWeight(&ranked_, weights_);
  }

  virtual ~DimensionLessThanConstant() {}

  virtual void Post() {}

  void PushFromTop(int bin_index) {
    const int64 slack =
        upper_bounds_[bin_index] - sum_of_bound_variables_vector_[bin_index];
    if (slack < 0) {
      solver()->Fail();
    }
    int last_unbound = first_unbound_backward_vector_[bin_index];
    for (; last_unbound >= 0; --last_unbound) {
      const int var_index = ranked_[last_unbound];
      if (IsUndecided(var_index, bin_index)) {
        if (weights_[var_index] > slack) {
          SetImpossible(var_index, bin_index);
        } else {
          break;
        }
      }
    }
    first_unbound_backward_vector_.SetValue(solver(), bin_index, last_unbound);
  }

  virtual void InitialPropagate(int bin_index, const std::vector<int>& forced,
                                const std::vector<int>& undecided) {
    Solver* const s = solver();
    int64 sum = 0LL;
    for (ConstIter<std::vector<int>> it(forced); !it.at_end(); ++it) {
      sum += weights_[*it];
    }
    sum_of_bound_variables_vector_.SetValue(s, bin_index, sum);
    first_unbound_backward_vector_.SetValue(s, bin_index, ranked_.size() - 1);
    PushFromTop(bin_index);
  }

  virtual void EndInitialPropagate() {}

  virtual void Propagate(int bin_index, const std::vector<int>& forced,
                         const std::vector<int>& removed) {
    if (forced.size() > 0) {
      Solver* const s = solver();
      int64 sum = sum_of_bound_variables_vector_[bin_index];
      for (ConstIter<std::vector<int>> it(forced); !it.at_end(); ++it) {
        sum += weights_[*it];
      }
      sum_of_bound_variables_vector_.SetValue(s, bin_index, sum);
      PushFromTop(bin_index);
    }
  }
  virtual void InitialPropagateUnassigned(const std::vector<int>& assigned,
                                          const std::vector<int>& unassigned) {}
  virtual void PropagateUnassigned(const std::vector<int>& assigned,
                                   const std::vector<int>& unassigned) {}

  virtual void EndPropagate() {}

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitExtension(ModelVisitor::kUsageLessConstantExtension);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kCoefficientsArgument,
                                       weights_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument,
                                       upper_bounds_);
    visitor->EndVisitExtension(ModelVisitor::kUsageLessConstantExtension);
  }

 private:
  const int vars_count_;
  const std::vector<int64> weights_;
  const int bins_count_;
  const std::vector<int64> upper_bounds_;
  RevArray<int> first_unbound_backward_vector_;
  RevArray<int64> sum_of_bound_variables_vector_;
  std::vector<int> ranked_;
};

class DimensionSumCallbackLessThanConstant : public Dimension {
 public:
  // Ownership is taken by the class. The callback has to be permanent.
  DimensionSumCallbackLessThanConstant(Solver* const s, Pack* const p,
                                       Pack::ItemUsageEvaluator* weights,
                                       int vars_count,
                                       const std::vector<int64>& upper_bounds)
      : Dimension(s, p),
        vars_count_(vars_count),
        weights_(weights),
        bins_count_(upper_bounds.size()),
        upper_bounds_(upper_bounds),
        first_unbound_backward_vector_(bins_count_, 0),
        sum_of_bound_variables_vector_(bins_count_, 0LL),
        ranked_(vars_count_) {
    DCHECK(weights);
    DCHECK_GT(vars_count, 0);
    weights->CheckIsRepeatable();
    for (int i = 0; i < vars_count_; ++i) {
      ranked_[i] = i;
    }
    SortIndexByWeight(&ranked_, weights_.get());
  }

  virtual ~DimensionSumCallbackLessThanConstant() {}

  virtual void Post() {}

  void PushFromTop(int bin_index) {
    const int64 slack =
        upper_bounds_[bin_index] - sum_of_bound_variables_vector_[bin_index];
    if (slack < 0) {
      solver()->Fail();
    }
    int last_unbound = first_unbound_backward_vector_[bin_index];
    for (; last_unbound >= 0; --last_unbound) {
      const int var_index = ranked_[last_unbound];
      if (IsUndecided(var_index, bin_index)) {
        if (weights_->Run(var_index) > slack) {
          SetImpossible(var_index, bin_index);
        } else {
          break;
        }
      }
    }
    first_unbound_backward_vector_.SetValue(solver(), bin_index, last_unbound);
  }

  virtual void InitialPropagate(int bin_index, const std::vector<int>& forced,
                                const std::vector<int>& undecided) {
    Solver* const s = solver();
    int64 sum = 0LL;
    for (ConstIter<std::vector<int>> it(forced); !it.at_end(); ++it) {
      sum += weights_->Run(*it);
    }
    sum_of_bound_variables_vector_.SetValue(s, bin_index, sum);
    first_unbound_backward_vector_.SetValue(s, bin_index, ranked_.size() - 1);
    PushFromTop(bin_index);
  }

  virtual void EndInitialPropagate() {}

  virtual void Propagate(int bin_index, const std::vector<int>& forced,
                         const std::vector<int>& removed) {
    if (forced.size() > 0) {
      Solver* const s = solver();
      int64 sum = sum_of_bound_variables_vector_[bin_index];
      for (ConstIter<std::vector<int>> it(forced); !it.at_end(); ++it) {
        sum += weights_->Run(*it);
      }
      sum_of_bound_variables_vector_.SetValue(s, bin_index, sum);
      PushFromTop(bin_index);
    }
  }
  virtual void InitialPropagateUnassigned(const std::vector<int>& assigned,
                                          const std::vector<int>& unassigned) {}
  virtual void PropagateUnassigned(const std::vector<int>& assigned,
                                   const std::vector<int>& unassigned) {}

  virtual void EndPropagate() {}

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitExtension(ModelVisitor::kUsageLessConstantExtension);
    // TODO(user) : Visit weight correctly.
    // visitor->VisitIntegerArrayArgument(ModelVisitor::kCoefficientsArgument,
    //                                    weights_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument,
                                       upper_bounds_);
    visitor->EndVisitExtension(ModelVisitor::kUsageLessConstantExtension);
  }

 private:
  const int vars_count_;
  std::unique_ptr<Pack::ItemUsageEvaluator> weights_;
  const int bins_count_;
  const std::vector<int64> upper_bounds_;
  RevArray<int> first_unbound_backward_vector_;
  RevArray<int64> sum_of_bound_variables_vector_;
  std::vector<int> ranked_;
};

class DimensionLessThanConstantCallback2 : public Dimension {
 public:
  // Ownership is taken by the class. The callback has to be permanent.
  DimensionLessThanConstantCallback2(Solver* const s, Pack* const p,
                                     Pack::ItemUsagePerBinEvaluator* weights,
                                     int vars_count,
                                     const std::vector<int64>& upper_bounds)
      : Dimension(s, p),
        vars_count_(vars_count),
        weights_(weights),
        bins_count_(upper_bounds.size()),
        upper_bounds_(upper_bounds),
        first_unbound_backward_vector_(bins_count_, 0),
        sum_of_bound_variables_vector_(bins_count_, 0LL),
        ranked_(bins_count_) {
    DCHECK(weights);
    DCHECK_GT(vars_count, 0);
    weights->CheckIsRepeatable();
    for (int b = 0; b < bins_count_; ++b) {
      ranked_[b].resize(vars_count);
      for (int i = 0; i < vars_count_; ++i) {
        ranked_[b][i] = i;
      }
      SortIndexByWeight(&ranked_[b], weights_.get(), b);
    }
  }

  virtual ~DimensionLessThanConstantCallback2() {}

  virtual void Post() {}

  void PushFromTop(int bin_index) {
    const int64 slack =
        upper_bounds_[bin_index] - sum_of_bound_variables_vector_[bin_index];
    if (slack < 0) {
      solver()->Fail();
    }
    int last_unbound = first_unbound_backward_vector_[bin_index];
    for (; last_unbound >= 0; --last_unbound) {
      const int var_index = ranked_[bin_index][last_unbound];
      if (IsUndecided(var_index, bin_index)) {
        if (weights_->Run(var_index, bin_index) > slack) {
          SetImpossible(var_index, bin_index);
        } else {
          break;
        }
      }
    }
    first_unbound_backward_vector_.SetValue(solver(), bin_index, last_unbound);
  }

  virtual void InitialPropagate(int bin_index, const std::vector<int>& forced,
                                const std::vector<int>& undecided) {
    Solver* const s = solver();
    int64 sum = 0LL;
    for (ConstIter<std::vector<int>> it(forced); !it.at_end(); ++it) {
      sum += weights_->Run(*it, bin_index);
    }
    sum_of_bound_variables_vector_.SetValue(s, bin_index, sum);
    first_unbound_backward_vector_.SetValue(s, bin_index,
                                            ranked_[bin_index].size() - 1);
    PushFromTop(bin_index);
  }

  virtual void EndInitialPropagate() {}

  virtual void Propagate(int bin_index, const std::vector<int>& forced,
                         const std::vector<int>& removed) {
    if (forced.size() > 0) {
      Solver* const s = solver();
      int64 sum = sum_of_bound_variables_vector_[bin_index];
      for (ConstIter<std::vector<int>> it(forced); !it.at_end(); ++it) {
        sum += weights_->Run(*it, bin_index);
      }
      sum_of_bound_variables_vector_.SetValue(s, bin_index, sum);
      PushFromTop(bin_index);
    }
  }
  virtual void InitialPropagateUnassigned(const std::vector<int>& assigned,
                                          const std::vector<int>& unassigned) {}
  virtual void PropagateUnassigned(const std::vector<int>& assigned,
                                   const std::vector<int>& unassigned) {}

  virtual void EndPropagate() {}

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitExtension(ModelVisitor::kUsageLessConstantExtension);
    // TODO(user): Visit weight correctly
    // visitor->VisitIntegerArrayArgument(ModelVisitor::kCoefficientsArgument,
    //                                    weights_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument,
                                       upper_bounds_);
    visitor->EndVisitExtension(ModelVisitor::kUsageLessConstantExtension);
  }

 private:
  const int vars_count_;
  std::unique_ptr<Pack::ItemUsagePerBinEvaluator> weights_;
  const int bins_count_;
  const std::vector<int64> upper_bounds_;
  RevArray<int> first_unbound_backward_vector_;
  RevArray<int64> sum_of_bound_variables_vector_;
  std::vector<std::vector<int>> ranked_;
};

class DimensionWeightedSumEqVar : public Dimension {
 public:
  class VarDemon : public Demon {
   public:
    VarDemon(DimensionWeightedSumEqVar* const dim, int index)
        : dim_(dim), index_(index) {}
    virtual ~VarDemon() {}

    virtual void Run(Solver* const s) { dim_->PushFromTop(index_); }

   private:
    DimensionWeightedSumEqVar* const dim_;
    const int index_;
  };

  DimensionWeightedSumEqVar(Solver* const s, Pack* const p,
                            const std::vector<int64>& weights,
                            const std::vector<IntVar*>& loads)
      : Dimension(s, p),
        vars_count_(weights.size()),
        weights_(weights),
        bins_count_(loads.size()),
        loads_(loads),
        first_unbound_backward_vector_(bins_count_, 0),
        sum_of_bound_variables_vector_(bins_count_, 0LL),
        sum_of_all_variables_vector_(bins_count_, 0LL),
        ranked_(vars_count_) {
    DCHECK_GT(vars_count_, 0);
    DCHECK_GT(bins_count_, 0);
    for (int i = 0; i < vars_count_; ++i) {
      ranked_[i] = i;
    }
    SortIndexByWeight(&ranked_, weights_);
  }

  virtual ~DimensionWeightedSumEqVar() {}

  virtual string DebugString() const { return "DimensionWeightedSumEqVar"; }

  virtual void Post() {
    for (int i = 0; i < bins_count_; ++i) {
      Demon* const d = solver()->RevAlloc(new VarDemon(this, i));
      loads_[i]->WhenRange(d);
    }
  }

  void PushFromTop(int bin_index) {
    IntVar* const load = loads_[bin_index];
    const int64 sum_min = sum_of_bound_variables_vector_[bin_index];
    const int64 sum_max = sum_of_all_variables_vector_[bin_index];
    load->SetRange(sum_min, sum_max);
    const int64 slack_up = load->Max() - sum_min;
    const int64 slack_down = sum_max - load->Min();
    DCHECK_GE(slack_down, 0);
    DCHECK_GE(slack_up, 0);
    int last_unbound = first_unbound_backward_vector_[bin_index];
    for (; last_unbound >= 0; --last_unbound) {
      const int var_index = ranked_[last_unbound];
      const int64 weight = weights_[var_index];
      if (IsUndecided(var_index, bin_index)) {
        if (weight > slack_up) {
          SetImpossible(var_index, bin_index);
        } else if (weight > slack_down) {
          Assign(var_index, bin_index);
        } else {
          break;
        }
      }
    }
    first_unbound_backward_vector_.SetValue(solver(), bin_index, last_unbound);
  }

  virtual void InitialPropagate(int bin_index, const std::vector<int>& forced,
                                const std::vector<int>& undecided) {
    Solver* const s = solver();
    int64 sum = 0LL;
    for (ConstIter<std::vector<int>> it(forced); !it.at_end(); ++it) {
      sum += weights_[*it];
    }
    sum_of_bound_variables_vector_.SetValue(s, bin_index, sum);
    for (ConstIter<std::vector<int>> it(undecided); !it.at_end(); ++it) {
      sum += weights_[*it];
    }
    sum_of_all_variables_vector_.SetValue(s, bin_index, sum);
    first_unbound_backward_vector_.SetValue(s, bin_index, ranked_.size() - 1);
    PushFromTop(bin_index);
  }

  virtual void EndInitialPropagate() {}

  virtual void Propagate(int bin_index, const std::vector<int>& forced,
                         const std::vector<int>& removed) {
    Solver* const s = solver();
    int64 down = sum_of_bound_variables_vector_[bin_index];
    for (ConstIter<std::vector<int>> it(forced); !it.at_end(); ++it) {
      down += weights_[*it];
    }
    sum_of_bound_variables_vector_.SetValue(s, bin_index, down);
    int64 up = sum_of_all_variables_vector_[bin_index];
    for (ConstIter<std::vector<int>> it(removed); !it.at_end(); ++it) {
      up -= weights_[*it];
    }
    sum_of_all_variables_vector_.SetValue(s, bin_index, up);
    PushFromTop(bin_index);
  }
  virtual void InitialPropagateUnassigned(const std::vector<int>& assigned,
                                          const std::vector<int>& unassigned) {}
  virtual void PropagateUnassigned(const std::vector<int>& assigned,
                                   const std::vector<int>& unassigned) {}

  virtual void EndPropagate() {}

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitExtension(ModelVisitor::kUsageEqualVariableExtension);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kCoefficientsArgument,
                                       weights_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               loads_);
    visitor->EndVisitExtension(ModelVisitor::kUsageEqualVariableExtension);
  }

 private:
  const int vars_count_;
  const std::vector<int64> weights_;
  const int bins_count_;
  const std::vector<IntVar*> loads_;
  RevArray<int> first_unbound_backward_vector_;
  RevArray<int64> sum_of_bound_variables_vector_;
  RevArray<int64> sum_of_all_variables_vector_;
  std::vector<int> ranked_;
};

class DimensionWeightedCallback2SumEqVar : public Dimension {
 public:
  class VarDemon : public Demon {
   public:
    VarDemon(DimensionWeightedCallback2SumEqVar* const dim, int index)
        : dim_(dim), index_(index) {}
    virtual ~VarDemon() {}

    virtual void Run(Solver* const s) { dim_->PushFromTop(index_); }

   private:
    DimensionWeightedCallback2SumEqVar* const dim_;
    const int index_;
  };

  DimensionWeightedCallback2SumEqVar(Solver* const s, Pack* const p,
                                     Pack::ItemUsagePerBinEvaluator* weights,
                                     int vars_count,
                                     const std::vector<IntVar*>& loads)
      : Dimension(s, p),
        vars_count_(vars_count),
        weights_(weights),
        bins_count_(loads.size()),
        loads_(loads),
        first_unbound_backward_vector_(bins_count_, 0),
        sum_of_bound_variables_vector_(bins_count_, 0LL),
        sum_of_all_variables_vector_(bins_count_, 0LL),
        ranked_(bins_count_) {
    DCHECK(weights);
    DCHECK_GT(vars_count_, 0);
    DCHECK_GT(bins_count_, 0);
    weights->CheckIsRepeatable();
    for (int b = 0; b < bins_count_; ++b) {
      ranked_[b].resize(vars_count);
      for (int i = 0; i < vars_count_; ++i) {
        ranked_[b][i] = i;
      }
      SortIndexByWeight(&ranked_[b], weights_.get(), b);
    }
  }

  virtual ~DimensionWeightedCallback2SumEqVar() {}

  virtual string DebugString() const {
    return "DimensionWeightedCallback2SumEqVar";
  }

  virtual void Post() {
    for (int i = 0; i < bins_count_; ++i) {
      Demon* const d = solver()->RevAlloc(new VarDemon(this, i));
      loads_[i]->WhenRange(d);
    }
  }

  void PushFromTop(int bin_index) {
    IntVar* const load = loads_[bin_index];
    const int64 sum_min = sum_of_bound_variables_vector_[bin_index];
    const int64 sum_max = sum_of_all_variables_vector_[bin_index];
    load->SetRange(sum_min, sum_max);
    const int64 slack_up = load->Max() - sum_min;
    const int64 slack_down = sum_max - load->Min();
    DCHECK_GE(slack_down, 0);
    DCHECK_GE(slack_up, 0);
    int last_unbound = first_unbound_backward_vector_[bin_index];
    for (; last_unbound >= 0; --last_unbound) {
      const int var_index = ranked_[bin_index][last_unbound];
      const int64 weight = weights_->Run(var_index, bin_index);
      if (IsUndecided(var_index, bin_index)) {
        if (weight > slack_up) {
          SetImpossible(var_index, bin_index);
        } else if (weight > slack_down) {
          Assign(var_index, bin_index);
        } else {
          break;
        }
      }
    }
    first_unbound_backward_vector_.SetValue(solver(), bin_index, last_unbound);
  }

  virtual void InitialPropagate(int bin_index, const std::vector<int>& forced,
                                const std::vector<int>& undecided) {
    Solver* const s = solver();
    int64 sum = 0LL;
    for (ConstIter<std::vector<int>> it(forced); !it.at_end(); ++it) {
      sum += weights_->Run(*it, bin_index);
    }
    sum_of_bound_variables_vector_.SetValue(s, bin_index, sum);
    for (ConstIter<std::vector<int>> it(undecided); !it.at_end(); ++it) {
      sum += weights_->Run(*it, bin_index);
    }
    sum_of_all_variables_vector_.SetValue(s, bin_index, sum);
    first_unbound_backward_vector_.SetValue(s, bin_index,
                                            ranked_[bin_index].size() - 1);
    PushFromTop(bin_index);
  }

  virtual void EndInitialPropagate() {}

  virtual void Propagate(int bin_index, const std::vector<int>& forced,
                         const std::vector<int>& removed) {
    Solver* const s = solver();
    int64 down = sum_of_bound_variables_vector_[bin_index];
    for (ConstIter<std::vector<int>> it(forced); !it.at_end(); ++it) {
      down += weights_->Run(*it, bin_index);
    }
    sum_of_bound_variables_vector_.SetValue(s, bin_index, down);
    int64 up = sum_of_all_variables_vector_[bin_index];
    for (ConstIter<std::vector<int>> it(removed); !it.at_end(); ++it) {
      up -= weights_->Run(*it, bin_index);
    }
    sum_of_all_variables_vector_.SetValue(s, bin_index, up);
    PushFromTop(bin_index);
  }
  virtual void InitialPropagateUnassigned(const std::vector<int>& assigned,
                                          const std::vector<int>& unassigned) {}
  virtual void PropagateUnassigned(const std::vector<int>& assigned,
                                   const std::vector<int>& unassigned) {}

  virtual void EndPropagate() {}

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitExtension(ModelVisitor::kUsageEqualVariableExtension);
    // TODO(user): Visit weight correctly
    // visitor->VisitIntegerArrayArgument(ModelVisitor::kCoefficientsArgument,
    //                                    weights_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               loads_);
    visitor->EndVisitExtension(ModelVisitor::kUsageEqualVariableExtension);
  }

 private:
  const int vars_count_;
  std::unique_ptr<Pack::ItemUsagePerBinEvaluator> weights_;
  const int bins_count_;
  const std::vector<IntVar*> loads_;
  RevArray<int> first_unbound_backward_vector_;
  RevArray<int64> sum_of_bound_variables_vector_;
  RevArray<int64> sum_of_all_variables_vector_;
  std::vector<std::vector<int>> ranked_;
};

class AssignedWeightedSumDimension : public Dimension {
 public:
  class VarDemon : public Demon {
   public:
    explicit VarDemon(AssignedWeightedSumDimension* const dim) : dim_(dim) {}
    virtual ~VarDemon() {}

    virtual void Run(Solver* const s) { dim_->PropagateAll(); }

   private:
    AssignedWeightedSumDimension* const dim_;
  };

  AssignedWeightedSumDimension(Solver* const s, Pack* const p,
                               const std::vector<int64>& weights, int bins_count,
                               IntVar* const cost_var)
      : Dimension(s, p),
        vars_count_(weights.size()),
        weights_(weights),
        bins_count_(bins_count),
        cost_var_(cost_var),
        first_unbound_backward_(0),
        sum_of_assigned_items_(0LL),
        sum_of_unassigned_items_(0LL),
        ranked_(vars_count_),
        sum_all_weights_(0LL) {
    DCHECK(cost_var);
    DCHECK_GT(vars_count_, 0);
    DCHECK_GT(bins_count_, 0);
    for (int i = 0; i < vars_count_; ++i) {
      ranked_[i] = i;
    }
    SortIndexByWeight(&ranked_, weights_);
    first_unbound_backward_.SetValue(s, ranked_.size() - 1);
  }

  virtual ~AssignedWeightedSumDimension() {}

  virtual void Post() {
    Demon* const uv = solver()->RevAlloc(new VarDemon(this));
    cost_var_->WhenRange(uv);
  }

  void PropagateAll() {
    cost_var_->SetRange(sum_of_assigned_items_.Value(),
                        sum_all_weights_ - sum_of_unassigned_items_.Value());
    const int64 slack_up = cost_var_->Max() - sum_of_assigned_items_.Value();
    const int64 slack_down = sum_all_weights_ - cost_var_->Min();
    int last_unbound = first_unbound_backward_.Value();
    for (; last_unbound >= 0; --last_unbound) {
      const int var_index = ranked_[last_unbound];
      if (!IsAssignedStatusKnown(var_index)) {
        const int64 coefficient = weights_[var_index];
        if (coefficient > slack_up) {
          SetUnassigned(var_index);
        } else if (coefficient > slack_down) {
          SetAssigned(var_index);
        } else {
          break;
        }
      }
    }
    first_unbound_backward_.SetValue(solver(), last_unbound);
  }

  virtual void InitialPropagate(int bin_index, const std::vector<int>& forced,
                                const std::vector<int>& undecided) {}

  virtual void EndInitialPropagate() {}

  virtual void InitialPropagateUnassigned(const std::vector<int>& assigned,
                                          const std::vector<int>& unassigned) {
    for (int index = 0; index < vars_count_; ++index) {
      sum_all_weights_ += weights_[index];
    }

    PropagateUnassigned(assigned, unassigned);
  }

  virtual void Propagate(int bin_index, const std::vector<int>& forced,
                         const std::vector<int>& removed) {}

  virtual void PropagateUnassigned(const std::vector<int>& assigned,
                                   const std::vector<int>& unassigned) {
    int64 sum_assigned = sum_of_assigned_items_.Value();
    for (int index = 0; index < assigned.size(); ++index) {
      const int var_index = assigned[index];
      sum_assigned += weights_[var_index];
    }

    int64 sum_unassigned = sum_of_unassigned_items_.Value();
    for (int index = 0; index < unassigned.size(); ++index) {
      const int var_index = unassigned[index];
      sum_unassigned += weights_[var_index];
    }

    Solver* const s = solver();
    sum_of_assigned_items_.SetValue(s, sum_assigned);
    sum_of_unassigned_items_.SetValue(s, sum_unassigned);
    PropagateAll();
  }

  virtual void EndPropagate() {}

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitExtension(
        ModelVisitor::kWeightedSumOfAssignedEqualVariableExtension);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kCoefficientsArgument,
                                       weights_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            cost_var_);
    visitor->EndVisitExtension(
        ModelVisitor::kWeightedSumOfAssignedEqualVariableExtension);
  }

 private:
  const int vars_count_;
  const std::vector<int64> weights_;
  const int bins_count_;
  IntVar* const cost_var_;
  Rev<int> first_unbound_backward_;
  Rev<int64> sum_of_assigned_items_;
  Rev<int64> sum_of_unassigned_items_;
  std::vector<int> ranked_;
  int64 sum_all_weights_;
};

// ----- Count unassigned jobs dimension -----

class CountAssignedItemsDimension : public Dimension {
 public:
  class VarDemon : public Demon {
   public:
    explicit VarDemon(CountAssignedItemsDimension* const dim) : dim_(dim) {}
    virtual ~VarDemon() {}

    virtual void Run(Solver* const s) { dim_->PropagateAll(); }

   private:
    CountAssignedItemsDimension* const dim_;
  };

  CountAssignedItemsDimension(Solver* const s, Pack* const p, int vars_count,
                              int bins_count, IntVar* const cost_var)
      : Dimension(s, p),
        vars_count_(vars_count),
        bins_count_(bins_count),
        cost_var_(cost_var),
        first_unbound_backward_(0),
        assigned_count_(0),
        unassigned_count_(0) {
    DCHECK(cost_var);
    DCHECK_GT(vars_count, 0);
    DCHECK_GT(bins_count, 0);
  }

  virtual ~CountAssignedItemsDimension() {}

  virtual void Post() {
    Demon* const uv = solver()->RevAlloc(new VarDemon(this));
    cost_var_->WhenRange(uv);
  }

  void PropagateAll() {
    cost_var_->SetRange(assigned_count_.Value(),
                        vars_count_ - unassigned_count_.Value());
    if (assigned_count_.Value() == cost_var_->Max()) {
      UnassignAllRemainingItems();
    } else if (cost_var_->Min() == vars_count_ - unassigned_count_.Value()) {
      AssignAllRemainingItems();
    }
  }

  virtual void InitialPropagate(int bin_index, const std::vector<int>& forced,
                                const std::vector<int>& undecided) {}

  virtual void EndInitialPropagate() {}

  virtual void InitialPropagateUnassigned(const std::vector<int>& assigned,
                                          const std::vector<int>& unassigned) {
    PropagateUnassigned(assigned, unassigned);
  }

  virtual void Propagate(int bin_index, const std::vector<int>& forced,
                         const std::vector<int>& removed) {}

  virtual void PropagateUnassigned(const std::vector<int>& assigned,
                                   const std::vector<int>& unassigned) {
    Solver* const s = solver();
    assigned_count_.SetValue(s, assigned_count_.Value() + assigned.size());
    unassigned_count_.SetValue(s,
                               unassigned_count_.Value() + unassigned.size());
    PropagateAll();
  }

  virtual void EndPropagate() {}

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitExtension(ModelVisitor::kCountAssignedItemsExtension);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            cost_var_);
    visitor->EndVisitExtension(ModelVisitor::kCountAssignedItemsExtension);
  }

 private:
  const int vars_count_;
  const int bins_count_;
  IntVar* const cost_var_;
  Rev<int> first_unbound_backward_;
  Rev<int> assigned_count_;
  Rev<int> unassigned_count_;
};

// ----- Count used bin dimension -----

class CountUsedBinDimension : public Dimension {
 public:
  class VarDemon : public Demon {
   public:
    explicit VarDemon(CountUsedBinDimension* const dim) : dim_(dim) {}
    virtual ~VarDemon() {}

    virtual void Run(Solver* const s) { dim_->PropagateAll(); }

   private:
    CountUsedBinDimension* const dim_;
  };

  CountUsedBinDimension(Solver* const s, Pack* const p, int vars_count,
                        int bins_count, IntVar* const count_var)
      : Dimension(s, p),
        vars_count_(vars_count),
        bins_count_(bins_count),
        count_var_(count_var),
        used_(bins_count_),
        candidates_(bins_count_, 0),
        card_min_(0),
        card_max_(bins_count_),
        initial_min_(0),
        initial_max_(0) {
    DCHECK(count_var);
    DCHECK_GT(vars_count, 0);
    DCHECK_GT(bins_count, 0);
  }

  virtual ~CountUsedBinDimension() {}

  virtual void Post() {
    Demon* const uv = solver()->RevAlloc(new VarDemon(this));
    count_var_->WhenRange(uv);
    initial_min_ = 0;
    initial_max_ = bins_count_;
  }

  void PropagateAll() {
    count_var_->SetRange(card_min_.Value(), card_max_.Value());
    if (card_min_.Value() == count_var_->Max()) {
      for (int bin_index = 0; bin_index < bins_count_; ++bin_index) {
        if (!used_.IsSet(bin_index) && candidates_[bin_index] > 0) {
          RemoveAllPossibleFromBin(bin_index);
        }
      }
    } else if (card_max_.Value() == count_var_->Min()) {
      for (int bin_index = 0; bin_index < bins_count_; ++bin_index) {
        if (candidates_[bin_index] == 1) {
          AssignFirstPossibleToBin(bin_index);
        }
      }
    }
  }

  virtual void InitialPropagate(int bin_index, const std::vector<int>& forced,
                                const std::vector<int>& undecided) {
    if (forced.size() > 0) {
      used_.SetToOne(solver(), bin_index);
      initial_min_++;
    } else if (undecided.size() > 0) {
      candidates_.SetValue(solver(), bin_index, undecided.size());
    } else {
      initial_max_--;
    }
  }

  virtual void EndInitialPropagate() {
    card_min_.SetValue(solver(), initial_min_);
    card_max_.SetValue(solver(), initial_max_);
    PropagateAll();
  }

  virtual void InitialPropagateUnassigned(const std::vector<int>& assigned,
                                          const std::vector<int>& unassigned) {}

  virtual void Propagate(int bin_index, const std::vector<int>& forced,
                         const std::vector<int>& removed) {
    if (!used_.IsSet(bin_index)) {
      if (forced.size() > 0) {
        used_.SetToOne(solver(), bin_index);
        card_min_.SetValue(solver(), card_min_.Value() + 1);
      } else if (removed.size() > 0) {
        candidates_.SetValue(solver(), bin_index,
                             candidates_.Value(bin_index) - removed.size());
        if (candidates_[bin_index] == 0) {
          card_max_.SetValue(solver(), card_max_.Value() - 1);
        }
      }
    }
  }

  virtual void PropagateUnassigned(const std::vector<int>& assigned,
                                   const std::vector<int>& unassigned) {}

  virtual void EndPropagate() { PropagateAll(); }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitExtension(ModelVisitor::kCountUsedBinsExtension);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            count_var_);
    visitor->EndVisitExtension(ModelVisitor::kCountUsedBinsExtension);
  }

 private:
  const int vars_count_;
  const int bins_count_;
  IntVar* const count_var_;
  RevBitSet used_;
  RevArray<int> candidates_;
  Rev<int> card_min_;
  Rev<int> card_max_;
  int initial_min_;
  int initial_max_;
};

// ---------- Variable Usage Dimension ----------

// This is a very naive, but correct implementation of the constraint.
class VariableUsageDimension : public Dimension {
 public:
  VariableUsageDimension(Solver* const solver, Pack* const pack,
                         const std::vector<int64>& capacities,
                         const std::vector<IntVar*>& weights)
      : Dimension(solver, pack), capacities_(capacities), weights_(weights) {}

  virtual ~VariableUsageDimension() {}

  virtual void Post() {
    Solver* const s = solver();
    const int num_bins = capacities_.size();
    const int num_items = weights_.size();

    for (int bin_index = 0; bin_index < num_bins; ++bin_index) {
      std::vector<IntVar*> terms;
      for (int item_index = 0; item_index < num_items; ++item_index) {
        IntVar* const assign_var = AssignVar(item_index, bin_index);
        terms.push_back(s->MakeProd(assign_var, weights_[item_index])->Var());
      }
      s->AddConstraint(s->MakeSumLessOrEqual(terms, capacities_[bin_index]));
    }
  }

  virtual void InitialPropagate(int bin_index, const std::vector<int>& forced,
                                const std::vector<int>& undecided) {}
  virtual void InitialPropagateUnassigned(const std::vector<int>& assigned,
                                          const std::vector<int>& unassigned) {}
  virtual void EndInitialPropagate() {}
  virtual void Propagate(int bin_index, const std::vector<int>& forced,
                         const std::vector<int>& removed) {}
  virtual void PropagateUnassigned(const std::vector<int>& assigned,
                                   const std::vector<int>& unassigned) {}
  virtual void EndPropagate() {}

  virtual string DebugString() const { return "VariableUsageDimension"; }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitExtension(
        ModelVisitor::kVariableUsageLessConstantExtension);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument,
                                       capacities_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               weights_);
    visitor->EndVisitExtension(
        ModelVisitor::kVariableUsageLessConstantExtension);
  }

 private:
  const std::vector<int64> capacities_;
  const std::vector<IntVar*> weights_;
};
}  // namespace

// ---------- API ----------

void Pack::AddWeightedSumLessOrEqualConstantDimension(
    const std::vector<int64>& weights, const std::vector<int64>& bounds) {
  CHECK_EQ(weights.size(), vars_.size());
  CHECK_EQ(bounds.size(), bins_);
  Solver* const s = solver();
  Dimension* const dim =
      s->RevAlloc(new DimensionLessThanConstant(s, this, weights, bounds));
  dims_.push_back(dim);
}

void Pack::AddWeightedSumLessOrEqualConstantDimension(
    ItemUsageEvaluator* weights, const std::vector<int64>& bounds) {
  CHECK(weights != nullptr);
  CHECK_EQ(bounds.size(), bins_);
  Solver* const s = solver();
  Dimension* const dim = s->RevAlloc(new DimensionSumCallbackLessThanConstant(
      s, this, weights, vars_.size(), bounds));
  dims_.push_back(dim);
}

void Pack::AddWeightedSumLessOrEqualConstantDimension(
    ItemUsagePerBinEvaluator* weights, const std::vector<int64>& bounds) {
  CHECK(weights != nullptr);
  CHECK_EQ(bounds.size(), bins_);
  Solver* const s = solver();
  Dimension* const dim = s->RevAlloc(new DimensionLessThanConstantCallback2(
      s, this, weights, vars_.size(), bounds));
  dims_.push_back(dim);
}

void Pack::AddWeightedSumEqualVarDimension(const std::vector<int64>& weights,
                                           const std::vector<IntVar*>& loads) {
  CHECK_EQ(weights.size(), vars_.size());
  CHECK_EQ(loads.size(), bins_);
  Solver* const s = solver();
  Dimension* const dim =
      s->RevAlloc(new DimensionWeightedSumEqVar(s, this, weights, loads));
  dims_.push_back(dim);
}

void Pack::AddWeightedSumEqualVarDimension(ItemUsagePerBinEvaluator* weights,
                                           const std::vector<IntVar*>& loads) {
  CHECK(weights != nullptr);
  CHECK_EQ(loads.size(), bins_);
  Solver* const s = solver();
  Dimension* const dim = s->RevAlloc(new DimensionWeightedCallback2SumEqVar(
      s, this, weights, vars_.size(), loads));
  dims_.push_back(dim);
}

void Pack::AddWeightedSumOfAssignedDimension(const std::vector<int64>& weights,
                                             IntVar* const cost_var) {
  CHECK_EQ(weights.size(), vars_.size());
  Solver* const s = solver();
  Dimension* const dim = s->RevAlloc(
      new AssignedWeightedSumDimension(s, this, weights, bins_, cost_var));
  dims_.push_back(dim);
}

void Pack::AddSumVariableWeightsLessOrEqualConstantDimension(
    const std::vector<IntVar*>& usage, const std::vector<int64>& capacity) {
  CHECK_EQ(usage.size(), vars_.size());
  CHECK_EQ(capacity.size(), bins_);
  Solver* const s = solver();
  Dimension* const dim =
      s->RevAlloc(new VariableUsageDimension(s, this, capacity, usage));
  dims_.push_back(dim);
}

void Pack::AddCountUsedBinDimension(IntVar* const count_var) {
  Solver* const s = solver();
  Dimension* const dim = s->RevAlloc(
      new CountUsedBinDimension(s, this, vars_.size(), bins_, count_var));
  dims_.push_back(dim);
}

void Pack::AddCountAssignedItemsDimension(IntVar* const count_var) {
  Solver* const s = solver();
  Dimension* const dim = s->RevAlloc(
      new CountAssignedItemsDimension(s, this, vars_.size(), bins_, count_var));
  dims_.push_back(dim);
}

Pack* Solver::MakePack(const std::vector<IntVar*>& vars, int number_of_bins) {
  return RevAlloc(new Pack(this, vars, number_of_bins));
}
}  // namespace operations_research
