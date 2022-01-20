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

#include "ortools/math_opt/solvers/gurobi/g_gurobi.h"

#include <string>
#include <string_view>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "ortools/base/cleanup.h"
#include "ortools/base/source_location.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"

namespace operations_research::math_opt {

namespace {
constexpr int kGrbOk = 0;

struct UserCallbackData {
  Gurobi::Callback user_cb;
  absl::Status status = absl::OkStatus();
  Gurobi* gurobi = nullptr;
};

int GurobiCallback(GRBmodel* const model, void* const cbdata, const int where,
                   void* const usrdata) {
  CHECK(usrdata != nullptr);
  CHECK(model != nullptr);
  auto user_cb_data = static_cast<UserCallbackData*>(usrdata);
  CHECK_EQ(model, user_cb_data->gurobi->model());
  // NOTE: if a previous callback failed, we never run the callback again.
  if (!user_cb_data->status.ok()) {
    return GRB_ERROR_CALLBACK;
  }
  const Gurobi::CallbackContext context(user_cb_data->gurobi, cbdata, where);
  user_cb_data->status = user_cb_data->user_cb(context);
  if (!user_cb_data->status.ok()) {
    user_cb_data->gurobi->Terminate();
    return GRB_ERROR_CALLBACK;
  }
  return kGrbOk;
}

}  // namespace

void GurobiFreeEnv::operator()(GRBenv* const env) const {
  if (env != nullptr) {
    GRBfreeenv(env);
  }
}

absl::StatusOr<GRBenvUniquePtr> GurobiNewMasterEnv(
    const std::optional<GurobiIsvKey>& isv_key) {
  GRBenv* naked_master_env = nullptr;
  int err;
  std::string_view init_env_method;
  if (isv_key.has_value()) {
    err = GRBisqp(&naked_master_env, /*logfilename=*/
                  nullptr, isv_key->name.c_str(),
                  isv_key->application_name.c_str(), isv_key->expiration,
                  isv_key->key.c_str());
    init_env_method = "GRBisqp()";
  } else {
    err = GRBloadenv(&naked_master_env, /*logfilename=*/nullptr);
    init_env_method = "GRBloadenv()";
  }
  if (err != kGrbOk) {
    // Surprisingly, even when Gurobi fails to load the environment, it still
    // creates one. Here we make sure to free it properly.
    //
    // We can also use it with GRBgeterrormsg() to get the associated error
    // message that goes with the error and the contains additional data like
    // the user, the host and the hostid.
    const GRBenvUniquePtr master_env(naked_master_env);
    return util::InvalidArgumentErrorBuilder()
           << "failed to create Gurobi master environment, " << init_env_method
           << " returned the error (" << err
           << "): " << GRBgeterrormsg(master_env.get());
  }
  return GRBenvUniquePtr(naked_master_env);
}

absl::StatusOr<std::unique_ptr<Gurobi>> Gurobi::NewWithSharedMasterEnv(
    GRBenv* const master_env) {
  CHECK(master_env != nullptr);
  return New(nullptr, master_env);
}

absl::StatusOr<std::unique_ptr<Gurobi>> Gurobi::New(
    GRBenvUniquePtr master_env) {
  if (master_env == nullptr) {
    ASSIGN_OR_RETURN(master_env, GurobiNewMasterEnv());
  }
  GRBenv* const raw_master_env = master_env.get();
  return New(std::move(master_env), raw_master_env);
}

Gurobi::Gurobi(GRBenvUniquePtr optional_owned_master_env, GRBmodel* const model,
               GRBenv* const model_env)
    : owned_master_env_(std::move(optional_owned_master_env)),
      gurobi_model_(ABSL_DIE_IF_NULL(model)),
      model_env_(ABSL_DIE_IF_NULL(model_env)) {}

absl::StatusOr<std::unique_ptr<Gurobi>> Gurobi::New(
    GRBenvUniquePtr optional_owned_master_env, GRBenv* const master_env) {
  CHECK(master_env != nullptr);
  GRBmodel* model = nullptr;
  const int err = GRBnewmodel(master_env, &model,
                              /*Pname=*/nullptr,
                              /*numvars=*/0,
                              /*obj=*/nullptr, /*lb=*/nullptr,
                              /*ub=*/nullptr, /*vtype=*/nullptr,
                              /*varnames=*/nullptr);
  if (err != kGrbOk) {
    return util::InvalidArgumentErrorBuilder()
           << "Error creating gurobi model on GRBnewmodel(), error code: "
           << err << " message: " << GRBgeterrormsg(master_env);
  }
  CHECK(model != nullptr);
  GRBenv* const model_env = GRBgetenv(model);

  if (VLOG_IS_ON(3)) {
    int gurobi_major, gurobi_minor, gurobi_technical;
    GRBversion(&gurobi_major, &gurobi_minor, &gurobi_technical);
    VLOG(3) << absl::StrFormat(
        "Successfully created model for Gurobi v%d.%d.%d (%s)", gurobi_major,
        gurobi_minor, gurobi_technical, GRBplatform());
  }
  return absl::WrapUnique(
      new Gurobi(std::move(optional_owned_master_env), model, model_env));
}

Gurobi::~Gurobi() {
  const int err = GRBfreemodel(gurobi_model_);
  if (err != kGrbOk) {
    LOG(ERROR) << "Error freeing gurobi model, code: " << err
               << ", message: " << GRBgeterrormsg(model_env_);
  }
}

absl::Status Gurobi::ToStatus(const int grb_err, const absl::StatusCode code,
                              const absl::SourceLocation loc) const {
  if (grb_err == kGrbOk) {
    return absl::OkStatus();
  }

  return util::StatusBuilder(code)
         << "Gurobi error code: " << grb_err
         << ", message: " << GRBgeterrormsg(model_env_);
}

absl::Status Gurobi::AddVars(const absl::Span<const double> obj,
                             const absl::Span<const double> lb,
                             const absl::Span<const double> ub,
                             const absl::Span<const char> vtype,
                             const absl::Span<const std::string> names) {
  return AddVars({}, {}, {}, obj, lb, ub, vtype, names);
}

absl::Status Gurobi::AddVars(const absl::Span<const int> vbegin,
                             const absl::Span<const int> vind,
                             const absl::Span<const double> vval,
                             const absl::Span<const double> obj,
                             const absl::Span<const double> lb,
                             const absl::Span<const double> ub,
                             const absl::Span<const char> vtype,
                             const absl::Span<const std::string> names) {
  CHECK_EQ(vind.size(), vval.size());
  const int num_vars = lb.size();
  CHECK_EQ(ub.size(), num_vars);
  CHECK_EQ(vtype.size(), num_vars);
  double* c_obj = nullptr;
  if (!obj.empty()) {
    CHECK_EQ(obj.size(), num_vars);
    c_obj = const_cast<double*>(obj.data());
  }
  if (!vbegin.empty()) {
    CHECK_EQ(vbegin.size(), num_vars);
  }
  char** c_names = nullptr;
  std::vector<char*> c_names_data;
  if (!names.empty()) {
    CHECK_EQ(num_vars, names.size());
    for (const std::string& name : names) {
      c_names_data.push_back(const_cast<char*>(name.c_str()));
    }
    c_names = c_names_data.data();
  }
  return ToStatus(GRBaddvars(/*model=*/gurobi_model_, /*numvars=*/num_vars,
                             /*numnz=*/vind.size(),
                             /*vbeg=*/const_cast<int*>(vbegin.data()),
                             /*vind=*/const_cast<int*>(vind.data()),
                             /*vval=*/const_cast<double*>(vval.data()),
                             /*obj=*/c_obj,
                             /*lb=*/const_cast<double*>(lb.data()),
                             /*ub=*/const_cast<double*>(ub.data()),
                             /*vtype=*/const_cast<char*>(vtype.data()),
                             /*varnames=*/c_names));
}

absl::Status Gurobi::DelVars(const absl::Span<const int> ind) {
  return ToStatus(
      GRBdelvars(gurobi_model_, ind.size(), const_cast<int*>(ind.data())));
}

absl::Status Gurobi::AddConstrs(const absl::Span<const char> sense,
                                const absl::Span<const double> rhs,
                                const absl::Span<const std::string> names) {
  const int num_cons = sense.size();
  CHECK_EQ(rhs.size(), num_cons);
  char** c_names = nullptr;
  std::vector<char*> c_names_data;
  if (!names.empty()) {
    CHECK_EQ(num_cons, names.size());
    for (const std::string& name : names) {
      c_names_data.push_back(const_cast<char*>(name.c_str()));
    }
    c_names = c_names_data.data();
  }
  return ToStatus(GRBaddconstrs(
      /*model=*/gurobi_model_,
      /*numconstrs=*/num_cons,
      /*numnz=*/0, /*cbeg=*/nullptr, /*cind=*/nullptr,
      /*cval=*/nullptr, /*sense=*/const_cast<char*>(sense.data()),
      /*rhs=*/const_cast<double*>(rhs.data()), /*constrnames=*/c_names));
}

absl::Status Gurobi::DelConstrs(const absl::Span<const int> ind) {
  return ToStatus(
      GRBdelconstrs(gurobi_model_, ind.size(), const_cast<int*>(ind.data())));
}

absl::Status Gurobi::AddQpTerms(const absl::Span<const int> qrow,
                                const absl::Span<const int> qcol,
                                const absl::Span<const double> qval) {
  const int numqnz = qrow.size();
  CHECK_EQ(qcol.size(), numqnz);
  CHECK_EQ(qval.size(), numqnz);
  return ToStatus(GRBaddqpterms(
      gurobi_model_, numqnz, const_cast<int*>(qcol.data()),
      const_cast<int*>(qrow.data()), const_cast<double*>(qval.data())));
}

absl::Status Gurobi::DelQ() { return ToStatus(GRBdelq(gurobi_model_)); }

absl::Status Gurobi::ChgCoeffs(const absl::Span<const int> cind,
                               const absl::Span<const int> vind,
                               const absl::Span<const double> val) {
  const int num_changes = cind.size();
  CHECK_EQ(vind.size(), num_changes);
  CHECK_EQ(val.size(), num_changes);
  return ToStatus(GRBchgcoeffs(
      gurobi_model_, num_changes, const_cast<int*>(cind.data()),
      const_cast<int*>(vind.data()), const_cast<double*>(val.data())));
}

absl::StatusOr<int> Gurobi::GetNnz(const int first_var, const int num_vars) {
  int nnz = 0;
  RETURN_IF_ERROR(ToStatus(GRBgetvars(gurobi_model_, &nnz, nullptr, nullptr,
                                      nullptr, first_var, num_vars)));
  return nnz;
}

absl::Status Gurobi::GetVars(const absl::Span<int> vbegin,
                             const absl::Span<int> vind,
                             const absl::Span<double> vval, const int first_var,
                             const int num_vars) {
  CHECK_EQ(vbegin.size(), num_vars);
  CHECK_EQ(vind.size(), vval.size());
  int nnz = 0;
  RETURN_IF_ERROR(
      ToStatus(GRBgetvars(gurobi_model_, &nnz, vbegin.data(), vind.data(),
                          vval.data(), first_var, num_vars)));
  CHECK_EQ(nnz, vind.size());
  return absl::OkStatus();
}

absl::StatusOr<Gurobi::SparseMat> Gurobi::GetVars(const int first_var,
                                                  const int num_vars) {
  SparseMat result;
  ASSIGN_OR_RETURN(const int nnz, GetNnz(first_var, num_vars));
  result.begins.resize(num_vars);
  result.inds.resize(nnz);
  result.vals.resize(nnz);
  int read_nnz = 0;
  RETURN_IF_ERROR(ToStatus(
      GRBgetvars(gurobi_model_, &read_nnz, result.begins.data(),
                 result.inds.data(), result.vals.data(), first_var, num_vars)));
  CHECK_EQ(read_nnz, nnz);
  return result;
}

absl::Status Gurobi::UpdateModel() {
  return ToStatus(GRBupdatemodel(gurobi_model_));
}

absl::Status Gurobi::Optimize(Callback cb) {
  bool needs_cb_cleanup = false;
  UserCallbackData user_cb_data;
  if (cb != nullptr) {
    user_cb_data.user_cb = std::move(cb);
    user_cb_data.gurobi = this;
    RETURN_IF_ERROR(ToStatus(
        GRBsetcallbackfunc(gurobi_model_, GurobiCallback, &user_cb_data)));
    needs_cb_cleanup = true;
  }

  // Failsafe to try and clear the callback if there is another error. We cannot
  // raise an error in a destructor, we can only log it.
  auto callback_cleanup = absl::MakeCleanup([&]() {
    if (needs_cb_cleanup) {
      int error = GRBsetcallbackfunc(gurobi_model_, nullptr, nullptr);
      if (error != kGrbOk) {
        LOG(ERROR) << "Error cleaning up callback";
      }
    }
  });
  absl::Status solve_status = ToStatus(GRBoptimize(gurobi_model_));
  RETURN_IF_ERROR(user_cb_data.status) << "Error in Optimize callback.";
  RETURN_IF_ERROR(solve_status);
  if (needs_cb_cleanup) {
    needs_cb_cleanup = false;
    RETURN_IF_ERROR(
        ToStatus(GRBsetcallbackfunc(gurobi_model_, nullptr, nullptr)));
  }
  return absl::OkStatus();
}

bool Gurobi::IsAttrAvailable(const char* name) const {
  return GRBisattravailable(gurobi_model_, name) > 0;
}

absl::StatusOr<int> Gurobi::GetIntAttr(const char* const name) const {
  int result;
  RETURN_IF_ERROR(ToStatus(GRBgetintattr(gurobi_model_, name, &result)))
      << "Error getting Gurobi int attribute: " << name;
  return result;
}

absl::StatusOr<double> Gurobi::GetDoubleAttr(const char* const name) const {
  double result;
  RETURN_IF_ERROR(ToStatus(GRBgetdblattr(gurobi_model_, name, &result)))
      << "Error getting Gurobi double attribute: " << name;
  return result;
}

absl::StatusOr<std::string> Gurobi::GetStringAttr(
    const char* const name) const {
  // WARNING: if a string attribute is the empty string, we need to be careful,
  // std::string(char*) cannot take a nullptr.
  char* result = nullptr;
  RETURN_IF_ERROR(ToStatus(GRBgetstrattr(gurobi_model_, name, &result)))
      << "Error getting Gurobi string attribute: " << name;
  if (result == nullptr) {
    return std::string();
  }
  return std::string(result);
}

absl::Status Gurobi::SetStringAttr(const char* const attr_name,
                                   const std::string& value) {
  return ToStatus(GRBsetstrattr(gurobi_model_, attr_name, value.c_str()));
}

absl::Status Gurobi::SetIntAttr(const char* const attr_name, const int value) {
  return ToStatus(GRBsetintattr(gurobi_model_, attr_name, value));
}

absl::Status Gurobi::SetDoubleAttr(const char* const attr_name,
                                   const double value) {
  return ToStatus(GRBsetdblattr(gurobi_model_, attr_name, value));
}

absl::Status Gurobi::SetIntAttrArray(const char* const name,
                                     const absl::Span<const int> new_values) {
  return ToStatus(GRBsetintattrarray(gurobi_model_, name, 0, new_values.size(),
                                     const_cast<int*>(new_values.data())));
}

absl::Status Gurobi::SetDoubleAttrArray(
    const char* const name, const absl::Span<const double> new_values) {
  return ToStatus(GRBsetdblattrarray(gurobi_model_, name, 0, new_values.size(),
                                     const_cast<double*>(new_values.data())));
}

absl::Status Gurobi::SetCharAttrArray(const char* const name,
                                      const absl::Span<const char> new_values) {
  return ToStatus(GRBsetcharattrarray(gurobi_model_, name, 0, new_values.size(),
                                      const_cast<char*>(new_values.data())));
}

absl::Status Gurobi::GetIntAttrArray(const char* const name,
                                     const absl::Span<int> attr_out) const {
  RETURN_IF_ERROR(ToStatus(GRBgetintattrarray(
      gurobi_model_, name, 0, attr_out.size(), attr_out.data())))
      << "Error getting Gurobi int array attribute: " << name;
  return absl::OkStatus();
}

absl::StatusOr<std::vector<int>> Gurobi::GetIntAttrArray(const char* const name,
                                                         const int len) const {
  std::vector<int> result(len);
  RETURN_IF_ERROR(GetIntAttrArray(name, absl::MakeSpan(result)));
  return result;
}

absl::Status Gurobi::GetDoubleAttrArray(
    const char* const name, const absl::Span<double> attr_out) const {
  RETURN_IF_ERROR(ToStatus(GRBgetdblattrarray(
      gurobi_model_, name, 0, attr_out.size(), attr_out.data())))
      << "Error getting Gurobi double array attribute: " << name;
  return absl::OkStatus();
}

absl::StatusOr<std::vector<double>> Gurobi::GetDoubleAttrArray(
    const char* const name, const int len) const {
  std::vector<double> result(len);
  RETURN_IF_ERROR(GetDoubleAttrArray(name, absl::MakeSpan(result)));
  return result;
}

absl::Status Gurobi::GetCharAttrArray(const char* const name,
                                      const absl::Span<char> attr_out) const {
  RETURN_IF_ERROR(ToStatus(GRBgetcharattrarray(
      gurobi_model_, name, 0, attr_out.size(), attr_out.data())))
      << "Error getting Gurobi char array attribute: " << name;
  return absl::OkStatus();
}

absl::StatusOr<std::vector<char>> Gurobi::GetCharAttrArray(
    const char* const name, const int len) const {
  std::vector<char> result(len);
  RETURN_IF_ERROR(GetCharAttrArray(name, absl::MakeSpan(result)));
  return result;
}

absl::Status Gurobi::SetIntAttrList(const char* const name,
                                    const absl::Span<const int> ind,
                                    const absl::Span<const int> new_values) {
  const int len = ind.size();
  CHECK_EQ(new_values.size(), len);
  return ToStatus(GRBsetintattrlist(gurobi_model_, name, len,
                                    const_cast<int*>(ind.data()),
                                    const_cast<int*>(new_values.data())));
}

absl::Status Gurobi::SetDoubleAttrList(
    const char* const name, const absl::Span<const int> ind,
    const absl::Span<const double> new_values) {
  const int len = ind.size();
  CHECK_EQ(new_values.size(), len);
  return ToStatus(GRBsetdblattrlist(gurobi_model_, name, len,
                                    const_cast<int*>(ind.data()),
                                    const_cast<double*>(new_values.data())));
}

absl::Status Gurobi::SetCharAttrList(const char* const name,
                                     const absl::Span<const int> ind,
                                     const absl::Span<const char> new_values) {
  const int len = ind.size();
  CHECK_EQ(new_values.size(), len);
  return ToStatus(GRBsetcharattrlist(gurobi_model_, name, len,
                                     const_cast<int*>(ind.data()),
                                     const_cast<char*>(new_values.data())));
}

absl::Status Gurobi::SetParam(const char* const name,
                              const std::string& value) {
  return ToStatus(GRBsetparam(model_env_, name, value.c_str()));
}

absl::Status Gurobi::SetIntParam(const char* const name, const int value) {
  return ToStatus(GRBsetintparam(model_env_, name, value));
}

absl::Status Gurobi::SetDoubleParam(const char* const name,
                                    const double value) {
  return ToStatus(GRBsetdblparam(model_env_, name, value));
}

absl::Status Gurobi::SetStringParam(const char* const name,
                                    const std::string& value) {
  return ToStatus(GRBsetstrparam(model_env_, name, value.c_str()));
}

absl::StatusOr<int> Gurobi::GetIntParam(const char* const name) {
  int result;
  RETURN_IF_ERROR(ToStatus(GRBgetintparam(model_env_, name, &result)));
  return result;
}

absl::StatusOr<double> Gurobi::GetDoubleParam(const char* const name) {
  double result;
  RETURN_IF_ERROR(ToStatus(GRBgetdblparam(model_env_, name, &result)));
  return result;
}

absl::StatusOr<std::string> Gurobi::GetStringParam(const char* const name) {
  std::vector<char> result(GRB_MAX_STRLEN);
  RETURN_IF_ERROR(ToStatus(GRBgetstrparam(model_env_, name, result.data())));
  return std::string(result.data());
}

absl::Status Gurobi::ResetParameters() {
  return ToStatus(GRBresetparams(model_env_));
}

void Gurobi::Terminate() { GRBterminate(gurobi_model_); }

Gurobi::CallbackContext::CallbackContext(Gurobi* const gurobi,
                                         void* const cb_data, const int where)
    : gurobi_(ABSL_DIE_IF_NULL(gurobi)), cb_data_(cb_data), where_(where) {}

absl::StatusOr<int> Gurobi::CallbackContext::CbGetInt(const int what) const {
  int result;
  RETURN_IF_ERROR(gurobi_->ToStatus(
      GRBcbget(cb_data_, where_, what, static_cast<void*>(&result))));
  return result;
}

absl::StatusOr<double> Gurobi::CallbackContext::CbGetDouble(
    const int what) const {
  double result;
  RETURN_IF_ERROR(gurobi_->ToStatus(
      GRBcbget(cb_data_, where_, what, static_cast<void*>(&result))));
  return result;
}

absl::Status Gurobi::CallbackContext::CbGetDoubleArray(
    const int what, const absl::Span<double> result) const {
  return gurobi_->ToStatus(
      GRBcbget(cb_data_, where_, what, static_cast<void*>(result.data())));
}

absl::StatusOr<std::string> Gurobi::CallbackContext::CbGetMessage() const {
  char* result = nullptr;
  RETURN_IF_ERROR(gurobi_->ToStatus(GRBcbget(
      cb_data_, where_, GRB_CB_MSG_STRING, static_cast<void*>(&result))));
  if (result == nullptr) {
    return std::string();
  }
  return std::string(result);
}

absl::Status Gurobi::CallbackContext::CbCut(
    const absl::Span<const int> cutind, const absl::Span<const double> cutval,
    const char cutsense, const double cutrhs) const {
  const int cut_len = cutind.size();
  CHECK_EQ(cutval.size(), cut_len);
  return gurobi_->ToStatus(
      GRBcbcut(cb_data_, cut_len, const_cast<int*>(cutind.data()),
               const_cast<double*>(cutval.data()), cutsense, cutrhs));
}

absl::Status Gurobi::CallbackContext::CbLazy(
    const absl::Span<const int> lazyind, const absl::Span<const double> lazyval,
    const char lazysense, const double lazyrhs) const {
  const int lazy_len = lazyind.size();
  CHECK_EQ(lazyval.size(), lazy_len);
  return gurobi_->ToStatus(
      GRBcblazy(cb_data_, lazy_len, const_cast<int*>(lazyind.data()),
                const_cast<double*>(lazyval.data()), lazysense, lazyrhs));
}

absl::StatusOr<double> Gurobi::CallbackContext::CbSolution(
    const absl::Span<const double> solution) const {
  double result;
  RETURN_IF_ERROR(gurobi_->ToStatus(
      GRBcbsolution(cb_data_, const_cast<double*>(solution.data()), &result)));
  return result;
}

}  // namespace operations_research::math_opt
