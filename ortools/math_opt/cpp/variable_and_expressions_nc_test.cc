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

// This is a non-compilation test: go/cc_clang_verify_test.

#include "ortools/math_opt/cpp/math_opt.h"
#include "ortools/math_opt/cpp/variable_and_expressions.h"

namespace operations_research::math_opt {
namespace {

// This is kind of obvious, but here as a baseline.
void CannotMutateTermsThroughConstReference() {
  ModelStorage storage;
  LinearExpression expr;
  for (const auto& [var, coeff] : expr.terms()) {
    // expected-error-re@+1 {{{{.*}}cannot assign to variable 'coeff'{{.*}}}}
    coeff += 1.0;
    // expected-error-re@+1 {{{{.*}}no viable overloaded '='{{.*}}}}
    var = Variable(&storage, storage.AddVariable("foo"));
  }
  for (const auto& term : expr.terms()) {
    // expected-error-re@+1 {{{{.*}}cannot assign to variable 'term'{{.*}}}}
    term.second += 1.0;
    // And of course, one can't mutate the key.
    // expected-error-re@+1 {{{{.*}}no viable overloaded '='{{.*}}}}
    term.first = Variable(&storage, storage.AddVariable("foo"));
  }
  for (const auto [var, coeff] : expr.terms()) {
    // expected-error-re@+1 {{{{.*}}cannot assign to variable 'coeff'{{.*}}}}
    coeff += 1.0;
    // expected-error-re@+1 {{{{.*}}no viable overloaded '='{{.*}}}}
    var = Variable(&storage, storage.AddVariable("foo"));
  }
  for (const auto term : expr.terms()) {
    // expected-error-re@+1 {{{{.*}}cannot assign to variable 'term'{{.*}}}}
    term.second += 1.0;
    // And of course, one can't mutate the key.
    // expected-error-re@+1 {{{{.*}}no viable overloaded '='{{.*}}}}
    term.first = Variable(&storage, storage.AddVariable("foo"));
  }
}

// People might think that they can mutate the coefficients through a non-const
// reference. This is not supported.
void CannotMutateTermsThroughReference() {
  ModelStorage storage;
  LinearExpression expr;
  for (auto& term : expr.terms()) {
    // expected-error-re@+1 {{{{.*}}cannot assign to variable 'term'{{.*}}}}
    term.second += 1.0;
    // And of course, one can't mutate the key.
    // expected-error-re@+1 {{{{.*}}no viable overloaded '='{{.*}}}}
    term.first = Variable(&storage, storage.AddVariable("foo"));
  }
}
void CannotMutateTermsThroughReferenceBinding() {
  ModelStorage storage;
  LinearExpression expr;
  for (auto& [var, coeff] : expr.terms()) {
    // expected-error-re@+1 {{{{.*}}cannot assign to variable 'coeff'{{.*}}}}
    coeff += 1.0;
    // And of course, one can't mutate the key.
    // expected-error-re@+1 {{{{.*}}no viable overloaded '='{{.*}}}}
    var = Variable(&storage, storage.AddVariable("foo"));
  }
}

// People might also think that the keys are proxy object that allow mutating
// the coefficients through a non-const value. This is not supported either.
void CannotMutateTermsThroughValue() {
  ModelStorage storage;
  LinearExpression expr;
  for (auto term : expr.terms()) {
    term.second += 1.0;
    // expected-error-re@+1 {{{{.*}}no viable overloaded '='{{.*}}}}
    term.first = Variable(&storage, storage.AddVariable("foo"));
  }
}
void CannotMutateTermsThroughValueBinding() {
  ModelStorage storage;
  LinearExpression expr;
  for (auto [var, coeff] : expr.terms()) {
    coeff += 1.0;
    // expected-error-re@+1 {{{{.*}}no viable overloaded '='{{.*}}}}
    var = Variable(&storage, storage.AddVariable("foo"));
  }
}

}  // namespace
}  // namespace operations_research::math_opt
