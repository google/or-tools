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

#include "ortools/linear_solver/proto_solver/xpress_proto_solver.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/cleanup/cleanup.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "ortools/base/logging.h"
#include "ortools/base/status_macros.h"
#include "ortools/base/timer.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_validator.h"
#include "ortools/util/lazy_mutable_copy.h"
#include "ortools/xpress/environment.h"

namespace operations_research {

// namespace {
// constexpr int XPRS_OK = 0;

// bool XPressCodeToInvalidResponse(int error_code, const char* source_file,
//                                  int source_line, const char* statement,
//                                  XPRSprob prob, MPSolutionResponse* response)
//                                  {
//   if (error_code == XPRS_OK) return true;
//   response->set_status();
//   response->set_status_message(absl::StrFormat(
//       "XPress error code %d (file '%s', line %d) on '%s': %s", error_code,
//       source_file, source_line, statement, XPRSgeterrormsg(prob)));
//   return false;
// }

// int AddIndicatorConstraint(const MPGeneralConstraintProto& gen_cst,
//                            XPRSprob xpress_model,
//                            std::vector<int>* tmp_variables,
//                            std::vector<double>* tmp_coefficients) {
//   CHECK(xpress_model != nullptr);
//   CHECK(tmp_variables != nullptr);
//   CHECK(tmp_coefficients != nullptr);

//   const auto& ind_cst = gen_cst.indicator_constraint();
//   MPConstraintProto cst = ind_cst.constraint();
//   if (cst.lower_bound() > -std::numeric_limits<double>::infinity()) {
//     int status = XPRSaddgenconstrIndicator(
//         xpress_model, gen_cst.name().c_str(), ind_cst.var_index(),
//         ind_cst.var_value(), cst.var_index_size(),
//         cst.mutable_var_index()->mutable_data(),
//         cst.mutable_coefficient()->mutable_data(),
//         cst.upper_bound() == cst.lower_bound() ? XPRS_EQUAL
//                                                : XPRS_GREATER_EQUAL,
//         cst.lower_bound());
//     if (status != XPRS_OK) return status;
//   }
//   if (cst.upper_bound() < std::numeric_limits<double>::infinity() &&
//       cst.lower_bound() != cst.upper_bound()) {
//     return XPRSaddgenconstrIndicator(xpress_model, gen_cst.name().c_str(),
//                                      ind_cst.var_index(),
//                                      ind_cst.var_value(),
//                                      cst.var_index_size(),
//                                      cst.mutable_var_index()->mutable_data(),
//                                      cst.mutable_coefficient()->mutable_data(),
//                                      XPRS_LESS_EQUAL, cst.upper_bound());
//   }

//   return XPRS_OK;
// }

// int AddSosConstraint(const MPSosConstraint& sos_cst, XPRSprob xpress_model,
//                      std::vector<int>* tmp_variables,
//                      std::vector<double>* tmp_weights) {
//   CHECK(xpress_model != nullptr);
//   CHECK(tmp_variables != nullptr);
//   CHECK(tmp_weights != nullptr);

//   tmp_variables->resize(sos_cst.var_index_size(), 0);
//   for (int v = 0; v < sos_cst.var_index_size(); ++v) {
//     (*tmp_variables)[v] = sos_cst.var_index(v);
//   }
//   tmp_weights->resize(sos_cst.var_index_size(), 0);
//   if (sos_cst.weight_size() == sos_cst.var_index_size()) {
//     for (int w = 0; w < sos_cst.weight_size(); ++w) {
//       (*tmp_weights)[w] = sos_cst.weight(w);
//     }
//   } else {
//     DCHECK_EQ(sos_cst.weight_size(), 0);
//     // XPress requires variable weights in their SOS constraints.
//     std::iota(tmp_weights->begin(), tmp_weights->end(), 1);
//   }

//   std::vector<int> types = {sos_cst.type() == MPSosConstraint::SOS1_DEFAULT
//                                 ? XPRS_SOS_TYPE1
//                                 : XPRS_SOS_TYPE2};
//   std::vector<int> begins = {0};
//   return XPRSaddsos(xpress_model, /*numsos=*/1,
//                     /*nummembers=*/sos_cst.var_index_size(),
//                     /*types=*/types.data(),
//                     /*beg=*/begins.data(), /*ind=*/tmp_variables->data(),
//                     /*weight*/ tmp_weights->data());
// }

// int AddQuadraticConstraint(const MPGeneralConstraintProto& gen_cst,
//                            XPRSprob xpress_model) {
//   CHECK(xpress_model != nullptr);
//   constexpr double kInfinity = std::numeric_limits<double>::infinity();

//   CHECK(gen_cst.has_quadratic_constraint());
//   const MPQuadraticConstraint& quad_cst = gen_cst.quadratic_constraint();

//   auto addqconstr = [](XPRSprob xpress_model, MPQuadraticConstraint quad_cst,
//                        char sense, double rhs, const std::string& name) {
//     return XPRSaddqconstr(
//         xpress_model,
//         /*numlnz=*/quad_cst.var_index_size(),
//         /*lind=*/quad_cst.mutable_var_index()->mutable_data(),
//         /*lval=*/quad_cst.mutable_coefficient()->mutable_data(),
//         /*numqnz=*/quad_cst.qvar1_index_size(),
//         /*qrow=*/quad_cst.mutable_qvar1_index()->mutable_data(),
//         /*qcol=*/quad_cst.mutable_qvar2_index()->mutable_data(),
//         /*qval=*/quad_cst.mutable_qcoefficient()->mutable_data(),
//         /*sense=*/sense,
//         /*rhs=*/rhs,
//         /*QCname=*/name.c_str());
//   };

//   if (quad_cst.has_lower_bound() && quad_cst.lower_bound() > -kInfinity) {
//     const int xprs_status =
//         addqconstr(xpress_model, gen_cst.quadratic_constraint(),
//                    XPRS_GREATER_EQUAL, quad_cst.lower_bound(),
//                    gen_cst.has_name() ? gen_cst.name() + "_lb" : "");
//     if (xprs_status != XPRS_OK) return xprs_status;
//   }
//   if (quad_cst.has_upper_bound() && quad_cst.upper_bound() < kInfinity) {
//     const int xprs_status =
//         addqconstr(xpress_model, gen_cst.quadratic_constraint(),
//                    XPRS_LESS_EQUAL, quad_cst.upper_bound(),
//                    gen_cst.has_name() ? gen_cst.name() + "_ub" : "");
//     if (xprs_status != XPRS_OK) return xprs_status;
//   }

//   return XPRS_OK;
// }

// int AddAndConstraint(const MPGeneralConstraintProto& gen_cst,
//                      XPRSprob xpress_model, std::vector<int>* tmp_variables)
//                      {
//   CHECK(xpress_model != nullptr);
//   CHECK(tmp_variables != nullptr);

//   auto and_cst = gen_cst.and_constraint();
//   return XPRSaddgenconstrAnd(
//       xpress_model,
//       /*name=*/gen_cst.name().c_str(),
//       /*resvar=*/and_cst.resultant_var_index(),
//       /*nvars=*/and_cst.var_index_size(),
//       /*vars=*/and_cst.mutable_var_index()->mutable_data());
// }

// int AddOrConstraint(const MPGeneralConstraintProto& gen_cst,
//                     XPRSprob xpress_model, std::vector<int>* tmp_variables) {
//   CHECK(xpress_model != nullptr);
//   CHECK(tmp_variables != nullptr);

//   auto or_cst = gen_cst.or_constraint();
//   return XPRSaddgenconstrOr(
//       xpress_model,
//       /*name=*/gen_cst.name().c_str(),
//       /*resvar=*/or_cst.resultant_var_index(),
//       /*nvars=*/or_cst.var_index_size(),
//       /*vars=*/or_cst.mutable_var_index()->mutable_data());
// }

// int AddMinConstraint(const MPGeneralConstraintProto& gen_cst,
//                      XPRSprob xpress_model, std::vector<int>* tmp_variables)
//                      {
//   CHECK(xpress_model != nullptr);
//   CHECK(tmp_variables != nullptr);

//   auto min_cst = gen_cst.min_constraint();
//   return XPRSaddgenconstrMin(
//       xpress_model,
//       /*name=*/gen_cst.name().c_str(),
//       /*resvar=*/min_cst.resultant_var_index(),
//       /*nvars=*/min_cst.var_index_size(),
//       /*vars=*/min_cst.mutable_var_index()->mutable_data(),
//       /*constant=*/min_cst.has_constant()
//           ? min_cst.constant()
//           : std::numeric_limits<double>::infinity());
// }

// int AddMaxConstraint(const MPGeneralConstraintProto& gen_cst,
//                      XPRSprob xpress_model, std::vector<int>* tmp_variables)
//                      {
//   CHECK(xpress_model != nullptr);
//   CHECK(tmp_variables != nullptr);

//   auto max_cst = gen_cst.max_constraint();
//   return XPRSaddgenconstrMax(
//       xpress_model,
//       /*name=*/gen_cst.name().c_str(),
//       /*resvar=*/max_cst.resultant_var_index(),
//       /*nvars=*/max_cst.var_index_size(),
//       /*vars=*/max_cst.mutable_var_index()->mutable_data(),
//       /*constant=*/max_cst.has_constant()
//           ? max_cst.constant()
//           : -std::numeric_limits<double>::infinity());
// }
// }  // namespace

// std::string SetSolverSpecificParameters(absl::string_view parameters,
//                                         XPRSprob xpress) {
//   if (parameters.empty()) return absl::OkStatus();
//   std::vector<std::string> error_messages;
//   for (absl::string_view line : absl::StrSplit(parameters, '\n')) {
//     // Empty lines are simply ignored.
//     if (line.empty()) continue;
//     // Comment tokens end at the next new-line, or the end of the string.
//     // The first character must be '#'
//     if (line[0] == '#') continue;
//     for (absl::string_view token :
//          absl::StrSplit(line, ',', absl::SkipWhitespace())) {
//       if (token.empty()) continue;
//       std::vector<std::string> key_value =
//           absl::StrSplit(token, absl::ByAnyChar(" ="),
//           absl::SkipWhitespace());
//       // If one parameter fails, we keep processing the list of parameters.
//       if (key_value.size() != 2) {
//         const std::string current_message =
//             absl::StrCat("Cannot parse parameter '", token,
//                          "'. Expected format is 'ParameterName value' or "
//                          "'ParameterName=value'");
//         error_messages.push_back(current_message);
//         continue;
//       }
//       const int xpress_code =
//           XPRSsetparam(xpress, key_value[0].c_str(), key_value[1].c_str());
//       if (xpress_code != XPRS_OK) {
//         const std::string current_message = absl::StrCat(
//             "Error setting parameter '", key_value[0], "' to value '",
//             key_value[1], "': ", XPRSgeterrormsg(xpress));
//         error_messages.push_back(current_message);
//         continue;
//       }
//       VLOG(2) << absl::StrCat("Set parameter '", key_value[0], "' to value
//       '",
//                               key_value[1]);
//     }
//   }

//   if (error_messages.empty()) return "";
//   return absl::StrJoin(error_messages, "\n");
// }

MPSolutionResponse XPressSolveProto(LazyMutableCopy<MPModelRequest> request) {
  MPSolutionResponse response;
  response.set_status(MPSolverResponseStatus::MPSOLVER_SOLVER_TYPE_UNAVAILABLE);

  //   const absl::optional<LazyMutableCopy<MPModelProto>> optional_model =
  //       ExtractValidMPModelOrPopulateResponseStatus(request, &response);
  //   if (!optional_model) return response;
  //   const MPModelProto& model = optional_model->get();

  //   // We set `xpress_env` to point to a new environment if no existing one
  //   is
  //   // provided. We must make sure that we free this environment when we exit
  //   this
  //   // function.
  //   bool xpress_env_was_created = false;
  //   auto xpress_env_deleter = absl::MakeCleanup([&]() {
  //     if (xpress_env_was_created && xpress_env != nullptr) {
  //       XPRSfreeenv(xpress_env);
  //     }
  //   });
  //   if (xpress_env == nullptr) {
  //     ASSIGN_OR_RETURN(xpress_env, GetXPressEnv());
  //     xpress_env_was_created = true;
  //   }

  //   XPRSprob xpress_model = nullptr;
  //   auto xpress_model_deleter = absl::MakeCleanup([&]() {
  //     const int error_code = XPRSfreemodel(xpress_model);
  //     LOG_IF(DFATAL, error_code != XPRS_OK)
  //         << "XPRSfreemodel failed with error " << error_code << ": "
  //         << XPRSgeterrormsg(xpress_env);
  //   });

  // // `xpress_env` references ther XPRSenv argument.
  // #define RETURN_IF_XPRESS_ERROR(x) \
//   RETURN_IF_ERROR(                \
//       if (!XPressCodeToInvalidResponse(x, __FILE__, __LINE__, #x, xpress,
  //       &response)) { \
//         return response; \
//       })

  //   RETURN_IF_XPRESS_ERROR(XPRSnewmodel(xpress_env, &xpress_model,
  //                                       model.name().c_str(),
  //                                       /*numvars=*/0,
  //                                       /*obj=*/nullptr,
  //                                       /*lb=*/nullptr,
  //                                       /*ub=*/nullptr,
  //                                       /*vtype=*/nullptr,
  //                                       /*varnames=*/nullptr));
  //   XPRSprob const model_env = XPRSgetenv(xpress_model);

  //   if (request.has_solver_specific_parameters()) {
  //     const auto parameters_status = SetSolverSpecificParameters(
  //         request.solver_specific_parameters(), model_env);
  //     if (!parameters_status.ok()) {
  //       response.set_status(MPSOLVER_MODEL_INVALID_SOLVER_PARAMETERS);
  //       response.set_status_str(
  //           std::string(parameters_status.message()));  // NOLINT
  //       return response;
  //     }
  //   }
  //   if (request.solver_time_limit_seconds() > 0) {
  //     RETURN_IF_XPRESS_ERROR(
  //         XPRSsetdblparam(model_env, XPRS_DBL_PAR_TIMELIMIT,
  //                         request.solver_time_limit_seconds()));
  //   }
  //   RETURN_IF_XPRESS_ERROR(
  //       XPRSsetintparam(model_env, XPRS_INT_PAR_OUTPUTFLAG,
  //                       request.enable_internal_solver_output()));

  //   const int variable_size = model.variable_size();
  //   bool has_integer_variables = false;
  //   {
  //     std::vector<double> obj_coeffs(variable_size, 0);
  //     std::vector<double> lb(variable_size);
  //     std::vector<double> ub(variable_size);
  //     std::vector<char> ctype(variable_size);
  //     std::vector<const char*> varnames(variable_size);
  //     for (int v = 0; v < variable_size; ++v) {
  //       const MPVariableProto& variable = model.variable(v);
  //       obj_coeffs[v] = variable.objective_coefficient();
  //       lb[v] = variable.lower_bound();
  //       ub[v] = variable.upper_bound();
  //       ctype[v] = variable.is_integer() &&
  //                          request.solver_type() ==SolutionRes
  //                      : XPRS_CONTINUOUS;
  //       if (variable.is_integer()) has_integer_variables = true;
  //       if (!variable.name().empty()) varnames[v] = variable.name().c_str();
  //     }

  //     RETURN_IF_XPRESS_ERROR(
  //         XPRSaddvars(xpress_model, variable_size, 0, nullptr, nullptr,
  //         nullptr,
  //                     /*obj=*/obj_coeffs.data(),
  //                     /*lb=*/lb.data(), /*ub=*/ub.data(),
  //                     /*vtype=*/ctype.data(),
  //                     /*varnames=*/const_cast<char**>(varnames.data())));

  //     // Set solution hints if any.
  //     for (int i = 0; i < model.solution_hint().var_index_size(); ++i) {
  //       RETURN_IF_XPRESS_ERROR(XPRSsetdblattrelement(
  //           xpress_model, XPRS_DBL_ATTR_START, model.solution_hint().var_inde
  //           const absl::optional<LazyMutableCopy<MPModelProto>>
  //           optional_model =
  //       ExtractValidMPModelOrPopulateResponseStatus(request, &response);
  //   if (!optional_model) return response;
  //   const MPModelProto& model = optional_model->get();

  //   // We set `xpress_env` to point to a new environment if no existing one
  //   is
  //   // provided. We must make sure that we free this environment when we exit
  //   this
  //   // function.
  //   bool xpress_env_was_created = false;
  //   auto xpress_env_deleter = absl::MakeCleanup([&]() {
  //     if (xpress_env_was_created && xpress_env != nullptr) {
  //       XPRSfreeenv(xpress_env);
  //     }
  //   });
  //   if (xpress_env == nullptr) {
  //     ASSIGN_OR_RETURN(xpress_env, GetXPressEnv());
  //     xpress_env_was_created = true;
  //   }

  //   XPRSprob xpress_model = nullptr;
  //   auto xpress_model_deleter = absl::MakeCleanup([&]() {
  //     const int error_code = XPRSfreemodel(xpress_model);
  //     LOG_IF(DFATAL, error_code != XPRS_OK)
  //         << "XPRSfreemodel failed with error " << error_code << ": "
  //         << XPRSgeterrormsg(xpress_env);
  //   });

  // // `xpress_env` references ther XPRSenv argument.
  // #define RETURN_IF_XPRESS_ERROR(x) \
//   RETURN_IF_ERROR(                \
//       XPressCodeToUtilStatus(x, __FILE__, __LINE__, #x, xpress_env));

  //   RETURN_IF_XPRESS_ERROR(XPRSnewmodel(xpress_env, &xpress_model,
  //                                       model.name().c_str(),
  //                                       /*numvars=*/0,
  //                                       /*obj=*/nullptr,
  //                                       /*lb=*/nullptr,
  //                                       /*ub=*/nullptr,
  //                                       /*vtype=*/nullptr,
  //                                       /*varnames=*/nullptr));
  //   XPRSprob const model_env = XPRSgetenv(xpress_model);

  //   if (request.has_solver_specific_parameters()) {
  //     const auto parameters_status = SetSolverSpecificParameters(
  //         request.solver_specific_parameters(), model_env);
  //     if (!parameters_status.ok()) {
  //       response.set_status(MPSOLVER_MODEL_INVALID_SOLVER_PARAMETERS);
  //       response.set_status_str(
  //           std::string(parameters_status.message()));  // NOLINT
  //       return response;
  //     }
  //   }
  //   if (request.solver_time_limit_seconds() > 0) {
  //     RETURN_IF_XPRESS_ERROR(
  //         XPRSsetdblparam(model_env, XPRS_DBL_PAR_TIMELIMIT,
  //                         request.solver_time_limit_seconds()));
  //   }
  //   RETURN_IF_XPRESS_ERROR(
  //       XPRSsetintparam(model_env, XPRS_INT_PAR_OUTPUTFLAG,
  //                       request.enable_internal_solver_output()));

  //   const int variable_size = model.variable_size();
  //   bool has_integer_variables = false;
  //   {
  //     std::vector<double> obj_coeffs(variable_size, 0);
  //     std::vector<double> lb(variable_size);
  //     std::vector<double> ub(variable_size);
  //     std::vector<char> ctype(variable_size);
  //     std::vector<const char*> varnames(variable_size);
  //     for (int v = 0; v < variable_size; ++v) {
  //       const MPVariableProto& variable = model.variable(v);
  //       obj_coeffs[v] = variable.objective_coefficient();
  //       lb[v] = variable.lower_bound();
  //       ub[v] = variable.upper_bound();
  //       ctype[v] = variable.is_integer() &&
  //                          request.solver_type() ==
  //                              MPModelRequest::XPRESS_MIXED_INTEGER_PROGRAMMING
  //                      ? XPRS_INTEGER
  //                      : XPRS_CONTINUOUS;
  //       if (variable.is_integer()) has_integer_variables = true;
  //       if (!variable.name().empty()) varnames[v] = variable.name().c_str();
  //     }

  //     RETURN_IF_XPRESS_ERROR(
  //         XPRSaddvars(xpress_model, variable_size, 0, nullptr, nullptr,
  //         nullptr,
  //                     /*obj=*/obj_coeffs.data(),
  //                     /*lb=*/lb.data(), /*ub=*/ub.data(),
  //                     /*vtype=*/ctype.data(),
  //                     /*varnames=*/const_cast<char**>(varnames.data())));

  //     // Set solution hints if any.
  //     for (int i = 0; i < model.solution_hint().var_index_size(); ++i) {
  //       RETURN_IF_XPRESS_ERROR(XPRSsetdblattrelement(
  //           xpress_model, XPRS_DBL_ATTR_START,
  //           model.solution_hint().var_index(i),
  //           model.solution_hint().var_value(i)));
  //     }
  //   }

  //   {
  //     std::vector<int> ct_variables;
  //     std::vector<double> ct_coefficients;
  //     for (int c = 0; c < model.constraint_size(); ++c) {
  //       const MPConstraintProto& constraint = model.constraint(c);
  //       const int size = constraint.var_index_size();
  //       ct_variables.resize(size, 0);
  //       ct_coefficients.resize(size, 0);
  //       for (int i = 0; i < size; ++i) {
  //         ct_variables[i] = constraint.var_index(i);
  //         ct_coefficients[i] = constraint.coefficient(i);
  //       }
  //       // Using XPRSaddrangeconstr for constraints that don't require it
  //       adds
  //       // a slack which is not always removed by presolve.
  //       if (constraint.lower_bound() == constraint.upper_bound()) {
  //         RETURN_IF_XPRESS_ERROR(XPRSaddconstr(
  //             xpress_model, /*numnz=*/size, /*cind=*/ct_variables.data(),
  //             /*cval=*/ct_coefficients.data(),
  //             /*sense=*/XPRS_EQUAL, /*rhs=*/constraint.lower_bound(),
  //             /*constrname=*/constraint.name().c_str()));
  //       } else if (constraint.lower_bound() ==
  //                  -std::numeric_limits<double>::infinity()) {
  //         RETURN_IF_XPRESS_ERROR(XPRSaddconstr(
  //             xpress_model, /*numnz=*/size, /*cind=*/ct_variables.data(),
  //             /*cval=*/ct_coefficients.data(),
  //             /*sense=*/XPRS_LESS_EQUAL, /*rhs=*/constraint.upper_bound(),
  //             /*constrname=*/constraint.name().c_str()));
  //       } else if (constraint.upper_bound() ==
  //                  std::numeric_limits<double>::infinity()) {
  //         RETURN_IF_XPRESS_ERROR(XPRSaddconstr(
  //             xpress_model, /*numnz=*/size, /*cind=*/ct_variables.data(),
  //             /*cval=*/ct_coefficients.data(),
  //             /*sense=*/XPRS_GREATER_EQUAL, /*rhs=*/constraint.lower_bound(),
  //             /*constrname=*/constraint.name().c_str()));
  //       } else {
  //         RETURN_IF_XPRESS_ERROR(XPRSaddrangeconstr(
  //             xpress_model, /*numnz=*/size, /*cind=*/ct_variables.data(),
  //             /*cval=*/ct_coefficients.data(),
  //             /*lower=*/constraint.lower_bound(),
  //             /*upper=*/constraint.upper_bound(),
  //             /*constrname=*/constraint.name().c_str()));
  //       }
  //     }

  //     for (const auto& gen_cst : model.general_constraint()) {
  //       switch (gen_cst.general_constraint_case()) {
  //         case MPGeneralConstraintProto::kIndicatorConstraint: {
  //           RETURN_IF_XPRESS_ERROR(AddIndicatorConstraint(
  //               gen_cst, xpress_model, &ct_variables, &ct_coefficients));
  //           break;
  //         }
  //         case MPGeneralConstraintProto::kSosConstraint: {
  //           RETURN_IF_XPRESS_ERROR(AddSosConstraint(gen_cst.sos_constraint(),
  //                                                   xpress_model,
  //                                                   &ct_variables,
  //                                                   &ct_coefficients));
  //           break;
  //         }
  //         case MPGeneralConstraintProto::kQuadraticConstraint: {
  //           RETURN_IF_XPRESS_ERROR(AddQuadraticConstraint(gen_cst,
  //           xpress_model)); break;
  //         }
  //         case MPGeneralConstraintProto::kAbsConstraint: {
  //           RETURN_IF_XPRESS_ERROR(XPRSaddgenconstrAbs(
  //               xpress_model,
  //               /*name=*/gen_cst.name().c_str(),
  //               /*resvar=*/gen_cst.abs_constraint().resultant_var_index(),
  //               /*argvar=*/gen_cst.abs_constraint().var_index()));
  //           break;
  //         }
  //         case MPGeneralConstraintProto::kAndConstraint: {
  //           RETURN_IF_XPRESS_ERROR(
  //               AddAndConstraint(gen_cst, xpress_model, &ct_variables));
  //           break;
  //         }
  //         case MPGeneralConstraintProto::kOrConstraint: {
  //           RETURN_IF_XPRESS_ERROR(
  //               AddOrConstraint(gen_cst, xpress_model, &ct_variables));
  //           break;
  //         }
  //         case MPGeneralConstraintProto::kMinConstraint: {
  //           RETURN_IF_XPRESS_ERROR(
  //               AddMinConstraint(gen_cst, xpress_model, &ct_variables));
  //           break;
  //         }
  //         case MPGeneralConstraintProto::kMaxConstraint: {
  //           RETURN_IF_XPRESS_ERROR(
  //               AddMaxConstraint(gen_cst, xpress_model, &ct_variables));
  //           break;
  //         }
  //         default:
  //           return absl::UnimplementedError(
  //               absl::StrFormat("General constraints of type %i not
  //               supported.",
  //                               gen_cst.general_constraint_case()));
  //       }
  //     }
  //   }

  //   RETURN_IF_XPRESS_ERROR(XPRSsetintattr(xpress_model,
  //   XPRS_INT_ATTR_MODELSENSE,
  //                                         model.maximize() ? -1 : 1));
  //   RETURN_IF_XPRESS_ERROR(XPRSsetdblattr(xpress_model, XPRS_DBL_ATTR_OBJCON,
  //                                         model.objective_offset()));
  //   if (model.has_quadratic_objective()) {
  //     MPQuadraticObjective qobj = model.quadratic_objective();
  //     if (qobj.coefficient_size() > 0) {
  //       RETURN_IF_XPRESS_ERROR(
  //           XPRSaddqpterms(xpress_model, /*numqnz=*/qobj.coefficient_size(),
  //                          /*qrow=*/qobj.mutable_qvar1_index()->mutable_data(),
  //                          /*qcol=*/qobj.mutable_qvar2_index()->mutable_data(),
  //                          /*qval=*/qobj.mutable_coefficient()->mutable_data()));
  //     }
  //   }

  //   RETURN_IF_XPRESS_ERROR(XPRSupdatemodel(xpress_model));

  //   const absl::Time time_before = absl::Now();
  //   UserTimer user_timer;
  //   user_timer.Start();

  //   RETURN_IF_XPRESS_ERROR(XPRSoptimize(xpress_model));

  //   const absl::Duration solving_duration = absl::Now() - time_before;
  //   user_timer.Stop();
  //   VLOG(1) << "Finished solving in XPressSolveProto(), walltime = "
  //           << solving_duration << ", usertime = " <<
  //           user_timer.GetDuration();
  //   response.mutable_solve_info()->set_solve_wall_time_seconds(
  //       absl::ToDoubleSeconds(solving_duration));
  //   response.mutable_solve_info()->set_solve_user_time_seconds(
  //       absl::ToDoubleSeconds(user_timer.GetDuration()));

  //   int optimization_status = 0;
  //   RETURN_IF_XPRESS_ERROR(
  //       XPRSgetintattr(xpress_model, XPRS_INT_ATTR_STATUS,
  //       &optimization_status));
  //   int solution_count = 0;
  //   RETURN_IF_XPRESS_ERROR(
  //       XPRSgetintattr(xpress_model, XPRS_INT_ATTR_SOLCOUNT,
  //       &solution_count));
  //   switch (optimization_status) {
  //     case XPRS_OPTIMAL:
  //       response.set_status(MPSOLVER_OPTIMAL);
  //       break;
  //     case XPRS_INF_OR_UNBD:
  //       DLOG(INFO) << "XPress solve returned XPRS_INF_OR_UNBD, which we treat
  //       as "
  //                     "INFEASIBLE even though it may mean UNBOUNDED.";
  //       response.set_status_str(
  //           "The model may actually be unbounded: XPress returned "
  //           "XPRS_INF_OR_UNBD");
  //       ABSL_FALLTHROUGH_INTENDED;
  //     case XPRS_INFEASIBLE:
  //       response.set_status(MPSOLVER_INFEASIBLE);
  //       break;
  //     case XPRS_UNBOUNDED:
  //       response.set_status(MPSOLVER_UNBOUNDED);
  //       break;
  //     default: {
  //       if (solution_count > 0) {
  //         response.set_status(MPSOLVER_FEASIBLE);
  //       } else {
  //         response.set_status(MPSOLVER_NOT_SOLVED);
  //         response.set_status_str(
  //             absl::StrFormat("XPress status code %d", optimization_status));
  //       }
  //       break;
  //     }
  //   }

  //   if (solution_count > 0 && (response.status() == MPSOLVER_FEASIBLE ||
  //                              response.status() == MPSOLVER_OPTIMAL)) {
  //     double objective_value = 0;
  //     RETURN_IF_XPRESS_ERROR(
  //         XPRSgetdblattr(xpress_model, XPRS_DBL_ATTR_OBJVAL,
  //         &objective_value));
  //     response.set_objective_value(objective_value);
  //     double best_objective_bound = 0;
  //     const int error = XPRSgetdblattr(xpress_model, XPRS_DBL_ATTR_OBJBOUND,
  //                                      &best_objective_bound);
  //     if (response.status() == MPSOLVER_OPTIMAL &&
  //         error == XPRS_ERROR_DATA_NOT_AVAILABLE) {
  //       // If the presolve deletes all variables, there's no best bound.
  //       response.set_best_objective_bound(objective_value);
  //     } else {
  //       RETURN_IF_XPRESS_ERROR(error);
  //       response.set_best_objective_bound(best_objective_bound);
  //     }

  //     response.mutable_variable_value()->Resize(variable_size, 0);
  //     RETURN_IF_XPRESS_ERROR(
  //         XPRSgetdblattrarray(xpress_model, XPRS_DBL_ATTR_X, 0,
  //         variable_size,
  //                             response.mutable_variable_value()->mutable_data()));
  //     // NOTE, XPressSolveProto() is exposed to external clients via MPSolver
  //     API,
  //     // which assumes the solution values of integer variables are rounded
  //     to
  //     // integer values.
  //     auto round_values_of_integer_variables_fn =
  //         [&](google::protobuf::RepeatedField<double>* values) {
  //           for (int v = 0; v < variable_size; ++v) {
  //             if (model.variable(v).is_integer()) {
  //               (*values)[v] = std::round((*values)[v]);
  //             }
  //           }
  //         };
  //     round_values_of_integer_variables_fn(response.mutable_variable_value());
  //     if (!has_integer_variables && model.general_constraint_size() == 0) {
  //       response.mutable_dual_value()->Resize(model.constraint_size(), 0);
  //       RETURN_IF_XPRESS_ERROR(XPRSgetdblattrarray(
  //           xpress_model, XPRS_DBL_ATTR_PI, 0, model.constraint_size(),
  //           response.mutable_dual_value()->mutable_data()));
  //     }
  //     const int additional_solutions = std::min(
  //         solution_count,
  //         std::min(request.populate_additional_solutions_up_to(),
  //                                  std::numeric_limits<int32_t>::max() - 1) +
  //                             1);
  //     for (int i = 1; i < additional_solutions; ++i) {
  //       RETURN_IF_XPRESS_ERROR(
  //           XPRSsetintparam(model_env, XPRS_INT_PAR_SOLUTIONNUMBER, i));
  //       MPSolution* solution = response.add_additional_solutions();
  //       solution->mutable_variable_value()->Resize(variable_size, 0);
  //       double objective_value = 0;
  //       RETURN_IF_XPRESS_ERROR(XPRSgetdblattr(
  //           xpress_model, XPRS_DBL_ATTR_POOLOBJVAL, &objective_value));
  //       solution->set_objective_value(objective_value);
  //       RETURN_IF_XPRESS_ERROR(XPRSgetdblattrarray(
  //           xpress_model, XPRS_DBL_ATTR_XN, 0, variable_size,
  //           solution->mutable_variable_value()->mutable_data()));
  //       round_values_of_integer_variables_fn(solution->mutable_variable_value());
  //     }
  //   }
  // #undef RETURN_IF_XPRESS_ERRORx(i),
  //           model.solution_hint().var_value(i)));
  //     }
  //   }

  //   {
  //     std::vector<int> ct_variables;
  //     std::vector<double> ct_coefficients;
  //     for (int c = 0; c < model.constraint_size(); ++c) {
  //       const MPConstraintProto& constraint = model.constraint(c);
  //       const int size = constraint.var_index_size();
  //       ct_variables.resize(size, 0);
  //       ct_coefficients.resize(size, 0);
  //       for (int i = 0; i < size; ++i) {
  //         ct_variables[i] = constraint.var_index(i);
  //         ct_coefficients[i] = constraint.coefficient(i);
  //       }
  //       // Using XPRSaddrangeconstr for constraints that don't require it
  //       adds
  //       // a slack which is not always removed by presolve.
  //       if (constraint.lower_bound() == constraint.upper_bound()) {
  //         RETURN_IF_XPRESS_ERROR(XPRSaddconstr(
  //             xpress_model, /*numnz=*/size, /*cind=*/ct_variables.data(),
  //             /*cval=*/ct_coefficients.data(),
  //             /*sense=*/XPRS_EQUAL, /*rhs=*/constraint.lower_bound(),
  //             /*constrname=*/constraint.name().c_str()));
  //       } else if (constraint.lower_bound() ==
  //                  -std::numeric_limits<double>::infinity()) {
  //         RETURN_IF_XPRESS_ERROR(XPRSaddconstr(
  //             xpress_model, /*numnz=*/size, /*cind=*/ct_variables.data(),
  //             /*cval=*/ct_coefficients.data(),
  //             /*sense=*/XPRS_LESS_EQUAL, /*rhs=*/constraint.upper_bound(),
  //             /*constrname=*/constraint.name().c_str()));
  //       } else if (constraint.upper_bound() ==
  //                  std::numeric_limits<double>::infinity()) {
  //         RETURN_IF_XPRESS_ERROR(XPRSaddconstr(
  //             xpress_model, /*numnz=*/size, /*cind=*/ct_variables.data(),
  //             /*cval=*/ct_coefficients.data(),
  //             /*sense=*/XPRS_GREATER_EQUAL, /*rhs=*/constraint.lower_bound(),
  //             /*constrname=*/constraint.name().c_str()));
  //       } else {
  //         RETURN_IF_XPRESS_ERROR(XPRSaddrangeconstr(
  //             xpress_model, /*numnz=*/size, /*cind=*/ct_variables.data(),
  //             /*cval=*/ct_coefficients.data(),
  //             /*lower=*/constraint.lower_bound(),
  //             /*upper=*/constraint.upper_bound(),
  //             /*constrname=*/constraint.name().c_str()));
  //       }
  //     }

  //     for (const auto& gen_cst : model.general_constraint()) {
  //       switch (gen_cst.general_constraint_case()) {
  //         case MPGeneralConstraintProto::kIndicatorConstraint: {
  //           RETURN_IF_XPRESS_ERROR(AddIndicatorConstraint(
  //               gen_cst, xpress_model, &ct_variables, &ct_coefficients));
  //           break;
  //         }
  //         case MPGeneralConstraintProto::kSosConstraint: {
  //           RETURN_IF_XPRESS_ERROR(AddSosConstraint(gen_cst.sos_constraint(),
  //                                                   xpress_model,
  //                                                   &ct_variables,
  //                                                   &ct_coefficients));
  //           break;
  //         }
  //         case MPGeneralConstraintProto::kQuadraticConstraint: {
  //           RETURN_IF_XPRESS_ERROR(AddQuadraticConstraint(gen_cst,
  //           xpress_model)); break;
  //         }
  //         case MPGeneralConstraintProto::kAbsConstraint: {
  //           RETURN_IF_XPRESS_ERROR(XPRSaddgenconstrAbs(
  //               xpress_model,
  //               /*name=*/gen_cst.name().c_str(),
  //               /*resvar=*/gen_cst.abs_constraint().resultant_var_index(),
  //               /*argvar=*/gen_cst.abs_constraint().var_index()));
  //           break;
  //         }
  //         case MPGeneralConstraintProto::kAndConstraint: {
  //           RETURN_IF_XPRESS_ERROR(
  //               AddAndConstraint(gen_cst, xpress_model, &ct_variables));
  //           break;
  //         }
  //         case MPGeneralConstraintProto::kOrConstraint: {
  //           RETURN_IF_XPRESS_ERROR(
  //               AddOrConstraint(gen_cst, xpress_model, &ct_variables));
  //           break;
  //         }
  //         case MPGeneralConstraintProto::kMinConstraint: {
  //           RETURN_IF_XPRESS_ERROR(
  //               AddMinConstraint(gen_cst, xpress_model, &ct_variables));
  //           break;
  //         }
  //         case MPGeneralConstraintProto::kMaxConstraint: {
  //           RETURN_IF_XPRESS_ERROR(
  //               AddMaxConstraint(gen_cst, xpress_model, &ct_variables));
  //           break;
  //         }
  //         default:
  //           return absl::UnimplementedError(
  //               absl::StrFormat("General constraints of type %i not
  //               supported.",
  //                               gen_cst.general_constraint_case()));
  //       }
  //     }
  //   }

  //   RETURN_IF_XPRESS_ERROR(XPRSsetintattr(xpress_model,
  //   XPRS_INT_ATTR_MODELSENSE,
  //                                         model.maximize() ? -1 : 1));
  //   RETURN_IF_XPRESS_ERROR(XPRSsetdblattr(xpress_model, XPRS_DBL_ATTR_OBJCON,
  //                                         model.objective_offset()));
  //   if (model.has_quadratic_objective()) {
  //     MPQuadraticObjective qobj = model.quadratic_objective();
  //     if (qobj.coefficient_size() > 0) {
  //       RETURN_IF_XPRESS_ERROR(
  //           XPRSaddqpterms(xpress_model, /*numqnz=*/qobj.coefficient_size(),
  //                          /*qrow=*/qobj.mutable_qvar1_index()->mutable_data(),
  //                          /*qcol=*/qobj.mutable_qvar2_index()->mutable_data(),
  //                          /*qval=*/qobj.mutable_coefficient()->mutable_data()));
  //     }
  //   }

  //   RETURN_IF_XPRESS_ERROR(XPRSupdatemodel(xpress_model));

  //   const absl::Time time_before = absl::Now();
  //   UserTimer user_timer;
  //   user_timer.Start();

  //   RETURN_IF_XPRESS_ERROR(XPRSoptimize(xpress_model));

  //   const absl::Duration solving_duration = absl::Now() - time_before;
  //   user_timer.Stop();
  //   VLOG(1) << "Finished solving in XPressSolveProto(), walltime = "
  //           << solving_duration << ", usertime = " <<
  //           user_timer.GetDuration();
  //   response.mutable_solve_info()->set_solve_wall_time_seconds(
  //       absl::ToDoubleSeconds(solving_duration));
  //   response.mutable_solve_info()->set_solve_user_time_seconds(
  //       absl::ToDoubleSeconds(user_timer.GetDuration()));

  //   int optimization_status = 0;
  //   RETURN_IF_XPRESS_ERROR(
  //       XPRSgetintattr(xpress_model, XPRS_INT_ATTR_STATUS,
  //       &optimization_status));
  //   int solution_count = 0;
  //   RETURN_IF_XPRESS_ERROR(
  //       XPRSgetintattr(xpress_model, XPRS_INT_ATTR_SOLCOUNT,
  //       &solution_count));
  //   switch (optimization_status) {
  //     case XPRS_OPTIMAL:
  //       response.set_status(MPSOLVER_OPTIMAL);
  //       break;
  //     case XPRS_INF_OR_UNBD:
  //       DLOG(INFO) << "XPress solve returned XPRS_INF_OR_UNBD, which we treat
  //       as "
  //                     "INFEASIBLE even though it may mean UNBOUNDED.";
  //       response.set_status_str(
  //           "The model may actually be unbounded: XPress returned "
  //           "XPRS_INF_OR_UNBD");
  //       ABSL_FALLTHROUGH_INTENDED;
  //     case XPRS_INFEASIBLE:
  //       response.set_status(MPSOLVER_INFEASIBLE);
  //       break;
  //     case XPRS_UNBOUNDED:
  //       response.set_status(MPSOLVER_UNBOUNDED);
  //       break;
  //     default: {
  //       if (solution_count > 0) {
  //         response.set_status(MPSOLVER_FEASIBLE);
  //       } else {
  //         response.set_status(MPSOLVER_NOT_SOLVED);
  //         response.set_status_str(
  //             absl::StrFormat("XPress status code %d", optimization_status));
  //       }
  //       break;
  //     }
  //   }

  //   if (solution_count > 0 && (response.status() == MPSOLVER_FEASIBLE ||
  //                              response.status() == MPSOLVER_OPTIMAL)) {
  //     double objective_value = 0;
  //     RETURN_IF_XPRESS_ERROR(
  //         XPRSgetdblattr(xpress_model, XPRS_DBL_ATTR_OBJVAL,
  //         &objective_value));
  //     response.set_objective_value(objective_value);
  //     double best_objective_bound = 0;
  //     const int error = XPRSgetdblattr(xpress_model, XPRS_DBL_ATTR_OBJBOUND,
  //                                      &best_objective_bound);
  //     if (response.status() == MPSOLVER_OPTIMAL &&
  //         error == XPRS_ERROR_DATA_NOT_AVAILABLE) {
  //       // If the presolve deletes all variables, there's no best bound.
  //       response.set_best_objective_bound(objective_value);
  //     } else {
  //       RETURN_IF_XPRESS_ERROR(error);
  //       response.set_best_objective_bound(best_objective_bound);
  //     }

  //     response.mutable_variable_value()->Resize(variable_size, 0);
  //     RETURN_IF_XPRESS_ERROR(
  //         XPRSgetdblattrarray(xpress_model, XPRS_DBL_ATTR_X, 0,
  //         variable_size,
  //                             response.mutable_variable_value()->mutable_data()));
  //     // NOTE, XPressSolveProto() is exposed to external clients via MPSolver
  //     API,
  //     // which assumes the solution values of integer variables are rounded
  //     to
  //     // integer values.
  //     auto round_values_of_integer_variables_fn =
  //         [&](google::protobuf::RepeatedField<double>* values) {
  //           for (int v = 0; v < variable_size; ++v) {
  //             if (model.variable(v).is_integer()) {
  //               (*values)[v] = std::round((*values)[v]);
  //             }
  //           }
  //         };
  //     round_values_of_integer_variables_fn(response.mutable_variable_value());
  //     if (!has_integer_variables && model.general_constraint_size() == 0) {
  //       response.mutable_dual_value()->Resize(model.constraint_size(), 0);
  //       RETURN_IF_XPRESS_ERROR(XPRSgetdblattrarray(
  //           xpress_model, XPRS_DBL_ATTR_PI, 0, model.constraint_size(),
  //           response.mutable_dual_value()->mutable_data()));
  //     }
  //     const int additional_solutions = std::min(
  //         solution_count,
  //         std::min(request.populate_additional_solutions_up_to(),
  //                                  std::numeric_limits<int32_t>::max() - 1) +
  //                             1);
  //     for (int i = 1; i < additional_solutions; ++i) {
  //       RETURN_IF_XPRESS_ERROR(
  //           XPRSsetintparam(model_env, XPRS_INT_PAR_SOLUTIONNUMBER, i));
  //       MPSolution* solution = response.add_additional_solutions();
  //       solution->mutable_variable_value()->Resize(variable_size, 0);
  //       double objective_value = 0;
  //       RETURN_IF_XPRESS_ERROR(XPRSgetdblattr(
  //           xpress_model, XPRS_DBL_ATTR_POOLOBJVAL, &objective_value));
  //       solution->set_objective_value(objective_value);
  //       RETURN_IF_XPRESS_ERROR(XPRSgetdblattrarray(
  //           xpress_model, XPRS_DBL_ATTR_XN, 0, variable_size,
  //           solution->mutable_variable_value()->mutable_data()));
  //       round_values_of_integer_variables_fn(solution->mutable_variable_value());
  //     }
  //   }
  // #undef RETURN_IF_XPRESS_ERROR

  return response;
}

}  // namespace operations_research
