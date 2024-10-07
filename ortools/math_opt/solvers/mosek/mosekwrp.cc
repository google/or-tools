#include "mosekwrp.h"

#include <mosek.h>

#include <cmath>
#include <limits>
#include <locale>
#include <ranges>
#include <type_traits>

#include "absl/cleanup/cleanup.h"
#include "absl/status/status.h"

namespace operations_research::math_opt {

Mosek* Mosek::Create() {
  MSKtask_t task;
  MSKrescodee r = MSK_makeemptytask(nullptr, &task);
  if (r != MSK_RES_OK) {
    return nullptr;
  }
  int64_t domidx;
  MSK_appendrzerodomain(task, 0, &domidx);

  return new Mosek(task);
}

Mosek::Mosek(MSKtask_t task) : task(task, delete_msk_task_func) {}
Mosek::Mosek(Mosek&& m) : task(std::move(m.task)) {}

void Mosek::PutName(const std::string& name) {
  MSK_puttaskname(task.get(), name.c_str());
}
void Mosek::PutObjName(const std::string& name) {
  MSK_putobjname(task.get(), name.c_str());
}
void Mosek::PutVarName(VariableIndex j, const std::string& name) {
  MSK_putvarname(task.get(), j, name.c_str());
}
void Mosek::PutConName(ConstraintIndex i, const std::string& name) {
  MSK_putconname(task.get(), i, name.c_str());
}
void Mosek::PutObjectiveSense(bool maximize) {
  MSK_putobjsense(task.get(), maximize ? MSK_OBJECTIVE_SENSE_MAXIMIZE
                                       : MSK_OBJECTIVE_SENSE_MINIMIZE);
}

absl::StatusOr<Mosek::VariableIndex> Mosek::AppendVars(
    const std::vector<double>& lb, const std::vector<double>& ub) {
  if (lb.size() != ub.size())
    return absl::InvalidArgumentError("Mismatching lengths of lb and ub");
  size_t n = lb.size();
  int firstj = NumVar();
  if (n > std::numeric_limits<int>::max())
    return absl::InvalidArgumentError("arguments lb and ub too large");

  MSK_appendvars(task.get(), (int)n);
  std::vector<MSKboundkeye> bk(n);
  for (ssize_t i = 0; i < n; ++i) {
    bk[i] = (lb[i] > ub[i]
                 ? MSK_BK_RA
                 : (std::isfinite(lb[i])
                        ? (std::isfinite(ub[i])
                               ? (lb[i] < ub[i] ? MSK_BK_RA : MSK_BK_FX)
                               : MSK_BK_LO)
                        : (std::isfinite(ub[i]) ? MSK_BK_UP : MSK_BK_FR)));
  }

  MSK_putvarboundslice(task.get(), firstj, firstj + (int)n, bk.data(),
                       lb.data(), ub.data());
  return firstj;
}
absl::StatusOr<Mosek::ConstraintIndex> Mosek::AppendCons(
    const std::vector<double>& lb, const std::vector<double>& ub) {
  if (lb.size() != ub.size())
    return absl::InvalidArgumentError("Mismatching lengths of lb and ub");
  size_t n = lb.size();
  int firsti = NumCon();
  if (n > std::numeric_limits<int>::max())
    return absl::InvalidArgumentError("arguments lb and ub too large");

  MSK_appendcons(task.get(), (int)n);
  std::vector<MSKboundkeye> bk(n);
  for (ssize_t i = 0; i < n; ++i) {
    bk[i] = (lb[i] > ub[i]
                 ? MSK_BK_RA
                 : (std::isfinite(lb[i])
                        ? (std::isfinite(ub[i])
                               ? (lb[i] < ub[i] ? MSK_BK_RA : MSK_BK_FX)
                               : MSK_BK_LO)
                        : (std::isfinite(ub[i]) ? MSK_BK_UP : MSK_BK_FR)));
  }

  MSK_putconboundslice(task.get(), firsti, firsti + (int)n, bk.data(),
                       lb.data(), ub.data());
  return firsti;
}
absl::Status Mosek::PutVarType(VariableIndex j, bool is_integer) {
  if (MSK_RES_OK !=
      MSK_putvartype(task.get(), j,
                     is_integer ? MSK_VAR_TYPE_INT : MSK_VAR_TYPE_CONT))
    return absl::InvalidArgumentError("Arguments j is invalid");
  return absl::OkStatus();
}

absl::Status Mosek::PutC(const std::vector<double>& c) {
  auto n = c.size();
  if (n > NumVar())
    return absl::InvalidArgumentError("Argument c is too large");
  for (int j = 0; j < n; ++j) MSK_putcj(task.get(), j, c[j]);
  return absl::OkStatus();
}

void Mosek::PutCFix(double cfix) { MSK_putcfix(task.get(), cfix); }

absl::Status Mosek::PutAIJList(const std::vector<ConstraintIndex>& subi,
                               const std::vector<VariableIndex>& subj,
                               const std::vector<double>& valij) {
  if (subi.size() != subj.size() || subi.size() != valij.size())
    return absl::InvalidArgumentError(
        "Mismatching arguments subi, subj, valij");
  size_t n = subi.size();
  if (n > std::numeric_limits<int>::max())
    return absl::InvalidArgumentError(
        "Arguments subi, subj, valij are too long");

  if (MSK_RES_OK != MSK_putaijlist(task.get(), (int)n, subi.data(), subj.data(),
                                   valij.data()))
    return absl::InvalidArgumentError("Invalid index argument subi or subj");
  return absl::OkStatus();
}

// Note: We implement indicator constraints as a disjunctive constraint of the
// form: [ indvar = (negate ? 1.0 : 0.0) ]
//   OR
// [ indvar = (negate ? 0.0 : 1.0)
//   lb <= Ax <= ub ]
//
absl::StatusOr<Mosek::DisjunctiveConstraintIndex>
Mosek::AppendIndicatorConstraint(bool negate, VariableIndex indvar,
                                 const std::vector<VariableIndex>& subj,
                                 const std::vector<double>& cof, double lb,
                                 double ub) {
  if (subj.size() != cof.size())
    return absl::InvalidArgumentError("Mismatching arguments subj, cof");

  size_t n = subj.size();
  if (n > std::numeric_limits<int>::max())
    return absl::InvalidArgumentError("Arguments subj or cof is too long");

  int64_t ndjc, nafe;
  MSK_getnumdjc(task.get(), &ndjc);
  MSK_getnumafe(task.get(), &nafe);

  MSK_appendafes(task.get(), 2);
  MSK_appenddjcs(task.get(), 1);

  MSK_putafefentry(task.get(), nafe, indvar, 1.0);
  MSK_putafefrow(task.get(), nafe + 1, (int)n, subj.data(), cof.data());
  int64_t dom_eq, dom_lb, dom_ub;
  MSK_appendrzerodomain(task.get(), 1, &dom_eq);
  if (std::isfinite(lb)) {
    MSK_appendrplusdomain(task.get(), 1, &dom_lb);
  } else {
    MSK_appendrdomain(task.get(), 1, &dom_lb);
  }
  if (std::isfinite(ub)) {
    MSK_appendrminusdomain(task.get(), 1, &dom_ub);
  } else {
    MSK_appendrdomain(task.get(), 1, &dom_ub);
  }

  int64_t afeidx[4] = {nafe, nafe, nafe + 1, nafe + 1};
  double b[4] = {negate ? 1.0 : 0.0, negate ? 0.0 : 1.0, lb, ub};
  int64_t domidxs[4] = {dom_eq, dom_eq, dom_lb, dom_ub};
  int64_t termsizes[2] = {1, 3};
  MSK_putdjc(task.get(), ndjc, 4, domidxs, 4, afeidx, b, 2, termsizes);

  return ndjc;
}
absl::Status Mosek::PutDJCName(DisjunctiveConstraintIndex djci,
                               const std::string& name) {
  if (MSK_RES_OK != MSK_putdjcname(task.get(), djci, name.c_str()))
    return absl::InvalidArgumentError("Invalid argument djci");
  return absl::OkStatus();
}
absl::Status Mosek::PutACCName(ConeConstraintIndex acci,
                               const std::string& name) {
  if (MSK_RES_OK != MSK_putaccname(task.get(), acci, name.c_str()))
    return absl::InvalidArgumentError("Invalid argument acci");
  return absl::OkStatus();
}

absl::StatusOr<Mosek::ConeConstraintIndex> Mosek::AppendConeConstraint(
    ConeType ct, const std::vector<int32_t>& sizes,
    const std::vector<VariableIndex>& subj, const std::vector<double>& cof,
    const std::vector<double>& b) {
  size_t n = sizes.size();
  size_t nnz = 0;
  for (auto& nz : sizes) nnz += nz;
  int64_t domidx;

  if (nnz != cof.size() || nnz != subj.size())
    return absl::InvalidArgumentError(
        "Mismatching argument lengths of subj and cof");
  if (n != b.size())
    return absl::InvalidArgumentError("Mismatching argument lengths b and ptr");

  switch (ct) {
    case ConeType::SecondOrderCone:
      MSK_appendquadraticconedomain(task.get(), n, &domidx);
      break;
    case ConeType::RotatedSecondOrderCone:
      MSK_appendrquadraticconedomain(task.get(), n, &domidx);
      break;
    default:
      return absl::InvalidArgumentError("Cone type not supported");
  }

  int64_t afei;
  MSK_getnumafe(task.get(), &afei);
  MSK_appendafes(task.get(), n);

  std::vector<int64_t> afeidxs(n);
  for (ssize_t i = 0; i < n; ++i) afeidxs[i] = afei + i;
  std::vector<int64_t> ptr(n);
  ptr[0] = 0;
  for (ssize_t i = 0; i < n; ++i) ptr[i + 1] = ptr[i] + sizes[i];

  std::vector<double> accb(n);

  int64_t acci;
  MSK_getnumacc(task.get(), &acci);
  MSK_appendaccseq(task.get(), domidx, n, afei, accb.data());
  MSK_putafefrowlist(task.get(), n, afeidxs.data(), sizes.data(), ptr.data(),
                     nnz, subj.data(), cof.data());
  for (ssize_t i = 0; i < n; ++i) MSK_putafeg(task.get(), afei + i, b[i]);
  return acci;
}

// Delete-ish
absl::Status Mosek::ClearVariable(VariableIndex j) {
  if (MSK_RES_OK != MSK_putvarbound(task.get(), j, MSK_BK_FR, 0.0, 0.0))
    return absl::InvalidArgumentError("Invalid variable index j");
  return absl::OkStatus();
}
absl::Status Mosek::ClearConstraint(ConstraintIndex i) {
  if (MSK_RES_OK != MSK_putconbound(task.get(), i, MSK_BK_FR, 0.0, 0.0))
    return absl::InvalidArgumentError("Invalid constraint index i");
  int subj;
  double cof;
  MSK_putarow(task.get(), i, 0, &subj, &cof);
  return absl::OkStatus();
}
absl::Status Mosek::ClearConeConstraint(ConeConstraintIndex i) {
  int64_t afeidxs;
  double b;
  if (MSK_RES_OK != MSK_putacc(task.get(), i, 0, 0, &afeidxs, &b))
    return absl::InvalidArgumentError("Invalid constraint index i");
  return absl::OkStatus();
}
absl::Status Mosek::ClearDisjunctiveConstraint(DisjunctiveConstraintIndex i) {
  int64_t i64s;
  double f64s;
  if (MSK_RES_OK !=
      MSK_putdjc(task.get(), i, 0, &i64s, 0, &i64s, &f64s, 0, &i64s))
    return absl::InvalidArgumentError("Invalid constraint index i");
  return absl::OkStatus();
}

// Update

static MSKboundkeye merge_lower_bound(MSKboundkeye bk, double bl, double bu,
                                      double b) {
  switch (bk) {
    case MSK_BK_FR:
      return std::isfinite(b) ? MSK_BK_LO : MSK_BK_FR;
    case MSK_BK_LO:
      return std::isfinite(b) ? MSK_BK_LO : MSK_BK_FR;
    case MSK_BK_UP:
      return std::isfinite(b) ? (b < bu || b > bu ? MSK_BK_RA : MSK_BK_FX)
                              : MSK_BK_UP;
    case MSK_BK_FX:
      return std::isfinite(b) ? (b < bu || b > bu ? MSK_BK_RA : MSK_BK_FX)
                              : MSK_BK_UP;
    case MSK_BK_RA:
      return std::isfinite(b) ? (b < bu || b > bu ? MSK_BK_RA : MSK_BK_FX)
                              : MSK_BK_UP;
  }
  return MSK_BK_FX;
}

static MSKboundkeye merge_upper_bound(MSKboundkeye bk, double bl, double bu,
                                      double b) {
  switch (bk) {
    case MSK_BK_FR:
      return std::isfinite(b) ? MSK_BK_UP : MSK_BK_FR;
    case MSK_BK_UP:
      return std::isfinite(b) ? MSK_BK_UP : MSK_BK_FR;
    case MSK_BK_LO:
      return std::isfinite(b) ? (b < bl || b > bl ? MSK_BK_RA : MSK_BK_FX)
                              : MSK_BK_UP;
    case MSK_BK_FX:
      return std::isfinite(b) ? (b < bl || b > bl ? MSK_BK_RA : MSK_BK_FX)
                              : MSK_BK_UP;
    case MSK_BK_RA:
      return std::isfinite(b) ? (b < bl || b > bl ? MSK_BK_RA : MSK_BK_FX)
                              : MSK_BK_UP;
  }
  return MSK_BK_FX;
}

absl::Status Mosek::UpdateVariableLowerBound(VariableIndex j, double b) {
  MSKboundkeye bk;
  double bl, bu;
  if (MSK_RES_OK != MSK_getvarbound(task.get(), j, &bk, &bl, &bu))
    return absl::InvalidArgumentError("Invalid variable index j");
  MSK_putvarbound(task.get(), j, merge_lower_bound(bk, bl, bu, b), b, bu);
  return absl::OkStatus();
}
absl::Status Mosek::UpdateVariableUpperBound(VariableIndex j, double b) {
  MSKboundkeye bk;
  double bl, bu;
  if (MSK_RES_OK != MSK_getvarbound(task.get(), j, &bk, &bl, &bu))
    return absl::InvalidArgumentError("Invalid variable index j");
  MSK_putvarbound(task.get(), j, merge_upper_bound(bk, bl, bu, b), b, bu);
  return absl::OkStatus();
}
absl::Status Mosek::UpdateVariableType(VariableIndex j, bool is_integer) {
  if (MSK_RES_OK !=
      MSK_putvartype(task.get(), j,
                     is_integer ? MSK_VAR_TYPE_INT : MSK_VAR_TYPE_CONT))
    return absl::InvalidArgumentError("Invalid variable index j");
  return absl::OkStatus();
}
absl::Status Mosek::UpdateConstraintLowerBound(ConstraintIndex i, double b) {
  MSKboundkeye bk;
  double bl, bu;
  if (MSK_RES_OK != MSK_getconbound(task.get(), i, &bk, &bl, &bu))
    return absl::InvalidArgumentError("Invalid constraint index i");
  MSK_putconbound(task.get(), i, merge_lower_bound(bk, bl, bu, b), b, bu);
  return absl::OkStatus();
}
absl::Status Mosek::UpdateConstraintUpperBound(ConstraintIndex i, double b) {
  MSKboundkeye bk;
  double bl, bu;
  if (MSK_RES_OK != MSK_getconbound(task.get(), i, &bk, &bl, &bu))
    return absl::InvalidArgumentError("Invalid constraint index i");
  MSK_putconbound(task.get(), i, merge_upper_bound(bk, bl, bu, b), b, bu);
  return absl::OkStatus();
}
absl::Status Mosek::UpdateObjectiveSense(bool maximize) {
  MSK_putobjsense(task.get(), maximize ? MSK_OBJECTIVE_SENSE_MAXIMIZE
                                       : MSK_OBJECTIVE_SENSE_MINIMIZE);
  return absl::OkStatus();
}
absl::Status Mosek::UpdateObjective(double fixterm,
                                    const std::vector<VariableIndex>& subj,
                                    const std::vector<double>& cof) {
  if (subj.size() != cof.size())
    return absl::InvalidArgumentError(
        "Mismatching argument lengths of subj and cof");
  if (subj.size() > std::numeric_limits<int>::max())
    return absl::InvalidArgumentError("Argument subj and cof are too long");
  if (MSK_RES_OK !=
      MSK_putclist(task.get(), (int)subj.size(), subj.data(), cof.data()))
    return absl::InvalidArgumentError("Invalid variable index in subj");
  MSK_putcfix(task.get(), fixterm);
  return absl::OkStatus();
}
absl::Status Mosek::UpdateA(const std::vector<ConstraintIndex>& subi,
                            const std::vector<VariableIndex>& subj,
                            const std::vector<double>& cof) {
  if (subi.size() != cof.size() || subj.size() != cof.size())
    return absl::InvalidArgumentError(
        "Mismatching lengths of arguments subi, subj, and cof");
  if (MSK_RES_OK != MSK_putaijlist(task.get(), subi.size(), subi.data(),
                                   subj.data(), cof.data()))
    return absl::InvalidArgumentError(
        "Invalid variable or constraint index in subi or subj");
  return absl::OkStatus();
}

void Mosek::WriteData(const char* filename) const {
  MSK_writedata(task.get(), filename);
}

absl::StatusOr<MSKrescodee> Mosek::Optimize() {
  MSKrescodee trm;
  MSKrescodee r = MSK_optimizetrm(task.get(), &trm);
  if (MSK_RES_OK != r) return absl::InternalError("Optimization failed");

  return trm;
}

void Mosek::message_callback(MSKuserhandle_t handle, const char* msg) {
  MessageCallback& msg_cb = *reinterpret_cast<MessageCallback*>(handle);
  if (msg_cb) (msg_cb)(msg);
}
int Mosek::info_callback(MSKtask_t task, MSKuserhandle_t h,
                         MSKcallbackcodee code, const double* dinf,
                         const int* iinf, const int64_t* liinf) {
  auto& info_cb = *reinterpret_cast<InfoCallback*>(h);

  bool interrupt = false;
  if (info_cb)
    interrupt = info_cb(code, absl::Span(dinf, MSK_DINF_END),
                        absl::Span(iinf, MSK_IINF_END),
                        absl::Span(liinf, MSK_LIINF_END));
  return interrupt ? 1 : 0;
}

absl::StatusOr<MSKrescodee> Mosek::Optimize(MessageCallback msg_cb,
                                            InfoCallback info_cb) {
  MSKrescodee trm;

  auto _cleanup = absl::MakeCleanup([&]() {
    MSK_linkfunctotaskstream(task.get(), MSK_STREAM_LOG, nullptr, nullptr);
    MSK_putcallbackfunc(task.get(), nullptr, nullptr);
  });

  bool interrupt = false;

  if (info_cb) MSK_putcallbackfunc(task.get(), info_callback, &info_cb);
  if (msg_cb)
    MSK_linkfunctotaskstream(task.get(), MSK_STREAM_LOG, &msg_cb,
                             message_callback);

  MSKrescodee r = MSK_optimizetrm(task.get(), &trm);
  if (MSK_RES_OK != r) return absl::InternalError("Optimization failed");

  return trm;
}

std::tuple<std::string, std::string, MSKrescodee> Mosek::LastError() const {
  int64_t msglen;
  MSKrescodee r;
  if (MSK_RES_OK != MSK_getlasterror64(task.get(), &r, 0, &msglen, nullptr)) {
    return {"", "", MSK_RES_ERR_UNKNOWN};
  } else {
    std::vector<char> msg(msglen + 1);
    char buf[MSK_MAX_STR_LEN];
    MSK_getlasterror64(task.get(), &r, msglen, &msglen, msg.data());
    msg[msglen] = 0;

    MSK_rescodetostr(r, buf);
    return {msg.data(), buf, r};
  }
}

double Mosek::GetPrimalObj(MSKsoltypee whichsol) const {
  if (!SolutionDef(whichsol)) return 0.0;
  double val;
  MSK_getprimalobj(task.get(), whichsol, &val);
  return val;
}
double Mosek::GetDualObj(MSKsoltypee whichsol) const {
  if (!SolutionDef(whichsol)) return 0.0;
  double val;
  MSK_getdualobj(task.get(), whichsol, &val);
  return val;
}
void Mosek::GetXX(MSKsoltypee whichsol, std::vector<double>& xx) const {
  xx.clear();
  if (!SolutionDef(whichsol)) return;
  xx.resize(NumVar());
  MSK_getxx(task.get(), whichsol, xx.data());
}
void Mosek::GetSLX(MSKsoltypee whichsol, std::vector<double>& slx) const {
  slx.clear();
  if (!SolutionDef(whichsol)) return;
  slx.resize(NumVar());
  MSK_getslx(task.get(), whichsol, slx.data());
}
void Mosek::GetSUX(MSKsoltypee whichsol, std::vector<double>& sux) const {
  sux.clear();
  if (!SolutionDef(whichsol)) return;
  sux.resize(NumVar());
  MSK_getsux(task.get(), whichsol, sux.data());
}
void Mosek::GetSLC(MSKsoltypee whichsol, std::vector<double>& slc) const {
  slc.clear();
  if (!SolutionDef(whichsol)) return;
  slc.resize(NumVar());
  MSK_getslc(task.get(), whichsol, slc.data());
}
void Mosek::GetSUC(MSKsoltypee whichsol, std::vector<double>& suc) const {
  suc.clear();
  if (!SolutionDef(whichsol)) return;
  suc.resize(NumVar());
  MSK_getsuc(task.get(), whichsol, suc.data());
}
void Mosek::GetY(MSKsoltypee whichsol, std::vector<double>& y) const {
  y.clear();
  if (!SolutionDef(whichsol)) return;
  y.resize(NumVar());
  MSK_gety(task.get(), whichsol, y.data());
}
void Mosek::GetSKX(MSKsoltypee whichsol, std::vector<MSKstakeye>& skx) const {
  skx.clear();
  if (!SolutionDef(whichsol)) return;
  skx.resize(NumVar());
  MSK_getskx(task.get(), whichsol, skx.data());
}
void Mosek::GetSKC(MSKsoltypee whichsol, std::vector<MSKstakeye>& skc) const {
  skc.clear();
  if (!SolutionDef(whichsol)) return;
  skc.resize(NumVar());
  MSK_getskc(task.get(), whichsol, skc.data());
}

void Mosek::PutParam(MSKiparame par, int value) {
  MSK_putintparam(task.get(), par, value);
}
void Mosek::PutParam(MSKdparame par, double value) {
  MSK_putdouparam(task.get(), par, value);
}
// Query
int Mosek::NumVar() const {
  int n;
  MSK_getnumvar(task.get(), &n);
  return n;
}
int Mosek::NumCon() const {
  int n;
  MSK_getnumcon(task.get(), &n);
  return n;
}
bool Mosek::IsMaximize() const {
  MSKobjsensee sense;
  MSK_getobjsense(task.get(), &sense);
  return sense == MSK_OBJECTIVE_SENSE_MAXIMIZE;
}
double Mosek::GetParam(MSKdparame dpar) const {
  double parval = 0.0;
  MSK_getdouparam(task.get(), dpar, &parval);
  return parval;
}
int Mosek::GetParam(MSKiparame ipar) const {
  int parval;
  MSK_getintparam(task.get(), ipar, &parval);
  return parval;
}

void Mosek::delete_msk_task_func(MSKtask_t t) { MSK_deletetask(&t); }
}  // namespace operations_research::math_opt
