// Copyright 2010-2024 Mosek
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

#include "mosek_solver.h"

#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cwctype>
//#include <ios>
//#include <limits>
#include <limits>
#include <memory>
#include <optional>
//#include <span>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/cleanup/cleanup.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "mosek.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/callback.pb.h"
//#include "ortools/math_opt/core/empty_bounds.h"
//#include "ortools/math_opt/core/inverted_bounds.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/solver_interface.h"
//#include "ortools/math_opt/core/sorted.h"
//#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/solvers/message_callback_data.h"
#include "ortools/math_opt/solvers/mosek.pb.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/util/solve_interrupter.h"
#include "ortools/util/status_macros.h"

namespace operations_research::math_opt {
namespace {

constexpr SupportedProblemStructures kMosekSupportedStructures = {
    .integer_variables = SupportType::kSupported,
    .quadratic_objectives = SupportType::kSupported,
    .quadratic_constraints = SupportType::kSupported,
    .second_order_cone_constraints = SupportType::kSupported,
    .indicator_constraints = SupportType::kSupported,
};


}  // namespace

std::ostream& mosek::operator<<(std::ostream& s, SolSta solsta) {
  switch (solsta) {
    case SolSta::UNKNOWN:
      s << "UNKNOWN";
      break;
    case SolSta::OPTIMAL:
      s << "OPTIMAL";
      break;
    case SolSta::PRIM_FEAS:
      s << "PRIM_FEAS";
      break;
    case SolSta::DUAL_FEAS:
      s << "DUAL_FEAS";
      break;
    case SolSta::PRIM_AND_DUAL_FEAS:
      s << "PRIM_AND_DUAL_FEAS";
      break;
    case SolSta::PRIM_INFEAS_CER:
      s << "PRIM_INFEAS_CER";
      break;
    case SolSta::DUAL_INFEAS_CER:
      s << "DUAL_INFEAS_CER";
      break;
    case SolSta::PRIM_ILLPOSED_CER:
      s << "PRIM_ILLPOSED_CER";
      break;
    case SolSta::DUAL_ILLPOSED_CER:
      s << "DUAL_ILLPOSED_CER";
      break;
    case SolSta::INTEGER_OPTIMAL:
      s << "INTEGER_OPTIMAL";
      break;
  }
  return s;
}

std::ostream& mosek::operator<<(std::ostream& s, ProSta prosta) {
  switch (prosta) {
    case ProSta::UNKNOWN:
      s << "UNKNOWN";
      break;
    case ProSta::PRIM_AND_DUAL_FEAS:
      s << "PRIM_AND_DUAL_FEAS";
      break;
    case ProSta::PRIM_FEAS:
      s << "PRIM_FEAS";
      break;
    case ProSta::DUAL_FEAS:
      s << "DUAL_FEAS";
      break;
    case ProSta::PRIM_INFEAS:
      s << "PRIM_INFEAS";
      break;
    case ProSta::DUAL_INFEAS:
      s << "DUAL_INFEAS";
      break;
    case ProSta::PRIM_AND_DUAL_INFEAS:
      s << "PRIM_AND_DUAL_INFEAS";
      break;
    case ProSta::ILL_POSED:
      s << "ILL_POSED";
      break;
    case ProSta::PRIM_INFEAS_OR_UNBOUNDED:
      s << "PRIM_INFEAS_OR_UNBOUNDED";
      break;
  }
  return s;
}

absl::Status MosekSolver::AddVariables(const VariablesProto& vars) {
  int num_vars = vars.ids_size();
  int firstvar = msk.NumVar();

  {
    int i = 0;
    for (const auto& v : vars.ids()) {
      variable_map[v] = firstvar + i;
      ++i;
    }
  }

  std::vector<double> lbx(num_vars);
  std::vector<double> ubx(num_vars);
  {
    int i = 0;
    for (const auto& v : vars.lower_bounds()) lbx[i++] = v;
  }
  {
    int i = 0;
    for (const auto& v : vars.upper_bounds()) ubx[i++] = v;
  }

  {
    auto r = msk.AppendVars(lbx, ubx);
    if (!r.ok()) return r.status();
  }

  {
    int i = 0;
    for (const bool is_integer : vars.integers()) {
      if (is_integer) {
        RETURN_IF_ERROR(msk.PutVarType(variable_map[i], true));
      }
      ++i;
    }
  }
  {
    int i = 0;
    for (const auto& name : vars.names()) {
      msk.PutVarName(firstvar + i, name.c_str());
      ++i;
    }
  }
  return absl::OkStatus();
}  // MosekSolver::AddVariables

absl::Status MosekSolver::ReplaceObjective(const ObjectiveProto& obj) {
  msk.PutObjName(obj.name());
  RETURN_IF_ERROR(msk.UpdateObjectiveSense(obj.maximize()));
  auto objcof = obj.linear_coefficients();
  msk.PutCFix(obj.offset());
  auto num_vars = msk.NumVar();
  std::vector<double> c(num_vars);
  auto n = objcof.ids_size();
  for (int64_t i = 0; i < n; ++i) {
    c[objcof.ids(i)] = objcof.values(i);
  }
  RETURN_IF_ERROR(msk.PutC(c));

  // quadratic terms 
  if (obj.quadratic_coefficients().row_ids_size() > 0) {
    std::vector<int> subk;
    std::vector<int> subl;
    std::vector<double> val;

    SparseDoubleMatrixToTril(obj.quadratic_coefficients(),subk,subl,val);
    //std::cerr << "subk,subl,val: " << subk.size() << "," << subl.size() << "," << val.size() << std::endl;
    //for (auto [ik,il,iv] = std::make_tuple(subk.begin(),subl.begin(),val.begin());
    //    ik != subk.end();
    //    ++ik,++il,++iv)
    //  std::cerr << "  -- [" << *ik << "," << *il << "]: " << *iv << std::endl;
    RETURN_IF_ERROR(msk.PutQObj(subk,subl,val));
  }

  return absl::OkStatus();
}  // MosekSolver::ReplaceObjective

void MosekSolver::SparseDoubleMatrixToTril(const SparseDoubleMatrixProto& qdata,
                                      std::vector<int>& subk,
                                      std::vector<int>& subl,
                                      std::vector<double>& val) {
  if (qdata.row_ids_size() > 0) {
    // NOTE: this specifies the full Q matrix, and we assume that it is
    // symmetric and only specifies the lower triangular part.
    size_t nqnz = qdata.row_ids_size();
    std::vector<std::tuple<int,int,double>> subklv; subklv.reserve(nqnz);
    for (auto [kit, lit, cit] = std::make_tuple(qdata.row_ids().begin(),
                                                qdata.column_ids().begin(),
                                                qdata.coefficients().begin());
         kit != qdata.row_ids().end() && lit != qdata.column_ids().end() &&
         cit != qdata.coefficients().end();
         ++kit, ++lit, ++cit) {

      if (variable_map.contains(*kit) &&
          variable_map.contains(*lit)) {
        int k = variable_map[*kit];
        int l = variable_map[*lit];
        double v = *cit;
      

        if (k < l) {
          subklv.push_back({l,k,v});
        }
        else {
          subklv.push_back({k,l,v});
        }
      }
    }

    std::sort(subklv.begin(),subklv.end());

    // count 
    size_t nunique = 0; 
    {
      int prevk = -1,prevl = -1;
      for (auto [k,l,v] : subklv) {
        if (prevk != k || prevl != l) {
          ++nunique; prevk = k; prevl = l;
        }
      }
    }
   
    subk.clear(); subk.reserve(nunique);
    subl.clear(); subl.reserve(nunique);
    val.clear();  val.reserve(nunique);

    int prevk = -1,prevl = -1;
    for (auto [k,l,v] : subklv) {
      if (prevk == k && prevl == l) {
        val.back() += v;
      }
      else {
        subk.push_back(k); prevk = k;
        subl.push_back(l); prevl = l;
        val.push_back(v);
      }
    }
  }
}

absl::Status MosekSolver::AddConstraint(int64_t id, const QuadraticConstraintProto& cons) {
  int coni = msk.NumCon();
  double clb = cons.lower_bound();
  double cub = cons.upper_bound();
  {
    auto r = msk.AppendCons(clb, cub);
    if (!r.ok()) return r.status();
  }

  size_t nnz = cons.linear_terms().ids_size();
  std::vector<Mosek::VariableIndex> subj; subj.reserve(nnz);
  std::vector<double> val; val.reserve(nnz);

  for (const auto id : cons.linear_terms().ids()) subj.push_back(variable_map[id]);
  for (const auto c : cons.linear_terms().values()) val.push_back(c);
  RETURN_IF_ERROR(msk.PutARow(coni,subj, val));

  // quadratic terms

  if (cons.quadratic_terms().row_ids_size() > 0) {
    std::vector<int> subk;
    std::vector<int> subl;
    std::vector<double> val;

    SparseDoubleMatrixToTril(cons.quadratic_terms(),subk,subl,val);

    RETURN_IF_ERROR(msk.PutQCon(coni,subk,subl,val));
  }

  quadconstr_map[id] = coni;

  return absl::OkStatus();
}

absl::Status MosekSolver::AddConstraints(const LinearConstraintsProto& cons,
                                         const SparseDoubleMatrixProto& adata) {
  int firstcon = msk.NumCon();
  auto numcon = cons.ids_size();
  {
    int i = 0;
    for (const auto& id : cons.ids()) {
      linconstr_map[id] = i;
      ++i;
    }
  }
  std::vector<double> clb(numcon);
  std::vector<double> cub(numcon);
  {
    int i = 0;
    for (const auto& b : cons.lower_bounds()) clb[i++] = b;
  }
  {
    int i = 0;
    for (const auto& b : cons.upper_bounds()) cub[i++] = b;
  }
  {
    auto r = msk.AppendCons(clb, cub);
    if (!r.ok()) return r.status();
  }

  {
    int i = 0;
    for (const auto& name : cons.names()) {
      msk.PutConName(firstcon + i, name.c_str());
      ++i;
    }
  }

  size_t nnz = adata.row_ids_size();
  std::vector<Mosek::VariableIndex> subj;
  subj.reserve(nnz);
  std::vector<Mosek::ConstraintIndex> subi;
  subi.reserve(nnz);
  std::vector<double> valij;
  valij.reserve(nnz);

  for (const auto id : adata.row_ids()) subi.push_back(linconstr_map[id]);
  for (const auto id : adata.column_ids()) subj.push_back(variable_map[id]);
  for (const auto c : adata.coefficients()) valij.push_back(c);
  RETURN_IF_ERROR(msk.PutAIJList(subi, subj, valij));

  return absl::OkStatus();
}
absl::Status MosekSolver::AddConstraints(const LinearConstraintsProto& cons) {
  int firstcon = msk.NumCon();
  auto numcon = cons.ids_size();
  {
    int i = 0;
    for (const auto& id : cons.ids()) {
      linconstr_map[id] = i;
      ++i;
    }
  }
  std::vector<double> clb(numcon);
  std::vector<double> cub(numcon);
  {
    int i = 0;
    for (const auto& b : cons.lower_bounds()) clb[i++] = b;
  }
  {
    int i = 0;
    for (const auto& b : cons.upper_bounds()) cub[i++] = b;
  }
  {
    auto r = msk.AppendCons(clb, cub);
    if (!r.ok()) return r.status();
  }

  {
    int i = 0;
    for (const auto& name : cons.names()) {
      msk.PutConName(firstcon + i, name.c_str());
      ++i;
    }
  }

  return absl::OkStatus();
}  // MosekSolver::AddConstraints

absl::Status MosekSolver::AddIndicatorConstraints(
    const ::google::protobuf::Map<int64_t, IndicatorConstraintProto>& cons) {
  int i = 0;
  std::vector<Mosek::VariableIndex> subj;
  std::vector<double> cof;
  for (const auto& [id, con] : cons) {
    indconstr_map[id] = i++;
    Mosek::VariableIndex indvar = indconstr_map[con.indicator_id()];

    subj.clear();
    subj.reserve(con.expression().ids_size());
    cof.clear();
    cof.reserve(con.expression().ids_size());

    for (auto id : con.expression().ids()) {
      subj.push_back(variable_map[id]);
    }
    for (auto c : con.expression().values()) {
      cof.push_back(c);
    }

    auto djci =
        msk.AppendIndicatorConstraint(con.activate_on_zero(), indvar, subj, cof,
                                      con.lower_bound(), con.upper_bound());
    if (!djci.ok()) {
      return djci.status();
    }

    RETURN_IF_ERROR(msk.PutDJCName(*djci, con.name()));
  }
  return absl::OkStatus();
}  // MosekSolver::AddIndicatorConstraints

absl::Status MosekSolver::AddConicConstraints(
    const ::google::protobuf::Map<int64_t, SecondOrderConeConstraintProto>&
        cons) {
  std::vector<Mosek::VariableIndex> subj;
  std::vector<double> cof;
  std::vector<int32_t> sizes;
  std::vector<double> b;

  sizes.reserve(cons.size());
  for (const auto& [idx, con] : cons) {
    auto& expr0 = con.upper_bound();
    int64_t totalnnz = expr0.ids_size();
    for (const auto& lexp : con.arguments_to_norm()) {
      totalnnz += lexp.ids_size();
    }

    subj.reserve(totalnnz);
    cof.reserve(totalnnz);
    b.push_back(expr0.offset());

    for (const auto& id : expr0.ids()) {
      subj.push_back(variable_map[id]);
    }
    for (auto c : expr0.coefficients()) {
      cof.push_back(c);
    }

    for (const auto& expri : con.arguments_to_norm()) {
      sizes.push_back(expri.ids_size());
      for (const auto& id : expri.ids()) {
        subj.push_back(variable_map[id]);
      }
      for (auto c : expri.coefficients()) {
        cof.push_back(c);
      }
      b.push_back(expri.offset());
    }

    auto acci = msk.AppendConeConstraint(Mosek::ConeType::SecondOrderCone,
                                         sizes, subj, cof, b);
    if (!acci.ok()) {
      return acci.status();
    }

    RETURN_IF_ERROR(msk.PutACCName(*acci, con.name()));
  }
  return absl::OkStatus();
}

absl::StatusOr<bool> MosekSolver::Update(const ModelUpdateProto& model_update) {
  for (auto id : model_update.deleted_variable_ids()) {
    if (variable_map.contains(id)) {
      int j = variable_map[id];
      variable_map.erase(id);
      RETURN_IF_ERROR(msk.ClearVariable(j));
    }
  }
  for (auto id : model_update.deleted_linear_constraint_ids()) {
    if (linconstr_map.contains(id)) {
      int i = linconstr_map[id];
      linconstr_map.erase(id);
      RETURN_IF_ERROR(msk.ClearConstraint(i));
    }
  }
  for (auto id : model_update.second_order_cone_constraint_updates()
                     .deleted_constraint_ids()) {
    if (coneconstr_map.contains(id)) {
      int64_t i = coneconstr_map[id];
      coneconstr_map.erase(id);
      RETURN_IF_ERROR(msk.ClearConeConstraint(i));
    }
  }
  for (auto id :
       model_update.indicator_constraint_updates().deleted_constraint_ids()) {
    if (indconstr_map.contains(id)) {
      int64_t i = indconstr_map[id];
      indconstr_map.erase(id);
      RETURN_IF_ERROR(msk.ClearDisjunctiveConstraint(i));
    }
  }

  for (auto id : model_update.quadratic_constraint_updates().deleted_constraint_ids()) {
    if (quadconstr_map.contains(id)) {
      int i = quadconstr_map[id];
      quadconstr_map.erase(id);
      RETURN_IF_ERROR(msk.ClearConstraint(i));
    }
  }
  
  for (auto &[id,con] : model_update.quadratic_constraint_updates().new_constraints()) {
    RETURN_IF_ERROR(AddConstraint(id, con));
  }

  RETURN_IF_ERROR(AddVariables(model_update.new_variables()));
  RETURN_IF_ERROR(UpdateVariables(model_update.variable_updates()));
  RETURN_IF_ERROR(AddConstraints(model_update.new_linear_constraints()));
  RETURN_IF_ERROR(
      UpdateConstraints(model_update.linear_constraint_updates(),
                        model_update.linear_constraint_matrix_updates()));

  RETURN_IF_ERROR(UpdateObjective(model_update.objective_updates()));
  RETURN_IF_ERROR(AddConicConstraints(
      model_update.second_order_cone_constraint_updates().new_constraints()));
  RETURN_IF_ERROR(AddIndicatorConstraints(
      model_update.indicator_constraint_updates().new_constraints()));
  //  RETURN_IF_ERROR(UpdateIndicatorConstraint(conupd));
  return true;
}

absl::Status MosekSolver::UpdateVariables(const VariableUpdatesProto& varupds) {
  // std::cout << "MosekSolver::UpdateVariables()" << std::endl;
  for (int64_t i = 0, n = varupds.lower_bounds().ids_size(); i < n; ++i) {
    RETURN_IF_ERROR(msk.UpdateVariableLowerBound(
        variable_map[varupds.lower_bounds().ids(i)],
        varupds.lower_bounds().values(i)));
  }
  for (int64_t i = 0, n = varupds.upper_bounds().ids_size(); i < n; ++i) {
    RETURN_IF_ERROR(msk.UpdateVariableUpperBound(
        variable_map[varupds.upper_bounds().ids(i)],
        varupds.upper_bounds().values(i)));
  }
  for (int64_t i = 0, n = varupds.integers().ids_size(); i < n; ++i) {
    RETURN_IF_ERROR(
        msk.UpdateVariableType(variable_map[varupds.upper_bounds().ids(i)],
                               varupds.integers().values(i)));
  }
  return absl::OkStatus();
}
absl::Status MosekSolver::UpdateConstraints(
    const LinearConstraintUpdatesProto& conupds,
    const SparseDoubleMatrixProto& lincofupds) {
  for (int64_t i = 0, n = conupds.lower_bounds().ids_size(); i < n; ++i) {
    RETURN_IF_ERROR(msk.UpdateConstraintLowerBound(
        linconstr_map[conupds.lower_bounds().ids(i)],
        conupds.lower_bounds().values(i)));
  }
  for (int64_t i = 0, n = conupds.upper_bounds().ids_size(); i < n; ++i) {
    RETURN_IF_ERROR(msk.UpdateConstraintUpperBound(
        linconstr_map[conupds.upper_bounds().ids(i)],
        conupds.upper_bounds().values(i)));
  }

  size_t n = lincofupds.row_ids_size();
  std::vector<int> subi(n);
  std::vector<int> subj(n);
  std::vector<double> valij(lincofupds.coefficients().begin(),
                            lincofupds.coefficients().end());
  {
    int i = 0;
    for (auto id : lincofupds.row_ids()) {
      subi[i] = linconstr_map[id];
      ++i;
    }
  }
  {
    int i = 0;
    for (auto id : lincofupds.column_ids()) {
      subj[i] = variable_map[id];
      ++i;
    }
  }

  RETURN_IF_ERROR(msk.UpdateA(subi, subj, valij));
  return absl::OkStatus();
}
absl::Status MosekSolver::UpdateObjective(
    const ObjectiveUpdatesProto& objupds) {
  const auto& vals = objupds.linear_coefficients();
  std::vector<double> cof(vals.values().begin(), vals.values().end());
  std::vector<Mosek::VariableIndex> subj;
  subj.reserve(cof.size());
  for (auto id : objupds.linear_coefficients().ids())
    subj.push_back(variable_map[id]);

  
  if (objupds.quadratic_coefficients().column_ids_size() > 0) {
    // note: this specifies the full q matrix, and we assume that it is
    // symmetric and only specifies the lower triangular part.
    auto & qobj = objupds.quadratic_coefficients();
    size_t nqnz = qobj.row_ids_size();
    std::vector<std::tuple<int,int,double>> subklv; subklv.reserve(nqnz);
    for (auto [kit,lit,cit] = std::make_tuple(
             qobj.row_ids().begin(),
             qobj.column_ids().begin(),
             qobj.coefficients().begin());
         kit != qobj.row_ids().end() &&
         lit != qobj.column_ids().end() &&
         cit != qobj.coefficients().end();
         ++kit, ++lit, ++cit) {
      int k = variable_map[*kit];
      int l = variable_map[*lit];
      int v = variable_map[*cit];

      if (k < l) {
        subklv.push_back({l,k,v});
      }
      else {
        subklv.push_back({k,l,v});
      }
    }

    std::sort(subklv.begin(),subklv.end());
    
    std::vector<int> subk; subk.reserve(nqnz);
    std::vector<int> subl; subl.reserve(nqnz);
    std::vector<double> val; val.reserve(nqnz);

    int prevk = -1,prevl = -1;
    for (auto [k,l,v] : subklv) {
      if (prevk == k && prevl == l) {
        val.back() += v;
      }
      else {
        subk.push_back(k); prevk = k;
        subk.push_back(l); prevl = l;
        val.push_back(v);
      }
    }

  }






  RETURN_IF_ERROR(msk.UpdateObjectiveSense(objupds.direction_update()));
  RETURN_IF_ERROR(msk.UpdateObjective(objupds.offset_update(), subj, cof));

  return absl::OkStatus();
}
absl::Status MosekSolver::UpdateConstraint(
    const SecondOrderConeConstraintUpdatesProto& conupds) {
  for (auto id : conupds.deleted_constraint_ids()) {
    RETURN_IF_ERROR(msk.ClearConeConstraint(coneconstr_map[id]));
  }

  RETURN_IF_ERROR(AddConicConstraints(conupds.new_constraints()));

  return absl::OkStatus();
}

absl::Status MosekSolver::UpdateConstraint(
    const IndicatorConstraintUpdatesProto& conupds) {
  for (auto id : conupds.deleted_constraint_ids()) {
    RETURN_IF_ERROR(msk.ClearDisjunctiveConstraint(indconstr_map[id]));
  }

  RETURN_IF_ERROR(AddIndicatorConstraints(conupds.new_constraints()));

  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<SolverInterface>> MosekSolver::New(
    const ModelProto& model, const InitArgs&) {
  RETURN_IF_ERROR(ModelIsSupported(model, kMosekSupportedStructures, "Mosek"));

  if (!model.auxiliary_objectives().empty())
    return util::InvalidArgumentErrorBuilder()
           << "Mosek does not support multi-objective models";
  //if (!model.objective().quadratic_coefficients().row_ids().empty()) {
  //  return util::InvalidArgumentErrorBuilder()
  //         << "Mosek does not support models with quadratic objectives";
  //}
  //if (!model.quadratic_constraints().empty()) {
  //  return util::InvalidArgumentErrorBuilder()
  //         << "Mosek does not support models with quadratic constraints";
  // }
  if (!model.sos1_constraints().empty() || !model.sos2_constraints().empty()) {
    return util::InvalidArgumentErrorBuilder()
           << "Mosek does not support models with SOS constraints";
  }

  std::unique_ptr<Mosek> msk(Mosek::Create());
  std::unique_ptr<MosekSolver> mskslv(new MosekSolver(std::move(*msk)));
  mskslv->msk.PutName(model.name());

  RETURN_IF_ERROR(mskslv->AddVariables(model.variables()));
  RETURN_IF_ERROR(mskslv->ReplaceObjective(model.objective()));
  RETURN_IF_ERROR(mskslv->AddConstraints(model.linear_constraints(),
                                         model.linear_constraint_matrix()));
  for (auto &[k,v] : model.quadratic_constraints())
    RETURN_IF_ERROR(mskslv->AddConstraint(k,v));
  RETURN_IF_ERROR(
      mskslv->AddIndicatorConstraints(model.indicator_constraints()));

  std::unique_ptr<SolverInterface> res(std::move(mskslv));

  return res;
}

MosekSolver::MosekSolver(Mosek&& msk) : msk(std::move(msk)) {}

absl::StatusOr<PrimalSolutionProto> MosekSolver::PrimalSolution(
    MSKsoltypee whichsol, const std::vector<int64_t>& ordered_var_ids,
    bool skip_zero_values) {
  auto solsta = msk.GetSolSta(whichsol);
  PrimalSolutionProto sol;
  switch (solsta) {
    case mosek::SolSta::OPTIMAL:
    case mosek::SolSta::INTEGER_OPTIMAL:
    case mosek::SolSta::PRIM_AND_DUAL_FEAS:
    case mosek::SolSta::PRIM_FEAS:
      sol.set_feasibility_status(SolutionStatusProto::SOLUTION_STATUS_FEASIBLE);
      {
        sol.set_objective_value(msk.GetPrimalObj(whichsol));
        std::vector<double> xx;
        msk.GetXX(whichsol, xx);
        SparseDoubleVectorProto vals;

        for (auto k : ordered_var_ids) {
          auto v = xx[variable_map[k]];
          if (!skip_zero_values || v < 0 || v > 0) {
            vals.add_ids(k);
            vals.add_values(v);
          }
        }

        *sol.mutable_variable_values() = std::move(vals);
      }
      break;
    default:
      return absl::NotFoundError("Primal solution not available");
  }
  return std::move(sol);
}
absl::StatusOr<DualSolutionProto> MosekSolver::DualSolution(
    MSKsoltypee whichsol, const std::vector<int64_t>& ordered_y_ids,
    bool skip_y_zeros, const std::vector<int64_t>& ordered_yx_ids,
    bool skip_yx_zeros) {
  auto solsta = msk.GetSolSta(whichsol);
  DualSolutionProto sol;
  switch (solsta) {
    case mosek::SolSta::OPTIMAL:
    case mosek::SolSta::PRIM_AND_DUAL_FEAS:
    case mosek::SolSta::DUAL_FEAS: {
      sol.set_objective_value(msk.GetDualObj(whichsol));
      sol.set_feasibility_status(SolutionStatusProto::SOLUTION_STATUS_FEASIBLE);
      std::vector<int64_t> keys;
      keys.reserve(std::max(variable_map.size(), linconstr_map.size()));
      {
        std::vector<double> slx;
        msk.GetSLX(whichsol, slx);
        std::vector<double> sux;
        msk.GetSUX(whichsol, sux);
        SparseDoubleVectorProto vals;

        for (auto k : ordered_yx_ids) {
          auto j = variable_map[k];
          auto v = slx[j] - sux[j];
          if (!skip_yx_zeros || v < 0.0 || v > 0.0) {
            vals.add_ids(k);
            vals.add_values(v);
          }
        }
        *sol.mutable_reduced_costs() = std::move(vals);
      }
      {
        std::vector<double> y;
        msk.GetY(whichsol, y);
        SparseDoubleVectorProto vals;
        for (auto k : ordered_y_ids) {
          auto v = y[linconstr_map[k]];
          if (!skip_y_zeros || v < 0 || v > 0) {
            vals.add_ids(k);
            vals.add_values(v);
          }
        }

        *sol.mutable_dual_values() = std::move(vals);
      }
    } break;
    default:
      return absl::NotFoundError("Primal solution not available");
  }
  return std::move(sol);
}
absl::StatusOr<SolutionProto> MosekSolver::Solution(
    MSKsoltypee whichsol, const std::vector<int64_t>& ordered_xc_ids,
    const std::vector<int64_t>& ordered_xx_ids, bool skip_xx_zeros,
    const std::vector<int64_t>& ordered_y_ids, bool skip_y_zeros,
    const std::vector<int64_t>& ordered_yx_ids, bool skip_yx_zeros) {
  // std::cout << "MosekSolver::Solution()" << std::endl;
  SolutionProto sol;
  {
    auto r = PrimalSolution(whichsol, ordered_xx_ids, skip_xx_zeros);
    if (r.ok()) *sol.mutable_primal_solution() = std::move(*r);
  }
  {
    auto r = DualSolution(whichsol, ordered_y_ids, skip_y_zeros, ordered_yx_ids,
                          skip_yx_zeros);
    if (r.ok()) *sol.mutable_dual_solution() = std::move(*r);
  }

  if (whichsol == MSK_SOL_BAS) {
    // std::cout << "MosekSolver::Solution(): Basis!" << std::endl;
    BasisProto bas;
    SparseBasisStatusVector csta;
    SparseBasisStatusVector xsta;
    std::vector<MSKstakeye> sk;
    msk.GetSKX(whichsol, sk);

    for (auto k : ordered_xx_ids) {
      auto v = variable_map[k];
      xsta.add_ids(k);
      switch (sk[v]) {
        case MSK_SK_LOW:
          xsta.add_values(BasisStatusProto::BASIS_STATUS_AT_LOWER_BOUND);
          break;
        case MSK_SK_UPR:
          xsta.add_values(BasisStatusProto::BASIS_STATUS_AT_UPPER_BOUND);
          break;
        case MSK_SK_FIX:
          xsta.add_values(BasisStatusProto::BASIS_STATUS_FIXED_VALUE);
          break;
        case MSK_SK_BAS:
          xsta.add_values(BasisStatusProto::BASIS_STATUS_BASIC);
          break;
        case MSK_SK_INF:
        case MSK_SK_SUPBAS:
        case MSK_SK_UNK:
          xsta.add_values(BasisStatusProto::BASIS_STATUS_UNSPECIFIED);
          break;
      }
    }
    sk.clear();
    msk.GetSKC(whichsol, sk);
    for (auto k : ordered_xc_ids) {
      auto v = linconstr_map[k];
      csta.add_ids(k);
      switch (sk[v]) {
        case MSK_SK_LOW:
          csta.add_values(BasisStatusProto::BASIS_STATUS_AT_LOWER_BOUND);
          break;
        case MSK_SK_UPR:
          csta.add_values(BasisStatusProto::BASIS_STATUS_AT_UPPER_BOUND);
          break;
        case MSK_SK_FIX:
          csta.add_values(BasisStatusProto::BASIS_STATUS_FIXED_VALUE);
          break;
        case MSK_SK_BAS:
          csta.add_values(BasisStatusProto::BASIS_STATUS_BASIC);
          break;
        case MSK_SK_INF:
        case MSK_SK_SUPBAS:
        case MSK_SK_UNK:
          csta.add_values(BasisStatusProto::BASIS_STATUS_UNSPECIFIED);
          break;
      }
    }
    *bas.mutable_variable_status() = std::move(xsta);
    *bas.mutable_constraint_status() = std::move(csta);

    auto solsta = msk.GetSolSta(whichsol);
    switch (solsta) {
      case mosek::SolSta::OPTIMAL:
      case mosek::SolSta::INTEGER_OPTIMAL:
      case mosek::SolSta::PRIM_AND_DUAL_FEAS:
      case mosek::SolSta::PRIM_FEAS:
        bas.set_basic_dual_feasibility(
            SolutionStatusProto::SOLUTION_STATUS_FEASIBLE);
        break;
      default:
        bas.set_basic_dual_feasibility(
            SolutionStatusProto::SOLUTION_STATUS_UNSPECIFIED);
        break;
    }

    *sol.mutable_basis() = std::move(bas);
  }
  return std::move(sol);
}

absl::StatusOr<PrimalRayProto> MosekSolver::PrimalRay(
    MSKsoltypee whichsol, const std::vector<int64_t>& ordered_xx_ids,
    bool skip_xx_zeros) {
  auto solsta = msk.GetSolSta(whichsol);
  if (solsta == mosek::SolSta::DUAL_INFEAS_CER)
    return absl::NotFoundError("Certificate not available");

  std::vector<double> xx;
  msk.GetXX(whichsol, xx);
  PrimalRayProto ray;
  SparseDoubleVectorProto data;
  for (auto k : ordered_xx_ids) {
    auto v = xx[variable_map[k]];
    if (!skip_xx_zeros || v < 0 || v > 0) {
      data.add_ids(k);
      data.add_values(v);
    }
  }
  *ray.mutable_variable_values() = data;
  return ray;
}

absl::StatusOr<DualRayProto> MosekSolver::DualRay(
    MSKsoltypee whichsol, const std::vector<int64_t>& ordered_y_ids,
    bool skip_y_zeros, const std::vector<int64_t>& ordered_yx_ids,
    bool skip_yx_zeros) {
  auto solsta = msk.GetSolSta(whichsol);

  if (solsta == mosek::SolSta::PRIM_INFEAS_CER)
    return absl::NotFoundError("Certificate not available");

  std::vector<double> slx;
  msk.GetSLX(whichsol, slx);
  std::vector<double> sux;
  msk.GetSUX(whichsol, slx);
  std::vector<double> y;
  msk.GetY(whichsol, y);
  DualRayProto ray;
  SparseDoubleVectorProto xdata;
  SparseDoubleVectorProto cdata;
  for (auto k : ordered_yx_ids) {
    auto j = variable_map[k];
    auto v = slx[j] - sux[j];
    if (!skip_yx_zeros || v < 0 || v > 0) {
      xdata.add_ids(k);
      xdata.add_values(v);
    }
  }
  for (auto& k : ordered_y_ids) {
    auto v = y[linconstr_map[k]];
    if (!skip_y_zeros || v < 0 || v > 0) {
      cdata.add_ids(k);
      cdata.add_values(v);
    }
  }
  *ray.mutable_dual_values() = xdata;
  *ray.mutable_reduced_costs() = cdata;
  return ray;
}

absl::StatusOr<SolveResultProto> MosekSolver::Solve(
    const SolveParametersProto& parameters,  // solver settings
    const ModelSolveParametersProto& model_parameters,
    MessageCallback message_cb,
    const CallbackRegistrationProto& callback_registration,
    Callback cb,  // using Callback = std::function<CallbackResultProto(const
                  // CallbackDataProto&)>, from base_solver.h
    const SolveInterrupter* const solve_interrupter) {
  // Solve parameters that we support:
  // - google.protobuf.Duration time_limit
  // - optional int64 iteration_limit
  // - optional int64 node_limit
  // - optional double cutoff_limit
  // - bool enable_output
  // - optional int32 threads
  // - optional double absolute_gap_tolerance
  // - optional double relative_gap_tolerance
  // - LPAlgorithmProto lp_algorithm
  // Solve parameters that we may support:
  // - optional double best_bound_limit
  // - optional double objective_limit
  // Solve parameters that we do not support:
  // - optional int32 solution_pool_size
  // - optional int32 solution_limit
  // - optional int32 random_seed
  // - EmphasisProto presolve
  // - EmphasisProto cuts
  // - EmphasisProto heuristics
  // - EmphasisProto scaling
  

  auto solve_start = absl::Now();

  double dpar_optimizer_max_time = msk.GetParam(MSK_DPAR_OPTIMIZER_MAX_TIME);
  int ipar_intpnt_max_iterations = msk.GetParam(MSK_IPAR_INTPNT_MAX_ITERATIONS);
  int ipar_sim_max_iterations = msk.GetParam(MSK_IPAR_SIM_MAX_ITERATIONS);
  double dpar_upper_obj_cut = msk.GetParam(MSK_DPAR_UPPER_OBJ_CUT);
  double dpar_lower_obj_cut = msk.GetParam(MSK_DPAR_LOWER_OBJ_CUT);
  int ipar_num_threads = msk.GetParam(MSK_IPAR_NUM_THREADS);
  double dpar_mio_tol_abs_gap = msk.GetParam(MSK_DPAR_MIO_TOL_ABS_GAP);
  double dpar_mio_tol_rel_gap = msk.GetParam(MSK_DPAR_MIO_TOL_REL_GAP);
  double dpar_intpnt_tol_rel_gap = msk.GetParam(MSK_DPAR_INTPNT_TOL_REL_GAP);
  double dpar_intpnt_co_tol_rel_gap =
      msk.GetParam(MSK_DPAR_INTPNT_CO_TOL_REL_GAP);
  int ipar_optimizer = msk.GetParam(MSK_IPAR_OPTIMIZER);

  auto _guard_reset_params = absl::MakeCleanup([&]() {
    msk.PutParam(MSK_DPAR_OPTIMIZER_MAX_TIME, dpar_optimizer_max_time);
    msk.PutParam(MSK_IPAR_INTPNT_MAX_ITERATIONS, ipar_intpnt_max_iterations);
    msk.PutParam(MSK_IPAR_SIM_MAX_ITERATIONS, ipar_sim_max_iterations);
    msk.PutParam(MSK_DPAR_UPPER_OBJ_CUT, dpar_upper_obj_cut);
    msk.PutParam(MSK_DPAR_LOWER_OBJ_CUT, dpar_lower_obj_cut);
    msk.PutParam(MSK_IPAR_NUM_THREADS, ipar_num_threads);
    msk.PutParam(MSK_DPAR_MIO_TOL_ABS_GAP, dpar_mio_tol_abs_gap);
    msk.PutParam(MSK_DPAR_MIO_TOL_REL_GAP, dpar_mio_tol_rel_gap);
    msk.PutParam(MSK_DPAR_INTPNT_TOL_REL_GAP, dpar_intpnt_tol_rel_gap);
    msk.PutParam(MSK_DPAR_INTPNT_CO_TOL_REL_GAP, dpar_intpnt_co_tol_rel_gap);
  });

  if (parameters.has_time_limit()) {
    OR_ASSIGN_OR_RETURN3(
        const absl::Duration time_limit,
        util_time::DecodeGoogleApiProto(parameters.time_limit()),
        _ << "invalid time_limit value for HiGHS.");
    msk.PutParam(MSK_DPAR_OPTIMIZER_MAX_TIME,
                 absl::ToDoubleSeconds(time_limit));
  }

  if (parameters.has_iteration_limit()) {
    const int iter_limit = parameters.iteration_limit();

    msk.PutParam(MSK_IPAR_INTPNT_MAX_ITERATIONS, iter_limit);
    msk.PutParam(MSK_IPAR_SIM_MAX_ITERATIONS, iter_limit);
  }

  // Not supported in MOSEK 10.2
  // int ipar_mio_
  // if (parameters.has_node_limit()) {
  //  ASSIGN_OR_RETURN(
  //      const int node_limit,
  //      SafeIntCast(parameters.node_limit(), "node_limit"));
  //  msk.PutIntParam(MSK_IPAR_MIO__MAX_NODES, node_limit);
  //}

  // Not supported by MOSEK?
  // if (parameters.has_cutoff_limit()) {
  //}
  if (parameters.has_objective_limit()) {
    if (msk.IsMaximize())
      msk.PutParam(MSK_DPAR_UPPER_OBJ_CUT, parameters.cutoff_limit());
    else
      msk.PutParam(MSK_DPAR_LOWER_OBJ_CUT, parameters.cutoff_limit());
  }

  if (parameters.has_threads()) {
    msk.PutParam(MSK_IPAR_NUM_THREADS, parameters.threads());
  }

  if (parameters.has_absolute_gap_tolerance()) {
    msk.PutParam(MSK_DPAR_MIO_TOL_ABS_GAP, parameters.absolute_gap_tolerance());
  }

  if (parameters.has_relative_gap_tolerance()) {
    msk.PutParam(MSK_DPAR_INTPNT_TOL_REL_GAP,
                 parameters.absolute_gap_tolerance());
    msk.PutParam(MSK_DPAR_INTPNT_CO_TOL_REL_GAP,
                 parameters.absolute_gap_tolerance());
    msk.PutParam(MSK_DPAR_MIO_TOL_REL_GAP, parameters.absolute_gap_tolerance());
  }

  switch (parameters.lp_algorithm()) {
    case LP_ALGORITHM_BARRIER:
      msk.PutParam(MSK_IPAR_OPTIMIZER, MSK_OPTIMIZER_INTPNT);
      break;
    case LP_ALGORITHM_DUAL_SIMPLEX:
      msk.PutParam(MSK_IPAR_OPTIMIZER, MSK_OPTIMIZER_DUAL_SIMPLEX);
      break;
    case LP_ALGORITHM_PRIMAL_SIMPLEX:
      msk.PutParam(MSK_IPAR_OPTIMIZER, MSK_OPTIMIZER_PRIMAL_SIMPLEX);
      break;
    default:
      // use default auto select, usually intpnt
      msk.PutParam(MSK_IPAR_OPTIMIZER, MSK_OPTIMIZER_FREE);
      break;
  }

  // TODO: parameter enable_output

  bool skip_xx_zeros =
      model_parameters.variable_values_filter().skip_zero_values();
  bool skip_y_zeros = model_parameters.dual_values_filter().skip_zero_values();
  bool skip_yx_zeros =
      model_parameters.reduced_costs_filter().skip_zero_values();
  bool filter_ids = model_parameters.variable_values_filter().filter_by_ids();

  std::vector<int64_t> ordered_xc_ids;
  std::vector<int64_t> ordered_xx_ids;
  std::vector<int64_t> ordered_y_ids;
  std::vector<int64_t> ordered_yx_ids;

  ordered_xc_ids.reserve(linconstr_map.size());
  for (auto [id, idx] : linconstr_map) ordered_xc_ids.push_back(id);
  std::sort(ordered_xc_ids.begin(), ordered_xc_ids.end());

  if (!skip_xx_zeros) {
    ordered_xx_ids.reserve(variable_map.size());
    for (auto [id, idx] : variable_map) {
      ordered_xx_ids.push_back(id);
    }
    std::sort(ordered_xx_ids.begin(), ordered_xx_ids.end());
  } else {
    ordered_xx_ids.reserve(
        model_parameters.variable_values_filter().filtered_ids().size());
    for (auto id : model_parameters.variable_values_filter().filtered_ids()) {
      if (variable_map.contains(id)) ordered_xx_ids.push_back(id);
    }
  }

  if (!model_parameters.dual_values_filter().filter_by_ids()) {
    ordered_y_ids.reserve(linconstr_map.size());
    for (auto [id, idx] : linconstr_map) ordered_y_ids.push_back(id);
    std::sort(ordered_y_ids.begin(), ordered_y_ids.end());
  } else {
    ordered_y_ids.reserve(
        model_parameters.dual_values_filter().filtered_ids().size());
    for (auto id : model_parameters.dual_values_filter().filtered_ids())
      ordered_y_ids.push_back(id);
  }

  if (!model_parameters.reduced_costs_filter().filter_by_ids()) {
    ordered_yx_ids.reserve(linconstr_map.size());
    for (auto [id, idx] : variable_map) ordered_yx_ids.push_back(id);
    std::sort(ordered_yx_ids.begin(), ordered_yx_ids.end());
  } else {
    ordered_yx_ids.reserve(
        model_parameters.reduced_costs_filter().filtered_ids().size());
    for (auto id : model_parameters.reduced_costs_filter().filtered_ids())
      ordered_yx_ids.push_back(id);
  }

  MSKrescodee trm = MSK_RES_OK;
  {
    BufferedMessageCallback bmsg_cb(message_cb);
    // TODO: Use model_parameters
    msk.WriteData("test.opf");
    auto r = msk.Optimize(
        [&](const std::string& msg) { bmsg_cb.OnMessage(msg); },
        [&](MSKcallbackcodee code, absl::Span<const double> dinf,
            absl::Span<const int> iinf, absl::Span<const int64_t> liinf) {
          if (cb) {
            CallbackDataProto cbdata;
            switch (code) {
              case MSK_CALLBACK_IM_SIMPLEX:
                cbdata.mutable_simplex_stats()->set_iteration_count(
                    liinf[MSK_LIINF_SIMPLEX_ITER]);
                cbdata.mutable_simplex_stats()->set_objective_value(
                    dinf[MSK_DINF_SIM_OBJ]);
                // cbdata.mutable_simplex_stats(/->set_primal_infeasibility(...);
                // cbdata.mutable_simplex_stats()->set_dual_infeasibility(...);
                // cbdata.mutable_simplex_stats()->is_perturbed(...);
                cbdata.set_event(CALLBACK_EVENT_SIMPLEX);
                break;
              case MSK_CALLBACK_IM_MIO:
                cbdata.mutable_mip_stats()->set_primal_bound(
                    dinf[MSK_DINF_MIO_OBJ_BOUND]);
                // cbdata.mutable_mip_stats()->set_dual_bound(...);
                cbdata.mutable_mip_stats()->set_explored_nodes(
                    iinf[MSK_IINF_MIO_NUM_SOLVED_NODES]);
                // cbdata.mutable_mip_stats()->set_open_nodes(...);
                cbdata.mutable_mip_stats()->set_simplex_iterations(
                    liinf[MSK_LIINF_MIO_SIMPLEX_ITER]);
                // cbdata.mutable_mip_stats()->set_number_of_solutions_found(...);
                // cbdata.mutable_mip_stats()->set_cutting_planes_in_lp(...);
                cbdata.set_event(CALLBACK_EVENT_MIP);
                break;
              case MSK_CALLBACK_NEW_INT_MIO:
                cbdata.set_event(CALLBACK_EVENT_MIP_SOLUTION);
                {
                  std::vector<double> xx;
                  msk.GetXX(MSK_SOL_ITG, xx);

                  SparseDoubleVectorProto primal;

                  for (auto id : ordered_xx_ids) {
                    auto v = xx[variable_map[id]];
                    if (!skip_xx_zeros || v > 0.0 || v < 0.0) {
                      primal.add_ids(id);
                      primal.add_values(v);
                    }
                  }
                  *cbdata.mutable_primal_solution_vector() = primal;
                }
                break;
              case MSK_CALLBACK_IM_PRESOLVE:
                cbdata.set_event(CALLBACK_EVENT_PRESOLVE);
                break;
              case MSK_CALLBACK_IM_CONIC:
              case MSK_CALLBACK_IM_INTPNT:
                cbdata.mutable_barrier_stats()->set_iteration_count(
                    liinf[MSK_IINF_INTPNT_ITER]);
                cbdata.mutable_barrier_stats()->set_primal_objective(
                    dinf[MSK_DINF_INTPNT_PRIMAL_OBJ]);
                cbdata.mutable_barrier_stats()->set_dual_objective(
                    dinf[MSK_DINF_INTPNT_DUAL_OBJ]);
                // cbdata.mutable_barrier_stats()->set_complementarity(...);
                // cbdata.mutable_barrier_stats()->set_primal_infeasibility(...);
                // cbdata.mutable_barrier_stats()->set_dual_infeasibility(...);

                cbdata.set_event(CALLBACK_EVENT_BARRIER);
                break;
              default:
                cbdata.set_event(CALLBACK_EVENT_UNSPECIFIED);
                break;
            }

            auto r = cb(cbdata);
            if (r.ok()) {
              return r->terminate();
            }
          }
          return false;
        });
    // msk.WriteData("__test.ptf");
    // std::cout << "MosekSolver::Solve() optimize -> " << r << std::endl;
    if (!r.ok()) return r.status();
    trm = *r;
  }

  MSKsoltypee whichsol{};
  bool soldef = true;
  if (msk.SolutionDef(MSK_SOL_ITG)) {
    whichsol = MSK_SOL_ITG;
  } else if (msk.SolutionDef(MSK_SOL_BAS)) {
    whichsol = MSK_SOL_BAS;
  } else if (msk.SolutionDef(MSK_SOL_ITR)) {
    whichsol = MSK_SOL_ITR;
  } else {
    soldef = false;
  }

  TerminationProto trmp;
  mosek::ProSta prosta{};
  mosek::SolSta solsta{};
  if (!soldef) {
    auto [msg, name, code] = msk.LastError();
    trmp = TerminateForReason(
        msk.IsMaximize(),
        TerminationReasonProto::TERMINATION_REASON_NO_SOLUTION_FOUND, msg);
  } else {
    prosta = msk.GetProSta(whichsol);
    solsta = msk.GetSolSta(whichsol);

    // Attempt to determine TerminationProto from Mosek Termination code,
    // problem status and solution status.

    if (solsta == mosek::SolSta::OPTIMAL ||
        solsta == mosek::SolSta::INTEGER_OPTIMAL) {

      trmp = OptimalTerminationProto(msk.GetPrimalObj(whichsol),
                                     msk.GetDualObj(whichsol), 
                                     "");
      // Hack: 
      trmp.mutable_objective_bounds()->set_primal_bound(trmp.objective_bounds().primal_bound());
      trmp.mutable_objective_bounds()->set_dual_bound(trmp.objective_bounds().dual_bound());

    } else if (solsta == mosek::SolSta::PRIM_INFEAS_CER)
      trmp = InfeasibleTerminationProto(
          msk.IsMaximize(),
          FeasibilityStatusProto::FEASIBILITY_STATUS_FEASIBLE);
    else if (prosta == mosek::ProSta::PRIM_INFEAS_OR_UNBOUNDED)
      trmp = InfeasibleOrUnboundedTerminationProto(msk.IsMaximize());
    else if (solsta == mosek::SolSta::DUAL_INFEAS_CER)
      trmp = UnboundedTerminationProto(msk.IsMaximize());
    else if (solsta == mosek::SolSta::PRIM_AND_DUAL_FEAS ||
             solsta == mosek::SolSta::PRIM_FEAS) {
      LimitProto lim = LimitProto::LIMIT_UNSPECIFIED;
      switch (trm) {
        case MSK_RES_TRM_MAX_ITERATIONS:
          lim = LimitProto::LIMIT_ITERATION;
          break;
        case MSK_RES_TRM_MAX_TIME:
          lim = LimitProto::LIMIT_TIME;
          break;
        case MSK_RES_TRM_NUM_MAX_NUM_INT_SOLUTIONS:
          lim = LimitProto::LIMIT_SOLUTION;
          break;
#if MSK_VERSION_MAJOR >= 11
        case MSK_RES_TRM_SERVER_MAX_MEMORY:
          lim = LimitProto::LIMIT_MEMORY;
          break;
#endif
        // LIMIT_CUTOFF
        case MSK_RES_TRM_OBJECTIVE_RANGE:
          lim = LimitProto::LIMIT_OBJECTIVE;
          break;
        case MSK_RES_TRM_NUMERICAL_PROBLEM:
          lim = LimitProto::LIMIT_NORM;
          break;
        case MSK_RES_TRM_USER_CALLBACK:
          lim = LimitProto::LIMIT_INTERRUPTED;
          break;
        case MSK_RES_TRM_STALL:
          lim = LimitProto::LIMIT_SLOW_PROGRESS;
          break;
        default:
          lim = LimitProto::LIMIT_OTHER;
          break;
      }
      if (solsta == mosek::SolSta::PRIM_AND_DUAL_FEAS)
        trmp = FeasibleTerminationProto(msk.IsMaximize(), lim,
                                        msk.GetPrimalObj(whichsol),
                                        msk.GetDualObj(whichsol));
      else
        trmp = FeasibleTerminationProto(
            msk.IsMaximize(), lim, msk.GetPrimalObj(whichsol), std::nullopt);
    } else {
      trmp = NoSolutionFoundTerminationProto(msk.IsMaximize(),
                                             LimitProto::LIMIT_UNSPECIFIED);
    }
  }

  SolveResultProto result;
  *result.mutable_termination() = trmp;

  if (soldef) {
    // TODO: Use model_parameters
    switch (solsta) {
      case mosek::SolSta::OPTIMAL:
      case mosek::SolSta::INTEGER_OPTIMAL:
      case mosek::SolSta::PRIM_FEAS:
      case mosek::SolSta::DUAL_FEAS:
      case mosek::SolSta::PRIM_AND_DUAL_FEAS: 
        {
          auto r = Solution(whichsol, ordered_xc_ids, ordered_xx_ids,
                            skip_xx_zeros, ordered_y_ids, skip_y_zeros,
                            ordered_yx_ids, skip_yx_zeros);          
          if (r.ok()) { 
            *result.add_solutions() = std::move(*r);
          }
          //if (msk.IsMaximize()) {
          //  result.mutable_termination()->mutable_objective_bounds()->set_primal_bound(- std::numeric_limits<double>::infinity());
          //  result.mutable_termination()->mutable_objective_bounds()->set_dual_bound(std::numeric_limits<double>::infinity());
          //} 
          //else {
          //  result.mutable_termination()->mutable_objective_bounds()->set_primal_bound(std::numeric_limits<double>::infinity());
          //  result.mutable_termination()->mutable_objective_bounds()->set_dual_bound(- std::numeric_limits<double>::infinity());
          //}
        }
        break;
      case mosek::SolSta::DUAL_INFEAS_CER: {
        auto r = PrimalRay(whichsol, ordered_xx_ids, skip_xx_zeros);
        if (r.ok()) {
          *result.add_primal_rays() = std::move(*r);
        }
      } break;
      case mosek::SolSta::PRIM_INFEAS_CER: {
        auto r = DualRay(whichsol, ordered_y_ids, skip_y_zeros, ordered_yx_ids,
                         skip_yx_zeros);
        if (r.ok()) {
          *result.add_dual_rays() = std::move(*r);
        }
      } break;
      case mosek::SolSta::PRIM_ILLPOSED_CER:
      case mosek::SolSta::DUAL_ILLPOSED_CER:
      case mosek::SolSta::UNKNOWN:
        break;
    }
  }

  SolveStatsProto * stats = result.mutable_solve_stats();
  stats->set_simplex_iterations(msk.GetIntInfoItem(MSK_IINF_SIM_PRIMAL_ITER)+msk.GetIntInfoItem(MSK_IINF_SIM_DUAL_ITER));
  stats->set_barrier_iterations(msk.GetIntInfoItem(MSK_IINF_INTPNT_ITER));
  stats->set_node_count(msk.GetIntInfoItem(MSK_IINF_MIO_NUM_SOLVED_NODES));

  auto solve_time_proto = util_time::EncodeGoogleApiProto(absl::Now() - solve_start);
  if (solve_time_proto.ok()) {
    *stats->mutable_solve_time() = *solve_time_proto;
  }

  return result;
}

absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
MosekSolver::ComputeInfeasibleSubsystem(const SolveParametersProto&,
                                        MessageCallback,
                                        const SolveInterrupter*) {
  return absl::UnimplementedError(
      "MOSEK does not yet support computing an infeasible subsystem");
}

MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_MOSEK, MosekSolver::New);

}  // namespace operations_research::math_opt
