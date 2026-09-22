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

#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/inlined_vector.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_exporter.h"
#include "ortools/linear_solver/model_validator.h"
#include "pybind11/pybind11.h"
#include "pybind11/pytypes.h"
#include "pybind11/stl.h"
#include "pybind11_protobuf/native_proto_caster.h"

namespace operations_research {

namespace py = ::pybind11;

namespace python {

py::handle g_offset_key;

class FakeMPVariableRepresentingTheConstantOffset {
 public:
  double solution_value() const { return 1.0; }
  std::string repr() const { return "OFFSET_KEY"; }
};

struct Coefficients {
  absl::flat_hash_map<const MPVariable*, double> terms;
  double offset = 0.0;
};

class LinearExpr : public std::enable_shared_from_this<LinearExpr> {
 public:
  virtual ~LinearExpr() = default;

  virtual std::string ToString() const { return "LinearExpr"; }

  virtual void AddSelfToCoeffMapOrStack(
      absl::flat_hash_map<const MPVariable*, double>& coeffs, double& offset,
      double multiplier,
      std::vector<std::pair<double, std::shared_ptr<const LinearExpr>>>& stack)
      const = 0;

  Coefficients GetCoeffs() const {
    Coefficients result;
    std::vector<std::pair<double, std::shared_ptr<const LinearExpr>>> stack;
    stack.push_back({1.0, shared_from_this()});
    while (!stack.empty()) {
      auto [current_multiplier, current_expr] = stack.back();
      stack.pop_back();
      current_expr->AddSelfToCoeffMapOrStack(result.terms, result.offset,
                                             current_multiplier, stack);
    }
    return result;
  }

  double SolutionValue() const {
    const Coefficients coeffs = GetCoeffs();
    std::vector<std::pair<const MPVariable*, double>> terms;
    terms.reserve(coeffs.terms.size());
    for (const auto& [var, coeff] : coeffs.terms) {
      terms.push_back({var, coeff});
    }
    absl::c_stable_sort(terms,
                        [](const std::pair<const MPVariable*, double>& a,
                           const std::pair<const MPVariable*, double>& b) {
                          return a.first->index() < b.first->index();
                        });
    double total = coeffs.offset;
    for (const auto& [var, coeff] : terms) {
      total += var->solution_value() * coeff;
    }
    return total;
  }
};

class Constant;
class VariableExpr;
class ProductCst;
class SumArray;
class LinearConstraint;

using LinearExprTerm =
    std::variant<std::shared_ptr<LinearExpr>, const MPVariable*, double>;

std::shared_ptr<LinearExpr> AsLinearExpr(const LinearExprTerm& term);

class Constant : public LinearExpr {
 public:
  explicit Constant(double value) : value_(value) {}

  std::string ToString() const override { return absl::StrCat(value_); }

  void AddSelfToCoeffMapOrStack(
      absl::flat_hash_map<const MPVariable*, double>& coeffs, double& offset,
      double multiplier,
      std::vector<std::pair<double, std::shared_ptr<const LinearExpr>>>& stack)
      const override {
    offset += value_ * multiplier;
  }

  double value() const { return value_; }

 private:
  double value_;
};

class VariableExpr : public LinearExpr {
 public:
  explicit VariableExpr(const MPVariable* var) : var_(var) {}

  std::string ToString() const override {
    return var_ != nullptr ? var_->name() : "";
  }

  void AddSelfToCoeffMapOrStack(
      absl::flat_hash_map<const MPVariable*, double>& coeffs, double& offset,
      double multiplier,
      std::vector<std::pair<double, std::shared_ptr<const LinearExpr>>>& stack)
      const override {
    coeffs[var_] += multiplier;
  }

  const MPVariable* var() const { return var_; }

 private:
  const MPVariable* var_;
};

class ProductCst : public LinearExpr {
 public:
  ProductCst(const LinearExprTerm& expr, double coef)
      : expr_(AsLinearExpr(expr)), coef_(coef) {}

  std::string ToString() const override {
    if (coef_ == -1.0) {
      return absl::StrCat("-", expr_->ToString());
    } else {
      return absl::StrCat("(", coef_, " * ", expr_->ToString(), ")");
    }
  }

  void AddSelfToCoeffMapOrStack(
      absl::flat_hash_map<const MPVariable*, double>& coeffs, double& offset,
      double multiplier,
      std::vector<std::pair<double, std::shared_ptr<const LinearExpr>>>& stack)
      const override {
    double current_multiplier = multiplier * coef_;
    if (current_multiplier != 0.0) {
      stack.push_back({current_multiplier, expr_});
    }
  }

  std::shared_ptr<LinearExpr> expr() const { return expr_; }
  double coef() const { return coef_; }

 private:
  std::shared_ptr<LinearExpr> expr_;
  double coef_;
};

class SumArray : public LinearExpr {
 public:
  using SumInput = std::variant<std::vector<LinearExprTerm>, py::iterable>;

  explicit SumArray(absl::InlinedVector<std::shared_ptr<LinearExpr>, 2> array)
      : array_(std::move(array)) {}

  explicit SumArray(const std::vector<std::shared_ptr<LinearExpr>>& array)
      : array_(array.begin(), array.end()) {}

  explicit SumArray(const SumInput& input) {
    if (std::holds_alternative<std::vector<LinearExprTerm>>(input)) {
      const auto& vec = std::get<std::vector<LinearExprTerm>>(input);
      array_.reserve(vec.size());
      for (const auto& term : vec) {
        array_.push_back(AsLinearExpr(term));
      }
    } else {
      for (py::handle elem : std::get<py::iterable>(input)) {
        if (py::isinstance<LinearExpr>(elem)) {
          array_.push_back(elem.cast<std::shared_ptr<LinearExpr>>());
        } else if (py::isinstance<MPVariable>(elem)) {
          array_.push_back(
              std::make_shared<VariableExpr>(elem.cast<const MPVariable*>()));
        } else {
          try {
            double val = elem.cast<double>();
            array_.push_back(std::make_shared<Constant>(val));
          } catch (const py::cast_error&) {
            throw py::type_error(
                "Element in SumArray must be LinearExpr, Variable, or number");
          }
        }
      }
    }
  }

  std::string ToString() const override {
    std::vector<std::string> parts;
    parts.reserve(array_.size());
    for (const auto& elem : array_) {
      std::string term = elem->ToString();
      if (parts.empty()) {
        parts.push_back(term);
        continue;
      }
      if (!term.empty() && term[0] == '-') {
        parts.push_back(absl::StrCat(" - ", term.substr(1)));
      } else {
        parts.push_back(absl::StrCat(" + ", term));
      }
    }
    return absl::StrCat("(", absl::StrJoin(parts, ""), ")");
  }

  void AddSelfToCoeffMapOrStack(
      absl::flat_hash_map<const MPVariable*, double>& coeffs, double& offset,
      double multiplier,
      std::vector<std::pair<double, std::shared_ptr<const LinearExpr>>>& stack)
      const override {
    for (auto it = array_.rbegin(); it != array_.rend(); ++it) {
      stack.push_back({multiplier, *it});
    }
  }

  const absl::InlinedVector<std::shared_ptr<LinearExpr>, 2>& array() const {
    return array_;
  }

 private:
  absl::InlinedVector<std::shared_ptr<LinearExpr>, 2> array_;
};

inline std::shared_ptr<LinearExpr> AsLinearExpr(const LinearExprTerm& term) {
  return std::visit(
      [](auto&& arg) -> std::shared_ptr<LinearExpr> {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::shared_ptr<LinearExpr>>) {
          return arg;
        } else if constexpr (std::is_same_v<T, const MPVariable*>) {
          return std::make_shared<VariableExpr>(arg);
        } else if constexpr (std::is_same_v<T, double>) {
          return std::make_shared<Constant>(arg);
        }
      },
      term);
}

inline std::shared_ptr<LinearExpr> CastToLinExp(const LinearExprTerm& v) {
  return AsLinearExpr(v);
}

inline py::object MakeSum(py::args args) {
  if (args.size() == 1) {
    py::handle first = args[0];
    if (py::isinstance<py::iterable>(first) &&
        !py::isinstance<py::str>(first) && !py::isinstance<LinearExpr>(first) &&
        !py::isinstance<MPVariable>(first)) {
      return py::cast(
          std::make_shared<SumArray>(first.cast<SumArray::SumInput>()));
    }
  }
  return py::cast(std::make_shared<SumArray>(args.cast<SumArray::SumInput>()));
}

class LinearConstraint {
 public:
  LinearConstraint(const LinearExprTerm& expr, double lb, double ub)
      : expr_(AsLinearExpr(expr)), lb_(lb), ub_(ub) {}

  std::string ToString() const {
    double inf_val = std::numeric_limits<double>::infinity();
    std::string expr_str = expr_->ToString();
    if (lb_ > -inf_val && ub_ < inf_val) {
      if (lb_ == ub_) {
        return absl::StrCat(expr_str, " == ", lb_);
      } else {
        return absl::StrCat(lb_, " <= ", expr_str, " <= ", ub_);
      }
    } else if (lb_ > -inf_val) {
      return absl::StrCat(expr_str, " >= ", lb_);
    } else if (ub_ < inf_val) {
      return absl::StrCat(expr_str, " <= ", ub_);
    } else {
      return "Trivial inequality (always true)";
    }
  }

  MPConstraint* Extract(MPSolver& solver, const std::string& name = "") const {
    auto [coeffs, constant] = expr_->GetCoeffs();
    double solver_inf = solver.infinity();
    double lb = -solver_inf;
    double ub = solver_inf;
    double inf_val = std::numeric_limits<double>::infinity();
    if (lb_ > -inf_val) {
      lb = lb_ - constant;
    }
    if (ub_ < inf_val) {
      ub = ub_ - constant;
    }
    MPConstraint* constraint = solver.MakeRowConstraint(lb, ub, name);
    for (const auto& [var, c] : coeffs) {
      constraint->SetCoefficient(var, c);
    }
    return constraint;
  }

  std::shared_ptr<LinearExpr> expr() const { return expr_; }
  double lb() const { return lb_; }
  double ub() const { return ub_; }

 private:
  std::shared_ptr<LinearExpr> expr_;
  double lb_;
  double ub_;
};

}  // namespace python

PYBIND11_MODULE(pywraplp, m) {
  pybind11_protobuf::ImportNativeProtoCasters();

  m.doc() = "pywraplp python module";

  // ---------------------------------------------------------------------------
  // MPSolver (bound as Solver)
  // ---------------------------------------------------------------------------
  py::class_<MPSolver> solver_class(m, "Solver");

  py::enum_<MPSolver::OptimizationProblemType>(solver_class,
                                               "OptimizationProblemType")
      .value("CLP_LINEAR_PROGRAMMING", MPSolver::CLP_LINEAR_PROGRAMMING)
      .value("GLPK_LINEAR_PROGRAMMING", MPSolver::GLPK_LINEAR_PROGRAMMING)
      .value("GLOP_LINEAR_PROGRAMMING", MPSolver::GLOP_LINEAR_PROGRAMMING)
      .value("PDLP_LINEAR_PROGRAMMING", MPSolver::PDLP_LINEAR_PROGRAMMING)
      .value("SCIP_MIXED_INTEGER_PROGRAMMING",
             MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING)
      .value("GLPK_MIXED_INTEGER_PROGRAMMING",
             MPSolver::GLPK_MIXED_INTEGER_PROGRAMMING)
      .value("CBC_MIXED_INTEGER_PROGRAMMING",
             MPSolver::CBC_MIXED_INTEGER_PROGRAMMING)
      .value("BOP_INTEGER_PROGRAMMING", MPSolver::BOP_INTEGER_PROGRAMMING)
      .value("SAT_INTEGER_PROGRAMMING", MPSolver::SAT_INTEGER_PROGRAMMING)
      .value("GUROBI_LINEAR_PROGRAMMING", MPSolver::GUROBI_LINEAR_PROGRAMMING)
      .value("GUROBI_MIXED_INTEGER_PROGRAMMING",
             MPSolver::GUROBI_MIXED_INTEGER_PROGRAMMING)
      .value("CPLEX_LINEAR_PROGRAMMING", MPSolver::CPLEX_LINEAR_PROGRAMMING)
      .value("CPLEX_MIXED_INTEGER_PROGRAMMING",
             MPSolver::CPLEX_MIXED_INTEGER_PROGRAMMING)
      .value("XPRESS_LINEAR_PROGRAMMING", MPSolver::XPRESS_LINEAR_PROGRAMMING)
      .value("XPRESS_MIXED_INTEGER_PROGRAMMING",
             MPSolver::XPRESS_MIXED_INTEGER_PROGRAMMING)
      .export_values();

  py::enum_<MPSolver::ResultStatus>(solver_class, "ResultStatus")
      .value("OPTIMAL", MPSolver::OPTIMAL)
      .value("FEASIBLE", MPSolver::FEASIBLE)
      .value("INFEASIBLE", MPSolver::INFEASIBLE)
      .value("UNBOUNDED", MPSolver::UNBOUNDED)
      .value("ABNORMAL", MPSolver::ABNORMAL)
      .value("MODEL_INVALID", MPSolver::MODEL_INVALID)
      .value("NOT_SOLVED", MPSolver::NOT_SOLVED)
      .export_values();

  py::enum_<MPSolver::BasisStatus>(solver_class, "BasisStatus")
      .value("FREE", MPSolver::FREE)
      .value("AT_LOWER_BOUND", MPSolver::AT_LOWER_BOUND)
      .value("AT_UPPER_BOUND", MPSolver::AT_UPPER_BOUND)
      .value("FIXED_VALUE", MPSolver::FIXED_VALUE)
      .value("BASIC", MPSolver::BASIC)
      .export_values();

  solver_class
      .def(py::init([](const std::string& name, int problem_type) {
             return std::make_unique<MPSolver>(
                 name,
                 static_cast<MPSolver::OptimizationProblemType>(problem_type));
           }),
           py::arg("name"), py::arg("problem_type"))
      .def_static("CreateSolver", &MPSolver::CreateSolver,
                  py::return_value_policy::take_ownership, py::arg("solver_id"))
      .def_static(
          "SupportsProblemType",
          [](int problem_type) {
            return MPSolver::SupportsProblemType(
                static_cast<MPSolver::OptimizationProblemType>(problem_type));
          },
          py::arg("problem_type"))
      .def("SolverVersion", &MPSolver::SolverVersion)
      .def_static("infinity", &MPSolver::infinity)
      .def_static("Infinity", &MPSolver::infinity)
      .def("Clear", &MPSolver::Clear)
      .def("NumVariables", &MPSolver::NumVariables)
      .def("variables", &MPSolver::variables,
           py::return_value_policy::reference_internal)
      .def("variable", &MPSolver::variable,
           py::return_value_policy::reference_internal, py::arg("index"))
      .def("LookupVariable", &MPSolver::LookupVariableOrNull,
           py::return_value_policy::reference_internal, py::arg("var_name"))
      .def("Var", &MPSolver::MakeVar,
           py::return_value_policy::reference_internal, py::arg("lb"),
           py::arg("ub"), py::arg("integer"), py::arg("name") = "")
      .def("NumVar", &MPSolver::MakeNumVar,
           py::return_value_policy::reference_internal, py::arg("lb"),
           py::arg("ub"), py::arg("name") = "")
      .def("IntVar", &MPSolver::MakeIntVar,
           py::return_value_policy::reference_internal, py::arg("lb"),
           py::arg("ub"), py::arg("name") = "")
      .def("BoolVar", &MPSolver::MakeBoolVar,
           py::return_value_policy::reference_internal, py::arg("name") = "")
      .def("NumConstraints", &MPSolver::NumConstraints)
      .def("constraints", &MPSolver::constraints,
           py::return_value_policy::reference_internal)
      .def("constraint", &MPSolver::constraint,
           py::return_value_policy::reference_internal, py::arg("index"))
      .def("LookupConstraint", &MPSolver::LookupConstraintOrNull,
           py::return_value_policy::reference_internal,
           py::arg("constraint_name"))
      .def("Constraint",
           py::overload_cast<double, double, const std::string&>(
               &MPSolver::MakeRowConstraint),
           py::return_value_policy::reference_internal, py::arg("lb"),
           py::arg("ub"), py::arg("name") = "")
      .def("Constraint",
           py::overload_cast<const std::string&>(&MPSolver::MakeRowConstraint),
           py::return_value_policy::reference_internal, py::arg("name"))
      .def("Constraint", py::overload_cast<>(&MPSolver::MakeRowConstraint),
           py::return_value_policy::reference_internal)
      .def("Objective", &MPSolver::MutableObjective,
           py::return_value_policy::reference_internal)
      .def("Solve",
           [](MPSolver& self) {
             py::gil_scoped_release release;
             return self.Solve();
           })
      .def(
          "Solve",
          [](MPSolver& self, const MPSolverParameters& param) {
            py::gil_scoped_release release;
            return self.Solve(param);
          },
          py::arg("param"))
      .def("VerifySolution", &MPSolver::VerifySolution, py::arg("tolerance"),
           py::arg("log_errors"))
      .def("InterruptSolve", &MPSolver::InterruptSolve)
      .def("wall_time", &MPSolver::wall_time)
      .def("WallTime", &MPSolver::wall_time)
      .def("iterations", &MPSolver::iterations)
      .def("Iterations", &MPSolver::iterations)
      .def("nodes", &MPSolver::nodes)
      .def("set_time_limit", &MPSolver::set_time_limit,
           py::arg("time_limit_milliseconds"))
      .def("SetTimeLimit", &MPSolver::set_time_limit,
           py::arg("time_limit_milliseconds"))
      .def("EnableOutput", &MPSolver::EnableOutput)
      .def("SuppressOutput", &MPSolver::SuppressOutput)
      .def("IsMip", &MPSolver::IsMIP)
      .def("SetSolverSpecificParametersAsString",
           &MPSolver::SetSolverSpecificParametersAsString,
           py::arg("parameters"))
      .def("NextSolution", &MPSolver::NextSolution)
      .def("ComputeConstraintActivities",
           &MPSolver::ComputeConstraintActivities)
      .def("ComputeExactConditionNumber",
           &MPSolver::ComputeExactConditionNumber)
      .def("Write", &MPSolver::Write, py::arg("file_name"))
      .def(
          "SetNumThreads",
          [](MPSolver& self, int num_threads) {
            return self.SetNumThreads(num_threads).ok();
          },
          py::arg("num_threads"))
      .def(
          "SetHint",
          [](MPSolver& self, const std::vector<MPVariable*>& variables,
             const std::vector<double>& values) {
            if (variables.size() != values.size()) {
              throw py::value_error(
                  "Different number of variables and values when setting "
                  "hint.");
            }
            std::vector<std::pair<const MPVariable*, double>> hint(
                variables.size());
            for (int i = 0; i < variables.size(); ++i) {
              hint[i] = std::make_pair(variables[i], values[i]);
            }
            self.SetHint(hint);
          },
          py::arg("variables"), py::arg("values"))
      .def(
          "SetStartingLpBasis",
          [](MPSolver& self, const py::sequence& variable_statuses,
             const py::sequence& constraint_statuses) {
            std::vector<MPSolver::BasisStatus> var_enum(
                variable_statuses.size());
            for (int i = 0; i < variable_statuses.size(); ++i) {
              var_enum[i] = static_cast<MPSolver::BasisStatus>(
                  variable_statuses[i].cast<int>());
            }
            std::vector<MPSolver::BasisStatus> con_enum(
                constraint_statuses.size());
            for (int i = 0; i < constraint_statuses.size(); ++i) {
              con_enum[i] = static_cast<MPSolver::BasisStatus>(
                  constraint_statuses[i].cast<int>());
            }
            self.SetStartingLpBasis(var_enum, con_enum);
          },
          py::arg("variable_statuses"), py::arg("constraint_statuses"))
      .def(
          "LoadModelFromProto",
          [](MPSolver& self, const MPModelProto& input_model) {
            std::string error_message;
            self.LoadModelFromProto(input_model, &error_message);
            return error_message;
          },
          py::arg("input_model"))
      .def(
          "LoadModelFromProtoKeepNames",
          [](MPSolver& self, const MPModelProto& input_model) {
            std::string error_message;
            self.LoadModelFromProto(input_model, &error_message,
                                    /*clear_names=*/false);
            return error_message;
          },
          py::arg("input_model"))
      .def(
          "LoadModelFromProtoWithUniqueNamesOrDie",
          [](MPSolver& self, const MPModelProto& input_model) {
            std::string error_message;
            self.LoadModelFromProtoWithUniqueNamesOrDie(input_model,
                                                        &error_message);
            return error_message;
          },
          py::arg("input_model"))
      .def(
          "LoadSolutionFromProto",
          [](MPSolver& self, const MPSolutionResponse& response,
             double tolerance) {
            const absl::Status status =
                self.LoadSolutionFromProto(response, tolerance);
            LOG_IF(ERROR, !status.ok())
                << "LoadSolutionFromProto() failed: " << status;
            return status.ok();
          },
          py::arg("response"),
          py::arg("tolerance") = std::numeric_limits<double>::infinity())
      .def(
          "FillSolutionResponseProto",
          [](const MPSolver& self,
             std::optional<py::object> response) -> py::object {
            MPSolutionResponse solution_response;
            self.FillSolutionResponseProto(&solution_response);
            if (response.has_value() && !response->is_none()) {
              response->attr("CopyFrom")(py::cast(solution_response));
              return *response;
            }
            return py::cast(solution_response);
          },
          py::arg("response") = py::none())
      .def(
          "ExportModelToProto",
          [](const MPSolver& self,
             std::optional<py::object> output_model) -> py::object {
            MPModelProto model_proto;
            self.ExportModelToProto(&model_proto);
            if (output_model.has_value() && !output_model->is_none()) {
              output_model->attr("CopyFrom")(py::cast(model_proto));
              return *output_model;
            }
            return py::cast(model_proto);
          },
          py::arg("output_model") = py::none())
      .def(
          "ExportModelAsLpFormat",
          [](const MPSolver& self, bool obfuscate) {
            MPModelExportOptions options;
            options.obfuscate = obfuscate;
            MPModelProto model;
            self.ExportModelToProto(&model);
            return ExportModelAsLpFormat(model, options).value_or("");
          },
          py::arg("obfuscate") = false)
      .def(
          "ExportModelAsMpsFormat",
          [](const MPSolver& self, bool fixed_format, bool obfuscate) {
            MPModelExportOptions options;
            options.obfuscate = obfuscate;
            MPModelProto model;
            self.ExportModelToProto(&model);
            return ExportModelAsMpsFormat(model, options).value_or("");
          },
          py::arg("fixed_format") = false, py::arg("obfuscate") = false)
      .def(
          "WriteModelToMpsFile",
          [](const MPSolver& self, const std::string& filename,
             bool fixed_format, bool obfuscate) {
            MPModelExportOptions options;
            options.obfuscate = obfuscate;
            MPModelProto model;
            self.ExportModelToProto(&model);
            return WriteModelToMpsFile(filename, model, options).ok();
          },
          py::arg("filename"), py::arg("fixed_format") = false,
          py::arg("obfuscate") = false)
      .def_static(
          "SolveWithProto",
          [](const MPModelRequest& model_request,
             std::optional<py::object> response) -> py::object {
            MPSolutionResponse solution_response;
            {
              py::gil_scoped_release release;
              MPSolver::SolveWithProto(model_request, &solution_response);
            }
            if (response.has_value() && !response->is_none()) {
              response->attr("CopyFrom")(py::cast(solution_response));
              return *response;
            }
            return py::cast(solution_response);
          },
          py::arg("model_request"), py::arg("response") = py::none());

  // ---------------------------------------------------------------------------
  // MPVariable (bound as Variable)
  // ---------------------------------------------------------------------------
  py::class_<MPVariable> variable_class(m, "Variable");
  variable_class.def("solution_value", &MPVariable::solution_value)
      .def("SolutionValue", &MPVariable::solution_value)
      .def("lb", &MPVariable::lb)
      .def("ub", &MPVariable::ub)
      .def("Lb", &MPVariable::lb)
      .def("Ub", &MPVariable::ub)
      .def("SetLb", &MPVariable::SetLB, py::arg("lb"))
      .def("SetUb", &MPVariable::SetUB, py::arg("ub"))
      .def("SetBounds", &MPVariable::SetBounds, py::arg("lb"), py::arg("ub"))
      .def("integer", &MPVariable::integer)
      .def("Integer", &MPVariable::integer)
      .def("SetInteger", &MPVariable::SetInteger, py::arg("integer"))
      .def("name", &MPVariable::name)
      .def("index", &MPVariable::index)
      .def("basis_status", &MPVariable::basis_status)
      .def("reduced_cost", &MPVariable::reduced_cost)
      .def("ReducedCost", &MPVariable::reduced_cost)
      .def("branching_priority", &MPVariable::branching_priority)
      .def("SetBranchingPriority", &MPVariable::SetBranchingPriority,
           py::arg("priority"))
      .def("__str__", &MPVariable::name)
      .def("__repr__", &MPVariable::name)
      .def("__hash__",
           [](const MPVariable& self) {
             return reinterpret_cast<uintptr_t>(&self);
           })
      .def("__getattr__", [](const MPVariable& self, const std::string& name) {
        py::object var_expr =
            py::cast(std::make_shared<python::VariableExpr>(&self));
        return py::getattr(var_expr, py::str(name));
      });

  // ---------------------------------------------------------------------------
  // MPConstraint (bound as Constraint)
  // ---------------------------------------------------------------------------
  py::class_<MPConstraint> constraint_class(m, "Constraint");
  constraint_class
      .def("SetCoefficient", &MPConstraint::SetCoefficient, py::arg("var"),
           py::arg("coeff"))
      .def("GetCoefficient", &MPConstraint::GetCoefficient, py::arg("var"))
      .def("lb", &MPConstraint::lb)
      .def("ub", &MPConstraint::ub)
      .def("Lb", &MPConstraint::lb)
      .def("Ub", &MPConstraint::ub)
      .def("SetLb", &MPConstraint::SetLB, py::arg("lb"))
      .def("SetUb", &MPConstraint::SetUB, py::arg("ub"))
      .def("SetBounds", &MPConstraint::SetBounds, py::arg("lb"), py::arg("ub"))
      .def("set_is_lazy", &MPConstraint::set_is_lazy, py::arg("laziness"))
      .def("Clear", &MPConstraint::Clear)
      .def("name", &MPConstraint::name)
      .def("index", &MPConstraint::index)
      .def("dual_value", &MPConstraint::dual_value)
      .def("DualValue", &MPConstraint::dual_value)
      .def("basis_status", &MPConstraint::basis_status);

  // ---------------------------------------------------------------------------
  // MPObjective (bound as Objective)
  // ---------------------------------------------------------------------------
  py::class_<MPObjective> objective_class(m, "Objective");
  objective_class.def("Clear", &MPObjective::Clear)
      .def("SetCoefficient", &MPObjective::SetCoefficient, py::arg("var"),
           py::arg("coeff"))
      .def("GetCoefficient", &MPObjective::GetCoefficient, py::arg("var"))
      .def("SetMinimization", &MPObjective::SetMinimization)
      .def("SetMaximization", &MPObjective::SetMaximization)
      .def("SetOptimizationDirection", &MPObjective::SetOptimizationDirection,
           py::arg("maximize"))
      .def("SetOffset", &MPObjective::SetOffset, py::arg("value"))
      .def(
          "AddOffset",
          [](MPObjective& self, double value) {
            self.SetOffset(self.offset() + value);
          },
          py::arg("value"))
      .def("offset", &MPObjective::offset)
      .def("Offset", &MPObjective::offset)
      .def("minimization", &MPObjective::minimization)
      .def("maximization", &MPObjective::maximization)
      .def("Value", &MPObjective::Value)
      .def("BestBound", &MPObjective::BestBound);

  // ---------------------------------------------------------------------------
  // MPSolverParameters
  // ---------------------------------------------------------------------------
  py::class_<MPSolverParameters> params_class(m, "MPSolverParameters");
  params_class.def(py::init<>())
      .def(
          "SetDoubleParam",
          [](MPSolverParameters& self, int param, double value) {
            self.SetDoubleParam(
                static_cast<MPSolverParameters::DoubleParam>(param), value);
          },
          py::arg("param"), py::arg("value"))
      .def(
          "GetDoubleParam",
          [](const MPSolverParameters& self, int param) {
            return self.GetDoubleParam(
                static_cast<MPSolverParameters::DoubleParam>(param));
          },
          py::arg("param"))
      .def(
          "SetIntegerParam",
          [](MPSolverParameters& self, int param, int value) {
            self.SetIntegerParam(
                static_cast<MPSolverParameters::IntegerParam>(param), value);
          },
          py::arg("param"), py::arg("value"))
      .def(
          "GetIntegerParam",
          [](const MPSolverParameters& self, int param) {
            return self.GetIntegerParam(
                static_cast<MPSolverParameters::IntegerParam>(param));
          },
          py::arg("param"));

  py::enum_<MPSolverParameters::DoubleParam>(params_class, "DoubleParam")
      .value("RELATIVE_MIP_GAP", MPSolverParameters::RELATIVE_MIP_GAP)
      .value("PRIMAL_TOLERANCE", MPSolverParameters::PRIMAL_TOLERANCE)
      .value("DUAL_TOLERANCE", MPSolverParameters::DUAL_TOLERANCE)
      .export_values();

  py::enum_<MPSolverParameters::IntegerParam>(params_class, "IntegerParam")
      .value("PRESOLVE", MPSolverParameters::PRESOLVE)
      .value("LP_ALGORITHM", MPSolverParameters::LP_ALGORITHM)
      .value("INCREMENTALITY", MPSolverParameters::INCREMENTALITY)
      .value("SCALING", MPSolverParameters::SCALING)
      .export_values();

  py::enum_<MPSolverParameters::PresolveValues>(params_class, "PresolveValues")
      .value("PRESOLVE_OFF", MPSolverParameters::PRESOLVE_OFF)
      .value("PRESOLVE_ON", MPSolverParameters::PRESOLVE_ON)
      .export_values();

  py::enum_<MPSolverParameters::LpAlgorithmValues>(params_class,
                                                   "LpAlgorithmValues")
      .value("DUAL", MPSolverParameters::DUAL)
      .value("PRIMAL", MPSolverParameters::PRIMAL)
      .value("BARRIER", MPSolverParameters::BARRIER)
      .export_values();

  py::enum_<MPSolverParameters::IncrementalityValues>(params_class,
                                                      "IncrementalityValues")
      .value("INCREMENTALITY_OFF", MPSolverParameters::INCREMENTALITY_OFF)
      .value("INCREMENTALITY_ON", MPSolverParameters::INCREMENTALITY_ON)
      .export_values();

  py::enum_<MPSolverParameters::ScalingValues>(params_class, "ScalingValues")
      .value("SCALING_OFF", MPSolverParameters::SCALING_OFF)
      .value("SCALING_ON", MPSolverParameters::SCALING_ON)
      .export_values();

  params_class.attr("kDefaultRelativeMipGap") =
      MPSolverParameters::kDefaultRelativeMipGap;
  params_class.attr("kDefaultPrimalTolerance") =
      MPSolverParameters::kDefaultPrimalTolerance;
  params_class.attr("kDefaultDualTolerance") =
      MPSolverParameters::kDefaultDualTolerance;
  params_class.attr("kDefaultPresolve") = MPSolverParameters::kDefaultPresolve;
  params_class.attr("kDefaultIncrementality") =
      MPSolverParameters::kDefaultIncrementality;

  // ---------------------------------------------------------------------------
  // ModelExportOptions
  // ---------------------------------------------------------------------------
  py::class_<MPModelExportOptions> model_export_options_class(
      m, "ModelExportOptions");
  model_export_options_class.def(py::init<>())
      .def_readwrite("obfuscate", &MPModelExportOptions::obfuscate)
      .def_readwrite("log_invalid_names",
                     &MPModelExportOptions::log_invalid_names)
      .def_readwrite("show_unused_variables",
                     &MPModelExportOptions::show_unused_variables)
      .def_readwrite("max_line_length", &MPModelExportOptions::max_line_length);

  // ---------------------------------------------------------------------------
  // Free functions
  // ---------------------------------------------------------------------------
  m.def(
      "ExportModelAsLpFormat",
      [](const MPModelProto& input_model, const MPModelExportOptions& options) {
        return ExportModelAsLpFormat(input_model, options).value_or("");
      },
      py::arg("input_model"), py::arg("options") = MPModelExportOptions());

  m.def(
      "ExportModelAsMpsFormat",
      [](const MPModelProto& input_model, const MPModelExportOptions& options) {
        return ExportModelAsMpsFormat(input_model, options).value_or("");
      },
      py::arg("input_model"), py::arg("options") = MPModelExportOptions());

  m.def(
      "FindErrorInModelProto",
      [](const MPModelProto& input_model) {
        return operations_research::FindErrorInMPModelProto(input_model);
      },
      py::arg("input_model"));

  // ---------------------------------------------------------------------------
  // Linear solver natural API: C++ implementation and bindings
  // ---------------------------------------------------------------------------
  py::class_<python::FakeMPVariableRepresentingTheConstantOffset>(
      m, "_FakeMPVariableRepresentingTheConstantOffset")
      .def(py::init<>())
      .def("solution_value",
           &python::FakeMPVariableRepresentingTheConstantOffset::solution_value)
      .def("__repr__",
           &python::FakeMPVariableRepresentingTheConstantOffset::repr)
      .def("__str__",
           &python::FakeMPVariableRepresentingTheConstantOffset::repr);

  py::object offset_key_obj =
      py::cast(python::FakeMPVariableRepresentingTheConstantOffset());
  m.attr("OFFSET_KEY") = offset_key_obj;
  python::g_offset_key = m.attr("OFFSET_KEY");
  m.attr("inf") = std::numeric_limits<double>::infinity();

  py::class_<python::LinearExpr, std::shared_ptr<python::LinearExpr>>
      linear_expr_class(m, "LinearExpr");
  linear_expr_class.def("solution_value", &python::LinearExpr::SolutionValue)
      .def("SolutionValue", &python::LinearExpr::SolutionValue)
      .def("GetCoeffs",
           [](const python::LinearExpr& self) {
             py::object defaultdict =
                 py::module_::import("collections").attr("defaultdict");
             py::object float_type =
                 py::module_::import("builtins").attr("float");
             py::object dict = defaultdict(float_type);
             auto [coeffs, offset] = self.GetCoeffs();
             CHECK(!python::g_offset_key.is_none());
             dict[python::g_offset_key] = offset;
             for (const auto& [var, coeff] : coeffs) {
               dict[py::cast(var, py::return_value_policy::reference)] = coeff;
             }
             return dict;
           })
      .def("__str__", &python::LinearExpr::ToString)
      .def("__repr__", &python::LinearExpr::ToString)
      .def(
          "__add__",
          [](const python::LinearExpr& self,
             const python::LinearExprTerm& expr) {
            absl::InlinedVector<std::shared_ptr<python::LinearExpr>, 2> items;
            items.push_back(
                const_cast<python::LinearExpr&>(self).shared_from_this());
            items.push_back(python::AsLinearExpr(expr));
            return std::make_shared<python::SumArray>(std::move(items));
          },
          py::arg("expr"))
      .def(
          "__radd__",
          [](const python::LinearExpr& self,
             const python::LinearExprTerm& cst) {
            absl::InlinedVector<std::shared_ptr<python::LinearExpr>, 2> items;
            items.push_back(
                const_cast<python::LinearExpr&>(self).shared_from_this());
            items.push_back(python::AsLinearExpr(cst));
            return std::make_shared<python::SumArray>(std::move(items));
          },
          py::arg("cst"))
      .def(
          "__sub__",
          [](const python::LinearExpr& self,
             const python::LinearExprTerm& expr) {
            absl::InlinedVector<std::shared_ptr<python::LinearExpr>, 2> items;
            items.push_back(
                const_cast<python::LinearExpr&>(self).shared_from_this());
            items.push_back(std::make_shared<python::ProductCst>(
                python::AsLinearExpr(expr), -1.0));
            return std::make_shared<python::SumArray>(std::move(items));
          },
          py::arg("expr"))
      .def(
          "__rsub__",
          [](const python::LinearExpr& self,
             const python::LinearExprTerm& cst) {
            absl::InlinedVector<std::shared_ptr<python::LinearExpr>, 2> items;
            items.push_back(std::make_shared<python::ProductCst>(
                const_cast<python::LinearExpr&>(self).shared_from_this(),
                -1.0));
            items.push_back(python::AsLinearExpr(cst));
            return std::make_shared<python::SumArray>(std::move(items));
          },
          py::arg("cst"))
      .def(
          "__mul__",
          [](const python::LinearExpr& self, double cst) {
            return std::make_shared<python::ProductCst>(
                const_cast<python::LinearExpr&>(self).shared_from_this(), cst);
          },
          py::arg("cst"))
      .def(
          "__rmul__",
          [](const python::LinearExpr& self, double cst) {
            return std::make_shared<python::ProductCst>(
                const_cast<python::LinearExpr&>(self).shared_from_this(), cst);
          },
          py::arg("cst"))
      .def(
          "__div__",
          [](const python::LinearExpr& self, double cst) {
            if (cst == 0.0) {
              PyErr_SetString(PyExc_ZeroDivisionError, "division by zero");
              throw py::error_already_set();
            }
            return std::make_shared<python::ProductCst>(
                const_cast<python::LinearExpr&>(self).shared_from_this(),
                1.0 / cst);
          },
          py::arg("cst"))
      .def(
          "__truediv__",
          [](const python::LinearExpr& self, double cst) {
            if (cst == 0.0) {
              PyErr_SetString(PyExc_ZeroDivisionError, "division by zero");
              throw py::error_already_set();
            }
            return std::make_shared<python::ProductCst>(
                const_cast<python::LinearExpr&>(self).shared_from_this(),
                1.0 / cst);
          },
          py::arg("cst"))
      .def("__neg__",
           [](const python::LinearExpr& self) {
             return std::make_shared<python::ProductCst>(
                 const_cast<python::LinearExpr&>(self).shared_from_this(),
                 -1.0);
           })
      .def(
          "__eq__",
          [](const python::LinearExpr& self,
             const python::LinearExprTerm& arg) {
            if (std::holds_alternative<double>(arg)) {
              double val = std::get<double>(arg);
              return std::make_shared<python::LinearConstraint>(
                  const_cast<python::LinearExpr&>(self).shared_from_this(), val,
                  val);
            } else {
              absl::InlinedVector<std::shared_ptr<python::LinearExpr>, 2> items;
              items.push_back(
                  const_cast<python::LinearExpr&>(self).shared_from_this());
              items.push_back(std::make_shared<python::ProductCst>(
                  python::AsLinearExpr(arg), -1.0));
              auto diff = std::make_shared<python::SumArray>(std::move(items));
              return std::make_shared<python::LinearConstraint>(diff, 0.0, 0.0);
            }
          },
          py::arg("arg"))
      .def(
          "__ge__",
          [](const python::LinearExpr& self,
             const python::LinearExprTerm& arg) {
            double inf = std::numeric_limits<double>::infinity();
            if (std::holds_alternative<double>(arg)) {
              double val = std::get<double>(arg);
              return std::make_shared<python::LinearConstraint>(
                  const_cast<python::LinearExpr&>(self).shared_from_this(), val,
                  inf);
            } else {
              absl::InlinedVector<std::shared_ptr<python::LinearExpr>, 2> items;
              items.push_back(
                  const_cast<python::LinearExpr&>(self).shared_from_this());
              items.push_back(std::make_shared<python::ProductCst>(
                  python::AsLinearExpr(arg), -1.0));
              auto diff = std::make_shared<python::SumArray>(std::move(items));
              return std::make_shared<python::LinearConstraint>(diff, 0.0, inf);
            }
          },
          py::arg("arg"))
      .def(
          "__le__",
          [](const python::LinearExpr& self,
             const python::LinearExprTerm& arg) {
            double inf = std::numeric_limits<double>::infinity();
            if (std::holds_alternative<double>(arg)) {
              double val = std::get<double>(arg);
              return std::make_shared<python::LinearConstraint>(
                  const_cast<python::LinearExpr&>(self).shared_from_this(),
                  -inf, val);
            } else {
              absl::InlinedVector<std::shared_ptr<python::LinearExpr>, 2> items;
              items.push_back(
                  const_cast<python::LinearExpr&>(self).shared_from_this());
              items.push_back(std::make_shared<python::ProductCst>(
                  python::AsLinearExpr(arg), -1.0));
              auto diff = std::make_shared<python::SumArray>(std::move(items));
              return std::make_shared<python::LinearConstraint>(diff, -inf,
                                                                0.0);
            }
          },
          py::arg("arg"))
      .def(
          "__lt__",
          [](const python::LinearExpr& /*self*/,
             const python::LinearExprTerm& /*arg*/) {
            throw py::value_error(
                "Operators \"<\" and \">\" not supported with the linear "
                "solver");
          },
          py::arg("arg"))
      .def(
          "__gt__",
          [](const python::LinearExpr& /*self*/,
             const python::LinearExprTerm& /*arg*/) {
            throw py::value_error(
                "Operators \"<\" and \">\" not supported with the linear "
                "solver");
          },
          py::arg("arg"))
      .def(
          "__ne__",
          [](const python::LinearExpr& /*self*/,
             const python::LinearExprTerm& /*arg*/) {
            throw py::value_error(
                "Operator \"!=\" not supported with the linear solver");
          },
          py::arg("arg"));

  py::list overridden_operator_methods;
  for (const char* opname : {
           "__add__",
           "__radd__",
           "__sub__",
           "__rsub__",
           "__mul__",
           "__rmul__",
           "__div__",
           "__truediv__",
           "__neg__",
           "__eq__",
           "__ge__",
           "__le__",
           "__gt__",
           "__lt__",
           "__ne__",
       }) {
    overridden_operator_methods.append(opname);
  }
  linear_expr_class.attr("OVERRIDDEN_OPERATOR_METHODS") =
      overridden_operator_methods;

  py::class_<python::VariableExpr, python::LinearExpr,
             std::shared_ptr<python::VariableExpr>>(m, "VariableExpr")
      .def(py::init<const MPVariable*>(), py::arg("mpvar"))
      .def("__str__", &python::VariableExpr::ToString)
      .def("__repr__", &python::VariableExpr::ToString);

  py::class_<python::ProductCst, python::LinearExpr,
             std::shared_ptr<python::ProductCst>>(m, "ProductCst")
      .def(py::init<const python::LinearExprTerm&, double>(), py::arg("expr"),
           py::arg("coef"))
      .def("__str__", &python::ProductCst::ToString)
      .def("__repr__", &python::ProductCst::ToString);

  py::class_<python::Constant, python::LinearExpr,
             std::shared_ptr<python::Constant>>(m, "Constant")
      .def(py::init<double>(), py::arg("val"))
      .def("__str__", &python::Constant::ToString)
      .def("__repr__", &python::Constant::ToString);

  py::class_<python::SumArray, python::LinearExpr,
             std::shared_ptr<python::SumArray>>(m, "SumArray")
      .def(py::init<const python::SumArray::SumInput&>(), py::arg("array"))
      .def("__str__", &python::SumArray::ToString)
      .def("__repr__", &python::SumArray::ToString);

  py::class_<python::LinearConstraint,
             std::shared_ptr<python::LinearConstraint>>
      linear_constraint_class(m, "LinearConstraint");
  linear_constraint_class
      .def(py::init<const python::LinearExprTerm&, double, double>(),
           py::arg("expr"), py::arg("lb"), py::arg("ub"))
      .def("__str__", &python::LinearConstraint::ToString)
      .def("__repr__", &python::LinearConstraint::ToString)
      .def("Extract", &python::LinearConstraint::Extract, py::arg("solver"),
           py::arg("name") = "", py::return_value_policy::reference);

  m.def("Sum", &python::MakeSum);
  m.attr("SumCst") = m.attr("Sum");
  m.def("CastToLinExp", &python::CastToLinExp, py::arg("v"));

  using ConstraintVariant =
      std::variant<std::shared_ptr<python::LinearConstraint>, bool>;

  solver_class.def(
      "Add",
      [](MPSolver& self, const ConstraintVariant& constraint,
         const std::string& name) -> MPConstraint* {
        if (std::holds_alternative<bool>(constraint)) {
          if (std::get<bool>(constraint)) {
            return self.MakeRowConstraint(0.0, 0.0, name);
          } else {
            return self.MakeRowConstraint(1.0, 1.0, name);
          }
        }
        return std::get<std::shared_ptr<python::LinearConstraint>>(constraint)
            ->Extract(self, name);
      },
      py::arg("constraint"), py::arg("name") = "",
      py::return_value_policy::reference);

  solver_class.def(
      "Sum",
      [](MPSolver& /*self*/, const python::SumArray::SumInput& expr_array) {
        return std::make_shared<python::SumArray>(expr_array);
      },
      py::arg("expr_array"));

  solver_class.def("RowConstraint", [](MPSolver& self, py::args args,
                                       py::kwargs kwargs) {
    py::object py_self = py::cast(&self, py::return_value_policy::reference);
    return py_self.attr("Constraint")(*args, **kwargs);
  });

  auto set_objective_from_expr = [](MPSolver& self, bool maximize,
                                    const python::LinearExprTerm& expr) {
    MPObjective* objective = self.MutableObjective();
    objective->Clear();
    if (maximize) {
      objective->SetMaximization();
    } else {
      objective->SetMinimization();
    }
    if (std::holds_alternative<double>(expr)) {
      objective->SetOffset(std::get<double>(expr));
      return;
    }
    std::shared_ptr<python::LinearExpr> lin_expr = python::AsLinearExpr(expr);
    auto [coeffs, offset] = lin_expr->GetCoeffs();
    objective->SetOffset(offset);
    for (const auto& [var, c] : coeffs) {
      objective->SetCoefficient(var, c);
    }
  };  // NOLINT(readability/fn_size)

  solver_class.def(
      "Minimize",
      [set_objective_from_expr](MPSolver& self,
                                const python::LinearExprTerm& expr) {
        set_objective_from_expr(self, false, expr);
      },
      py::arg("expr"));

  solver_class.def(
      "Maximize",
      [set_objective_from_expr](MPSolver& self,
                                const python::LinearExprTerm& expr) {
        set_objective_from_expr(self, true, expr);
      },
      py::arg("expr"));

  auto setup_variable_operator =
      [variable_class](const std::string& opname) mutable {
        variable_class.def(
            opname.c_str(), [opname](const MPVariable& self, py::args args) {
              py::object var_expr =
                  py::cast(std::make_shared<python::VariableExpr>(&self));
              return var_expr.attr(opname.c_str())(*args);
            });
      };
  m.def("setup_variable_operator", setup_variable_operator, py::arg("opname"));

  for (py::handle op : overridden_operator_methods) {
    std::string opname = op.cast<std::string>();
    setup_variable_operator(opname);
  }
}  // NOLINT(readability/fn_size)

}  // namespace operations_research
