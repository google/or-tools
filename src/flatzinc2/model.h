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
#ifndef OR_TOOLS_FLATZINC_MODEL_H_
#define OR_TOOLS_FLATZINC_MODEL_H_

#include <iostream>  // NOLINT
#include <string>
#include "base/hash.h"
#include "base/commandlineflags.h"
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/hash.h"
#include "util/iterators.h"
#include "util/string_array.h"

namespace operations_research {
class FzConstraint;
class FzModel;
class FzIntegerVariable;
#define FZLOG \
  if (FLAGS_logging) std::cout << "%% "

#define FZVLOG \
  if (FLAGS_verbose_logging) std::cout << "%% "

// A domain represents the possible values of a variable.
// It can be:
//  - an explicit list of all possible values, in which case is_interval is
//    false.
//  - an interval, in which case is_interval is true and values.size() == 2,
//    and the interval is [values[0], values[1]].
//  - all integers, in which case values is empty, and is_interval is true.
// Note that semi-infinite intervals aren't supported.
struct FzDomain {
  static FzDomain IntegerList(const std::vector<int64>& values);
  static FzDomain AllInt64();
  static FzDomain Singleton(int64 value);
  static FzDomain Interval(int64 included_min, int64 included_max);

  bool IsSingleton() const;
  void IntersectWith(const FzDomain& domain);
  void ReduceDomain(int64 interval_min, int64 interval_max);
  void ReduceDomain(const std::vector<int64>& values);
  bool Contains(int64 value) const;
  // Returns true if the value is removed.
  bool RemoveValue(int64 value);
  std::string DebugString() const;

  bool is_interval;
  std::vector<int64> values;
};

// An int var is a name with a domain of possible values, along with
// some tags. Typically, a FzIntegerVariable is on the heap, and owned by the
// global FzModel object.
struct FzIntegerVariable {
  static FzIntegerVariable* Constant(int64 value) {
    return new FzIntegerVariable(StringPrintf("%" GG_LL_FORMAT "d", value),
                                 FzDomain::Singleton(value), true);
  }

  // This method tries to unify two variables. This can happen during the
  // parsing of the model or during presolve. This is possible is at least one
  // of the two variable is not the target of a constraint. (otherwise it
  // returns false).
  // The semantic of the merge is the following:
  //   - the resulting domain is the intersection of the two domains.
  //   - if one variable is not temporary, the result is not temporary.
  //   - if one (and exactly one) variable is a target var, the result is a
  //     target var.
  //   - if one variable is temporary, the name is the name of the other
  //     variable. If both variables are temporary or both variables are not
  //     temporary, the name is chosen arbitrarily between the two names.
  bool Merge(const std::string& other_name, const FzDomain& other_domain,
             FzConstraint* const other_constraint, bool other_temporary);
  std::string DebugString() const;

  std::string name;
  FzDomain domain;
  // The constraint that defines this variable, if any, and nullptr otherwise.
  // This is the reverse field of FzConstraint::target_variable. This constraint
  // is not owned by the variable.
  FzConstraint* defining_constraint;
  // Indicates if the variable is a temporary variable created when flattening
  // the model. For instance, if you write x == y * z + y, then it will be
  // expanded into y * z == t and x = t + y. And t will be a temporary variable.
  bool temporary;

  friend class FzModel;

 private:
  FzIntegerVariable(const std::string& name_, const FzDomain& domain_,
                    bool temporary_);
};

// An argument is either an integer value, an integer domain, a
// reference to a variable, or an array of variable references.
struct FzArgument {
  enum Type {
    INT_VALUE,
    INT_DOMAIN,
    INT_VAR_REF,
    INT_VAR_REF_ARRAY,
    VOID_ARGUMENT,
  };

  static FzArgument IntegerValue(int64 value);
  static FzArgument Domain(const FzDomain& domain);
  static FzArgument IntVarRef(FzIntegerVariable* const var);
  static FzArgument IntVarRefArray(const std::vector<FzIntegerVariable*>& vars);
  static FzArgument VoidArgument();

  std::string DebugString() const;

  Type type;
  int64 integer_value;
  FzDomain domain;
  FzIntegerVariable* variable;
  std::vector<FzIntegerVariable*> variables;
};

// A constraint has a type, some arguments, and a few tags. Typically, a
// FzConstraint is on the heap, and owned by the global FzModel object.
struct FzConstraint {
  FzConstraint(const std::string& type_,
               const std::vector<FzArgument>& arguments_,
               bool strong_propagation_,
               FzIntegerVariable* const target_variable_)
      : type(type_),
        arguments(arguments_),
        strong_propagation(strong_propagation_),
        target_variable(target_variable_),
        is_trivially_true(false) {}

  std::string DebugString() const;

  // The flatzinc type of the constraint (i.e. "int_eq" for integer equality)
  // stored as a std::string.
  std::string type;
  std::vector<FzArgument> arguments;
  // Is true if the constraint should use the strongest level of propagation.
  // This is a hint in the model. For instance, in the AllDifferent constraint,
  // there are different algorithms to propagate with different pruning/speed
  // ratios. When strong_propagation is true, one should use, if possible, the
  // algorithm with the strongest pruning.
  bool strong_propagation;
  // Indicates if the constraint actually propagates towards a target variable
  // (target_variable will be nullptr otherwise). This is the reverse field of
  // FzIntegerVariable::defining_constraint.
  FzIntegerVariable* target_variable;
  // Indicates if the constraint is trivially true. Presolve can make it so
  // if the presolve transformation ensures that the constraints is always true.
  bool is_trivially_true;

  // Helpers
  void MarkAsTriviallyTrue();
  // Cleans the field target_variable, as well as the field defining_constraint
  // on the target_variable.
  void RemoveTargetVariable();
  // Returns true if the argument is a variable that is not a target variable.
  bool IsIntegerVariable(int position) const;
  // Returns true if the argument is bound (integer value, singleton domain,
  // variable with a singleton domain)
  bool IsBound(int position) const;
  // Returns the bound of the argument. IsBound() must have returned true for
  // this method to succeed.
  int64 GetBound(int position) const;
  // Returns the variable at the given position, or nullptr if there is no
  // variable at that position.
  FzIntegerVariable* GetVar(int position) const;
};

// An annotation is a set of information. It has two use cases. One during
// parsing to store intermediate information on model objects (i.e. the defines
// part of a constraint). The other use case is to store all search
// declarations. This persists after model parsing.
struct FzAnnotation {
  enum Type {
    ANNOTATION_LIST,
    IDENTIFIER,
    FUNCTION_CALL,
    INTERVAL,
    INT_VAR_REF,
    INT_VAR_REF_ARRAY,
  };

  static FzAnnotation Empty();
  static FzAnnotation AnnotationList(const std::vector<FzAnnotation>& list);
  static FzAnnotation Identifier(const std::string& id);
  static FzAnnotation FunctionCall(const std::string& id,
                                   const std::vector<FzAnnotation>& args);
  static FzAnnotation Interval(int64 interval_min, int64 interval_max);
  static FzAnnotation Variable(FzIntegerVariable* const var);
  static FzAnnotation VariableList(const std::vector<FzIntegerVariable*>& vars);

  std::string DebugString() const;
  bool IsFunctionCallWithIdentifier(const std::string& identifier) const {
    return type == FUNCTION_CALL && id == identifier;
  }
  // Copy all the variable references contained in this annotation (and its
  // children). Depending on the type of this annotation, there can be zero,
  // one, or several.
  void GetAllIntegerVariables(
      std::vector<FzIntegerVariable*>* const vars) const;

  Type type;
  int64 interval_min;
  int64 interval_max;
  std::string id;
  std::vector<FzAnnotation> annotations;
  FzIntegerVariable* variable;
  std::vector<FzIntegerVariable*> variables;
};

// Information on what should be displayed when a solution is found.
// It follows the flatzinc specification (www.minizinc.org).
struct FzOnSolutionOutput {
  struct Bounds {
    Bounds(int64 min_value_, int64 max_value_)
        : min_value(min_value_), max_value(max_value_) {}
    std::string DebugString() const;
    int64 min_value;
    int64 max_value;
  };

  // Will output: name = <variable value>.
  static FzOnSolutionOutput SingleVariable(const std::string& name,
                                           FzIntegerVariable* const variable);
  // Will output (for example):
  //     name = array2d(min1..max1, min2..max2, [list of variable values])
  // for a 2d array (bounds.size() == 2).
  static FzOnSolutionOutput MultiDimensionalArray(
      const std::string& name, const std::vector<Bounds>& bounds,
      const std::vector<FzIntegerVariable*>& flat_variables);
  // Empty output.
  static FzOnSolutionOutput VoidOutput();

  std::string DebugString() const;

  std::string name;
  FzIntegerVariable* variable;
  std::vector<FzIntegerVariable*> flat_variables;
  // These are the starts and ends of intervals for displaying (potentially
  // multi-dimensional) arrays.
  std::vector<Bounds> bounds;
};

class FzModel {
 public:
  explicit FzModel(const std::string& name)
      : name_(name), objective_(nullptr), maximize_(true) {}
  ~FzModel();

  // ----- Builder methods -----

  // The objects returned by AddVariable() and AddConstraint() are owned by the
  // FzModel and will remain live for its lifetime.
  FzIntegerVariable* AddVariable(const std::string& name,
                                 const FzDomain& domain, bool temporary);
  void AddConstraint(const std::string& type,
                     const std::vector<FzArgument>& arguments, bool is_domain,
                     FzIntegerVariable* const target_variable);
  void AddOutput(const FzOnSolutionOutput& output);

  // Set the search annotations and the objective: either simply satisfy the
  // problem, or minimize or maximize the given variable (which must have been
  // added with AddVariable() already).
  void Satisfy(const std::vector<FzAnnotation>& search_annotations);
  void Minimize(FzIntegerVariable* const obj,
                const std::vector<FzAnnotation>& search_annotations);
  void Maximize(FzIntegerVariable* const obj,
                const std::vector<FzAnnotation>& search_annotations);

  // ----- Accessors and mutators -----

  const std::vector<FzIntegerVariable*>& variables() const {
    return variables_;
  }
  const std::vector<FzConstraint*>& constraints() const { return constraints_; }
  void DeleteConstraintAtIndex(int index) {
    delete constraints_[index];
    constraints_[index] = nullptr;
  }
  const std::vector<FzAnnotation>& search_annotations() const {
    return search_annotations_;
  }
  MutableVectorIteration<FzAnnotation> mutable_search_annotations() {
    return MutableVectorIteration<FzAnnotation>(&search_annotations_);
  }
  const std::vector<FzOnSolutionOutput>& output() const { return output_; }
  MutableVectorIteration<FzOnSolutionOutput> mutable_output() {
    return MutableVectorIteration<FzOnSolutionOutput>(&output_);
  }
  bool maximize() const { return maximize_; }
  FzIntegerVariable* objective() const { return objective_; }

  // Services.
  std::string DebugString() const;

  const std::string& name() const { return name_; }

 private:
  const std::string name_;
  // owned.
  std::vector<FzIntegerVariable*> variables_;
  // owned.
  std::vector<FzConstraint*> constraints_;
  // The objective variable (it belongs to variables_).
  FzIntegerVariable* objective_;
  bool maximize_;
  // All search annotations are stored as a vector of FzAnnotation.
  std::vector<FzAnnotation> search_annotations_;
  std::vector<FzOnSolutionOutput> output_;
};

// Stand-alone statistics class on the model.
class FzModelStatistics {
 public:
  explicit FzModelStatistics(const FzModel& model) : model_(model) {}
  int VariableOccurrences(FzIntegerVariable* const var) {
    return constraints_per_variables_[var].size();
  }
  void BuildStatistics();
  void PrintStatistics();

 private:
  const FzModel& model_;
  hash_map<const std::string, std::vector<FzConstraint*> >
      constraints_per_type_;
  hash_map<const FzIntegerVariable*, std::vector<FzConstraint*> >
      constraints_per_variables_;
};
}  // namespace operations_research

#endif  // OR_TOOLS_FLATZINC_MODEL_H_
