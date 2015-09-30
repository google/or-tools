// Copyright 2010-2014 Google
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
#include "base/stringprintf.h"
#include "base/hash.h"
#include "util/iterators.h"
#include "util/string_array.h"

namespace operations_research {
class FzConstraint;
class FzModel;
class FzIntegerVariable;

#define FZENDL std::endl
#define FZLOG \
  if (FLAGS_fz_logging) std::cout << "%% "

#define FZVLOG \
  if (FLAGS_fz_verbose) std::cout << "%%%% "

#define FZDLOG \
  if (FLAGS_fz_debug) std::cout << "%%%%%% "

// A domain represents the possible values of a variable, and its type
// (which carries display information, i.e. a boolean will be displayed
// differently than an integer with domain {0, 1}).
// It can be:
//  - an explicit list of all possible values, in which case is_interval is
//    false.
//  - an interval, in which case is_interval is true and values.size() == 2,
//    and the interval is [values[0], values[1]].
//  - all integers, in which case values is empty, and is_interval is true.
// Note that semi-infinite intervals aren't supported.
// - A boolean domain({ 0, 1 } with boolean display tag).
struct FzDomain {
  static FzDomain IntegerList(std::vector<int64> values);
  static FzDomain AllInt64();
  static FzDomain Singleton(int64 value);
  static FzDomain Interval(int64 included_min, int64 included_max);
  static FzDomain Boolean();
  static FzDomain EmptyDomain();

  bool IsSingleton() const;
  void IntersectWithFzDomain(const FzDomain& domain);
  void IntersectWithInterval(int64 interval_min, int64 interval_max);
  void IntersectWithListOfIntegers(const std::vector<int64>& values);
  bool Contains(int64 value) const;
  // Returns true iff the value did belong to the domain, and was removed.
  bool RemoveValue(int64 value);
  std::string DebugString() const;

  std::vector<int64> values;
  bool is_interval;

  bool display_as_boolean;
  // TODO(user): Rework domains, all int64 should be kintmin..kint64max.
  // Empty should means invalid.
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
  // Returns true if the domain has one value.
  bool HasOneValue() const;
  // Returns the min of the domain.
  int64 Min() const;
  // Returns the max of the domain.
  int64 Max() const;
  // Returns true if the domain is [kint64min..kint64max]
  bool IsAllInt64() const;

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
  bool temporary : 1;
  // Indicates if the variable should be created at all. A temporary variable
  // can be unreachable in the active model if nobody uses it. In that case,
  // there is no need to create it.
  bool active : 1;

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
    INT_INTERVAL,
    INT_LIST,
    DOMAIN_LIST,
    INT_VAR_REF,
    INT_VAR_REF_ARRAY,
    VOID_ARGUMENT,
  };

  static FzArgument IntegerValue(int64 value);
  static FzArgument Interval(int64 imin, int64 imax);
  static FzArgument IntegerList(std::vector<int64> values);
  static FzArgument DomainList(std::vector<FzDomain> domains);
  static FzArgument IntVarRef(FzIntegerVariable* const var);
  static FzArgument IntVarRefArray(std::vector<FzIntegerVariable*> vars);
  static FzArgument VoidArgument();
  static FzArgument FromDomain(const FzDomain& domain);

  std::string DebugString() const;

  Type type;
  std::vector<int64> values;
  std::vector<FzIntegerVariable*> variables;
  std::vector<FzDomain> domains;

  // Helpers

  // Returns true if the argument is a variable.
  bool IsVariable() const;
  // Returns true if the argument has only one value (single value, or variable
  // with a singleton domain).
  bool HasOneValue() const;
  // Returns the value of the argument. Does DCHECK(HasOneValue()).
  int64 Value() const;
  // Returns the variable inside the argument, or nullptr if there is no
  // variable.
  FzIntegerVariable* Var() const;
};

// A constraint has a type, some arguments, and a few tags. Typically, a
// FzConstraint is on the heap, and owned by the global FzModel object.
struct FzConstraint {
  FzConstraint(const std::string& type_, std::vector<FzArgument> arguments_,
               bool strong_propagation_,
               FzIntegerVariable* const target_variable_)
      : type(type_),
        arguments(std::move(arguments_)),
        target_variable(target_variable_),
        strong_propagation(strong_propagation_),
        active(true),
        presolve_propagation_done(false) {}

  std::string DebugString() const;

  // The flatzinc type of the constraint (i.e. "int_eq" for integer equality)
  // stored as a std::string.
  std::string type;
  std::vector<FzArgument> arguments;
  // Indicates if the constraint actually propagates towards a target variable
  // (target_variable will be nullptr otherwise). This is the reverse field of
  // FzIntegerVariable::defining_constraint.
  FzIntegerVariable* target_variable;
  // Is true if the constraint should use the strongest level of propagation.
  // This is a hint in the model. For instance, in the AllDifferent constraint,
  // there are different algorithms to propagate with different pruning/speed
  // ratios. When strong_propagation is true, one should use, if possible, the
  // algorithm with the strongest pruning.
  bool strong_propagation : 1;
  // Indicates if the constraint is active. Presolve can make it inactive by
  // propagating it, or by regrouping it. Once a constraint is inactive, it is
  // logically removed from the model, it is not extracted, and it is ignored by
  // presolve.
  bool active : 1;

  // The following boolean are used to trace presolve. They are used to avoid
  // repeating the same presolve rule again and again.

  // Indicates if presolve has finished propagating this constraint.
  bool presolve_propagation_done : 1;

  // Helpers
  void MarkAsInactive();
  // Cleans the field target_variable, as well as the field defining_constraint
  // on the target_variable.
  void RemoveTargetVariable();

  const FzArgument& Arg(int arg_pos) const { return arguments[arg_pos]; }
#if !defined(SWIG)
  FzArgument* MutableArg(int arg_pos) { return &arguments[arg_pos]; }
#endif
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
    INT_VALUE,
    INTERVAL,
    INT_VAR_REF,
    INT_VAR_REF_ARRAY,
    STRING_VALUE,
  };

  static FzAnnotation Empty();
  static FzAnnotation AnnotationList(std::vector<FzAnnotation> list);
  static FzAnnotation Identifier(const std::string& id);
  static FzAnnotation FunctionCallWithArguments(const std::string& id,
                                                std::vector<FzAnnotation> args);
  static FzAnnotation FunctionCall(const std::string& id);
  static FzAnnotation Interval(int64 interval_min, int64 interval_max);
  static FzAnnotation IntegerValue(int64 value);
  static FzAnnotation Variable(FzIntegerVariable* const var);
  static FzAnnotation VariableList(std::vector<FzIntegerVariable*> vars);
  static FzAnnotation String(const std::string& str);

  std::string DebugString() const;
  bool IsFunctionCallWithIdentifier(const std::string& identifier) const {
    return type == FUNCTION_CALL && id == identifier;
  }
  // Copy all the variable references contained in this annotation (and its
  // children). Depending on the type of this annotation, there can be zero,
  // one, or several.
  void GetAllIntegerVariables(std::vector<FzIntegerVariable*>* const vars) const;

  Type type;
  int64 interval_min;
  int64 interval_max;
  std::string id;
  std::vector<FzAnnotation> annotations;
  std::vector<FzIntegerVariable*> variables;
  std::string string_value_;
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
                                           FzIntegerVariable* const variable,
                                           bool display_as_boolean);
  // Will output (for example):
  //     name = array2d(min1..max1, min2..max2, [list of variable values])
  // for a 2d array (bounds.size() == 2).
  static FzOnSolutionOutput MultiDimensionalArray(
      const std::string& name, std::vector<Bounds> bounds,
      std::vector<FzIntegerVariable*> flat_variables,
      bool display_as_boolean);
  // Empty output.
  static FzOnSolutionOutput VoidOutput();

  std::string DebugString() const;

  std::string name;
  FzIntegerVariable* variable;
  std::vector<FzIntegerVariable*> flat_variables;
  // These are the starts and ends of intervals for displaying (potentially
  // multi-dimensional) arrays.
  std::vector<Bounds> bounds;
  bool display_as_boolean;
};

class FzModel {
 public:
  explicit FzModel(const std::string& name)
      : name_(name), objective_(nullptr), maximize_(true) {}
  ~FzModel();

  // ----- Builder methods -----

  // The objects returned by AddVariable() and AddConstraint() are owned by the
  // FzModel and will remain live for its lifetime.
  FzIntegerVariable* AddVariable(const std::string& name, const FzDomain& domain,
                                 bool temporary);
  void AddConstraint(const std::string& type, std::vector<FzArgument> arguments,
                     bool is_domain, FzIntegerVariable* const target_variable);
  void AddOutput(const FzOnSolutionOutput& output);

  // Set the search annotations and the objective: either simply satisfy the
  // problem, or minimize or maximize the given variable (which must have been
  // added with AddVariable() already).
  void Satisfy(std::vector<FzAnnotation> search_annotations);
  void Minimize(FzIntegerVariable* obj,
                std::vector<FzAnnotation> search_annotations);
  void Maximize(FzIntegerVariable* obj,
                std::vector<FzAnnotation> search_annotations);

  // ----- Accessors and mutators -----

  const std::vector<FzIntegerVariable*>& variables() const { return variables_; }
  const std::vector<FzConstraint*>& constraints() const { return constraints_; }
  const std::vector<FzAnnotation>& search_annotations() const {
    return search_annotations_;
  }
#if !defined(SWIG)
  MutableVectorIteration<FzAnnotation> mutable_search_annotations() {
    return MutableVectorIteration<FzAnnotation>(&search_annotations_);
  }
#endif
  const std::vector<FzOnSolutionOutput>& output() const { return output_; }
#if !defined(SWIG)
  MutableVectorIteration<FzOnSolutionOutput> mutable_output() {
    return MutableVectorIteration<FzOnSolutionOutput>(&output_);
  }
#endif
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
  hash_map<const std::string, std::vector<FzConstraint*> > constraints_per_type_;
  hash_map<const FzIntegerVariable*, std::vector<FzConstraint*> >
      constraints_per_variables_;
};
}  // namespace operations_research

#endif  // OR_TOOLS_FLATZINC_MODEL_H_
