// Copyright 2010-2022 Google LLC
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

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/graph/iterators.h"
#include "ortools/util/logging.h"
#include "ortools/util/string_array.h"

namespace operations_research {
namespace fz {

struct Constraint;
class Model;

// A domain represents the possible values of a variable, and its type
// (which carries display information, i.e. a Boolean will be displayed
// differently than an integer with domain {0, 1}).
// It can be:
//  - an explicit list of all possible values, in which case is_interval is
//    false. If the list is empty, then the domain is empty.
//  - an interval, in which case is_interval is true and values.size() == 2,
//    and the interval is [values[0], values[1]].
//  - all integers, in which case values is empty, and is_interval is true.
//    Note that semi-infinite intervals aren't supported.
//  - a Boolean domain({ 0, 1 } with Boolean display tag).
// TODO(user): Rework domains, all int64_t should be kintmin..kint64max.
//                It is a bit tricky though as we must take care of overflows.
// If is_a_set is true, then this domain has a set semantics. For a set
// variable, any subset of the initial set of values is a valid assignment,
// instead of exactly one value.
struct Domain {
  // The values will be sorted and duplicate values will be removed.
  static Domain IntegerList(std::vector<int64_t> values);
  static Domain AllInt64();
  static Domain IntegerValue(int64_t value);
  static Domain Interval(int64_t included_min, int64_t included_max);
  static Domain Boolean();
  static Domain SetOfIntegerList(std::vector<int64_t> values);
  static Domain SetOfAllInt64();
  static Domain SetOfIntegerValue(int64_t value);
  static Domain SetOfInterval(int64_t included_min, int64_t included_max);
  static Domain SetOfBoolean();
  static Domain EmptyDomain();
  static Domain AllFloats();
  static Domain FloatValue(double value);
  static Domain FloatInterval(double lb, double ub);
  // TODO(user): Do we need SetOfFloats() ?

  bool HasOneValue() const;
  bool empty() const;

  // Returns the min of the domain.
  int64_t Min() const;

  // Returns the max of the domain.
  int64_t Max() const;

  // Returns the value of the domain. HasOneValue() must return true.
  int64_t Value() const;

  // Returns true if the domain is [kint64min..kint64max]
  bool IsAllInt64() const;

  // Various inclusion tests on a domain.
  bool Contains(int64_t value) const;
  bool OverlapsIntList(const std::vector<int64_t>& vec) const;
  bool OverlapsIntInterval(int64_t lb, int64_t ub) const;
  bool OverlapsDomain(const Domain& other) const;

  // All the following modifiers change the internal representation
  //   list to interval or interval to list.
  bool IntersectWithSingleton(int64_t value);
  bool IntersectWithDomain(const Domain& domain);
  bool IntersectWithInterval(int64_t interval_min, int64_t interval_max);
  bool IntersectWithListOfIntegers(const std::vector<int64_t>& integers);
  bool IntersectWithFloatDomain(const Domain& domain);

  // Returns true iff the value did belong to the domain, and was removed.
  // Try to remove the value. It returns true if it was actually removed.
  // If the value is inside a large interval, then it will not be removed.
  bool RemoveValue(int64_t value);
  // Sets the empty float domain. Returns true.
  bool SetEmptyFloatDomain();
  std::string DebugString() const;

  // These should never be modified from outside the class.
  std::vector<int64_t> values;
  bool is_interval = false;
  bool display_as_boolean = false;
  // Indicates if the domain was created as a set domain.
  bool is_a_set = false;
  // Float domain.
  bool is_float = false;
  std::vector<double> float_values;
};

// An int var is a name with a domain of possible values, along with
// some tags. Typically, an Variable is on the heap, and owned by the
// global Model object.
struct Variable {
  // This method tries to unify two variables. This can happen during the
  // parsing of the model or during presolve. This is possible if at least one
  // of the two variable is not the target of a constraint. (otherwise it
  // returns false).
  // The semantic of the merge is the following:
  //   - the resulting domain is the intersection of the two domains.
  //   - if one variable is not temporary, the result is not temporary.
  //   - if one variable is temporary, the name is the name of the other
  //     variable. If both variables are temporary or both variables are not
  //     temporary, the name is chosen arbitrarily between the two names.
  bool Merge(absl::string_view other_name, const Domain& other_domain,
             bool other_temporary);

  std::string DebugString() const;

  std::string name;
  Domain domain;
  // Indicates if the variable is a temporary variable created when flattening
  // the model. For instance, if you write x == y * z + y, then it will be
  // expanded into y * z == t and x = t + y. And t will be a temporary variable.
  bool temporary : 1;
  // Indicates if the variable should be created at all. A temporary variable
  // can be unreachable in the active model if nobody uses it. In that case,
  // there is no need to create it.
  bool active : 1;

 private:
  friend class Model;

  Variable(absl::string_view name_, const Domain& domain_, bool temporary_);
};

// An argument is either an integer value, an integer domain, a
// reference to a variable, or an array of variable references.
struct Argument {
  enum Type {
    INT_VALUE,
    INT_INTERVAL,
    INT_LIST,
    DOMAIN_LIST,
    FLOAT_VALUE,
    FLOAT_INTERVAL,
    FLOAT_LIST,
    VAR_REF,
    VAR_REF_ARRAY,
    VOID_ARGUMENT,
  };

  static Argument IntegerValue(int64_t value);
  static Argument Interval(int64_t imin, int64_t imax);
  static Argument IntegerList(std::vector<int64_t> values);
  static Argument DomainList(std::vector<Domain> domains);
  static Argument FloatValue(double value);
  static Argument FloatInterval(double lb, double ub);
  static Argument FloatList(std::vector<double> floats);
  static Argument VarRef(Variable* const var);
  static Argument VarRefArray(std::vector<Variable*> vars);
  static Argument VoidArgument();
  static Argument FromDomain(const Domain& domain);

  std::string DebugString() const;

  // Returns true if the argument is a variable.
  bool IsVariable() const;
  // Returns true if the argument has only one value (integer value, integer
  // list of size 1, interval of size 1, or variable with a singleton domain).
  bool HasOneValue() const;
  // Returns the value of the argument. Does DCHECK(HasOneValue()).
  int64_t Value() const;
  // Returns true if it an integer list, or an array of integer
  // variables (or domain) each having only one value.
  bool IsArrayOfValues() const;
  // Returns true if the argument is an integer value, an integer
  // list, or an interval, and it contains the given value.
  // It will check that the type is actually one of the above.
  bool Contains(int64_t value) const;
  // Returns the value of the pos-th element.
  int64_t ValueAt(int pos) const;
  // Returns the variable inside the argument if the type is VAR_REF,
  // or nullptr otherwise.
  Variable* Var() const;
  // Returns the variable at position pos inside the argument if the type is
  // VAR_REF_ARRAY or nullptr otherwise.
  Variable* VarAt(int pos) const;
  // Returns true is the pos-th argument is fixed.
  bool HasOneValueAt(int pos) const;
  // Returns the number of object in the argument.
  int Size() const;

  Type type;
  std::vector<int64_t> values;
  std::vector<Variable*> variables;
  std::vector<Domain> domains;
  std::vector<double> floats;
};

// A constraint has a type, some arguments, and a few tags. Typically, a
// Constraint is on the heap, and owned by the global Model object.
struct Constraint {
  Constraint(absl::string_view t, std::vector<Argument> args,
             bool strong_propag)
      : type(t),
        arguments(std::move(args)),
        strong_propagation(strong_propag),
        active(true),
        presolve_propagation_done(false) {}

  std::string DebugString() const;

  // Helpers to be used during presolve.
  void MarkAsInactive();
  // Helper method to remove one argument.
  void RemoveArg(int arg_pos);
  // Set as a False constraint.
  void SetAsFalse();

  // The flatzinc type of the constraint (i.e. "int_eq" for integer equality)
  // stored as a string.
  std::string type;
  std::vector<Argument> arguments;
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

  // Indicates if presolve has finished propagating this constraint.
  bool presolve_propagation_done : 1;
};

// An annotation is a set of information. It has two use cases. One during
// parsing to store intermediate information on model objects (i.e. the defines
// part of a constraint). The other use case is to store all search
// declarations. This persists after model parsing.
struct Annotation {
  enum Type {
    ANNOTATION_LIST,
    IDENTIFIER,
    FUNCTION_CALL,
    INT_VALUE,
    INT_LIST,
    INTERVAL,
    VAR_REF,
    VAR_REF_ARRAY,
    STRING_VALUE,
  };

  static Annotation Empty();
  static Annotation AnnotationList(std::vector<Annotation> list);
  static Annotation Identifier(absl::string_view id);
  static Annotation FunctionCallWithArguments(absl::string_view id,
                                              std::vector<Annotation> args);
  static Annotation FunctionCall(absl::string_view id);
  static Annotation Interval(int64_t interval_min, int64_t interval_max);
  static Annotation IntegerValue(int64_t value);
  static Annotation IntegerList(const std::vector<int64_t>& values);
  static Annotation VarRef(Variable* const var);
  static Annotation VarRefArray(std::vector<Variable*> variables);
  static Annotation String(absl::string_view str);

  std::string DebugString() const;
  bool IsFunctionCallWithIdentifier(absl::string_view identifier) const {
    return type == FUNCTION_CALL && id == identifier;
  }
  // Copy all the variable references contained in this annotation (and its
  // children). Depending on the type of this annotation, there can be zero,
  // one, or several.
  void AppendAllVariables(std::vector<Variable*>* vars) const;

  Type type;
  int64_t interval_min;
  int64_t interval_max;
  std::string id;
  std::vector<Annotation> annotations;
  std::vector<Variable*> variables;
  std::vector<int64_t> values;
  std::string string_value;
};

// Information on what should be displayed when a solution is found.
// It follows the flatzinc specification (www.minizinc.org).
struct SolutionOutputSpecs {
  struct Bounds {
    Bounds(int64_t min_value_, int64_t max_value_)
        : min_value(min_value_), max_value(max_value_) {}
    std::string DebugString() const;
    int64_t min_value;
    int64_t max_value;
  };

  // Will output: name = <variable value>.
  static SolutionOutputSpecs SingleVariable(absl::string_view name,
                                            Variable* variable,
                                            bool display_as_boolean);
  // Will output (for example):
  //     name = array2d(min1..max1, min2..max2, [list of variable values])
  // for a 2d array (bounds.size() == 2).
  static SolutionOutputSpecs MultiDimensionalArray(
      absl::string_view name, std::vector<Bounds> bounds,
      std::vector<Variable*> flat_variables, bool display_as_boolean);
  // Empty output.
  static SolutionOutputSpecs VoidOutput();

  std::string DebugString() const;

  std::string name;
  Variable* variable;
  std::vector<Variable*> flat_variables;
  // These are the starts and ends of intervals for displaying (potentially
  // multi-dimensional) arrays.
  std::vector<Bounds> bounds;
  bool display_as_boolean;
};

class Model {
 public:
  explicit Model(absl::string_view name)
      : name_(name), objective_(nullptr), maximize_(true) {}
  ~Model();

  // ----- Builder methods -----

  // The objects returned by AddVariable(), AddConstant(),  and AddConstraint()
  // are owned by the model and will remain live for its lifetime.
  Variable* AddVariable(absl::string_view name, const Domain& domain,
                        bool defined);
  Variable* AddConstant(int64_t value);
  Variable* AddFloatConstant(double value);
  // Creates and add a constraint to the model.
  void AddConstraint(absl::string_view id, std::vector<Argument> arguments,
                     bool is_domain);
  void AddConstraint(absl::string_view id, std::vector<Argument> arguments);
  void AddOutput(SolutionOutputSpecs output);

  // Set the search annotations and the objective: either simply satisfy the
  // problem, or minimize or maximize the given variable (which must have been
  // added with AddVariable() already).
  void Satisfy(std::vector<Annotation> search_annotations);
  void Minimize(Variable* obj, std::vector<Annotation> search_annotations);
  void Maximize(Variable* obj, std::vector<Annotation> search_annotations);

  bool IsInconsistent() const;

  // ----- Accessors and mutators -----

  const std::vector<Variable*>& variables() const { return variables_; }
  const std::vector<Constraint*>& constraints() const { return constraints_; }
  const std::vector<Annotation>& search_annotations() const {
    return search_annotations_;
  }
#if !defined(SWIG)
  util::MutableVectorIteration<Annotation> mutable_search_annotations() {
    return util::MutableVectorIteration<Annotation>(&search_annotations_);
  }
#endif
  const std::vector<SolutionOutputSpecs>& output() const { return output_; }
#if !defined(SWIG)
  util::MutableVectorIteration<SolutionOutputSpecs> mutable_output() {
    return util::MutableVectorIteration<SolutionOutputSpecs>(&output_);
  }
#endif
  bool maximize() const { return maximize_; }
  Variable* objective() const { return objective_; }
  void SetObjective(Variable* obj) { objective_ = obj; }

  // Services.
  std::string DebugString() const;

  const std::string& name() const { return name_; }

 private:
  const std::string name_;
  // owned.
  // TODO(user): use unique_ptr
  std::vector<Variable*> variables_;
  // owned.
  // TODO(user): use unique_ptr
  std::vector<Constraint*> constraints_;
  // The objective variable (it belongs to variables_).
  Variable* objective_;
  bool maximize_;
  // All search annotations are stored as a vector of Annotation.
  std::vector<Annotation> search_annotations_;
  std::vector<SolutionOutputSpecs> output_;
};

// Stand-alone statistics class on the model.
// TODO(user): Clean up API to pass a Model* in argument.
class ModelStatistics {
 public:
  explicit ModelStatistics(const Model& model, SolverLogger* logger)
      : model_(model), logger_(logger) {}
  int NumVariableOccurrences(Variable* var) {
    return constraints_per_variables_[var].size();
  }
  void BuildStatistics();
  void PrintStatistics() const;

 private:
  const Model& model_;
  SolverLogger* logger_;
  std::map<std::string, std::vector<Constraint*>> constraints_per_type_;
  absl::flat_hash_map<const Variable*, std::vector<Constraint*>>
      constraints_per_variables_;
};

// Helper method to flatten Search annotations.
void FlattenAnnotations(const Annotation& ann, std::vector<Annotation>* out);

}  // namespace fz
}  // namespace operations_research

#endif  // OR_TOOLS_FLATZINC_MODEL_H_
