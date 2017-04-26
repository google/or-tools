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

#include <map>
#include <string>
#include <unordered_map>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/util/iterators.h"
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
// TODO(user): Rework domains, all int64 should be kintmin..kint64max.
//                It is a bit tricky though as we must take care of overflows.
struct Domain {
  // The values will be sorted and duplicate values will be removed.
  static Domain IntegerList(std::vector<int64> values);
  static Domain AllInt64();
  static Domain IntegerValue(int64 value);
  static Domain Interval(int64 included_min, int64 included_max);
  static Domain Boolean();
  static Domain EmptyDomain();

  bool HasOneValue() const;
  bool empty() const;

  // Returns the min of the domain.
  int64 Min() const;

  // Returns the max of the domain.
  int64 Max() const;

  // Returns the value of the domain. HasOneValue() must return true.
  int64 Value() const;

  // Returns true if the domain is [kint64min..kint64max]
  bool IsAllInt64() const;

  // Various inclusion tests on a domain.
  bool Contains(int64 value) const;
  bool OverlapsIntList(const std::vector<int64>& values) const;
  bool OverlapsIntInterval(int64 lb, int64 ub) const;
  bool OverlapsDomain(const Domain& other) const;

  // All the following modifiers change the internal representation
  //   list to interval or interval to list.
  void IntersectWithSingleton(int64 value);
  void IntersectWithDomain(const Domain& domain);
  void IntersectWithInterval(int64 interval_min, int64 interval_max);
  void IntersectWithListOfIntegers(const std::vector<int64>& values);

  // Returns true iff the value did belong to the domain, and was removed.
  // Try to remove the value. It returns true if it was actually removed.
  // If the value is inside a large interval, then it will not be removed.
  bool RemoveValue(int64 value);
  std::string DebugString() const;

  // These should never be modified from outside the class.
  std::vector<int64> values;
  bool is_interval;
  bool display_as_boolean;
};

// An int var is a name with a domain of possible values, along with
// some tags. Typically, an IntegerVariable is on the heap, and owned by the
// global Model object.
struct IntegerVariable {
  // This method tries to unify two variables. This can happen during the
  // parsing of the model or during presolve. This is possible if at least one
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
  bool Merge(const std::string& other_name, const Domain& other_domain,
             Constraint* const other_constraint, bool other_temporary);

  std::string DebugString() const;

  std::string name;
  Domain domain;
  // The constraint that defines this variable, if any, and nullptr otherwise.
  // This is the reverse field of Constraint::target_variable. This constraint
  // is not owned by the variable.
  Constraint* defining_constraint;
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

  IntegerVariable(const std::string& name_, const Domain& domain_, bool temporary_);
};

// An argument is either an integer value, an integer domain, a
// reference to a variable, or an array of variable references.
struct Argument {
  enum Type {
    INT_VALUE,
    INT_INTERVAL,
    INT_LIST,
    DOMAIN_LIST,
    INT_VAR_REF,
    INT_VAR_REF_ARRAY,
    VOID_ARGUMENT,
  };

  static Argument IntegerValue(int64 value);
  static Argument Interval(int64 imin, int64 imax);
  static Argument IntegerList(std::vector<int64> values);
  static Argument DomainList(std::vector<Domain> domains);
  static Argument IntVarRef(IntegerVariable* const var);
  static Argument IntVarRefArray(std::vector<IntegerVariable*> vars);
  static Argument VoidArgument();
  static Argument FromDomain(const Domain& domain);

  std::string DebugString() const;

  // Returns true if the argument is a variable.
  bool IsVariable() const;
  // Returns true if the argument has only one value (integer value, integer
  // list of size 1, interval of size 1, or variable with a singleton domain).
  bool HasOneValue() const;
  // Returns the value of the argument. Does DCHECK(HasOneValue()).
  int64 Value() const;
  // Returns true if if it an integer list, or an array of integer
  // variables (or domain) each having only one value.
  bool IsArrayOfValues() const;
  // Returns true if the argument is an integer value, an integer
  // list, or an interval, and it contains the given value.
  // It will check that the type is actually one of the above.
  bool Contains(int64 value) const;
  // Returns the value of the pos-th element.
  int64 ValueAt(int pos) const;
  // Returns the variable inside the argument if the type is INT_VAR_REF,
  // or nullptr otherwise.
  IntegerVariable* Var() const;
  // Returns the variable at position pos inside the argument if the type is
  // INT_VAR_REF_ARRAY or nullptr otherwise.
  IntegerVariable* VarAt(int pos) const;

  Type type;
  std::vector<int64> values;
  std::vector<IntegerVariable*> variables;
  std::vector<Domain> domains;
};

// A constraint has a type, some arguments, and a few tags. Typically, a
// Constraint is on the heap, and owned by the global Model object.
struct Constraint {
  Constraint(const std::string& t, std::vector<Argument> args, bool strong_propag,
             IntegerVariable* target)
      : type(t),
        arguments(std::move(args)),
        target_variable(target),
        strong_propagation(strong_propag),
        active(true),
        presolve_propagation_done(false) {}

  std::string DebugString() const;

  // Helpers to be used during presolve.
  void MarkAsInactive();
  // Cleans the field target_variable, as well as the field defining_constraint
  // on the target_variable.
  void RemoveTargetVariable();
  // Helper method to remove one argument.
  void RemoveArg(int arg_pos);
  // Set as a False constraint.
  void SetAsFalse();

  // The flatzinc type of the constraint (i.e. "int_eq" for integer equality)
  // stored as a std::string.
  std::string type;
  std::vector<Argument> arguments;
  // Indicates if the constraint actually propagates towards a target variable
  // (target_variable will be nullptr otherwise). This is the reverse field of
  // IntegerVariable::defining_constraint.
  IntegerVariable* target_variable;
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
    INTERVAL,
    INT_VAR_REF,
    INT_VAR_REF_ARRAY,
    STRING_VALUE,
  };

  static Annotation Empty();
  static Annotation AnnotationList(std::vector<Annotation> list);
  static Annotation Identifier(const std::string& id);
  static Annotation FunctionCallWithArguments(const std::string& id,
                                              std::vector<Annotation> args);
  static Annotation FunctionCall(const std::string& id);
  static Annotation Interval(int64 interval_min, int64 interval_max);
  static Annotation IntegerValue(int64 value);
  static Annotation Variable(IntegerVariable* const var);
  static Annotation VariableList(std::vector<IntegerVariable*> vars);
  static Annotation String(const std::string& str);

  std::string DebugString() const;
  bool IsFunctionCallWithIdentifier(const std::string& identifier) const {
    return type == FUNCTION_CALL && id == identifier;
  }
  // Copy all the variable references contained in this annotation (and its
  // children). Depending on the type of this annotation, there can be zero,
  // one, or several.
  void AppendAllIntegerVariables(std::vector<IntegerVariable*>* vars) const;

  Type type;
  int64 interval_min;
  int64 interval_max;
  std::string id;
  std::vector<Annotation> annotations;
  std::vector<IntegerVariable*> variables;
  std::string string_value;
};

// Information on what should be displayed when a solution is found.
// It follows the flatzinc specification (www.minizinc.org).
struct SolutionOutputSpecs {
  struct Bounds {
    Bounds(int64 min_value_, int64 max_value_)
        : min_value(min_value_), max_value(max_value_) {}
    std::string DebugString() const;
    int64 min_value;
    int64 max_value;
  };

  // Will output: name = <variable value>.
  static SolutionOutputSpecs SingleVariable(const std::string& name,
                                            IntegerVariable* variable,
                                            bool display_as_boolean);
  // Will output (for example):
  //     name = array2d(min1..max1, min2..max2, [list of variable values])
  // for a 2d array (bounds.size() == 2).
  static SolutionOutputSpecs MultiDimensionalArray(
      const std::string& name, std::vector<Bounds> bounds,
      std::vector<IntegerVariable*> flat_variables, bool display_as_boolean);
  // Empty output.
  static SolutionOutputSpecs VoidOutput();

  std::string DebugString() const;

  std::string name;
  IntegerVariable* variable;
  std::vector<IntegerVariable*> flat_variables;
  // These are the starts and ends of intervals for displaying (potentially
  // multi-dimensional) arrays.
  std::vector<Bounds> bounds;
  bool display_as_boolean;
};

class Model {
 public:
  explicit Model(const std::string& name)
      : name_(name), objective_(nullptr), maximize_(true) {}
  ~Model();

  // ----- Builder methods -----

  // The objects returned by AddVariable(), AddConstant(),  and AddConstraint()
  // are owned by the model and will remain live for its lifetime.
  IntegerVariable* AddVariable(const std::string& name, const Domain& domain,
                               bool temporary);
  IntegerVariable* AddConstant(int64 value);
  // Creates and add a constraint to the model.
  // The parameter strong is an indication from the model that prefers stronger
  // (and more expensive version of the propagator).
  void AddConstraint(const std::string& type, std::vector<Argument> arguments,
                     bool strong, IntegerVariable* target_variable);
  void AddConstraint(const std::string& type, std::vector<Argument> arguments);
  void AddOutput(SolutionOutputSpecs output);

  // Set the search annotations and the objective: either simply satisfy the
  // problem, or minimize or maximize the given variable (which must have been
  // added with AddVariable() already).
  void Satisfy(std::vector<Annotation> search_annotations);
  void Minimize(IntegerVariable* obj,
                std::vector<Annotation> search_annotations);
  void Maximize(IntegerVariable* obj,
                std::vector<Annotation> search_annotations);

  bool IsInconsistent() const;

  // ----- Accessors and mutators -----

  const std::vector<IntegerVariable*>& variables() const { return variables_; }
  const std::vector<Constraint*>& constraints() const { return constraints_; }
  const std::vector<Annotation>& search_annotations() const {
    return search_annotations_;
  }
#if !defined(SWIG)
  MutableVectorIteration<Annotation> mutable_search_annotations() {
    return MutableVectorIteration<Annotation>(&search_annotations_);
  }
#endif
  const std::vector<SolutionOutputSpecs>& output() const { return output_; }
#if !defined(SWIG)
  MutableVectorIteration<SolutionOutputSpecs> mutable_output() {
    return MutableVectorIteration<SolutionOutputSpecs>(&output_);
  }
#endif
  bool maximize() const { return maximize_; }
  IntegerVariable* objective() const { return objective_; }
  void SetObjective(IntegerVariable* obj) { objective_ = obj; }

  // Services.
  std::string DebugString() const;

  const std::string& name() const { return name_; }

 private:
  const std::string name_;
  // owned.
  // TODO(user): use unique_ptr
  std::vector<IntegerVariable*> variables_;
  // owned.
  // TODO(user): use unique_ptr
  std::vector<Constraint*> constraints_;
  // The objective variable (it belongs to variables_).
  IntegerVariable* objective_;
  bool maximize_;
  // All search annotations are stored as a vector of Annotation.
  std::vector<Annotation> search_annotations_;
  std::vector<SolutionOutputSpecs> output_;
};

// Stand-alone statistics class on the model.
// TODO(user): Clean up API to pass a Model* in argument.
class ModelStatistics {
 public:
  explicit ModelStatistics(const Model& model) : model_(model) {}
  int NumVariableOccurrences(IntegerVariable* var) {
    return constraints_per_variables_[var].size();
  }
  void BuildStatistics();
  void PrintStatistics() const;

 private:
  const Model& model_;
  std::map<std::string, std::vector<Constraint*>> constraints_per_type_;
  std::unordered_map<const IntegerVariable*, std::vector<Constraint*>>
      constraints_per_variables_;
};
}  // namespace fz
}  // namespace operations_research

#endif  // OR_TOOLS_FLATZINC_MODEL_H_
