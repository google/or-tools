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

//  Packing constraints

#include <stddef.h>
#include <string.h>
#include <algorithm>
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
  virtual void InitialPropagate(int64 bin_index,
                                const std::vector<int64>& forced,
                                const std::vector<int64>& undecided) = 0;
  virtual void InitialPropagateUnassigned(const std::vector<int64>& assigned,
                                          const std::vector<int64>& unassigned) = 0;
  virtual void EndInitialPropagate() = 0;
  virtual void Propagate(int64 bin_index,
                         const std::vector<int64>& forced,
                         const std::vector<int64>& removed) = 0;
  virtual void PropagateUnassigned(const std::vector<int64>& assigned,
                                   const std::vector<int64>& unassigned) = 0;
  virtual void EndPropagate() = 0;
  virtual string DebugString() const {
    return "Dimension";
  }
  virtual void Accept(ModelVisitor* const visitor) const = 0;

  Solver* solver() const { return solver_; }

  bool IsUndecided(int64 var_index, int64 bin_index) const {
    return pack_->IsUndecided(var_index, bin_index);
  }

  bool IsPossible(int64 var_index, int64 bin_index) const {
    return pack_->IsPossible(var_index, bin_index);
  }

  IntVar* AssignVar(int64 var_index, int64 bin_index) const {
    return pack_->AssignVar(var_index, bin_index);
  }

  void SetImpossible(int64 var_index, int64 bin_index) {
    pack_->SetImpossible(var_index, bin_index);
  }

  void Assign(int64 var_index, int64 bin_index) {
    pack_->Assign(var_index, bin_index);
  }

  bool IsAssignedStatusKnown(int64 var_index) const {
    return pack_->IsAssignedStatusKnown(var_index);
  }

  void SetAssigned(int64 var_index) {
    pack_->SetAssigned(var_index);
  }

  void SetUnassigned(int64 var_index) {
    pack_->SetUnassigned(var_index);
  }

  void RemoveAllPossibleFromBin(int64 bin_index) {
    pack_->RemoveAllPossibleFromBin(bin_index);
  }

  void AssignAllPossibleToBin(int64 bin_index) {
    pack_->AssignAllPossibleToBin(bin_index);
  }

  void AssignFirstPossibleToBin(int64 bin_index) {
    pack_->AssignFirstPossibleToBin(bin_index);
  }

  void AssignAllRemainingItems() {
    pack_->AssignAllRemainingItems();
  }

  void UnassignAllRemainingItems() {
    pack_->UnassignAllRemainingItems();
  }

 private:
  Solver* const solver_;
  Pack* const pack_;
};

// ----- Pack -----

Pack::Pack(Solver* const s,
           const IntVar* const * vars,
           int vsize,
           int64 number_of_bins)
    : Constraint(s),
      vars_(new IntVar*[vsize]),
      vsize_(vsize),
      bins_(number_of_bins),
      unprocessed_(bins_ + 1, vsize_),
      forced_(bins_ + 1),
      removed_(bins_ + 1),
      holes_(new IntVarIterator*[vsize_]),
      stamp_(GG_ULONGLONG(0)),
      demon_(NULL),
      in_process_(false) {
  memcpy(vars_.get(), vars, vsize_ * sizeof(*vars));
  for (int i = 0; i < vsize_; ++i) {
    holes_[i] = vars_[i]->MakeHoleIterator(true);
  }
}

Pack::~Pack() {}

void Pack::Post() {
  for (int i = 0; i < vsize_; ++i) {
    IntVar* const var = vars_[i];
    if (!var->Bound()) {
      Demon* const d = MakeConstraintDemon1(solver(),
                                            this,
                                            &Pack::OneDomain,
                                            "OneDomain",
                                            i);
      var->WhenDomain(d);
    }
  }
  for (int i = 0; i < dims_.size(); ++i) {
    dims_[i]->Post();
  }
  demon_ = solver()->RegisterDemon(MakeDelayedConstraintDemon0(
      solver(),
      this,
      &Pack::Propagate,
      "Propagate"));
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
  void PushAssigned(int64 index) { assigned_.push_back(index); }
  void PushUnassigned(int64 index) { unassigned_.push_back(index); }
  void PushUndecided(int64 bin, int64 index) {
    undecided_.at(bin).push_back(index);
  }
  const std::vector<int64>& undecided(int64 bin) const { return undecided_.at(bin); }
  const std::vector<int64>& assigned() const { return assigned_; }
  const std::vector<int64>& unassigned() const { return unassigned_; }

 private:
  std::vector<std::vector<int64> > undecided_;
  std::vector<int64> unassigned_;
  std::vector<int64> assigned_;
};
}  // namespace

void Pack::InitialPropagate() {
  const bool need_context = solver()->InstrumentsVariables();
  ClearAll();
  Solver* const s = solver();
  in_process_ = true;
  InitialPropagateData* data = s->RevAlloc(new InitialPropagateData(bins_));
  for (int var_index = 0; var_index < vsize_; ++var_index) {
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
            DCHECK_GT(bins_, var->Min());
      if (var->Max() < bins_) {
        data->PushAssigned(var_index);
      }
      scoped_ptr<IntVarIterator> it(var->MakeDomainIterator(false));
      for (it->Init(); it->Ok(); it->Next()) {
        const int64 value = it->Value();
        if (value >= 0 && value <= bins_) {
          unprocessed_.SetToOne(s, value, var_index);
          if (value != bins_) {
            data->PushUndecided(value, var_index);
          }
        }
      }
    }
  }
  for (int bin_index = 0; bin_index < bins_; ++bin_index) {
    if (need_context) {
      solver()->GetPropagationMonitor()->PushContext(
          StringPrintf(
              "Pack(bin %d, forced = [%s], undecided = [%s])",
              bin_index,
              Int64VectorToString(forced_[bin_index], ", ").c_str(),
              Int64VectorToString(data->undecided(bin_index), ", ").c_str()));
    }

    for (int dim_index = 0; dim_index < dims_.size(); ++dim_index) {
      if (need_context) {
        solver()->GetPropagationMonitor()->PushContext(
            StringPrintf("InitialProgateDimension(%s)",
                         dims_[dim_index]->DebugString().c_str()));
      }
      dims_[dim_index]->InitialPropagate(bin_index,
                                         forced_[bin_index],
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
        StringPrintf(
            "Pack(assigned = [%s], unassigned = [%s])",
            Int64VectorToString(data->assigned(), ", ").c_str(),
            Int64VectorToString(data->unassigned(), ", ").c_str()));
  }
  for (int dim_index = 0; dim_index < dims_.size(); ++dim_index) {
    if (need_context) {
      solver()->GetPropagationMonitor()->PushContext(
          StringPrintf("InitialProgateDimension(%s)",
                       dims_[dim_index]->DebugString().c_str()));
    }
    dims_[dim_index]->InitialPropagateUnassigned(data->assigned(),
                                                 data->unassigned());
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
        solver()->GetPropagationMonitor()->PushContext(
            StringPrintf(
                "Pack(bin %d, forced = [%s], removed = [%s])",
                bin_index,
                Int64VectorToString(forced_[bin_index], ", ").c_str(),
                Int64VectorToString(removed_[bin_index], ", ").c_str()));
      }

      for (int dim_index = 0; dim_index < dims_.size(); ++dim_index) {
        if (need_context) {
          solver()->GetPropagationMonitor()->PushContext(
              StringPrintf("ProgateDimension(%s)",
                           dims_[dim_index]->DebugString().c_str()));
        }
        dims_[dim_index]->Propagate(bin_index,
                                    forced_[bin_index],
                                    removed_[bin_index]);
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
          StringPrintf(
              "Pack(removed = [%s], forced = [%s])",
              Int64VectorToString(removed_[bins_], ", ").c_str(),
              Int64VectorToString(forced_[bins_], ", ").c_str()));
    }

    for (int dim_index = 0; dim_index < dims_.size(); ++dim_index) {
      if (need_context) {
        solver()->GetPropagationMonitor()->PushContext(
            StringPrintf("ProgateDimension(%s)",
                         dims_[dim_index]->DebugString().c_str()));
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
  for (int64 value = std::max(oldmin, 0LL);
       value < std::min(vmin, bins_ + 1);
       ++value) {
    if (unprocessed_.IsSet(value, var_index)) {
      unprocessed_.SetToZero(s, value, var_index);
      removed_[value].push_back(var_index);
    }
  }
  if (!bound) {
    IntVarIterator* const holes = holes_[var_index];
    for (holes->Init(); holes->Ok(); holes->Next()) {
      const int64 value = holes->Value();
      if (value >= std::max(0LL, vmin) &&  value <= std::min(bins_, vmax)) {
        DCHECK(unprocessed_.IsSet(value, var_index));
        unprocessed_.SetToZero(s, value, var_index);
        removed_[value].push_back(var_index);
      }
    }
  }
  for (int64 value = std::max(vmax + 1, 0LL);
       value <= std::min(oldmax, bins_);
       ++value) {
    if (unprocessed_.IsSet(value, var_index)) {
      unprocessed_.SetToZero(s, value, var_index);
      removed_[value].push_back(var_index);
    }
  }
  if (bound) {
    unprocessed_.SetToZero(s, var->Min(), var_index);
    forced_[var->Min()].push_back(var_index);
  }
  Enqueue(demon_);
}

string Pack::DebugString() const {
  string result = "Pack([";
  for (int i = 0; i < vsize_; ++i) {
    result +=  vars_[i]->DebugString() + " ";
  }
  result += "], dimensions = [";
  for (int i = 0; i < dims_.size(); ++i) {
    result +=  dims_[i]->DebugString() + " ";
  }
  StringAppendF(&result, "], bins = %" GG_LL_FORMAT "d)", bins_);
  return result;
}

void Pack::Accept(ModelVisitor* const visitor) const {
  visitor->BeginVisitConstraint(ModelVisitor::kPack, this);
  visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                             vars_.get(),
                                             vsize_);
  visitor->VisitIntegerArgument(ModelVisitor::kSizeArgument, bins_);
  for (int i = 0; i < dims_.size(); ++i) {
    dims_[i]->Accept(visitor);
  }
  visitor->EndVisitConstraint(ModelVisitor::kPack, this);
}

void Pack::SetImpossible(int64 var_index, int64 bin_index) {
  if (IsInProcess()) {
    to_unset_.push_back(std::make_pair(var_index, bin_index));
  } else {
    vars_[var_index]->RemoveValue(bin_index);
  }
}

void Pack::Assign(int64 var_index, int64 bin_index) {
  if (IsInProcess()) {
    to_set_.push_back(std::make_pair(var_index, bin_index));
  } else {
    vars_[var_index]->SetValue(bin_index);
  }
}

bool Pack::IsAssignedStatusKnown(int64 var_index) const {
  return !unprocessed_.IsSet(bins_, var_index);
}

bool Pack::IsPossible(int64 var_index, int64 bin_index) const {
  return vars_[var_index]->Contains(bin_index);
}

IntVar* Pack::AssignVar(int64 var_index, int64 bin_index) const {
  return solver()->MakeIsEqualCstVar(vars_[var_index], bin_index);
}

void Pack::SetAssigned(int64 var_index) {
  if (IsInProcess()) {
    to_unset_.push_back(std::make_pair(var_index, bins_));
  } else {
    vars_[var_index]->RemoveValue(bins_);
  }
}

void Pack::SetUnassigned(int64 var_index) {
  if (IsInProcess()) {
    to_set_.push_back(std::make_pair(var_index, bins_));
  } else {
    vars_[var_index]->SetValue(bins_);
  }
}

bool Pack::IsInProcess() const {
  return in_process_ && (solver()->fail_stamp() == stamp_);
}

void Pack::RemoveAllPossibleFromBin(int64 bin_index) {
  int var_index = unprocessed_.GetFirstBit(bin_index, 0);
  while (var_index != -1 && var_index < vsize_) {
    SetImpossible(var_index, bin_index);
    var_index = var_index == vsize_ - 1 ?
        -1 :
        unprocessed_.GetFirstBit(bin_index, var_index + 1);
  }
}

void Pack::AssignAllPossibleToBin(int64 bin_index) {
  int var_index = unprocessed_.GetFirstBit(bin_index, 0);
  while (var_index != -1 && var_index < vsize_) {
    Assign(var_index, bin_index);
    var_index = var_index == vsize_ - 1 ?
        -1 :
        unprocessed_.GetFirstBit(bin_index, var_index + 1);
  }
}

void Pack::AssignFirstPossibleToBin(int64 bin_index) {
  int var_index = unprocessed_.GetFirstBit(bin_index, 0);
  if (var_index != -1 && var_index < vsize_) {
    Assign(var_index, bin_index);
  }
}

void Pack::AssignAllRemainingItems() {
  int var_index = unprocessed_.GetFirstBit(bins_, 0);
  while (var_index != -1 && var_index < vsize_) {
    SetAssigned(var_index);
    var_index = var_index == vsize_ - 1 ?
        -1 :
        unprocessed_.GetFirstBit(bins_, var_index + 1);
  }
}

void Pack::UnassignAllRemainingItems() {
  int var_index = unprocessed_.GetFirstBit(bins_, 0);
  while (var_index != -1 && var_index < vsize_) {
    SetUnassigned(var_index);
    var_index = var_index == vsize_ - 1 ?
        -1 :
        unprocessed_.GetFirstBit(bins_, var_index + 1);
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

int SortIndexByWeight(int64* const indices,
                      const int64* const weights,
                      int size) {
  std::vector<WeightContainer> to_sort;
  for (int index = 0; index < size; ++index) {
    if (weights[index] != 0) {
      to_sort.push_back(WeightContainer(indices[index], weights[index]));
    }
  }
  std::sort(to_sort.begin(), to_sort.end());
  for (int index = 0; index < to_sort.size(); ++index) {
    indices[index] = to_sort[index].index;
  }
  for (int index = to_sort.size(); index < size; ++index) {
    indices[index] = -1;
  }
  return to_sort.size();
}

class DimensionLessThanConstant : public Dimension {
 public:
  DimensionLessThanConstant(Solver* const s,
                            Pack* const p,
                            const int64* const weights,
                            int vars_count,
                            const int64* const upper_bounds,
                            int bins_count)
      : Dimension(s, p),
        vars_count_(vars_count),
        weights_(new int64[vars_count_]),
        bins_count_(bins_count),
        upper_bounds_(new int64[bins_count_]),
        first_unbound_backward_vector_(bins_count, Rev<int>(0)),
        sum_of_bound_variables_vector_(bins_count, Rev<int64>(0LL)),
        ranked_(new int64[vars_count_]),
        ranked_size_(vars_count_) {
    DCHECK(weights);
    DCHECK(upper_bounds);
    DCHECK_GT(vars_count, 0);
    DCHECK_GT(bins_count, 0);
    memcpy(weights_, weights, vars_count * sizeof(*weights));
    memcpy(upper_bounds_, upper_bounds, bins_count * sizeof(*upper_bounds));
    for (int i = 0; i < vars_count_; ++i) {
      ranked_[i] = i;
    }
    ranked_size_ = SortIndexByWeight(ranked_, weights_, vars_count_);
  }

  virtual ~DimensionLessThanConstant() {
    delete [] weights_;
    delete [] upper_bounds_;
    delete [] ranked_;
  }

  virtual void Post() {}

  void PushFromTop(int64 bin_index) {
    const int64 slack =
        upper_bounds_[bin_index] -
        sum_of_bound_variables_vector_[bin_index].Value();
    if (slack < 0) {
      solver()->Fail();
    }
    int64 last_unbound = first_unbound_backward_vector_[bin_index].Value();
    for (; last_unbound >= 0; --last_unbound) {
      const int64 var_index = ranked_[last_unbound];
      if (IsUndecided(var_index, bin_index)) {
        if (weights_[var_index] > slack) {
          SetImpossible(var_index, bin_index);
        } else {
          break;
        }
      }
    }
    first_unbound_backward_vector_[bin_index].SetValue(solver(), last_unbound);
  }

  virtual void InitialPropagate(int64 bin_index,
                                const std::vector<int64>& forced,
                                const std::vector<int64>& undecided) {
    Solver* const s = solver();
    int64 sum = 0LL;
    for (ConstIter<std::vector<int64> > it(forced); !it.at_end(); ++it) {
      sum += weights_[*it];
    }
    sum_of_bound_variables_vector_[bin_index].SetValue(s, sum);
    first_unbound_backward_vector_[bin_index].SetValue(s, ranked_size_ - 1);
    PushFromTop(bin_index);
  }

  virtual void EndInitialPropagate() {}

  virtual void Propagate(int64 bin_index,
                         const std::vector<int64>& forced,
                         const std::vector<int64>& removed) {
    if (forced.size() > 0) {
      Solver* const s = solver();
      int64 sum = sum_of_bound_variables_vector_[bin_index].Value();
      for (ConstIter<std::vector<int64> > it(forced); !it.at_end(); ++it) {
        sum += weights_[*it];
      }
      sum_of_bound_variables_vector_[bin_index].SetValue(s, sum);
      PushFromTop(bin_index);
    }
  }
  virtual void InitialPropagateUnassigned(const std::vector<int64>& assigned,
                                          const std::vector<int64>& unassigned) {}
  virtual void PropagateUnassigned(const std::vector<int64>& assigned,
                                   const std::vector<int64>& unassigned) {}

  virtual void EndPropagate() {}

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitExtension(ModelVisitor::kUsageLessConstantExtension);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kCoefficientsArgument,
                                       weights_,
                                       vars_count_);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument,
                                       upper_bounds_,
                                       bins_count_);
    visitor->EndVisitExtension(ModelVisitor::kUsageLessConstantExtension);
  }

 private:
  const int vars_count_;
  int64* const weights_;
  const int bins_count_;
  int64* const upper_bounds_;
  std::vector<Rev<int> > first_unbound_backward_vector_;
  std::vector<Rev<int64> > sum_of_bound_variables_vector_;
  int64* const ranked_;
  int ranked_size_;
};

class DimensionWeightedSumEqVar : public Dimension {
 public:
  class VarDemon : public Demon {
   public:
    VarDemon(DimensionWeightedSumEqVar* const dim, int index)
        : dim_(dim), index_(index) {}
    virtual ~VarDemon() {}

    virtual void Run(Solver* const s) {
      dim_->PushFromTop(index_);
    }

   private:
    DimensionWeightedSumEqVar* const dim_;
    const int index_;
  };

  DimensionWeightedSumEqVar(Solver* const s,
                            Pack* const p,
                            const int64* const weights,
                            int vars_count,
                            IntVar* const* loads,
                            int bins_count)
      : Dimension(s, p),
        vars_count_(vars_count),
        weights_(new int64[vars_count_]),
        bins_count_(bins_count),
        loads_(new IntVar*[bins_count_]),
        first_unbound_backward_vector_(bins_count, Rev<int>(0)),
        sum_of_bound_variables_vector_(bins_count, Rev<int64>(0LL)),
        sum_of_all_variables_vector_(bins_count, Rev<int64>(0LL)),
        ranked_(new int64[vars_count_]),
        ranked_size_(vars_count_) {
    DCHECK(weights);
    DCHECK(loads);
    DCHECK_GT(vars_count, 0);
    DCHECK_GT(bins_count, 0);
    memcpy(weights_, weights, vars_count * sizeof(*weights));
    memcpy(loads_, loads, bins_count * sizeof(*loads));
    for (int i = 0; i < vars_count_; ++i) {
      ranked_[i] = i;
    }
    ranked_size_ = SortIndexByWeight(ranked_, weights_, vars_count_);
  }

  virtual ~DimensionWeightedSumEqVar() {
    delete [] weights_;
    delete [] loads_;
    delete [] ranked_;
  }

  virtual string DebugString() const {
    return "DimensionWeightedSumEqVar";
  }

  virtual void Post() {
    for (int i = 0; i < bins_count_; ++i) {
      Demon* const d = solver()->RevAlloc(new VarDemon(this, i));
      loads_[i]->WhenRange(d);
    }
  }

  void PushFromTop(int64 bin_index) {
    IntVar* const load = loads_[bin_index];
    const int64 sum_min = sum_of_bound_variables_vector_[bin_index].Value();
    const int64 sum_max = sum_of_all_variables_vector_[bin_index].Value();
    load->SetRange(sum_min, sum_max);
    const int64 slack_up = load->Max() - sum_min;
    const int64 slack_down = sum_max - load->Min();
    DCHECK_GE(slack_down, 0);
    DCHECK_GE(slack_up, 0);
    int64 last_unbound = first_unbound_backward_vector_[bin_index].Value();
    for (; last_unbound >= 0; --last_unbound) {
      const int64 var_index = ranked_[last_unbound];
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
    first_unbound_backward_vector_[bin_index].SetValue(solver(), last_unbound);
  }

  virtual void InitialPropagate(int64 bin_index,
                                const std::vector<int64>& forced,
                                const std::vector<int64>& undecided) {
    Solver* const s = solver();
    int64 sum = 0LL;
    for (ConstIter<std::vector<int64> > it(forced); !it.at_end(); ++it) {
      sum += weights_[*it];
    }
    sum_of_bound_variables_vector_[bin_index].SetValue(s, sum);
    for (ConstIter<std::vector<int64> > it(undecided); !it.at_end(); ++it) {
      sum += weights_[*it];
    }
    sum_of_all_variables_vector_[bin_index].SetValue(s, sum);
    first_unbound_backward_vector_[bin_index].SetValue(s, ranked_size_ - 1);
    PushFromTop(bin_index);
  }

  virtual void EndInitialPropagate() {}

  virtual void Propagate(int64 bin_index,
                         const std::vector<int64>& forced,
                         const std::vector<int64>& removed) {
    Solver* const s = solver();
    int64 down = sum_of_bound_variables_vector_[bin_index].Value();
    for (ConstIter<std::vector<int64> > it(forced); !it.at_end(); ++it) {
      down += weights_[*it];
    }
    sum_of_bound_variables_vector_[bin_index].SetValue(s, down);
    int64 up = sum_of_all_variables_vector_[bin_index].Value();
    for (ConstIter<std::vector<int64> > it(removed); !it.at_end(); ++it) {
      up -= weights_[*it];
    }
    sum_of_all_variables_vector_[bin_index].SetValue(s, up);
    PushFromTop(bin_index);
  }
  virtual void InitialPropagateUnassigned(const std::vector<int64>& assigned,
                                          const std::vector<int64>& unassigned) {}
  virtual void PropagateUnassigned(const std::vector<int64>& assigned,
                                   const std::vector<int64>& unassigned) {}

  virtual void EndPropagate() {}

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitExtension(ModelVisitor::kUsageEqualVariableExtension);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kCoefficientsArgument,
                                       weights_,
                                       vars_count_);
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               loads_,
                                               bins_count_);
    visitor->EndVisitExtension(ModelVisitor::kUsageEqualVariableExtension);
  }

 private:
  const int vars_count_;
  int64* const weights_;
  const int bins_count_;
  IntVar** const loads_;
  std::vector<Rev<int> > first_unbound_backward_vector_;
  std::vector<Rev<int64> > sum_of_bound_variables_vector_;
  std::vector<Rev<int64> > sum_of_all_variables_vector_;
  int64* const ranked_;
  int ranked_size_;
};

class AssignedWeightedSumDimension : public Dimension {
 public:
  class VarDemon : public Demon {
   public:
    explicit VarDemon(AssignedWeightedSumDimension* const dim) : dim_(dim) {}
    virtual ~VarDemon() {}

    virtual void Run(Solver* const s) {
      dim_->PropagateAll();
    }

   private:
    AssignedWeightedSumDimension* const dim_;
  };

  AssignedWeightedSumDimension(Solver* const s,
                               Pack* const p,
                               const int64* const weights,
                               int vars_count,
                               int bins_count,
                               IntVar* const cost_var)
      : Dimension(s, p),
        vars_count_(vars_count),
        weights_(new int64[vars_count_]),
        bins_count_(bins_count),
        cost_var_(cost_var),
        first_unbound_backward_(0),
        sum_of_assigned_items_(0LL),
        sum_of_unassigned_items_(0LL),
        ranked_(new int64[vars_count_]),
        ranked_size_(vars_count_),
        sum_all_weights_(0LL) {
    DCHECK(weights);
    DCHECK(cost_var);
    DCHECK_GT(vars_count, 0);
    DCHECK_GT(bins_count, 0);
    memcpy(weights_, weights, vars_count * sizeof(*weights));
    for (int i = 0; i < vars_count_; ++i) {
      ranked_[i] = i;
    }
    ranked_size_ = SortIndexByWeight(ranked_, weights_, vars_count_);
    first_unbound_backward_.SetValue(s, ranked_size_ - 1);
  }

  virtual ~AssignedWeightedSumDimension() {
    delete [] weights_;
    delete [] ranked_;
  }

  virtual void Post() {
    Demon* const uv = solver()->RevAlloc(new VarDemon(this));
    cost_var_->WhenRange(uv);
  }

  void PropagateAll() {
    cost_var_->SetRange(sum_of_assigned_items_.Value(),
                        sum_all_weights_ - sum_of_unassigned_items_.Value());
    const int64 slack_up = cost_var_->Max() - sum_of_assigned_items_.Value();
    const int64 slack_down = sum_all_weights_ - cost_var_->Min();
    int64 last_unbound = first_unbound_backward_.Value();
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

  virtual void InitialPropagate(int64 bin_index,
                                const std::vector<int64>& forced,
                                const std::vector<int64>& undecided) {}

  virtual void EndInitialPropagate() {}


  virtual void InitialPropagateUnassigned(const std::vector<int64>& assigned,
                                          const std::vector<int64>& unassigned) {
    for (int index = 0; index < vars_count_; ++index) {
      sum_all_weights_ += weights_[index];
    }

    PropagateUnassigned(assigned, unassigned);
  }

  virtual void Propagate(int64 bin_index,
                         const std::vector<int64>& forced,
                         const std::vector<int64>& removed) {}

  virtual void PropagateUnassigned(const std::vector<int64>& assigned,
                                   const std::vector<int64>& unassigned) {
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
                                       weights_,
                                       vars_count_);
    visitor->VisitIntegerExpressionArgument(ModelVisitor::kTargetArgument,
                                            cost_var_);
    visitor->EndVisitExtension(
        ModelVisitor::kWeightedSumOfAssignedEqualVariableExtension);
  }

 private:
  const int vars_count_;
  int64* const weights_;
  const int bins_count_;
  IntVar* const cost_var_;
  Rev<int> first_unbound_backward_;
  Rev<int64> sum_of_assigned_items_;
  Rev<int64> sum_of_unassigned_items_;
  int64* const ranked_;
  int ranked_size_;
  int64 sum_all_weights_;
};

// ----- Count unassigned jobs dimension -----

class CountAssignedItemsDimension : public Dimension {
 public:
  class VarDemon : public Demon {
   public:
    explicit VarDemon(CountAssignedItemsDimension* const dim) : dim_(dim) {}
    virtual ~VarDemon() {}

    virtual void Run(Solver* const s) {
      dim_->PropagateAll();
    }

   private:
    CountAssignedItemsDimension* const dim_;
  };

  CountAssignedItemsDimension(Solver* const s,
                              Pack* const p,
                              int vars_count,
                              int bins_count,
                              IntVar* const cost_var)
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
                        vars_count_- unassigned_count_.Value());
    if (assigned_count_.Value() == cost_var_->Max()) {
      UnassignAllRemainingItems();
    } else if (cost_var_->Min() == vars_count_ - unassigned_count_.Value()) {
      AssignAllRemainingItems();
    }
  }

  virtual void InitialPropagate(int64 bin_index,
                                const std::vector<int64>& forced,
                                const std::vector<int64>& undecided) {}

  virtual void EndInitialPropagate() {}

  virtual void InitialPropagateUnassigned(const std::vector<int64>& assigned,
                                          const std::vector<int64>& unassigned) {
    PropagateUnassigned(assigned, unassigned);
  }

  virtual void Propagate(int64 bin_index,
                         const std::vector<int64>& forced,
                         const std::vector<int64>& removed) {}

  virtual void PropagateUnassigned(const std::vector<int64>& assigned,
                                   const std::vector<int64>& unassigned) {
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

    virtual void Run(Solver* const s) {
      dim_->PropagateAll();
    }

   private:
    CountUsedBinDimension* const dim_;
  };

  CountUsedBinDimension(Solver* const s,
                        Pack* const p,
                        int vars_count,
                        int bins_count,
                        IntVar* const count_var)
      : Dimension(s, p),
        vars_count_(vars_count),
        bins_count_(bins_count),
        count_var_(count_var),
        used_(new bool[bins_count_]),
        candidates_(new int[bins_count_]),
        card_min_(0),
        card_max_(bins_count_),
        initial_min_(0),
        initial_max_(0) {
    DCHECK(count_var);
    DCHECK_GT(vars_count, 0);
    DCHECK_GT(bins_count, 0);
    for (int i = 0; i < bins_count; ++i) {
      used_[i] = false;
      candidates_[i] = 0;
    }
  }

  virtual ~CountUsedBinDimension() {
    delete [] used_;
    delete [] candidates_;
  }

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
        if (!used_[bin_index] && candidates_[bin_index] > 0) {
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

  virtual void InitialPropagate(int64 bin_index,
                                const std::vector<int64>& forced,
                                const std::vector<int64>& undecided) {
    if (forced.size() > 0) {
      solver()->SaveAndSetValue(&used_[bin_index], true);
      initial_min_++;
    } else if (undecided.size() > 0) {
      solver()->SaveValue(&candidates_[bin_index]);
      candidates_[bin_index] = undecided.size();
    } else {
      initial_max_--;
    }
  }

  virtual void EndInitialPropagate() {
    card_min_.SetValue(solver(), initial_min_);
    card_max_.SetValue(solver(), initial_max_);
    PropagateAll();
  }

  virtual void InitialPropagateUnassigned(const std::vector<int64>& assigned,
                                          const std::vector<int64>& unassigned) {}

  virtual void Propagate(int64 bin_index,
                         const std::vector<int64>& forced,
                         const std::vector<int64>& removed) {
    if (!used_[bin_index]) {
      if (forced.size() > 0) {
        solver()->SaveValue(&used_[bin_index]);
        used_[bin_index] = true;
        card_min_.SetValue(solver(), card_min_.Value() + 1);
      } else if (removed.size() > 0) {
        solver()->SaveValue(&candidates_[bin_index]);
        candidates_[bin_index] -= removed.size();
        if (candidates_[bin_index] == 0) {
          card_max_.SetValue(solver(), card_max_.Value() - 1);
        }
      }
    }
  }

  virtual void PropagateUnassigned(const std::vector<int64>& assigned,
                                   const std::vector<int64>& unassigned) {
  }

  virtual void EndPropagate() {
    PropagateAll();
  }

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
  bool* const used_;
  int* const candidates_;
  Rev<int> card_min_;
  Rev<int> card_max_;
  int initial_min_;
  int initial_max_;
};

// ---------- Variable Usage Dimension ----------

// This is a very naive, but correct implementation of the constraint.
class VariableUsageDimension : public Dimension {
 public:
  VariableUsageDimension(Solver* const solver,
                         Pack* const pack,
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

  virtual void InitialPropagate(int64 bin_index,
                                const std::vector<int64>& forced,
                                const std::vector<int64>& undecided) {}
  virtual void InitialPropagateUnassigned(const std::vector<int64>& assigned,
                                          const std::vector<int64>& unassigned) {}
  virtual void EndInitialPropagate() {}
  virtual void Propagate(int64 bin_index,
                         const std::vector<int64>& forced,
                         const std::vector<int64>& removed) {}
  virtual void PropagateUnassigned(const std::vector<int64>& assigned,
                                   const std::vector<int64>& unassigned) {}
  virtual void EndPropagate() {}

  virtual string DebugString() const {
    return "VariableUsageDimension";
  }

  virtual void Accept(ModelVisitor* const visitor) const {
    visitor->BeginVisitExtension(
        ModelVisitor::kVariableUsageLessConstantExtension);
    visitor->VisitIntegerArrayArgument(ModelVisitor::kValuesArgument,
                                       capacities_.data(),
                                       capacities_.size());
    visitor->VisitIntegerVariableArrayArgument(ModelVisitor::kVarsArgument,
                                               weights_.data(),
                                               weights_.size());
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
    const std::vector<int64>& weights,
    const std::vector<int64>& bounds) {
  CHECK_EQ(weights.size(), vsize_);
  CHECK_EQ(bounds.size(), bins_);
  Solver* const s = solver();
  Dimension* const dim =
      s->RevAlloc(new DimensionLessThanConstant(s,
                                                this,
                                                weights.data(),
                                                weights.size(),
                                                bounds.data(),
                                                bounds.size()));
  dims_.push_back(dim);
}

void Pack::AddWeightedSumEqualVarDimension(const std::vector<int64>& weights,
                                           const std::vector<IntVar*>& loads) {
  CHECK_EQ(weights.size(), vsize_);
  CHECK_EQ(loads.size(), bins_);
  Solver* const s = solver();
  Dimension* const dim =
      s->RevAlloc(new DimensionWeightedSumEqVar(s,
                                                this,
                                                weights.data(),
                                                weights.size(),
                                                loads.data(),
                                                loads.size()));
  dims_.push_back(dim);
}

void Pack::AddWeightedSumOfAssignedDimension(const std::vector<int64>& weights,
                                             IntVar* const cost_var) {
  CHECK_EQ(weights.size(), vsize_);
  Solver* const s = solver();
  Dimension* const dim =
      s->RevAlloc(new AssignedWeightedSumDimension(s,
                                                   this,
                                                   weights.data(),
                                                   weights.size(),
                                                   bins_,
                                                   cost_var));
  dims_.push_back(dim);
}

void Pack::AddSumVariableWeightsLessOrEqualConstantDimension(
    const std::vector<IntVar*>& usage,
    const std::vector<int64>& capacity) {
  CHECK_EQ(usage.size(), vsize_);
  CHECK_EQ(capacity.size(), bins_);
  Solver* const s = solver();
  Dimension* const dim =
      s->RevAlloc(new VariableUsageDimension(s,
                                             this,
                                             capacity,
                                             usage));
  dims_.push_back(dim);
}


void Pack::AddCountUsedBinDimension(IntVar* const count_var) {
  Solver* const s = solver();
  Dimension* const dim =
      s->RevAlloc(new CountUsedBinDimension(s, this, vsize_, bins_, count_var));
  dims_.push_back(dim);
}

void Pack::AddCountAssignedItemsDimension(IntVar* const count_var) {
  Solver* const s = solver();
  Dimension* const dim =
      s->RevAlloc(new CountAssignedItemsDimension(s, this, vsize_,
                                                  bins_, count_var));
  dims_.push_back(dim);
}

Pack* Solver::MakePack(const std::vector<IntVar*>& vars, int number_of_bins) {
  return RevAlloc(new Pack(this, vars.data(), vars.size(), number_of_bins));
}
}  // Namespace operations_research
