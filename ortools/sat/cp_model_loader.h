// Copyright 2010-2018 Google LLC
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

#ifndef OR_TOOLS_SAT_CP_MODEL_LOADER_H_
#define OR_TOOLS_SAT_CP_MODEL_LOADER_H_

#include <functional>
#include <vector>

#include "ortools/base/integral_types.h"
#include <unordered_set>
#include "ortools/base/int_type.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/intervals.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"

namespace operations_research {
namespace sat {

// Holds the sat::model and the mapping between the proto indices and the
// sat::model ones.
class ModelWithMapping {
 public:
  // Extracts all the used variables in the CpModelProto and creates a
  // sat::Model representation for them. More precisely
  //  - All Boolean variables will be mapped.
  //  - All Interval variables will be mapped.
  //  - All non-Boolean variable will have a corresponding IntegerVariable, and
  //    depending on the options, some or all of the BooleanVariable will also
  //    have an IntegerVariable corresponding to its "integer view".
  //
  // Note(user): We could create IntegerVariable on the fly as they are needed,
  // but that loose the original variable order which might be useful in
  // heuristics later.
  ModelWithMapping(const CpModelProto& model_proto, Model* model);

  // Automatically detect optional variables.
  void DetectOptionalVariables(const CpModelProto& model_proto);

  // Extract the encodings (IntegerVariable <-> Booleans) present in the model.
  // This effectively load some linear constraints of size 1 that will be marked
  // as already loaded.
  void ExtractEncoding(const CpModelProto& model_proto);

  // Returns true if the given CpModelProto variable reference refers to a
  // Boolean varaible. Such variable will always have an associated Literal(),
  // but not always an associated Integer().
  bool IsBoolean(int ref) const {
    CHECK_LT(PositiveRef(ref), booleans_.size());
    return booleans_[PositiveRef(ref)] != kNoBooleanVariable;
  }

  sat::Literal Literal(int ref) const {
    DCHECK(IsBoolean(ref));
    return sat::Literal(booleans_[PositiveRef(ref)], RefIsPositive(ref));
  }

  IntegerVariable Integer(int ref) const {
    const IntegerVariable var = integers_[PositiveRef(ref)];
    DCHECK_NE(var, kNoIntegerVariable);
    return RefIsPositive(ref) ? var : NegationOf(var);
  }

  IntervalVariable Interval(int i) const {
    CHECK_GE(i, 0);
    CHECK_LT(i, intervals_.size());
    CHECK_NE(intervals_[i], kNoIntervalVariable);
    return intervals_[i];
  }

  template <typename List>
  std::vector<IntegerVariable> Integers(const List& list) const {
    std::vector<IntegerVariable> result;
    for (const auto i : list) result.push_back(Integer(i));
    return result;
  }

  template <typename ProtoIndices>
  std::vector<sat::Literal> Literals(const ProtoIndices& indices) const {
    std::vector<sat::Literal> result;
    for (const int i : indices) result.push_back(ModelWithMapping::Literal(i));
    return result;
  }

  template <typename ProtoIndices>
  std::vector<IntervalVariable> Intervals(const ProtoIndices& indices) const {
    std::vector<IntervalVariable> result;
    for (const int i : indices) result.push_back(Interval(i));
    return result;
  }

  // This assumes that all the variable are currently fixed in the underlying
  // solver. Returns their current value.
  std::vector<int64> ExtractFullAssignment() const;

  // Depending on the option, we will load constraints in stages. This is used
  // to detect constraints that are already loaded. For instance the interval
  // constraints and the linear constraint of size 1 (encodings) are usually
  // loaded first.
  bool ConstraintIsAlreadyLoaded(const ConstraintProto* ct) const {
    return gtl::ContainsKey(already_loaded_ct_, ct);
  }

  // Note that both these functions returns positive reference or -1.
  int GetProtoVariableFromBooleanVariable(BooleanVariable var) {
    if (var.value() >= reverse_boolean_map_.size()) return -1;
    return reverse_boolean_map_[var];
  }
  int GetProtoVariableFromIntegerVariable(IntegerVariable var) {
    if (var.value() >= reverse_integer_map_.size()) return -1;
    return reverse_integer_map_[var];
  }

  const std::vector<IntegerVariable>& GetVariableMapping() const {
    return integers_;
  }

  // For logging only, these are not super efficient.
  int NumIntegerVariables() const {
    int result = 0;
    for (const IntegerVariable var : integers_) {
      if (var != kNoIntegerVariable) result++;
    }
    return result;
  }
  int NumBooleanVariables() const {
    int result = 0;
    for (const BooleanVariable var : booleans_) {
      if (var != kNoBooleanVariable) result++;
    }
    return result;
  }

  Model* model() const { return model_; }

  // Shortcuts for the underlying model_ functions.
  template <typename T>
  T Add(std::function<T(Model*)> f) {
    return f(model_);
  }
  template <typename T>
  T Get(std::function<T(const Model&)> f) const {
    return f(*model_);
  }
  template <typename T>
  T* GetOrCreate() {
    return model_->GetOrCreate<T>();
  }
  template <typename T>
  void TakeOwnership(T* t) {
    return model_->TakeOwnership<T>(t);
  }

 private:
  Model* model_;

  // Note that only the variables used by at least one constraint will be
  // created, the other will have a kNo[Integer,Interval,Boolean]VariableValue.
  std::vector<IntegerVariable> integers_;
  std::vector<IntervalVariable> intervals_;
  std::vector<BooleanVariable> booleans_;

  // Recover from a IntervalVariable/BooleanVariable its associated CpModelProto
  // index. The value of -1 is used to indicate that there is no correspondence
  // (i.e. this variable is only used internally).
  gtl::ITIVector<BooleanVariable, int> reverse_boolean_map_;
  gtl::ITIVector<IntegerVariable, int> reverse_integer_map_;

  // Used to return a feasible solution for the unused variables.
  std::vector<int64> lower_bounds_;

  // Set of constraints to ignore because they were already dealt with by
  // ExtractEncoding().
  std::unordered_set<const ConstraintProto*> already_loaded_ct_;
};

// Calls one of the functions below.
// Returns false if we do not know how to load the given cosntraints.
bool LoadConstraint(const ConstraintProto& ct, ModelWithMapping* m);

void LoadBoolOrConstraint(const ConstraintProto& ct, ModelWithMapping* m);
void LoadBoolAndConstraint(const ConstraintProto& ct, ModelWithMapping* m);
void LoadAtMostOneConstraint(const ConstraintProto& ct, ModelWithMapping* m);
void LoadBoolXorConstraint(const ConstraintProto& ct, ModelWithMapping* m);
void LoadLinearConstraint(const ConstraintProto& ct, ModelWithMapping* m);
void LoadAllDiffConstraint(const ConstraintProto& ct, ModelWithMapping* m);
void LoadIntProdConstraint(const ConstraintProto& ct, ModelWithMapping* m);
void LoadIntDivConstraint(const ConstraintProto& ct, ModelWithMapping* m);
void LoadIntMinConstraint(const ConstraintProto& ct, ModelWithMapping* m);
void LoadIntMaxConstraint(const ConstraintProto& ct, ModelWithMapping* m);
void LoadNoOverlapConstraint(const ConstraintProto& ct, ModelWithMapping* m);
void LoadNoOverlap2dConstraint(const ConstraintProto& ct, ModelWithMapping* m);
void LoadCumulativeConstraint(const ConstraintProto& ct, ModelWithMapping* m);
void LoadElementConstraintBounds(const ConstraintProto& ct,
                                 ModelWithMapping* m);
void LoadElementConstraintAC(const ConstraintProto& ct, ModelWithMapping* m);
void LoadElementConstraint(const ConstraintProto& ct, ModelWithMapping* m);
void LoadTableConstraint(const ConstraintProto& ct, ModelWithMapping* m);
void LoadAutomataConstraint(const ConstraintProto& ct, ModelWithMapping* m);
void LoadCircuitConstraint(const ConstraintProto& ct, ModelWithMapping* m);
void LoadRoutesConstraint(const ConstraintProto& ct, ModelWithMapping* m);
void LoadCircuitCoveringConstraint(const ConstraintProto& ct,
                                   ModelWithMapping* m);
void LoadInverseConstraint(const ConstraintProto& ct, ModelWithMapping* m);

}  // namespace sat
}  // namespace operations_research

#endif  // OR_TOOLS_SAT_CP_MODEL_LOADER_H_
