// Copyright 2010-2021 Google LLC
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

// An object oriented wrapper for the linear objective in IndexedModel.
#ifndef OR_TOOLS_MATH_OPT_CPP_OBJECTIVE_H_
#define OR_TOOLS_MATH_OPT_CPP_OBJECTIVE_H_

#include "ortools/base/logging.h"
#include "ortools/math_opt/core/indexed_model.h"
#include "ortools/math_opt/cpp/key_types.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"

namespace operations_research {
namespace math_opt {

// The objective of an optimization problem for an IndexedModel.
//
// Objective is a value type and is typically passed by copy.
class Objective {
 public:
  inline explicit Objective(IndexedModel* model);

  inline IndexedModel* model() const;

  // Setting a value to 0.0 will delete the variable from the underlying sparse
  // representation (and has no effect if the variable is not present).
  inline void set_linear_coefficient(Variable variable, double value) const;

  inline bool is_linear_coefficient_nonzero(Variable variable) const;

  // Returns 0.0 if this variable has no linear objective coefficient.
  inline double linear_coefficient(Variable variable) const;

  inline void set_offset(double value) const;
  inline double offset() const;

  // Equivalent to calling set_linear_coefficient(v, 0.0) for every variable
  // with nonzero objective coefficient.
  //
  // Runs in O(#variables with nonzero objective coefficient).
  inline void clear() const;
  inline bool is_maximize() const;
  inline void set_maximize() const;
  inline void set_minimize() const;

  // Prefer set_maximize() and set_minimize() above for more readable code.
  inline void set_is_maximize(bool is_maximize) const;

  void Maximize(const LinearExpression& objective) const;
  void Minimize(const LinearExpression& objective) const;
  void SetObjective(const LinearExpression& objective, bool is_maximize) const;
  void Add(const LinearExpression& objective_terms) const;

  LinearExpression AsLinearExpression() const;

 private:
  IndexedModel* model_;
};

////////////////////////////////////////////////////////////////////////////////
// Inline function implementations
////////////////////////////////////////////////////////////////////////////////

Objective::Objective(IndexedModel* const model) : model_(model) {}

IndexedModel* Objective::model() const { return model_; }

void Objective::set_linear_coefficient(const Variable variable,
                                       const double value) const {
  CHECK_EQ(variable.model(), model_) << internal::kObjectsFromOtherIndexedModel;
  model_->set_linear_objective_coefficient(variable.typed_id(), value);
}
bool Objective::is_linear_coefficient_nonzero(const Variable variable) const {
  CHECK_EQ(variable.model(), model_) << internal::kObjectsFromOtherIndexedModel;
  return model_->is_linear_objective_coefficient_nonzero(variable.typed_id());
}
double Objective::linear_coefficient(const Variable variable) const {
  CHECK_EQ(variable.model(), model_) << internal::kObjectsFromOtherIndexedModel;
  return model_->linear_objective_coefficient(variable.typed_id());
}

void Objective::set_offset(const double value) const {
  model_->set_objective_offset(value);
}
double Objective::offset() const { return model_->objective_offset(); }

void Objective::clear() const { model_->clear_objective(); }
bool Objective::is_maximize() const { return model_->is_maximize(); }
void Objective::set_is_maximize(const bool is_maximize) const {
  model_->set_is_maximize(is_maximize);
}
void Objective::set_maximize() const { model_->set_maximize(); }
void Objective::set_minimize() const { model_->set_minimize(); }

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_CPP_OBJECTIVE_H_
