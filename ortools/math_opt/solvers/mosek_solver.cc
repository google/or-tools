// Copyright 2010-2024 Google LLC
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

#include <absl/container/flat_hash_map.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ios>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "mosek_solver.h"
#include "absl/log/check.h"
#include "absl/cleanup/cleanup.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
//#include "absl/status/status.h"
#include "absl/types/span.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_builder.h"
//#include "ortools/base/status_macros.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/empty_bounds.h"
#include "ortools/math_opt/core/inverted_bounds.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/core/sorted.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/solvers/message_callback_data.h"
#include "ortools/math_opt/solvers/mosek.pb.h"
#include "ortools/third_party_solvers/mosekstable12_environment.h"
#include "ortools/util/solve_interrupter.h"
#include "ortools/util/status_macros.h"

namespace operations_research::math_opt {
    namespace MSK = operations_research::MSK12;


constexpr SupportedProblemStructures kMosekSupportedStructures = {
    .integer_variables = SupportType::kSupported,
    .second_order_cone_constraints = SupportType::kSupported,
    .indicator_constraints = SupportType::kSupported,
};

absl::Status MosekSolver::AddVariables(const VariablesProto& vars) {
    int first_var = MSK::get_num_var(task.get());
    int add_num_vars = vars.ids_size();
    std::vector<double> lbx(vars.lower_bounds().begin(),vars.lower_bounds().begin()+add_num_vars);
    std::vector<double> ubx(vars.upper_bounds().begin(),vars.upper_bounds().begin()+add_num_vars);

    {
        int j = first_var;
        for (const auto &i : vars.ids()) {
            variable_map[i] = j++;
        }
    }

    if (MSK::RES_OK != MSK::append_vars(task.get(),add_num_vars)) {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }
    if (MSK::RES_OK != MSK::put_var_bound_slice(task.get(),first_var,add_num_vars,lbx.data(),ubx.data())) {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }

    {
        int i = 0;
        for (const bool is_integer : vars.integers()) {
            if (is_integer) {
                MSK::put_var_type(task.get(),first_var+i,MSK::VariableType::INTEGER);
            }
            ++i;
        }
    }
    {
        int j = first_var;
        for (const auto& name : vars.names()) {
            MSK::put_var_name(task.get(),j,name.c_str());
            ++j;
        }
    }
    return absl::OkStatus();
}  // MosekSolver::AddVariables



absl::Status MosekSolver::ReplaceObjective(const ObjectiveProto& obj) {
    MSK::put_obj_name(task.get(),obj.name().c_str());
    MSK::put_obj_sense(task.get(),obj.maximize() ? MSK::ObjSense::MAXIMIZE : MSK::ObjSense::MINIMIZE);
    auto objcof = obj.linear_coefficients();

    MSK::put_row_g(task.get(),0,obj.offset());

    auto num_vars = MSK::get_num_var(task.get());
    std::vector<int32_t> cj(num_vars); for (int j = 0; j < num_vars; ++j) cj[j] = j;
    std::vector<double> c(num_vars);
    auto n = objcof.ids_size();
    for (int64_t i = 0; i < n; ++i) {
        c[variable_map[objcof.ids(i)]] = objcof.values(i);
    }

    if (MSK::RES_OK != MSK::put_row(task.get(),0,num_vars,cj.data(),c.data())) {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }
    return absl::OkStatus();
}  // MosekSolver::ReplaceObjective


absl::Status MosekSolver::AddConstraints(const LinearConstraintsProto& cons,
                                         const SparseDoubleMatrixProto& adata)
{
    //std::cerr << __FILE__ << ":" << __LINE__ << ": MosekSolver::AddConstraints" << std::endl;
    int64_t first_con = MSK::get_num_con(task.get());
    int64_t first_row = MSK::get_num_row(task.get());

    int64_t add_num_con = cons.ids_size();
    {
        int64_t rowi = first_row;
        int64_t coni = first_con;
        for (const auto& id : cons.ids()) {
            linconstr_map[id] = std::make_tuple(coni,coni+1,rowi);
            coni += 2; ++rowi;
        }
    }

    int64_t nnz = adata.row_ids_size();
    std::vector<int32_t> subj(nnz);
    std::vector<int64_t> subi(nnz);
    for (int64_t i = 0; i < nnz; ++i) {
        auto [lowi,upri,rowi] = linconstr_map[adata.row_ids(i)];
        subi[i] = rowi;
        subj[i] = variable_map[adata.column_ids(i)];
    }
    std::vector<double> valij(adata.coefficients().begin(),adata.coefficients().end());

    std::vector<int64_t> con_dom(add_num_con*2);
    std::vector<int64_t> con_rows(add_num_con*2);
    std::vector<double>  con_rhs(add_num_con*2);
    //std::vector<int64_t> con_nrows(add_num_con*2,1);

    int64_t dom_rminus, dom_rplus, dom_r;
    if (MSK::RES_OK != MSK::get_domain_rplus(task.get(),&dom_rplus) ||
        MSK::RES_OK != MSK::get_domain_rminus(task.get(),&dom_rminus) ||
        MSK::RES_OK != MSK::get_domain_r(task.get(),&dom_r))
    {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }

    for (int64_t i = 0,k = 0; i < add_num_con; ++i, k += 2) {
        con_rows[k]   = first_row+i;
        con_rows[k+1] = first_row+i;
        con_rhs[k]   = cons.lower_bounds(i);
        con_rhs[k+1] = cons.upper_bounds(i);
        con_dom[k]   = std::isfinite(con_rhs[k]) ? dom_rplus : dom_r;
        con_dom[k+1] = std::isfinite(con_rhs[k+1]) ? dom_rminus : dom_r;
    }

    if (MSK::RES_OK != MSK::append_rows(task.get(),add_num_con) ||
        MSK::RES_OK != MSK::append_cons(task.get(),
                                        2*add_num_con,
                                        con_dom.data(),
                                        con_rows.size(),
                                        con_rows.data(),
                                        con_rhs.data()) ||
        MSK::RES_OK != MSK::put_ijc_list(task.get(),
                                         nnz,
                                         subi.data(),
                                         subj.data(),
                                         valij.data()))
    {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }

    {
        int64_t i = first_con;
        for (const auto& name : cons.names()) {
            MSK::put_con_name(task.get(),i++, name.c_str());
            MSK::put_con_name(task.get(),i++, name.c_str());
        }
    }

    return absl::OkStatus();
}

// Add empty constraints block
absl::Status MosekSolver::AddConstraints(const LinearConstraintsProto& cons) {
    int64_t first_con = MSK::get_num_con(task.get());
    int64_t first_row = MSK::get_num_row(task.get());

    auto add_num_con = cons.ids_size();
    {
        int64_t rowi = first_row;
        int64_t coni = first_con;
        for (const auto& id : cons.ids()) {
            linconstr_map[id] = std::make_tuple(coni,coni+1,rowi);
            coni += 2; ++rowi;
        }
    }

    std::vector<int64_t> con_dom(add_num_con*2);
    std::vector<int64_t> con_rows(add_num_con*2);
    std::vector<int64_t> con_nrows(add_num_con*2,1);
    std::vector<double>  con_rhs(add_num_con*2);

    int64_t dom_rminus, dom_rplus, dom_r;
    if (MSK::RES_OK != MSK::get_domain_rplus(task.get(),&dom_rplus) ||
        MSK::RES_OK != MSK::get_domain_rminus(task.get(),&dom_rminus) ||
        MSK::RES_OK != MSK::get_domain_r(task.get(),&dom_r))
    {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }

    for (int64_t i = 0,k = 0; i < add_num_con; ++i, k += 2) {
        con_rows[k]   = first_row+i;
        con_rows[k+1] = first_row+i;
        con_rhs[k]   = cons.lower_bounds(i);
        con_rhs[k+1] = cons.upper_bounds(i);
        con_dom[k]   = std::isfinite(con_rhs[k]) ? dom_rplus : dom_r;
        con_dom[k+1] = std::isfinite(con_rhs[k+1]) ? dom_rminus : dom_r;
    }

    if (MSK::RES_OK != MSK::append_rows(task.get(),add_num_con) ||
        MSK::RES_OK != MSK::append_cons(task.get(),
                                        2*add_num_con,
                                        con_dom.data(),
                                        con_rows.size(),
                                        con_rows.data(),
                                        con_rhs.data()))
    {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }

    {
        int64_t i = first_con;
        for (const auto& name : cons.names()) {
            MSK::put_con_name(task.get(),i++, name.c_str());
            MSK::put_con_name(task.get(),i++, name.c_str());
        }
    }

    return absl::OkStatus();
}  // MosekSolver::AddConstraints

absl::Status MosekSolver::AddIndicatorConstraints(
    const ::google::protobuf::Map<int64_t, IndicatorConstraintProto>& cons)
{
    if (cons.size() == 0)
        return absl::OkStatus();
    // Implemented as a DJC:
    //     ind = 0
    //   OR
    //     ind  = 1
    //     expr > lb // included if lb is finite
    //     expr < ub // included if ub is finite
    int64_t first_djc = MSK::get_num_djc(task.get());
    int64_t first_row = MSK::get_num_row(task.get());


    int64_t
        // Number of added AFE rows
        nrow    = cons.size(),
        // Number of nonzeros in all expressions
        nnz     = 0,
        // Number of rows in total in the DJCs.
        ndjcrow = 0;
    for (const auto& [id, con] : cons) {
        //++nrow;
        nnz += con.expression().ids_size();
        ndjcrow += 2;
        if (std::isfinite(con.lower_bound())) { ++ndjcrow; }
        if (std::isfinite(con.upper_bound())) { ++ndjcrow; }
    }

    std::vector<int32_t> rowlen(nrow*2);
    std::vector<int64_t> rowidx(nrow*3);
    std::vector<int32_t> subj(nnz+nrow);
    std::vector<double>  cof(nnz+nrow);
    std::vector<int64_t> djc_dom(ndjcrow);
    std::vector<double>  djc_rhs(ndjcrow);
    std::vector<int64_t> djc_term_size(nrow*2);
    std::vector<int64_t> djc_num_term(nrow);

    int64_t dom_r, dom_rplus, dom_rminus, dom_rzero;
    if (MSK::RES_OK != MSK::get_domain_r(task.get(),&dom_r) ||
        MSK::RES_OK != MSK::get_domain_rplus(task.get(),&dom_rplus) ||
        MSK::RES_OK != MSK::get_domain_rminus(task.get(),&dom_rminus) ||
        MSK::RES_OK != MSK::get_domain_rzero(task.get(),&dom_rzero))
    {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }

    {
        int64_t djc_i = 0, p = 0, rowi = 0;
        for (const auto& [id, con] : cons) {
            indconstr_map[id] = first_djc+djc_i;
            rowlen[djc_i] = con.expression().ids_size();
            rowlen[nrow+djc_i]  = 1;
            cof[nnz+djc_i]  = 1.0;
            subj[nnz+djc_i] = variable_map[con.indicator_id()];
            {
                int64_t pj = p;
                for (const auto & var_id : con.expression().ids())
                    subj[pj++] = variable_map[var_id];
                for (const auto & c : con.expression().values())
                    cof[p++] = c;
            }

            djc_num_term[djc_i] = 2;

            rowidx[rowi] = first_row+nrow+djc_i;
            djc_rhs[rowi] = 0.0;
            djc_dom[rowi] = dom_rzero;
            ++rowi;

            djc_term_size[djc_i*2] = 1;

            djc_term_size[djc_i*2+1] = 1;
            rowidx[rowi] = first_row+nrow+djc_i;
            djc_rhs[rowi] = 1.0;
            djc_dom[rowi] = dom_rzero;
            ++rowi;

            if (std::isfinite(con.lower_bound())) {
                djc_term_size[djc_i*2+1] += 1;
                rowidx[rowi] = first_row+djc_i;
                djc_rhs[rowi] = con.lower_bound();
                djc_dom[rowi] = dom_rplus;
                ++rowi;
            }

            if (std::isfinite(con.upper_bound())) {
                djc_term_size[djc_i*2+1] += 1;
                rowidx[rowi] = first_row+djc_i;
                djc_rhs[rowi] = con.upper_bound();
                djc_dom[rowi] = dom_rplus;
                ++rowi;
            }

            ++djc_i;
            p += con.expression().ids_size();
        }
    }

    if (MSK::RES_OK != MSK::append_empty_djcs(task.get(),nrow) ||
        MSK::RES_OK != MSK::append_rows(task.get(),nrow*2) ||
        MSK::RES_OK != MSK::put_row_slice(task.get(),first_row,nrow*2,rowlen.data(),subj.data(),cof.data()) ||
        MSK::RES_OK != MSK::put_djc_slice(task.get(),
                                          first_djc, // first
                                          nrow, // num
                                          ndjcrow, // num_rows
                                          nrow*2, // num_domains
                                          djc_term_size.size(),// num_terms
                                          djc_dom.data(), // domains
                                          djc_term_size.data(), // term_size
                                          rowidx.data(), // row_idx
                                          djc_rhs.data(), // offset
                                          djc_num_term.data())) // djc_numterm
    {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }

    {
        int64_t djc_i = 0;
        for (const auto& [id, con] : cons) {
            if (MSK::RES_OK != MSK::put_djc_name(task.get(),first_djc+djc_i,con.name().c_str())) {
                auto [rname,rdesc,msg] = last_error();
                return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
            }
            ++djc_i;
        }
    }

    return absl::OkStatus();
}  // MosekSolver::AddIndicatorConstraints

absl::Status MosekSolver::AddConicConstraints(
    const ::google::protobuf::Map<int64_t, SecondOrderConeConstraintProto>& cons)
{
    // Implementors note:
    //   A list of constraints of the form:
    //   || exprs ||_2 < b
    // Each constraint is implemented as an acc:
    //
    //   (exprb, expr[1], exprs[2],...) in QCone

    int64_t nnz = 0, nrow = 0, ncon = 0;
    for (const auto& [idx, con] : cons) {
        nnz += con.upper_bound().ids_size();
        nrow += 1;
        for (const auto& lexp : con.arguments_to_norm()) {
            nnz += lexp.ids_size();
            nrow += 1;
        }
        ncon += 1;
    }

    std::vector<int32_t>  subj; subj.reserve(nnz);
    std::vector<double>   cof; cof.reserve(nnz);
    std::vector<double>   row_b; row_b.reserve(nrow);
    std::vector<int32_t>  row_len; row_len.reserve(nrow);
    //std::vector<int64_t>  con_num_row; con_num_row.reserve(ncon);
    std::vector<int64_t>  con_dom; con_dom.reserve(ncon);
    std::vector<int64_t>  row_idx; row_idx.reserve(nrow);

    int64_t rowi = 0;

    for (const auto& [idx, con] : cons) {
        auto& expr0 = con.upper_bound();
        row_b[rowi] = expr0.offset();
        for (const auto& id : expr0.ids()) subj.push_back(variable_map[id]);
        for (double c : expr0.coefficients()) cof.push_back(c);
        row_len.push_back(expr0.ids_size());

        for (const auto& expri : con.arguments_to_norm()) {
            row_b.push_back(expri.offset());
            row_len.push_back(expri.ids_size());
            for (const auto& id : expri.ids()) { subj.push_back(variable_map[id]); }
            for (double c : expri.coefficients()) { cof.push_back(c); }
        }

        int64_t consize = 1+con.arguments_to_norm_size();
        //con_num_row.push_back(consize);
        int64_t domidx;
        if (MSK::RES_OK != MSK::get_domain_quadratic_cone(task.get(),consize,&domidx)) {
            auto [rname,rdesc,msg] = last_error();
            return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
        }
    }

    int64_t first_row = MSK::get_num_row(task.get());
    for (int64_t i = 0; i < nrow; ++i) row_idx.push_back(first_row+i);
    if (MSK::RES_OK != MSK::append_rows(task.get(),nrow) ||
        MSK::RES_OK != MSK::put_row_slice(task.get(),first_row,nrow,row_len.data(),subj.data(),cof.data()) ||
        MSK::RES_OK != MSK::append_cons(task.get(), ncon, con_dom.data(), row_idx.size(), row_idx.data(), row_b.data()))
    {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }
    return absl::OkStatus();
}

absl::StatusOr<bool> MosekSolver::Update(const ModelUpdateProto& model_update) {
    for (auto id : model_update.deleted_variable_ids()) {
        int32_t j = variable_map[id];
        variable_map.erase(id);
        int64_t dummyi64 = -1; double dummyf64 = 0.0;
        if (MSK::RES_OK != MSK::put_col(task.get(),j,0,&dummyi64,&dummyf64)) {
            auto [rname,rdesc,msg] = last_error();
            return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
        }
    }

    int64_t dom_empty; MSK::get_domain_empty(task.get(),&dom_empty);
    for (auto id : model_update.deleted_linear_constraint_ids()) {
        auto [con_lb,con_ub,rowi] = linconstr_map[id];
        linconstr_map.erase(id);
        int64_t dummy1{};
        double dummy2{};
        if (MSK::RES_OK != MSK::put_con(task.get(),con_lb,0,dom_empty,&dummy1,&dummy2) ||
            MSK::RES_OK != MSK::put_con(task.get(),con_ub,0,dom_empty,&dummy1,&dummy2)) {
            auto [rname,rdesc,msg] = last_error();
            return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
        }
    }

    for (auto id : model_update.second_order_cone_constraint_updates().deleted_constraint_ids()) {
        int64_t i = coneconstr_map[id];
        coneconstr_map.erase(id);

        int64_t dummy1{};
        double dummy2{};
        if (MSK::RES_OK != MSK::put_con(task.get(),i,0,dom_empty,&dummy1,&dummy2)) {
            auto [rname,rdesc,msg] = last_error();
            return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
        }
    }

    for (auto id : model_update.indicator_constraint_updates().deleted_constraint_ids()) {
        int64_t djci = indconstr_map[id];
        indconstr_map.erase(id);

        int64_t dummy1{};
        double dummy2{};
        if (MSK::RES_OK != MSK::put_djc(task.get(),djci,0,0,0,&dummy1,&dummy1,&dummy1,&dummy2)) {
            auto [rname,rdesc,msg] = last_error();
            return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
        }
    }

    ABSL_RETURN_IF_ERROR(AddVariables(model_update.new_variables()));
    ABSL_RETURN_IF_ERROR(UpdateVariables(model_update.variable_updates()));
    ABSL_RETURN_IF_ERROR(AddConstraints(model_update.new_linear_constraints()));
    ABSL_RETURN_IF_ERROR(
        UpdateConstraints(model_update.linear_constraint_updates(),
                            model_update.linear_constraint_matrix_updates()));

    ABSL_RETURN_IF_ERROR(UpdateObjective(model_update.objective_updates()));

    ABSL_RETURN_IF_ERROR(AddConicConstraints(
        model_update.second_order_cone_constraint_updates().new_constraints()));
    ABSL_RETURN_IF_ERROR(AddIndicatorConstraints(
        model_update.indicator_constraint_updates().new_constraints()));
    return true;
}

absl::Status MosekSolver::UpdateVariables(const VariableUpdatesProto& varupds)
{
    for (int64_t i = 0, n = varupds.lower_bounds().ids_size(); i < n; ++i) {
        int32_t j = variable_map[varupds.lower_bounds().ids(i)];
        double bl,bu;
        if (MSK::RES_OK != MSK::get_var_bound(task.get(),j,&bl,&bu) ||
            MSK::RES_OK != MSK::put_var_bound(task.get(),j,varupds.lower_bounds().values(i),bu))
        {
            auto [rname,rdesc,msg] = last_error();
            return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
        }
    }

    for (int64_t i = 0, n = varupds.upper_bounds().ids_size(); i < n; ++i) {
        int32_t j = variable_map[varupds.upper_bounds().ids(i)];
        double bl,bu;
        if (MSK::RES_OK != MSK::get_var_bound(task.get(),j,&bl,&bu) ||
            MSK::RES_OK != MSK::put_var_bound(task.get(),j,varupds.upper_bounds().values(i),bu))
        {
            auto [rname,rdesc,msg] = last_error();
            return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
        }
    }

    for (int64_t i = 0, n = varupds.integers().ids_size(); i < n; ++i) {
        int32_t j = variable_map[varupds.integers().ids(i)];
        if (MSK::RES_OK != MSK::put_var_type(task.get(),j, varupds.integers().values(i) ? MSK::VariableType::INTEGER : MSK::VariableType::CONTINUOUS)) {
            auto [rname,rdesc,msg] = last_error();
            return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
        }
    }
    return absl::OkStatus();
}


absl::Status MosekSolver::UpdateConstraints(
    const LinearConstraintUpdatesProto& conupds,
    const SparseDoubleMatrixProto& lincofupds)
{
    int64_t dom_r,dom_rplus,dom_rminus;
    if (MSK::RES_OK != MSK::get_domain_r(task.get(),&dom_r) ||
        MSK::RES_OK != MSK::get_domain_rplus(task.get(),&dom_rplus) ||
        MSK::RES_OK != MSK::get_domain_rminus(task.get(),&dom_rminus))
    {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }

    for (int64_t i = 0, n = conupds.lower_bounds().ids_size(); i < n; ++i) {
        auto [con_lo,con_up,rowi] = linconstr_map[conupds.lower_bounds().ids(i)];
        double b = conupds.lower_bounds().values(i);

        if (MSK::RES_OK != MSK::put_con(task.get(),con_lo,1,std::isfinite(b) ? dom_rplus : dom_r,&rowi,&b))
        {
            auto [rname,rdesc,msg] = last_error();
            return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
        }
    }

    for (int64_t i = 0, n = conupds.upper_bounds().ids_size(); i < n; ++i) {
        auto [con_lo,con_up,rowi] = linconstr_map[conupds.upper_bounds().ids(i)];
        double b = conupds.upper_bounds().values(i);

        if (MSK::RES_OK != MSK::put_con(task.get(),con_up,1,std::isfinite(b) ? dom_rminus : dom_r,&rowi,&b))
        {
            auto [rname,rdesc,msg] = last_error();
            return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
        }
    }

    size_t nnz = lincofupds.row_ids_size();
    std::vector<int64_t> subi; subi.reserve(nnz);
    std::vector<int32_t> subj; subj.reserve(nnz);
    std::vector<double> valij(lincofupds.coefficients().begin(),
                              lincofupds.coefficients().end());
    for (auto id : lincofupds.row_ids()) { auto [con_lo,con_up,rowi] = linconstr_map[id]; subi.push_back(rowi); }
    for (auto id : lincofupds.column_ids()) { subj.push_back(variable_map[id]); }

    if (MSK::RES_OK != MSK::put_ijc_list(task.get(),nnz,subi.data(),subj.data(),valij.data()))
    {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }

    return absl::OkStatus();
}


absl::Status MosekSolver::UpdateObjective(const ObjectiveUpdatesProto& objupds)
{
    const auto& vals = objupds.linear_coefficients();
    std::vector<double> cof(vals.values().begin(), vals.values().end());
    std::vector<int32_t> subj; subj.reserve(cof.size());
    std::vector<int64_t> subi(cof.size(),0);

    for (auto id : objupds.linear_coefficients().ids())
        subj.push_back(variable_map[id]);
    if (MSK::RES_OK != MSK::put_ijc_list(task.get(),cof.size(),subi.data(),subj.data(),cof.data()))
    {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }

    MSK::put_obj_sense(task.get(),objupds.direction_update() ? MSK::ObjSense::MAXIMIZE : MSK::ObjSense::MINIMIZE);

    return absl::OkStatus();
}


absl::Status MosekSolver::UpdateConstraint(const SecondOrderConeConstraintUpdatesProto& conupds)
{
    int64_t dom_empty; MSK::get_domain_empty(task.get(),&dom_empty);
    for (auto id : conupds.deleted_constraint_ids()) {
        int64_t coni = coneconstr_map[id];
        coneconstr_map.erase(id);

        int64_t dummy1{};
        double dummy2{};
        if (MSK::RES_OK != MSK::put_con(task.get(),coni,0,dom_empty,&dummy1,&dummy2))
        {
            auto [rname,rdesc,msg] = last_error();
            return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
        }
    }

    ABSL_RETURN_IF_ERROR(AddConicConstraints(conupds.new_constraints()));

    return absl::OkStatus();
}


absl::Status MosekSolver::UpdateConstraint(const IndicatorConstraintUpdatesProto& conupds)
{
    for (auto id : conupds.deleted_constraint_ids()) {
        int64_t djci = indconstr_map[id];
        indconstr_map.erase(id);

        int64_t dummy1{}; double dummy2{};
        if (MSK::RES_OK != MSK::put_djc(task.get(),djci,0,0,0,&dummy1,&dummy1,&dummy1,&dummy2)) {
            auto [rname,rdesc,msg] = last_error();
            return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
        }
    }

    ABSL_RETURN_IF_ERROR(AddIndicatorConstraints(conupds.new_constraints()));

    return absl::OkStatus();
}


static void delete_task(MSK::Task_t task) { MSK::delete_task(task); }

absl::StatusOr<std::unique_ptr<SolverInterface>> MosekSolver::New(const ModelProto& model, const InitArgs&)
{
    if (0 != MSK::initialize_library()) {
        return absl::InvalidArgumentError("Mosek is not correctly installed.");
    }
    ABSL_RETURN_IF_ERROR(ModelIsSupported(model, kMosekSupportedStructures, "Mosek"));

    if (!model.auxiliary_objectives().empty())
        return ortools::InvalidArgumentErrorBuilder()
            << "Mosek does not support multi-objective models";
    if (!model.objective().quadratic_coefficients().row_ids().empty()) {
        return ortools::InvalidArgumentErrorBuilder()
            << "Mosek does not support models with quadratic objectives";
    }
    if (!model.quadratic_constraints().empty()) {
        return ortools::InvalidArgumentErrorBuilder()
            << "Mosek does not support models with quadratic constraints";
    }
    if (!model.sos1_constraints().empty() || !model.sos2_constraints().empty()) {
        return ortools::InvalidArgumentErrorBuilder()
            << "Mosek does not support models with SOS constraints";
    }

    MSK::Task_t task_ptr = MSK::new_task();
    if (!task_ptr)
        return absl::InternalError("Failed to create internal MOSEK task");
    std::unique_ptr<MSK::Task_s,Task_deleter> task(task_ptr,&delete_task);
    std::unique_ptr<MosekSolver> mskslv(new MosekSolver(std::move(task)));
    MSK::put_task_name(mskslv->task.get(),model.name().c_str());
    MSK::append_rows(mskslv->task.get(),1);
    MSK::put_obj_row(mskslv->task.get(),0);
    ABSL_RETURN_IF_ERROR(mskslv->AddVariables(model.variables()));
    ABSL_RETURN_IF_ERROR(mskslv->ReplaceObjective(model.objective()));
    ABSL_RETURN_IF_ERROR(mskslv->AddConstraints(model.linear_constraints(),
                                            model.linear_constraint_matrix()));
    ABSL_RETURN_IF_ERROR(mskslv->AddIndicatorConstraints(model.indicator_constraints()));

    //absl::StatusOr<std::unique_ptr<SolverInterface>>
    std::unique_ptr<SolverInterface> res(std::move(mskslv));

    return res;
}

MosekSolver::MosekSolver(std::unique_ptr<MSK::Task_s,Task_deleter> && t) : task(std::move(t)) { }

absl::StatusOr<PrimalSolutionProto> MosekSolver::PrimalSolution(
    int sol_index,
    const std::vector<int64_t>& ordered_var_ids,
    bool skip_zero_values)
{
    MSK::SolSta psta,dsta;
    if (MSK::RES_OK != MSK::get_sol_status(task.get(),sol_index,&psta,&dsta))
    {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }

    PrimalSolutionProto sol;
    switch (psta) {
        case MSK::SolSta::OPTIMAL:
        case MSK::SolSta::INTEGER_OPTIMAL:
        case MSK::SolSta::FEASIBLE:
            sol.set_feasibility_status(SolutionStatusProto::SOLUTION_STATUS_FEASIBLE);
            {
                double pobj;
                int32_t numvar = MSK::get_num_var(task.get());
                std::vector<double> xx(numvar);
                if (MSK::RES_OK != MSK::get_primal_obj(task.get(),sol_index,&pobj) ||
                    MSK::RES_OK != MSK::get_sol_xx_slice(task.get(),sol_index,0,numvar,xx.data()))
                {
                    auto [rname,rdesc,msg] = last_error();
                    return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
                }

                sol.set_objective_value(pobj);
                SparseDoubleVectorProto vals;

                for (auto k : ordered_var_ids) {
                    auto v = xx[variable_map[k]];
                    if (!skip_zero_values || v < 0.0 || v > 0.0) {
                        vals.add_ids(k);
                        vals.add_values(v);
                    }
                }


                //std::cerr << __FILE__ << ":" << __LINE__ << ": MosekSolver::PrimalSolution pobj : " << pobj << std::endl;
                // if (numvar > 0) {
                //     for (auto x : xx)
                //         std::cerr << __FILE__ << ":" << __LINE__ << ": MosekSolver::PrimalSolution xx : " << x << std::endl;
                // }

                *sol.mutable_variable_values() = std::move(vals);
            }
            break;
        default:
            return absl::NotFoundError("Primal solution not available");
    }
    return sol;
}


absl::StatusOr<DualSolutionProto> MosekSolver::DualSolution(
    int32_t sol_index,
    const std::vector<int64_t>& ordered_y_ids,
    bool skip_y_zeros,
    const std::vector<int64_t>& ordered_yx_ids,
    bool skip_yx_zeros)
{
    MSK::SolSta psta,dsta;
    DualSolutionProto sol;

    if (MSK::RES_OK != MSK::get_sol_status(task.get(),0,&psta,&dsta)) {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }


    switch (dsta) {
        case MSK::SolSta::OPTIMAL:
        case MSK::SolSta::FEASIBLE:
            {
                //std::cerr << __FILE__ << ":" << __LINE__ << ": DualSolution : OPTIMAL/FEASIBLE" << std::endl;
                double dobj;
                int32_t numvar = MSK::get_num_var(task.get());
                int64_t numcon = MSK::get_num_con(task.get());
                int64_t numconrow;
                std::vector<double> slx(numvar);
                std::vector<double> sux(numvar);

                if (MSK::RES_OK != MSK::get_con_slice_num_row(task.get(),0,numcon,&numconrow) ||
                    MSK::RES_OK != MSK::get_dual_obj(task.get(),sol_index,&dobj) ||
                    MSK::RES_OK != MSK::get_sol_slx_slice(task.get(),sol_index,0,numvar,slx.data()) ||
                    MSK::RES_OK != MSK::get_sol_sux_slice(task.get(),sol_index,0,numvar,sux.data()))
                {
                    //std::cerr << __FILE__ << ":" << __LINE__ << ": DualSolution : failed" << std::endl;
                    auto [rname,rdesc,msg] = last_error();
                    return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
                }

                sol.set_objective_value(dobj);
                sol.set_feasibility_status(SolutionStatusProto::SOLUTION_STATUS_FEASIBLE);
                {
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
                    SparseDoubleVectorProto vals;
                    double slc = -999,suc = -999;
                    for (auto k : ordered_y_ids) {
                        auto [con_lo,con_up,rowi] = linconstr_map[k];
                        //std::cerr << __FILE__ << ":" << __LINE__ << ": DualSolution : " << con_lo << "/" << con_up << "/" << rowi << std::endl;
                        if (MSK::RES_OK != MSK::get_sol_y_slice(task.get(),sol_index,con_lo,1,1,&slc) ||
                            MSK::RES_OK != MSK::get_sol_y_slice(task.get(),sol_index,con_up,1,1,&suc))
                        {
                            auto [rname,rdesc,msg] = last_error();
                            return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
                        }
                        //std::cerr << __FILE__ << ":" << __LINE__ << ": DualSolution : " << slc << "/" << suc << std::endl;
                        double v = slc+suc;
                        if (!skip_y_zeros || v < 0.0 || v > 0.0) {
                            vals.add_ids(k);
                            vals.add_values(v);
                        }
                    }

                    *sol.mutable_dual_values() = std::move(vals);
                }
            }
            break;
        default:
            //std::cerr << __FILE__ << ":" << __LINE__ << ": DualSolution : failed" << std::endl;
            return absl::NotFoundError("Dual solution not available");
    }
    return sol;
}


absl::StatusOr<SolutionProto> MosekSolver::Solution(
    int32_t sol_index,
    const std::vector<int64_t>& ordered_xc_ids,
    const std::vector<int64_t>& ordered_xx_ids,
    bool skip_xx_zeros,
    const std::vector<int64_t>& ordered_y_ids,
    bool skip_y_zeros,
    const std::vector<int64_t>& ordered_yx_ids,
    bool skip_yx_zeros)
{
    // std::cout << "MosekSolver::Solution()" << std::endl;
    SolutionProto sol;
    {
        auto r = PrimalSolution(sol_index, ordered_xx_ids, skip_xx_zeros);
        //std::cerr << __FILE__ << ":" << __LINE__ << ": Solution/Primal : " << r.ok() << std::endl;
        if (r.ok()) *sol.mutable_primal_solution() = std::move(*r);
    }
    {
        auto r = DualSolution(sol_index, ordered_y_ids, skip_y_zeros, ordered_yx_ids,
                            skip_yx_zeros);
        //std::cerr << __FILE__ << ":" << __LINE__ << ": Solution/Dual : " << r.ok() << std::endl;
        if (r.ok()) {
            *sol.mutable_dual_solution() = std::move(*r);
        }
    }

    MSK::SolType soltp;
    if (MSK::RES_OK != MSK::get_sol_type(task.get(),sol_index,&soltp))
    {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }

    if (soltp == MSK::SolType::BASIC) {
        BasisProto bas;
        SparseBasisStatusVector csta;
        SparseBasisStatusVector xsta;

        int32_t numvar = MSK::get_num_var(task.get());

        std::vector<int32_t> x_basic(numvar);
        std::vector<int32_t> xlo_binding(numvar);
        std::vector<int32_t> xup_binding(numvar);
        MSK::SolSta psta,dsta;

        if (MSK::RES_OK != MSK::get_sol_sta_var_slice(task.get(),sol_index,0,numvar,xlo_binding.data(),xup_binding.data()) ||
            MSK::RES_OK != MSK::get_sol_basic_x_slice(task.get(),sol_index,0,numvar,x_basic.data()) ||
            MSK::RES_OK != MSK::get_sol_status(task.get(),sol_index,&psta,&dsta))
        {
            auto [rname,rdesc,msg] = last_error();
            return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
        }

        for (auto k : ordered_xx_ids) {
            auto j = variable_map[k];
            xsta.add_ids(k);

            if      (x_basic[j])     xsta.add_values(BasisStatusProto::BASIS_STATUS_BASIC);
            else if (xlo_binding[j]) {
                if (xup_binding[j])  xsta.add_values(BasisStatusProto::BASIS_STATUS_FIXED_VALUE);
                else                 xsta.add_values(BasisStatusProto::BASIS_STATUS_AT_LOWER_BOUND);
            }
            else if (xup_binding[j]) xsta.add_values(BasisStatusProto::BASIS_STATUS_AT_UPPER_BOUND);
            else                     xsta.add_values(BasisStatusProto::BASIS_STATUS_UNSPECIFIED);
        }

        for (auto k : ordered_xc_ids) {
            auto [con_lo,con_up,rowi] = linconstr_map[k];
            csta.add_ids(k);
            int32_t l_basic{},u_basic{},l_binding{},u_binding{};

            if (MSK::RES_OK != MSK::get_sol_sta_con(task.get(),sol_index,con_lo,&l_binding) ||
                MSK::RES_OK != MSK::get_sol_sta_con(task.get(),sol_index,con_up,&u_binding) ||
                MSK::RES_OK != MSK::get_sol_basic_con(task.get(),sol_index,con_lo,&l_basic) ||
                MSK::RES_OK != MSK::get_sol_basic_con(task.get(),sol_index,con_lo,&l_basic))
            {
                auto [rname,rdesc,msg] = last_error();
                return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
            }

            // TODO: Check if basic is interpreted correctly!
            if      (l_basic || u_basic) csta.add_values(BasisStatusProto::BASIS_STATUS_BASIC);
            else if (l_binding) {
                if (u_binding)           csta.add_values(BasisStatusProto::BASIS_STATUS_FIXED_VALUE);
                else                     csta.add_values(BasisStatusProto::BASIS_STATUS_AT_LOWER_BOUND);
            }
            else if (u_binding)          csta.add_values(BasisStatusProto::BASIS_STATUS_AT_UPPER_BOUND);
            else                         csta.add_values(BasisStatusProto::BASIS_STATUS_UNSPECIFIED);
        }
        *bas.mutable_variable_status()   = std::move(xsta);
        *bas.mutable_constraint_status() = std::move(csta);


        if ((psta == MSK::SolSta::OPTIMAL ||
             psta == MSK::SolSta::FEASIBLE) &&
            (dsta == MSK::SolSta::OPTIMAL ||
             dsta == MSK::SolSta::FEASIBLE))
            bas.set_basic_dual_feasibility(SolutionStatusProto::SOLUTION_STATUS_FEASIBLE);
        else
            bas.set_basic_dual_feasibility(SolutionStatusProto::SOLUTION_STATUS_UNSPECIFIED);

        *sol.mutable_basis() = std::move(bas);
    }
    return sol;
}

absl::StatusOr<PrimalRayProto> MosekSolver::PrimalRay(
    int32_t sol_index, const std::vector<int64_t>& ordered_xx_ids,
    bool skip_xx_zeros)
{
    MSK::SolSta psta,dsta;
    int32_t numvar = MSK::get_num_var(task.get());

    if (MSK::RES_OK != MSK::get_sol_status(task.get(),sol_index,&psta,&dsta))
    {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }

    if (psta != MSK::SolSta::INFEAS_CERT)
        return absl::NotFoundError("Certificate not available");

    std::vector<double> xx(numvar);
    if (MSK::RES_OK != MSK::get_sol_xx_slice(task.get(),sol_index,0,numvar,xx.data()))
    {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }
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
    int32_t sol_index,
    const std::vector<int64_t>& ordered_y_ids,
    bool skip_y_zeros,
    const std::vector<int64_t>& ordered_yx_ids,
    bool skip_yx_zeros)
{
    MSK::SolSta psta,dsta;
    int32_t numvar = MSK::get_num_var(task.get());

    if (MSK::RES_OK != MSK::get_sol_status(task.get(),sol_index,&psta,&dsta))
    {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }

    if (psta != MSK::SolSta::INFEAS_CERT)
        return absl::NotFoundError("Certificate not available");

    std::vector<double> slx(numvar), sux(numvar);
    if (MSK::RES_OK != MSK::get_sol_slx_slice(task.get(),sol_index,0,numvar,slx.data()) ||
        MSK::RES_OK != MSK::get_sol_sux_slice(task.get(),sol_index,0,numvar,sux.data()))
    {
        auto [rname,rdesc,msg] = last_error();
        return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
    }

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
        auto [con_lo,con_up,rowi] = linconstr_map[k];
        double sl,su;
        if (MSK::RES_OK != MSK::get_sol_y_slice(task.get(),sol_index,con_lo,1,1,&sl) ||
            MSK::RES_OK != MSK::get_sol_y_slice(task.get(),sol_index,con_up,1,1,&su))
        {
            auto [rname,rdesc,msg] = last_error();
            return absl::InternalError((std::stringstream() << rname << ": " << msg).str());
        }
        double v = sl-su;

        if (!skip_y_zeros || v < 0 || v > 0) {
            cdata.add_ids(k);
            cdata.add_values(v);
            }
    }
    *ray.mutable_dual_values()   = cdata;
    *ray.mutable_reduced_costs() = xdata;
    return ray;
}

// static void stream_cb(void * h, const char * msg) {
//     BufferedMessageCallback *bmsg_cb = (BufferedMessageCallback *) h;
//     bmsg_cb->OnMessage(msg);
// }


static void intsolution_cb(MSK::CallbackHandle h, int32_t len, double pobj, const double * xx) {
    auto [cb,ordered_xx_ids,variable_map,skip_xx_zeros,terminate] = *((std::tuple<MosekSolver::Callback&,std::vector<int64_t>&,absl::flat_hash_map<int64_t, int32_t>&,bool,bool&>*) h);
    CallbackDataProto cbdata;

    cbdata.set_event(CALLBACK_EVENT_MIP_SOLUTION);
    SparseDoubleVectorProto primal;

    for (auto id : ordered_xx_ids) {
        auto v = xx[variable_map[id]];
        if (!skip_xx_zeros || v > 0.0 || v < 0.0) {
            primal.add_ids(id);
            primal.add_values(v);
        }
    }
    *cbdata.mutable_primal_solution_vector() = primal;

    auto r = cb(cbdata);
    if (r.ok()) {
        terminate = r->terminate();
    }
}
static int info_cb(
    MSK::CallbackHandle h,
    int code,
    int32_t num_iinf,
    const int32_t * iinf,
    int32_t num_liinf,
    const int64_t * liinf,
    int32_t num_dinf,
    const double * dinf)
{
    auto [cb,terminate] = *((std::tuple<MosekSolver::Callback&,bool&>*) h);
    if (terminate) return 1;

    std::string_view codestr = MSK::get_callback_code_name(code);

    CallbackDataProto cbdata;
    bool skip = false;
    int paridx;
    if (0 == codestr.compare("callback_im_simplex")) {
        if (0 <= (paridx = MSK::get_liinf_index("liinf_simplex_iter")))
            cbdata.mutable_simplex_stats()->set_iteration_count(liinf[paridx]);
        if (0 <= (paridx = MSK::get_dinf_index("dinf_sim_obj")))
            cbdata.mutable_simplex_stats()->set_objective_value(dinf[paridx]);
        // cbdata.mutable_simplex_stats(/->set_primal_infeasibility(...);
        // cbdata.mutable_simplex_stats()->set_dual_infeasibility(...);
        // cbdata.mutable_simplex_stats()->is_perturbed(...);
        cbdata.set_event(CALLBACK_EVENT_SIMPLEX);
    }
    else if (0 == codestr.compare("callback_im_mio")) {
        if (0 <= (paridx = MSK::get_dinf_index("dinf_mio_obj_bound")))
            cbdata.mutable_mip_stats()->set_primal_bound(dinf[paridx]);
        // cbdata.mutable_mip_stats()->set_dual_bound(...);
        if (0 <= (paridx = MSK::get_iinf_index("iinf_mio_num_solved_nodes")))
            cbdata.mutable_mip_stats()->set_explored_nodes(iinf[paridx]);
        // cbdata.mutable_mip_stats()->set_open_nodes(...);
        if (0 <= (paridx = MSK::get_liinf_index("liinf_mio_simplex_iter")))
            cbdata.mutable_mip_stats()->set_simplex_iterations(liinf[paridx]);
        // cbdata.mutable_mip_stats()->set_number_of_solutions_found(...);
        // cbdata.mutable_mip_stats()->set_cutting_planes_in_lp(...);
        cbdata.set_event(CALLBACK_EVENT_MIP);
    }
    else if (0 == codestr.compare("callback_new_int_mio")) {
        // this is tricky. It means that we can't break on receiving a solution
        skip = true;
    }
    //else if (0 == codestr.compare("callback_im_presolve") {
    //    cbdata.set_event(CALLBACK_EVENT_PRESOLVE);
    //}
    //else if (0 == codestr.compare("callback_im_conic") {}
    //else if (0 == codestr.compare("callback_im_intpnt") {
    //    cbdata.mutable_barrier_stats()->set_iteration_count(
    //        liinf[MSK_IINF_INTPNT_ITER]);
    //    cbdata.mutable_barrier_stats()->set_primal_objective(
    //        dinf[MSK_DINF_INTPNT_PRIMAL_OBJ]);
    //    cbdata.mutable_barrier_stats()->set_dual_objective(
    //        dinf[MSK_DINF_INTPNT_DUAL_OBJ]);
    //  cbdata.mutable_barrier_stats()->set_complementarity(...);
    //  cbdata.mutable_barrier_stats()->set_primal_infeasibility(...);
    //  cbdata.mutable_barrier_stats()->set_dual_infeasibility(...);
    //  cbdata.set_event(CALLBACK_EVENT_BARRIER);
    //}
    else {
        cbdata.set_event(CALLBACK_EVENT_UNSPECIFIED);
    }

    if (! skip) {
        auto r = cb(cbdata);
        if (r.ok()) {
            return r->terminate();
        }
    }
    return 0;
}

// static void msgprn(void * h, const char * msg) {
//     fputs(msg, stderr);
// }

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

    // Stash all parameters to be restored after optimization
    int ok;
    double dpar_optimizer_max_time;    MSK::get_double_param(task.get(),"dpar_optimizer_max_time",&dpar_optimizer_max_time);
    int    ipar_intpnt_max_iterations; MSK::get_int_param(task.get(),"ipar_intpnt_max_iterations",&ipar_intpnt_max_iterations);
    int    ipar_sim_max_iterations;    MSK::get_int_param(task.get(),"ipar_sim_max_iterations",&ipar_sim_max_iterations);
    double dpar_upper_obj_cut;         MSK::get_double_param(task.get(),"dpar_upper_obj_cut",&dpar_upper_obj_cut);
    double dpar_lower_obj_cut;         MSK::get_double_param(task.get(),"dpar_lower_obj_cut",&dpar_lower_obj_cut);
    int    ipar_num_threads;           MSK::get_int_param(task.get(),"ipar_num_threads",&ipar_num_threads);
    double dpar_mio_tol_abs_gap;       MSK::get_double_param(task.get(),"dpar_mio_tol_abs_gap",&dpar_mio_tol_abs_gap);
    double dpar_mio_tol_rel_gap;       MSK::get_double_param(task.get(),"dpar_mio_tol_rel_gap",&dpar_mio_tol_rel_gap);
    double dpar_intpnt_tol_rel_gap;    MSK::get_double_param(task.get(),"dpar_intpnt_tol_rel_gap",&dpar_intpnt_tol_rel_gap);
    double dpar_intpnt_co_tol_rel_gap; MSK::get_double_param(task.get(),"dpar_intpnt_co_tol_rel_gap",&dpar_intpnt_co_tol_rel_gap);
    int    ipar_optimizer;             MSK::get_int_param(task.get(),"ipar_optimizer",&ipar_optimizer);

    auto _guard_reset_params = absl::MakeCleanup([&]() {
        MSK::put_double_param(task.get(),"dpar_optimizer_max_time", dpar_optimizer_max_time,&ok);
        MSK::put_int_param(task.get(),"ipar_intpnt_max_iterations", ipar_intpnt_max_iterations,&ok);
        MSK::put_int_param(task.get(),"ipar_sim_max_iterations", ipar_sim_max_iterations,&ok);
        MSK::put_double_param(task.get(),"dpar_upper_obj_cut", dpar_upper_obj_cut,&ok);
        MSK::put_double_param(task.get(),"dpar_lower_obj_cut", dpar_lower_obj_cut,&ok);
        MSK::put_int_param(task.get(),"ipar_num_threads", ipar_num_threads,&ok);
        MSK::put_double_param(task.get(),"dpar_mio_tol_abs_gap", dpar_mio_tol_abs_gap,&ok);
        MSK::put_double_param(task.get(),"dpar_mio_tol_rel_gap", dpar_mio_tol_rel_gap,&ok);
        MSK::put_double_param(task.get(),"dpar_intpnt_tol_rel_gap", dpar_intpnt_tol_rel_gap,&ok);
        MSK::put_int_param(task.get(),"dpar_intpnt_co_tol_rel_gap", dpar_intpnt_co_tol_rel_gap,&ok);
    });

    //std::cerr << __FILE__ << ":" << __LINE__ << ": MSK::optimize " << std::endl;

    if (parameters.has_time_limit()) {
        OR_ASSIGN_OR_RETURN3(
            const absl::Duration time_limit,
            util_time::DecodeGoogleApiProto(parameters.time_limit()),
            _ << "invalid time_limit value for HiGHS.");
        MSK::put_double_param(task.get(),"dpar_optimizer_max_time",
                    absl::ToDoubleSeconds(time_limit),&ok);
    }

    if (parameters.has_iteration_limit()) {
        const int iter_limit = parameters.iteration_limit();

        MSK::put_int_param(task.get(),"ipar_intpnt_max_iterations", iter_limit,&ok);
        MSK::put_int_param(task.get(),"ipar_sim_max_iterations", iter_limit,&ok);
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
        if (MSK::get_obj_sense(task.get()) == MSK::ObjSense::MAXIMIZE)
            MSK::put_double_param(task.get(),"dpar_upper_obj_cut", parameters.cutoff_limit(),&ok);
        else
            MSK::put_double_param(task.get(),"dpar_lower_obj_cut", parameters.cutoff_limit(),&ok);
    }

    if (parameters.has_threads()) {
        MSK::put_int_param(task.get(),"ipar_num_threads", parameters.threads(),&ok);
    }

    if (parameters.has_absolute_gap_tolerance()) {
        MSK::put_double_param(task.get(),"dpar_mio_tol_abs_gap", parameters.absolute_gap_tolerance(),&ok);
    }

    if (parameters.has_relative_gap_tolerance()) {
        MSK::put_double_param(task.get(),"dpar_intpnt_tol_rel_gap",
                    parameters.absolute_gap_tolerance(),&ok);
        MSK::put_double_param(task.get(),"dpar_intpnt_co_tol_rel_gap",
                    parameters.absolute_gap_tolerance(),&ok);
        MSK::put_double_param(task.get(),"dpar_mio_tol_rel_gap", parameters.absolute_gap_tolerance(),&ok);
    }

    switch (parameters.lp_algorithm()) {
        case LP_ALGORITHM_BARRIER:
            MSK::put_param_str(task.get(),"ipar_optimizer", "optimizer_intpnt",&ok);
            break;
        case LP_ALGORITHM_DUAL_SIMPLEX:
            MSK::put_param_str(task.get(),"ipar_optimizer", "optimizer_dual_simplex",&ok);
            break;
        case LP_ALGORITHM_PRIMAL_SIMPLEX:
            MSK::put_param_str(task.get(),"ipar_optimizer", "optimizer_primal_simplex",&ok);
            break;
        default:
            // use default auto select, usually intpnt
            MSK::put_param_str(task.get(),"ipar_optimizer", "optimizer_free", &ok);
        break;
    }

    // TODO: parameter enable_output

    bool skip_xx_zeros =
        model_parameters.variable_values_filter().skip_zero_values();
    bool skip_y_zeros = model_parameters.dual_values_filter().skip_zero_values();
    bool skip_yx_zeros =
        model_parameters.reduced_costs_filter().skip_zero_values();
    //bool filter_ids = model_parameters.variable_values_filter().filter_by_ids();

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

    MSK::TrmCode trm{};
    {
        BufferedMessageCallback bmsg_cb(message_cb);
        // TODO: Use model_parameters

        //MSK::put_stream_callback(task.get(),MSK::StreamType::LOG,&bmsg_cb,stream_cb);
        //auto _guard_reset_stream_cb = absl::MakeCleanup([&]() {
        //    MSK::put_stream_callback(task.get(),MSK::StreamType::LOG,nullptr,nullptr);
        //});


        // Expected: is OK and has a value that ((is an object whose field `termination` (is an object whose field
        // `reason` is equal to optimal) and (is an object whose field `problem_status` (is an object whose field
        // `primal_status` is equal to feasible) and (is an object whose field `dual_status` is equal to feasible) and
        // (is an object whose field `primal_or_dual_infeasible` is equal to false))) and (is an object whose property
        // `has_primal_feasible_solution` is equal to true) and (is an object whose property `objective_value` is
        // approximately 1 (absolute error <= 1.0000000000000001e-05))) and (is an object whose field `solutions`
        // contains at least one element that is an object whose field `primal_solution` value (is an object whose field
        // `variable_values` has identical keys to {{__var#0__, 1}} and values within 1e-05) and (is an object whose
        // field `objective_value` is approximately 1 (absolute error <= 1.0000000000000001e-05)) and (is an object
        // whose field `feasibility_status` is equal to feasible))
        //
        // Actual: {
        //     termination: {
        //         reason: optimal,
        //         problem_status: {
        //             primal_status: feasible,
        //             dual_status: feasible,
        //             primal_or_dual_infeasible: false
        //         },
        //         objective_bounds: {primal_bound: 1, dual_bound: 0}
        //     },
        //     solve_stats: {
        //         solve_time: 0,
        //         simplex_iterations: 0,
        //         barrier_iterations: 0,
        //         first_order_iterations: 0,
        //         node_count: 0 },
        //     solutions: [1 available],
        //     primal_rays: [],
        //     dual_rays: []
        // },
        // whose property `objective_value` is 0 (of type double), which is -1 from 1

        //MSK::put_stream_callback(task.get(),MSK::StreamType::LOG,nullptr,msgprn);
        if (cb) {
            //std::cerr << __FILE__ << ":" << __LINE__ << ": MSK::optimize_callback()" << std::endl;
            bool terminate;
            std::tuple<Callback&,std::vector<int64_t>&,absl::flat_hash_map<int64_t, int32_t>&,bool,bool&> handle(cb,ordered_xx_ids,variable_map,skip_xx_zeros,terminate);
            std::tuple<Callback&,bool&> cbh(cb,terminate);
            if (MSK::RES_OK != MSK::optimize_callback(task.get(),&trm,&cbh,&info_cb,&handle,&intsolution_cb)) {
                return absl::InternalError("Failed to extract solution data");
            }
        }
        else {
            //std::cerr << __FILE__ << ":" << __LINE__ << ": MSK::optimize_callback()" << std::endl;
            auto r = MSK::optimize(task.get(),&trm);
            //std::cerr << __FILE__ << ":" << __LINE__ << ": MSK::optimize -> " << r << ", trm = " << trm << std::endl;
            if (MSK::RES_OK != r) {
                return absl::InternalError("Failed to extract solution data");
            }
        }

#if 1
        for (int i = 0; ; ++i) {
            std::ostringstream s;  s  << "outfile_" << i << ".jtask";
            std::ostringstream s2; s2 << "outfile_" << i << ".ptf";
            FILE * f = fopen(s.str().c_str(),"rb");
            if (f) { fclose(f); }
            else {
                //std::cerr << __FILE__ << ":" << __LINE__ << ": MosekSolver::PrimalSolution -> " << s.str() << std::endl;
                MSK::write_task_to_file(task.get(),s.str().c_str());
                MSK::write_task_to_file(task.get(),s2.str().c_str());
                break;
            }
        }
#endif
    }
    //std::cerr << __FILE__ << ":" << __LINE__ << ": MSK::optimize" << std::endl;

    int sol_index = -1;
    MSK::SolType sol_type;
    int numsol = MSK::get_num_sol(task.get());
    for (int soli = 0; soli < numsol; ++soli) {
        MSK::SolType st;
        if (MSK::RES_OK == MSK::get_sol_type(task.get(),soli,&st)) {
            if (sol_index < 0 ||
                st == MSK::SolType::INTEGER ||
                (st == MSK::SolType::BASIC && sol_type != MSK::SolType::INTEGER) ||
                (st == MSK::SolType::INTERIOR && sol_type != MSK::SolType::INTEGER && sol_type != MSK::SolType::BASIC))
            {
                sol_index = soli;
                sol_type = st;
            }
        }
    }

    bool soldef = sol_index >= 0;

    TerminationProto trmp{};
    MSK::ProSta prosta{};
    MSK::SolSta psolsta{},dsolsta{};
    MSK::ResCode r;
    bool ismax = MSK::get_obj_sense(task.get()) == MSK::ObjSense::MAXIMIZE;
    if (!soldef) {
        //std::cerr << __FILE__ << ":" << __LINE__ << ": No solutions " << std::endl;
        //auto [msg, name, code] = last_error();
        trmp = TerminateForReason(
            ismax,
            TerminationReasonProto::TERMINATION_REASON_NO_SOLUTION_FOUND, "No solution found");
        trmp.set_limit(LimitProto::LIMIT_UNSPECIFIED);
    }
    else if (MSK::RES_OK != (r = MSK::get_problem_status(task.get(),sol_index,&prosta)) ||
             MSK::RES_OK != (r = MSK::get_sol_status(task.get(),sol_index,&psolsta,&dsolsta)))
    {
        //std::cerr << __FILE__ << ":" << __LINE__ << ": Failed get startus : " << r << std::endl;
        return absl::InternalError("Failed to extract solution data");
    }
    else {
        //std::cerr << __FILE__ << ":" << __LINE__ << ": MSK::optimize" << std::endl;
        // Attempt to determine TerminationProto from Mosek Termination code,
        // problem status and solution status.

        bool ismax = MSK::get_obj_sense(task.get()) == MSK::ObjSense::MAXIMIZE;
        if (psolsta == MSK::SolSta::INTEGER_OPTIMAL) {
            double pobj;
            if (MSK::RES_OK != (r = MSK::get_primal_obj(task.get(),sol_index,&pobj))) {
                return absl::InternalError("Failed to extract solution data");
            }
            //std::cerr << __FILE__ << ":" << __LINE__ << ": INTEGER_OPTIMAL pobj = "  << pobj << std::endl;
            trmp = OptimalTerminationProto(pobj,pobj,"");
            //trmp.set_limit(LimitProto::LIMIT_SOLUTION);
        }
        else if (psolsta == MSK::SolSta::OPTIMAL &&
                 dsolsta == MSK::SolSta::OPTIMAL) {
            double pobj,dobj;
            if (MSK::RES_OK != (r = MSK::get_primal_obj(task.get(),sol_index,&pobj)) ||
                MSK::RES_OK != (r = MSK::get_dual_obj(task.get(),sol_index,&dobj))) {
                    //std::cerr << __FILE__ << ":" << __LINE__ << ": r = " << r << std::endl;
                    return absl::InternalError("Optimization failedcode");
                }
            //std::cerr << __FILE__ << ":" << __LINE__ << ": OPTIMAL objective " << pobj << "/" << dobj << std::endl;
            trmp = OptimalTerminationProto(pobj,dobj, "");
        }
        else if (dsolsta == MSK::SolSta::INFEAS_CERT) {
            //std::cerr << __FILE__ << ":" << __LINE__ << ": !! " << std::endl;

            trmp = InfeasibleTerminationProto(
                ismax,
                FeasibilityStatusProto::FEASIBILITY_STATUS_FEASIBLE);
        }
        else if (prosta == MSK::ProSta::PRIMAL_INFEASIBLE_OR_UNBOUNDED) {
            //std::cerr << __FILE__ << ":" << __LINE__ << ": !! " << std::endl;
            trmp = InfeasibleOrUnboundedTerminationProto(ismax);
        }
        else if (psolsta == MSK::SolSta::INFEAS_CERT) {
            //std::cerr << __FILE__ << ":" << __LINE__ << ": !! " << std::endl;
            trmp = UnboundedTerminationProto(ismax);
        }
        else if (psolsta == MSK::SolSta::FEASIBLE) {
            //std::cerr << __FILE__ << ":" << __LINE__ << ": !! " << std::endl;
            LimitProto lim = LimitProto::LIMIT_UNSPECIFIED;
            std::string_view trmname(MSK::get_trm_name(trm));
            if      (0 == trmname.compare("res_trm_max_iterations"))
                lim = LimitProto::LIMIT_ITERATION;
            else if (0 == trmname.compare("res_trm_max_time"))
                lim = LimitProto::LIMIT_TIME;
            else if (0 == trmname.compare("res_trm_num_max_num_int_solutions"))
                lim = LimitProto::LIMIT_SOLUTION;
            else if (0 == trmname.compare("res_trm_server_max_memory"))
                lim = LimitProto::LIMIT_MEMORY;
            // LIMIT_CUTOFF
            else if (0 == trmname.compare("res_trm_objective_range"))
                lim = LimitProto::LIMIT_OBJECTIVE;
            else if (0 == trmname.compare("res_trm_numerical_problem"))
                lim = LimitProto::LIMIT_NORM;
            else if (0 == trmname.compare("res_trm_user_callback"))
                lim = LimitProto::LIMIT_INTERRUPTED;
            else if (0 == trmname.compare("res_trm_stall"))
                lim = LimitProto::LIMIT_SLOW_PROGRESS;
            else
                lim = LimitProto::LIMIT_OTHER;

            if (psolsta == MSK::SolSta::FEASIBLE &&
                dsolsta == MSK::SolSta::FEASIBLE)
            {
                double pobj,dobj;
                if (MSK::RES_OK != MSK::get_primal_obj(task.get(),sol_index,&pobj) ||
                    MSK::RES_OK != MSK::get_dual_obj(task.get(),sol_index,&dobj))
                    return absl::InternalError("Failed to extract solution data");

                trmp = FeasibleTerminationProto(ismax,lim,pobj,dobj);
            }
            else {
                double pobj;
                if (MSK::RES_OK != MSK::get_primal_obj(task.get(),sol_index,&pobj))
                    return absl::InternalError("Failed to extract solution data");
                trmp = FeasibleTerminationProto(ismax,lim,pobj,std::nullopt);
            }

        } else {
            //std::cerr << __FILE__ << ":" << __LINE__ << ": !! " << std::endl;
            trmp = NoSolutionFoundTerminationProto(ismax,LimitProto::LIMIT_UNSPECIFIED);
        }
    }

    SolveResultProto result;
    *result.mutable_termination() = trmp;

    if (soldef) {
        // TODO: Use model_parameters
        if (psolsta == MSK::SolSta::OPTIMAL ||
            psolsta == MSK::SolSta::INTEGER_OPTIMAL ||
            psolsta == MSK::SolSta::FEASIBLE ||
            dsolsta == MSK::SolSta::FEASIBLE)
        {
            auto r = Solution(sol_index, ordered_xc_ids, ordered_xx_ids,
                              skip_xx_zeros, ordered_y_ids, skip_y_zeros,
                              ordered_yx_ids, skip_yx_zeros);
            //std::cerr << __FILE__ << ":" << __LINE__ << ": r.ok() = " << r.ok() << std::endl;
            if (r.ok()) {
                *result.add_solutions() = std::move(*r);
            }
        }
        else if (psolsta == MSK::SolSta::INFEAS_CERT)
        {
            auto r = PrimalRay(sol_index, ordered_xx_ids, skip_xx_zeros);
            //std::cerr << __FILE__ << ":" << __LINE__ << ": r.ok() = " << r.ok() << std::endl;
            if (r.ok()) {
                *result.add_primal_rays() = std::move(*r);
            }
        }
        else if (dsolsta == MSK::SolSta::INFEAS_CERT)
        {
            auto r = DualRay(sol_index, ordered_y_ids, skip_y_zeros, ordered_yx_ids, skip_yx_zeros);
            //std::cerr << __FILE__ << ":" << __LINE__ << ": r.ok() = " << r.ok() << std::endl;
            if (r.ok()) {
                *result.add_dual_rays() = std::move(*r);
            }
        }
        else
        {
            //std::cerr << __FILE__ << ":" << __LINE__ << std::endl;
        }
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

std::tuple<const char *,const char *,std::string> MosekSolver::last_error()
{
    MSK::ResCode r = MSK::get_last_resp(task.get());
    const char * rname = MSK::get_resp_name(r);
    const char * rdesc = MSK::get_resp_descr(r);
    int64_t len = MSK::get_last_resp_msg_len(task.get());
    std::vector<char> buf(len+1);
    MSK::get_last_resp_msg(task.get(),buf.data(),buf.size()+1);
    std::string_view s(buf.data());
    return std::make_tuple(rname,rdesc,std::string(s));
}

MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_MOSEK, MosekSolver::New);

}  // namespace operations_research::math_opt
