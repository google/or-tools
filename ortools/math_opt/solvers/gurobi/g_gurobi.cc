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

#include "ortools/math_opt/solvers/gurobi/g_gurobi.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/die_if_null.h"
#include "absl/log/log.h"
#include "absl/log/vlog_is_on.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/types/span.h"
#include "ortools/base/source_location.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/gurobi/isv_public/gurobi_isv.h"
#include "ortools/math_opt/solvers/gurobi.pb.h"
#include "ortools/third_party_solvers/gurobi_environment.h"

namespace operations_research::math_opt {

namespace {
constexpr int kGrbOk = 0;

struct UserCallbackData {
  Gurobi::Callback user_cb;
  absl::Status status = absl::OkStatus();
  Gurobi* gurobi = nullptr;
};

#if defined(_MSC_VER)
#define GUROBI_STDCALL __stdcall
#else
#define GUROBI_STDCALL
#endif

int GUROBI_STDCALL GurobiCallback(GRBmodel* const model, void* const cbdata,
                                  const int where, void* const usrdata) {
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

// A class for handling callback management (setting/unsetting) and their
// associated errors. Users create this handler to register their callback, do
// something, then call `Flush()` to flush errors returned from the callback,
// and then finally call `Release()` to clear the registered callback. This
// class uses RAII to attempt to automatically clear the callback if your code
// returns prior to calling `Release()` manually, but note that this does not
// propagate any errors if it fails.

// A typical use case would be:
//
// ASSIGN_OR_RETURN(const auto scope, ScopedCallback::New(this, std::move(cb)));
// const int error = GRBxxx(gurobi_model_);
// RETURN_IF_ERROR(scope->Flush());
// RETURN_IF_ERROR(ToStatus(error));
// return scope->Release();
class ScopedCallback {
 public:
  ScopedCallback(const ScopedCallback&) = delete;
  ScopedCallback& operator=(const ScopedCallback&) = delete;
  ScopedCallback(ScopedCallback&&) = delete;
  ScopedCallback& operator=(ScopedCallback&&) = delete;

  // Returned object retains a pointer to `gurobi`, which must not be null.
  static absl::StatusOr<std::unique_ptr<ScopedCallback>> New(
      Gurobi* const gurobi, Gurobi::Callback cb) {
    CHECK(gurobi != nullptr);
    auto scope = absl::WrapUnique(new ScopedCallback(gurobi));
    if (cb != nullptr) {
      scope->user_cb_data_.user_cb = std::move(cb);
      scope->user_cb_data_.gurobi = gurobi;
      RETURN_IF_ERROR(gurobi->ToStatus(GRBsetcallbackfunc(
          gurobi->model(), GurobiCallback, &scope->user_cb_data_)));
      scope->needs_cleanup_ = true;
    }
    return scope;
  }

  // Propagates any errors returned from the callback.
  absl::Status Flush() {
    const absl::Status status = std::move(user_cb_data_.status);
    user_cb_data_.status = absl::OkStatus();
    return status;
  }

  // Clears the registered callback.
  absl::Status Release() {
    if (needs_cleanup_) {
      needs_cleanup_ = false;
      return gurobi_->ToStatus(
          GRBsetcallbackfunc(gurobi_->model(), nullptr, nullptr));
    }
    return absl::OkStatus();
  }

  ~ScopedCallback() {
    if (const absl::Status s = Flush(); !s.ok()) {
      LOG(ERROR) << "Error returned from callback: " << s;
    }
    if (const absl::Status s = Release(); !s.ok()) {
      LOG(ERROR) << "Error cleaning up callback: " << s;
    }
  }

 private:
  explicit ScopedCallback(Gurobi* const gurobi) : gurobi_(gurobi) {}

  bool needs_cleanup_ = false;
  Gurobi* gurobi_;
  UserCallbackData user_cb_data_;
};

// Returns true if both keys are equal.
bool AreISVKeyEqual(const GurobiIsvKey& key,
                    const GurobiInitializerProto::ISVKey& proto_key) {
  return key.name == proto_key.name() &&
         key.application_name == proto_key.application_name() &&
         key.expiration == proto_key.expiration() && key.key == proto_key.key();
}

}  // namespace

void GurobiFreeEnv::operator()(GRBenv* const env) const {
  if (env != nullptr) {
    GRBfreeenv(env);
  }
}

absl::StatusOr<GRBenvUniquePtr> GurobiNewPrimaryEnv(
    const std::optional<GurobiIsvKey>& isv_key) {
  if (isv_key.has_value()) {
    ASSIGN_OR_RETURN(GRBenv* const naked_primary_env,
                     NewPrimaryEnvFromISVKey(*isv_key));
    return GRBenvUniquePtr(naked_primary_env);
  }
  GRBenv* naked_primary_env = nullptr;
  const int err = GRBloadenv(&naked_primary_env, /*logfilename=*/nullptr);
  // Surprisingly, Gurobi will still create an environment if initialization
  // fails, so we want this wrapper even in the error case to free it properly.
  GRBenvUniquePtr primary_env(naked_primary_env);
  if (err == kGrbOk) {
    return primary_env;
  }
  return util::InvalidArgumentErrorBuilder()
         << "failed to create Gurobi primary environment, GRBloadenv() "
            "returned the error ("
         << err << "): " << GRBgeterrormsg(primary_env.get());
}

absl::StatusOr<std::unique_ptr<Gurobi>> Gurobi::NewWithSharedPrimaryEnv(
    GRBenv* const primary_env) {
  CHECK(primary_env != nullptr);
  return New(nullptr, primary_env);
}

absl::StatusOr<std::unique_ptr<Gurobi>> Gurobi::New(
    GRBenvUniquePtr primary_env) {
  if (primary_env == nullptr) {
    ASSIGN_OR_RETURN(primary_env, GurobiNewPrimaryEnv());
  }
  GRBenv* const raw_primary_env = primary_env.get();
  return New(std::move(primary_env), raw_primary_env);
}

Gurobi::Gurobi(GRBenvUniquePtr optional_owned_primary_env,
               GRBmodel* const model, GRBenv* const model_env)
    : owned_primary_env_(std::move(optional_owned_primary_env)),
      gurobi_model_(ABSL_DIE_IF_NULL(model)),
      model_env_(ABSL_DIE_IF_NULL(model_env)) {}

absl::StatusOr<std::unique_ptr<Gurobi>> Gurobi::New(
    GRBenvUniquePtr optional_owned_primary_env, GRBenv* const primary_env) {
  CHECK(primary_env != nullptr);
  GRBmodel* model = nullptr;
  const int err = GRBnewmodel(primary_env, &model,
                              /*Pname=*/nullptr,
                              /*numvars=*/0,
                              /*obj=*/nullptr, /*lb=*/nullptr,
                              /*ub=*/nullptr, /*vtype=*/nullptr,
                              /*varnames=*/nullptr);
  if (err != kGrbOk) {
    return util::InvalidArgumentErrorBuilder()
           << "Error creating gurobi model on GRBnewmodel(), error code: "
           << err << " message: " << GRBgeterrormsg(primary_env);
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
      new Gurobi(std::move(optional_owned_primary_env), model, model_env));
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

absl::Status Gurobi::AddVar(const double obj, const double lb, const double ub,
                            const char vtype, const std::string& name) {
  return AddVar({}, {}, obj, lb, ub, vtype, name);
}

absl::Status Gurobi::AddVar(const absl::Span<const int> vind,
                            const absl::Span<const double> vval,
                            const double obj, const double lb, const double ub,
                            const char vtype, const std::string& name) {
  CHECK_EQ(vind.size(), vval.size());
  const int numnz = static_cast<int>(vind.size());
  return ToStatus(GRBaddvar(/*model=*/gurobi_model_, /*numnz=*/numnz,
                            /*vind=*/const_cast<int*>(vind.data()),
                            /*vval=*/const_cast<double*>(vval.data()),
                            /*obj=*/obj,
                            /*lb=*/lb,
                            /*ub=*/ub,
                            /*vtype=*/vtype,
                            /*varname=*/name.empty() ? nullptr : name.data()));
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
  const int num_vars = static_cast<int>(lb.size());
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

absl::Status Gurobi::AddConstr(const char sense, const double rhs,
                               const std::string& name) {
  return AddConstr({}, {}, sense, rhs, name);
}

absl::Status Gurobi::AddConstr(const absl::Span<const int> cind,
                               const absl::Span<const double> cval,
                               const char sense, const double rhs,
                               const std::string& name) {
  CHECK_EQ(cind.size(), cval.size());
  const int numnz = static_cast<int>(cind.size());
  return ToStatus(
      GRBaddconstr(/*model=*/gurobi_model_, /*numnz=*/numnz,
                   /*cind=*/const_cast<int*>(cind.data()),
                   /*cval=*/const_cast<double*>(cval.data()),
                   /*sense=*/sense,
                   /*rhs=*/rhs,
                   /*constrname=*/name.empty() ? nullptr : name.data()));
}

absl::Status Gurobi::AddConstrs(const absl::Span<const char> sense,
                                const absl::Span<const double> rhs,
                                const absl::Span<const std::string> names) {
  const int num_cons = static_cast<int>(sense.size());
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
  const int numqnz = static_cast<int>(qrow.size());
  CHECK_EQ(qcol.size(), numqnz);
  CHECK_EQ(qval.size(), numqnz);
  return ToStatus(GRBaddqpterms(
      gurobi_model_, numqnz, const_cast<int*>(qcol.data()),
      const_cast<int*>(qrow.data()), const_cast<double*>(qval.data())));
}

absl::Status Gurobi::DelQ() { return ToStatus(GRBdelq(gurobi_model_)); }

absl::Status Gurobi::SetNthObjective(const int index, const int priority,
                                     const double weight, const double abs_tol,
                                     const double rel_tol,
                                     const std::string& name,
                                     const double constant,
                                     const absl::Span<const int> lind,
                                     const absl::Span<const double> lval) {
  const int numlnz = static_cast<int>(lind.size());
  CHECK_EQ(lval.size(), numlnz);
  return ToStatus(GRBsetobjectiven(
      /*model=*/gurobi_model_,
      /*index=*/index,
      /*priority=*/priority,
      /*weight=*/weight,
      /*abstol=*/abs_tol,
      /*reltol=*/rel_tol,
      /*name=*/const_cast<char*>(name.c_str()),
      /*constant=*/constant,
      /*lnz=*/numlnz,
      /*lind=*/const_cast<int*>(lind.data()),
      /*lval=*/const_cast<double*>(lval.data())));
}

absl::Status Gurobi::AddQConstr(const absl::Span<const int> lind,
                                const absl::Span<const double> lval,
                                const absl::Span<const int> qrow,
                                const absl::Span<const int> qcol,
                                const absl::Span<const double> qval,
                                const char sense, const double rhs,
                                const std::string& name) {
  const int numlnz = static_cast<int>(lind.size());
  CHECK_EQ(lval.size(), numlnz);

  const int numqlnz = static_cast<int>(qrow.size());
  CHECK_EQ(qcol.size(), numqlnz);
  CHECK_EQ(qval.size(), numqlnz);

  return ToStatus(GRBaddqconstr(
      /*model=*/gurobi_model_,
      /*numlnz=*/numlnz,
      /*lind=*/const_cast<int*>(lind.data()),
      /*lval=*/const_cast<double*>(lval.data()),
      /*numqlnz=*/numqlnz,
      /*qrow=*/const_cast<int*>(qrow.data()),
      /*qcol=*/const_cast<int*>(qcol.data()),
      /*qval=*/const_cast<double*>(qval.data()),
      /*sense=*/sense,
      /*rhs=*/rhs,
      /*constrname=*/const_cast<char*>(name.c_str())));
}

absl::Status Gurobi::DelQConstrs(const absl::Span<const int> ind) {
  return ToStatus(GRBdelqconstrs(gurobi_model_, static_cast<int>(ind.size()),
                                 const_cast<int*>(ind.data())));
}

absl::Status Gurobi::AddSos(const absl::Span<const int> types,
                            const absl::Span<const int> beg,
                            const absl::Span<const int> ind,
                            const absl::Span<const double> weight) {
  const int num_sos = static_cast<int>(types.size());
  CHECK_EQ(beg.size(), num_sos);

  const int num_members = static_cast<int>(ind.size());
  CHECK_EQ(weight.size(), num_members);

  return ToStatus(GRBaddsos(/*model=*/gurobi_model_, /*numsos=*/num_sos,
                            /*nummembers=*/num_members,
                            /*types=*/const_cast<int*>(types.data()),
                            /*beg=*/const_cast<int*>(beg.data()),
                            /*ind=*/const_cast<int*>(ind.data()),
                            /*weight=*/const_cast<double*>(weight.data())));
}

absl::Status Gurobi::DelSos(const absl::Span<const int> ind) {
  return ToStatus(GRBdelsos(gurobi_model_, static_cast<int>(ind.size()),
                            const_cast<int*>(ind.data())));
}

absl::Status Gurobi::AddIndicator(const std::string& name, const int binvar,
                                  const int binval,
                                  const absl::Span<const int> ind,
                                  const absl::Span<const double> val,
                                  const char sense, const double rhs) {
  const int nvars = static_cast<int>(ind.size());
  CHECK_EQ(val.size(), nvars);
  return ToStatus(GRBaddgenconstrIndicator(
      /*model=*/gurobi_model_, /*name=*/const_cast<char*>(name.c_str()),
      /*binvar=*/binvar, /*binval=*/binval, /*nvars=*/nvars,
      /*ind=*/const_cast<int*>(ind.data()),
      /*val=*/const_cast<double*>(val.data()), /*sense=*/sense, /*rhs=*/rhs));
}

absl::Status Gurobi::DelGenConstrs(const absl::Span<const int> ind) {
  return ToStatus(GRBdelgenconstrs(gurobi_model_, static_cast<int>(ind.size()),
                                   const_cast<int*>(ind.data())));
}

absl::Status Gurobi::ChgCoeffs(const absl::Span<const int> cind,
                               const absl::Span<const int> vind,
                               const absl::Span<const double> val) {
  const int num_changes = static_cast<int>(cind.size());
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
  ASSIGN_OR_RETURN(const auto scope, ScopedCallback::New(this, std::move(cb)));
  const int error = GRBoptimize(gurobi_model_);
  RETURN_IF_ERROR(scope->Flush());
  RETURN_IF_ERROR(ToStatus(error));
  return scope->Release();
}

absl::StatusOr<bool> Gurobi::ComputeIIS(Callback cb) {
  ASSIGN_OR_RETURN(const auto scope, ScopedCallback::New(this, std::move(cb)));
  const int error = GRBcomputeIIS(gurobi_model_);
  RETURN_IF_ERROR(scope->Flush());
  if (error == GRB_ERROR_IIS_NOT_INFEASIBLE) {
    RETURN_IF_ERROR(scope->Release());
    return false;
  } else if (error == kGrbOk) {
    // If Gurobi v11 terminates at a limit before determining if the model is
    // feasible or not, it will return an OK error code but then will fail to
    // return anything about the IIS it does not have. To detect this case, we
    // query the minimality attribute: we know that our env is valid at this
    // point, and this should fail iff an IIS is present, i.e., Gurobi proved
    // that the model was infeasible.
    const bool has_iis = GetIntAttr(GRB_INT_ATTR_IIS_MINIMAL).ok();
    RETURN_IF_ERROR(scope->Release());
    return has_iis;
  }
  RETURN_IF_ERROR(ToStatus(error));
  return scope->Release();
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
  const int len = static_cast<int>(ind.size());
  CHECK_EQ(new_values.size(), len);
  return ToStatus(GRBsetintattrlist(gurobi_model_, name, len,
                                    const_cast<int*>(ind.data()),
                                    const_cast<int*>(new_values.data())));
}

absl::Status Gurobi::SetDoubleAttrList(
    const char* const name, const absl::Span<const int> ind,
    const absl::Span<const double> new_values) {
  const int len = static_cast<int>(ind.size());
  CHECK_EQ(new_values.size(), len);
  return ToStatus(GRBsetdblattrlist(gurobi_model_, name, len,
                                    const_cast<int*>(ind.data()),
                                    const_cast<double*>(new_values.data())));
}

absl::Status Gurobi::SetCharAttrList(const char* const name,
                                     const absl::Span<const int> ind,
                                     const absl::Span<const char> new_values) {
  const int len = static_cast<int>(ind.size());
  CHECK_EQ(new_values.size(), len);
  return ToStatus(GRBsetcharattrlist(gurobi_model_, name, len,
                                     const_cast<int*>(ind.data()),
                                     const_cast<char*>(new_values.data())));
}

absl::StatusOr<int> Gurobi::GetIntAttrElement(const char* const name,
                                              const int element) const {
  int value;
  RETURN_IF_ERROR(
      ToStatus(GRBgetintattrelement(gurobi_model_, name, element, &value)));
  return value;
}

absl::Status Gurobi::SetIntAttrElement(const char* const name,
                                       const int element, const int new_value) {
  return ToStatus(
      GRBsetintattrelement(gurobi_model_, name, element, new_value));
}

absl::StatusOr<double> Gurobi::GetDoubleAttrElement(const char* const name,
                                                    const int element) const {
  double value;
  RETURN_IF_ERROR(
      ToStatus(GRBgetdblattrelement(gurobi_model_, name, element, &value)));
  return value;
}

absl::Status Gurobi::SetDoubleAttrElement(const char* const name, int element,
                                          double new_value) {
  return ToStatus(
      GRBsetdblattrelement(gurobi_model_, name, element, new_value));
}

absl::StatusOr<char> Gurobi::GetCharAttrElement(const char* const name,
                                                const int element) const {
  char value;
  RETURN_IF_ERROR(
      ToStatus(GRBgetcharattrelement(gurobi_model_, name, element, &value)));
  return value;
}

absl::Status Gurobi::SetCharAttrElement(const char* const name,
                                        const int element,
                                        const char new_value) {
  return ToStatus(
      GRBsetcharattrelement(gurobi_model_, name, element, new_value));
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

absl::Status Gurobi::SetMultiObjectiveDoubleParam(const char* const name,
                                                  const int obj_index,
                                                  const double value) {
  ASSIGN_OR_RETURN(GRBenv* const obj_env, GetMultiObjectiveEnv(obj_index));
  return util::StatusBuilder(ToStatus(GRBsetdblparam(obj_env, name, value)))
         << " for objective index: " << obj_index;
}

absl::StatusOr<double> Gurobi::GetMultiObjectiveDoubleParam(
    const char* const name, const int obj_index) {
  ASSIGN_OR_RETURN(GRBenv* const obj_env, GetMultiObjectiveEnv(obj_index));
  double result;
  RETURN_IF_ERROR(ToStatus(GRBgetdblparam(obj_env, name, &result)))
      << " for objective index: " << obj_index;
  return result;
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
  const int cut_len = static_cast<int>(cutind.size());
  CHECK_EQ(cutval.size(), cut_len);
  return gurobi_->ToStatus(
      GRBcbcut(cb_data_, cut_len, const_cast<int*>(cutind.data()),
               const_cast<double*>(cutval.data()), cutsense, cutrhs));
}

absl::Status Gurobi::CallbackContext::CbLazy(
    const absl::Span<const int> lazyind, const absl::Span<const double> lazyval,
    const char lazysense, const double lazyrhs) const {
  const int lazy_len = static_cast<int>(lazyind.size());
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

absl::StatusOr<GRBenv*> Gurobi::GetMultiObjectiveEnv(
    const int obj_index) const {
  GRBenv* const obj_env = GRBgetmultiobjenv(gurobi_model_, obj_index);
  if (obj_env == nullptr) {
    return util::InvalidArgumentErrorBuilder()
           << "Failed to get objective environment for objective index: "
           << obj_index;
  }
  return obj_env;
}

}  // namespace operations_research::math_opt
