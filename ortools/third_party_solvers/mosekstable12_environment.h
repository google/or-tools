/* Generated interface for MOSEK Stable API 12.0. */

#pragma once
#ifndef _MOSEK_STABLE_LOADER_H_
#define _MOSEK_STABLE_LOADER_H_

#include <cstddef>
#include <cstdint>
#include <functional>

#define NULLABLE
#define BOOLEAN


#ifndef __cplusplus
#error "Requires C++"
#endif

namespace operations_research::MSK12 {

static const int RES_OK = 0;
static const int TRM_OK = 0;


struct Task_s;
typedef struct Task_s * Task_t;

enum class DomainType {
  NIL,
  RZERO,
  RPLUS,
  RMINUS,
  R,
  QUADRATIC_CONE,
  ROTATED_QUADRATIC_CONE,
  PRIMAL_EXP_CONE,
  DUAL_EXP_CONE,
  PRIMAL_POWER_CONE,
  DUAL_POWER_CONE,
  PRIMAL_GEOMETRIC_MEAN_CONE,
  DUAL_GEOMETRIC_MEAN_CONE,
  SVEC_PSD_CONE,
};

enum class Feature {
  PTON,
  PTS,
};

enum class ObjSense {
  MINIMIZE,
  MAXIMIZE,
};

enum class SolType {
  BASIC,
  INTERIOR,
  INTEGER,
  UNKNOWN,
};

enum class SolSta {
  UNKNOWN,
  UNDEFINED,
  OPTIMAL,
  INTEGER_OPTIMAL,
  FEASIBLE,
  INFEAS_CERT,
  ILLPOSED_CERT,
};

enum class ProSta {
  UNKNOWN,
  PRIMAL_AND_DUAL_FEASIBLE,
  PRIMAL_FEASIBLE,
  DUAL_FEASIBLE,
  PRIMAL_INFEASIBLE,
  DUAL_INFEASIBLE,
  PRIMAL_AND_DUAL_INFEASIBLE,
  ILLPOSED,
  PRIMAL_INFEASIBLE_OR_UNBOUNDED,
};

enum class Format {
  PTF,
  TASK,
  JTASK,
};

enum class VariableType {
  INTEGER,
  CONTINUOUS,
};

enum class Compression {
  NONE,
  GZIP,
  ZSTD,
};

enum class SolutionFormat {
  TASK,
  JTASK,
  TEXT,
};

enum class StreamType {
  MSG,
  WRN,
  ERR,
  LOG,
};


typedef std::int32_t ResCode;
typedef std::int32_t TrmCode;
typedef void* ReadHandle;
typedef void* WriteHandle;
typedef std::size_t (*ReadFunc)(ReadHandle,void*,std::size_t);
typedef std::size_t (*WriteFunc)(WriteHandle,const void*,std::size_t);
typedef void (*StreamFunc)(WriteHandle,const char*);
typedef void* CallbackHandle;
typedef void* ErrorCallbackHandle;
typedef std::int32_t (*ErrorCallbackFunc)(ErrorCallbackHandle,std::int32_t,const char*,const char*,const char*);
typedef std::int32_t (*CallbackFunc)(CallbackHandle,std::int32_t,std::int32_t,const std::int32_t*,std::int32_t,const std::int64_t*,std::int32_t,const double*);
typedef void (*IntSolCallbackFunc)(CallbackHandle,std::int32_t,double,const double*);
typedef void* AllocHandle;
typedef std::uint8_t (**AllocFunc)(AllocHandle,std::size_t);

extern const char* get_callback_code_name(std::int32_t code);
extern const char* get_resp_name(ResCode r);
extern const char* get_resp_descr(ResCode r);
extern ResCode get_last_resp(Task_t task);
extern ResCode get_last_resp_msg(Task_t task,char* buf,std::size_t buf_len);
extern std::size_t get_last_resp_msg_len(Task_t task);
extern const char* get_trm_name(TrmCode trm);
extern const char* get_trm_descr(TrmCode trm);
extern Task_t new_task_from_task(Task_t task);
extern Task_t new_task();
extern void delete_task(Task_t task);
extern ResCode reserve_num_var(Task_t task,std::int32_t add_num);
extern ResCode reserve_num_barvar(Task_t task,std::int32_t num_barvar);
extern ResCode reserve_num_con(Task_t task,std::int32_t num_con);
extern ResCode reserve_num_row(Task_t task,std::int64_t num_row);
extern ResCode reserve_num_nz(Task_t task,std::int64_t num_nz);
extern ResCode reserve_num_dom(Task_t task,std::int64_t num_dom);
extern ResCode reserve_num_symmat(Task_t task,std::int64_t num_symmat);
extern ResCode reserve_num_symmat_nz(Task_t task,std::int64_t num_nz);
extern std::int32_t get_num_var(Task_t task);
extern std::int32_t get_num_barvar(Task_t task);
extern std::int64_t get_num_domain(Task_t task);
extern std::int64_t get_num_row(Task_t task);
extern std::int64_t get_num_symmat(Task_t task);
extern std::int64_t get_num_con(Task_t task);
extern std::int64_t get_num_djc(Task_t task);
extern ResCode append_vars(Task_t task,std::int32_t num_var);
extern ResCode append_rows(Task_t task,std::int64_t num_row);
extern ResCode append_barvar(Task_t task,std::int32_t dim);
extern ResCode append_barvars(Task_t task,std::int32_t num_barvar,const std::int32_t* dims);
extern ResCode append_symmat(Task_t task,std::int32_t dim,std::int64_t nnz,const std::int32_t* symmat_i,const std::int32_t* symmat_j,const double* symmat_val);
extern ResCode append_symmats(Task_t task,std::int64_t num_symmat,const std::int32_t* dim,const std::int64_t* nnz,const std::int32_t* symmat_i,const std::int32_t* symmat_j,const double* symmat_val);
extern ResCode append_empty_cons(Task_t task,std::int64_t num_con);
extern ResCode append_empty_djcs(Task_t task,std::int64_t num_djc);
extern ResCode put_var_type(Task_t task,std::int32_t j,VariableType var_type);
extern ResCode put_var_type_slice(Task_t task,std::int32_t first_var,std::int32_t num_var,const VariableType* var_types);
extern ResCode put_var_type_slice_value(Task_t task,std::int32_t first_var,std::int32_t num_var,VariableType var_type);
extern ResCode put_var_type_list(Task_t task,std::int32_t num_var,const std::int32_t* var_idxs,const VariableType* var_types);
extern ResCode get_var_type(Task_t task,std::int32_t var_idx,VariableType var_type[1]);
extern ResCode get_var_type_slice(Task_t task,std::int32_t first_var,std::int32_t num_var,VariableType* var_types);
extern ResCode put_var_bound(Task_t task,std::int32_t var_idx,double low,double upr);
extern ResCode put_var_bound_slice(Task_t task,std::int32_t first_var,std::int32_t num_var,const double* low,const double* upr);
extern ResCode put_var_bound_slice_value(Task_t task,std::int32_t first_var,std::int32_t num_var,double low,double upr);
extern ResCode put_var_low_bound(Task_t task,std::int32_t var_idx,double low);
extern ResCode put_var_low_bound_slice(Task_t task,std::int32_t first_var,std::int32_t num_var,const double* low);
extern ResCode put_var_low_bound_slice_value(Task_t task,std::int32_t first_var,std::int32_t num_var,double low);
extern ResCode put_var_upr_bound(Task_t task,std::int32_t var_idx,double upr);
extern ResCode put_var_upr_bound_slice(Task_t task,std::int32_t first_var,std::int32_t num_var,const double* upr);
extern ResCode put_var_upr_bound_slice_value(Task_t task,std::int32_t first_var,std::int32_t num_var,double upr);
extern ResCode get_var_bound(Task_t task,std::int32_t var_idx,double low[1],double upr[1]);
extern ResCode get_var_bound_slice(Task_t task,std::int32_t first_var,std::int32_t num_var,double* low,double* upr);
extern ResCode get_barvar_slice_num_elm(Task_t task,std::int32_t first_barvar,std::int32_t num_barvar,std::int64_t num_elm[1]);
extern ResCode get_barvar_dim(Task_t task,std::int32_t barvar_idx,std::int32_t dim[1]);
extern ResCode get_barvar_slice_dims(Task_t task,std::int32_t first_barvar,std::int32_t num_barvar,std::int32_t* dim);
extern ResCode get_domain(Task_t task,DomainType dom_type,std::int64_t dim,std::int32_t num_alpha,double* alpha,std::int64_t dom_idx[1]);
extern ResCode get_domain_empty(Task_t task,std::int64_t dom_idx[1]);
extern ResCode get_domain_rzero(Task_t task,std::int64_t dom_idx[1]);
extern ResCode get_domain_rplus(Task_t task,std::int64_t dom_idx[1]);
extern ResCode get_domain_rminus(Task_t task,std::int64_t dom_idx[1]);
extern ResCode get_domain_r(Task_t task,std::int64_t dom_idx[1]);
extern ResCode get_domain_quadratic_cone(Task_t task,std::int64_t n,std::int64_t dom_idx[1]);
extern ResCode get_domain_rotated_quadratic_cone(Task_t task,std::int64_t n,std::int64_t dom_idx[1]);
extern ResCode get_domain_primal_exponential_cone(Task_t task,std::int64_t dom_idx[1]);
extern ResCode get_domain_dual_exponential_cone(Task_t task,std::int64_t dom_idx[1]);
extern ResCode get_domain_primal_power_cone(Task_t task,std::int64_t n,std::int64_t num_alpha,const double* alpha,std::int64_t dom_idx[1]);
extern ResCode get_domain_dual_power_cone(Task_t task,std::int64_t n,std::int64_t num_alpha,const double* alpha,std::int64_t dom_idx[1]);
extern ResCode get_domain_primal_geometric_mean_cone(Task_t task,std::int64_t n,std::int64_t dom_idx[1]);
extern ResCode get_domain_dual_geometric_mean_cone(Task_t task,std::int64_t n,std::int64_t dom_idx[1]);
extern ResCode get_domain_svecpsd_cone(Task_t task,std::int64_t n,std::int64_t dom_idx[1]);
extern ResCode get_domain_info(Task_t task,std::int64_t dom_idx,DomainType dom_type[1],std::int64_t size[1],std::int32_t num_alpha[1]);
extern ResCode get_domain_alpha(Task_t task,std::int64_t dom_idx,std::int64_t num_alpha,double* alpha);
extern ResCode put_row(Task_t task,std::int64_t row_idx,std::int32_t num_nz,const std::int32_t* subj,const double* cof);
extern ResCode put_row_slice(Task_t task,std::int64_t first_row,std::int64_t num_row,const std::int32_t* row_num_nz,const std::int32_t* subj,const double* cof);
extern ResCode put_row_list(Task_t task,std::int64_t num_row,const std::int64_t* row_idxs,const std::int32_t* row_num_nz,const std::int32_t** subj,const double** cof);
extern ResCode put_row_g(Task_t task,std::int64_t row_idx,double g);
extern ResCode put_row_slice_g(Task_t task,std::int64_t first_row,std::int64_t num_row,const double* g);
extern ResCode put_row_list_g(Task_t task,std::int64_t num_row,const std::int64_t* row_idxs,const double* g);
extern ResCode put_col(Task_t task,std::int32_t col_idx,std::int64_t num_nz,const std::int64_t* row_idxs,const double* cof);
extern ResCode put_col_slice(Task_t task,std::int32_t first_col,std::int32_t num_col,const std::int64_t* col_len,const std::int64_t* row_idxs,const double* cof);
extern ResCode put_col_list(Task_t task,std::int32_t num_col,const std::int32_t* col_idxs,const std::int64_t* col_lens,const std::int64_t** row_idxs,const double** cof);
extern ResCode put_ijc(Task_t task,std::int64_t row_idx,std::int32_t var_idx,double cof);
extern ResCode put_ijc_list(Task_t task,std::int64_t num_nz,const std::int64_t* row_idxs,const std::int32_t* col_idxs,const double* cof);
extern ResCode get_row_num_nz(Task_t task,std::int64_t row_idx,std::int32_t num_nz[1]);
extern ResCode get_row_slice_num_nz(Task_t task,std::int64_t first_row,std::int64_t num_row,std::int64_t* num_nz);
extern ResCode get_row(Task_t task,std::int64_t row_idx,std::int32_t nnz,std::int32_t* subj,double* cof);
extern ResCode get_row_slice(Task_t task,std::int64_t first_row,std::int64_t num_row,std::int64_t nnz,std::int32_t* row_len,std::int32_t* subj,double* cof);
extern ResCode get_col_num_nz(Task_t task,std::int32_t col_idx,std::int64_t num_nz[1]);
extern ResCode get_col_slice_num_nz(Task_t task,std::int32_t first_col,std::int32_t num_col,std::int64_t* num_nz);
extern ResCode get_col(Task_t task,std::int32_t col_idx,std::int64_t nnz,std::int64_t* subi,double* cof);
extern ResCode get_col_slice(Task_t task,std::int32_t first_col,std::int32_t num_col,std::int64_t nnz,std::int64_t* col_len,std::int64_t* subi,double* cof);
extern ResCode put_bar_entry(Task_t task,std::int64_t row_idx,std::int32_t barvar_idx,std::int64_t num_weight,const std::int64_t* matrix_idx,const double* weight);
extern ResCode put_bar_entry_list(Task_t task,std::int64_t num_bar_entry,const std::int64_t* row_idx,const std::int32_t* barvar_idx,const std::int64_t* num_weight,const std::int64_t* matrix_idx,const double* weight);
extern ResCode put_bar_row(Task_t task,std::int64_t row_idx,std::int32_t num_bar_entry,const std::int32_t* barvar_idx,const std::int64_t* num_weight,const std::int64_t* matrix_idx,const double* weight);
extern ResCode get_symmat_info(Task_t task,std::int64_t symmat_idx,std::int32_t dim[1],std::int64_t nnz[1]);
extern ResCode get_symmat(Task_t task,std::int64_t symmat_idx,std::int64_t nnz,std::int32_t* symmat_i,std::int32_t* symmat_j,double* symmat_val);
extern ResCode get_symmat_slice_info(Task_t task,std::int64_t first_symmat,std::int64_t num_symmat,std::int32_t* dim,std::int64_t* nnz);
extern ResCode get_symmat_slice(Task_t task,std::int64_t first_symmat,std::int64_t num_symmat,std::int64_t total_nnz,std::int32_t* symmat_i,std::int32_t* symmat_j,double* symmat_val);
extern ResCode append_con(Task_t task,std::int64_t dom_idx,std::int64_t num_rows,const std::int64_t* row_idxs,NULLABLE const double* con_offset);
extern ResCode append_cons(Task_t task,std::int64_t num_con,const std::int64_t* dom_idxs,std::int64_t num_rows,const std::int64_t* row_idxs,NULLABLE const double* con_offset);
extern ResCode append_cons_seq(Task_t task,std::int64_t num_con,std::int64_t dom_idx,std::int64_t first_row,std::int64_t num_rows,NULLABLE const double* con_offset);
extern ResCode put_con(Task_t task,std::int64_t con_idx,std::int64_t num_rows,std::int64_t dom_idx,const std::int64_t* row_idxs,NULLABLE const double* rhs_offset);
extern ResCode put_scalar_con(Task_t task,std::int64_t con_idx,std::int64_t dom_idx,std::int64_t row_idx,double rhs_offset);
extern ResCode put_con_slice(Task_t task,std::int64_t first_con,std::int64_t num_con,std::int64_t num_rows,const std::int64_t* dom_idx,const std::int64_t* row_idx,NULLABLE const double* rhs_offset);
extern ResCode get_con_slice_domains(Task_t task,std::int64_t first_con,std::int64_t num_con,std::int64_t* dom_idx);
extern ResCode get_con_slice_num_row(Task_t task,std::int64_t first_con,std::int64_t num_con,std::int64_t num_row[1]);
extern ResCode get_con_slice(Task_t task,std::int64_t first_con,std::int64_t num_con,std::int64_t num_row,std::int64_t* row_idx,double* rhs_offset,std::int64_t* dom_idx);
extern ResCode append_djc(Task_t task,std::int64_t num_rows,std::int64_t num_dom,std::int64_t num_terms,const std::int64_t* dom_idx,const std::int64_t* term_size,const std::int64_t* row_idx,const double* rhs_offset);
extern ResCode put_djc(Task_t task,std::int64_t djc_idx,std::int64_t num_rows,std::int64_t num_dom,std::int64_t num_terms,const std::int64_t* dom_idx,const std::int64_t* term_size,const std::int64_t* row_idx,const double* rhs_offset);
extern ResCode put_djc_slice(Task_t task,std::int64_t first_djc,std::int64_t num_djc,std::int64_t num_rows,std::int64_t num_dom,std::int64_t num_terms,const std::int64_t* dom_idx,const std::int64_t* term_size,const std::int64_t* row_idx,const double* rhs_offset,const std::int64_t* djc_numterm);
extern ResCode get_djc_info(Task_t task,std::int64_t djc_idx,std::int64_t num_term[1],std::int64_t num_dom[1],std::int64_t num_row[1]);
extern ResCode get_djc(Task_t task,std::int64_t djc_idx,std::int64_t num_terms,std::int64_t num_dom,std::int64_t num_row,std::int64_t* term_size,std::int64_t* dom_idx,std::int64_t* row_idx,double* rhs_offset);
extern ResCode get_djc_slice_info(Task_t task,std::int64_t first_djc,std::int64_t num_djc,std::int64_t num_term[1],std::int64_t num_dom[1],std::int64_t num_row[1]);
extern ResCode get_djc_slice(Task_t task,std::int64_t first_djc,std::int64_t num_djc,std::int64_t num_term,std::int64_t num_dom,std::int64_t num_row,std::int64_t* term_size,std::int64_t* dom_idx,std::int64_t* row_idx,double* rhs_offset,std::int64_t* djc_num_term);
extern void put_obj_sense(Task_t task,ObjSense sense);
extern ObjSense get_obj_sense(Task_t task);
extern ResCode put_obj_row(Task_t task,std::int64_t row_idx);
extern void get_obj_row(Task_t task,std::int64_t row_idx[1],int asgn[1]);
extern ResCode optimize(Task_t task,TrmCode trm[1]);
extern ResCode solution_summary(Task_t task,StreamType whichstream);
extern ResCode optimize_callback(Task_t task,TrmCode trm[1],CallbackHandle cb_handle,CallbackFunc cb_func,CallbackHandle int_cb_handle,IntSolCallbackFunc int_cb_func);
extern void put_remote_solver(Task_t task,const char* server,NULLABLE const char* cert);
extern void put_optserver_access_token(Task_t task,NULLABLE const char* token);
extern std::int32_t get_num_sol(Task_t task);
extern ResCode get_sol_type(Task_t task,std::int32_t sol_idx,SolType sol_type[1]);
extern ResCode get_sol_status(Task_t task,std::int32_t sol_idx,SolSta primal_sol_sta[1],SolSta dual_sol_sta[1]);
extern ResCode get_problem_status(Task_t task,std::int32_t sol_idx,ProSta pro_sta[1]);
extern ResCode get_primal_obj(Task_t task,std::int32_t sol_idx,double obj_val[1]);
extern ResCode get_dual_obj(Task_t task,std::int32_t sol_idx,double obj_val[1]);
extern ResCode get_sol_xx_slice(Task_t task,std::int32_t sol_idx,std::int32_t first_var,std::int32_t num_var,double* xx);
extern ResCode get_sol_slx_slice(Task_t task,std::int32_t sol_idx,std::int32_t first_var,std::int32_t num_var,double* slx);
extern ResCode get_sol_sux_slice(Task_t task,std::int32_t sol_idx,std::int32_t first_var,std::int32_t num_var,double* sux);
extern ResCode get_sol_barxj(Task_t task,std::int32_t sol_idx,std::int32_t barvar_idx,std::int64_t num_var,double* barx);
extern ResCode get_sol_barsj(Task_t task,std::int32_t sol_idx,std::int32_t barvar_idx,std::int64_t num_elm,double* bars);
extern ResCode get_sol_barx_slice(Task_t task,std::int32_t sol_idx,std::int32_t first_barvar,std::int32_t num_barvar,std::int64_t num_elm,double* barx);
extern ResCode get_sol_bars_slice(Task_t task,std::int32_t sol_idx,std::int32_t first_barvar,std::int32_t num_barvar,std::int64_t num_elm,double* bars);
extern ResCode get_sol_basic_xj(Task_t task,std::int32_t sol_idx,std::int32_t var_idx,int basic[1]);
extern ResCode get_sol_basic_barx(Task_t task,std::int32_t sol_idx,std::int32_t barvar_idx,int basic[1]);
extern ResCode get_sol_basic_con(Task_t task,std::int32_t sol_idx,std::int64_t con_idx,int basic[1]);
extern ResCode get_sol_sta_var(Task_t task,std::int32_t sol_idx,std::int32_t var_idx,int low_binding[1],int upr_binding[1]);
extern ResCode get_sol_sta_barx(Task_t task,std::int32_t sol_idx,std::int32_t barvar_idx,int binding[1]);
extern ResCode get_sol_sta_con(Task_t task,std::int32_t sol_idx,std::int64_t con_idx,int binding[1]);
extern ResCode get_sol_basic_x_slice(Task_t task,std::int32_t sol_idx,std::int32_t first_var,std::int32_t num_var,int* basic);
extern ResCode get_sol_basic_barx_slice(Task_t task,std::int32_t sol_idx,std::int32_t first_var,std::int32_t num_var,int* basic);
extern ResCode get_sol_basic_con_slice(Task_t task,std::int32_t sol_idx,std::int64_t first_con,std::int64_t num_con,int* basic);
extern ResCode get_sol_sta_var_slice(Task_t task,std::int32_t sol_idx,std::int32_t first_var,std::int32_t num_var,int* low_binding,int* upr_binding);
extern ResCode get_sol_sta_barx_slice(Task_t task,std::int32_t sol_idx,std::int32_t first_barvar,std::int32_t num_barvar,int* bindnig);
extern ResCode get_sol_sta_con_slice(Task_t task,std::int32_t sol_idx,std::int64_t first_con,std::int64_t num_con,int* binding);
extern ResCode get_sol_xc_slice(Task_t task,std::int32_t sol_idx,std::int64_t first_con,std::int64_t num_con,std::int64_t num_elm,double* xc);
extern ResCode get_sol_y_slice(Task_t task,std::int32_t sol_idx,std::int64_t first_con,std::int64_t num_con,std::int64_t num_elm,double* y);
extern std::int32_t get_num_input_solutions(Task_t task);
extern ResCode copy_sol_to_input(Task_t task,std::int32_t sol_idx);
extern ResCode append_sol(Task_t task,SolType soltype);
extern ResCode put_sol_xx(Task_t task,std::int32_t sol_idx,std::int32_t num,const double* val);
extern ResCode put_sol_slx(Task_t task,std::int32_t sol_idx,std::int32_t num,const double* val);
extern ResCode put_sol_sux(Task_t task,std::int32_t sol_idx,std::int32_t num,const double* val);
extern ResCode put_sol_basic_x(Task_t task,std::int32_t sol_idx,std::int32_t num,const std::int32_t* val);
extern ResCode put_sol_barx(Task_t task,std::int32_t sol_idx,std::int64_t num,const double* val);
extern ResCode put_sol_bars(Task_t task,std::int32_t sol_idx,std::int64_t num,const double* xx);
extern ResCode put_sol_yi(Task_t task,std::int32_t sol_idx,std::int64_t i,std::int64_t num,const double* xx);
extern ResCode put_sol_basic_c(Task_t task,std::int32_t sol_idx,std::int64_t num,const std::int32_t* val);
extern std::int32_t get_num_iinf();
extern std::int32_t get_num_liinf();
extern std::int32_t get_num_dinf();
extern ResCode get_iinf(Task_t task,std::int32_t par_idx,std::int32_t value[1]);
extern ResCode get_liinf(Task_t task,std::int32_t par_idx,std::int64_t value[1]);
extern ResCode get_dinf(Task_t task,std::int32_t par_idx,double value[1]);
extern const char* get_iinf_name(std::int32_t par_idx);
extern const char* get_liinf_name(std::int32_t par_idx);
extern const char* get_dinf_name(std::int32_t par_idx);
extern std::int32_t get_iinf_index(const char* par_name);
extern std::int32_t get_liinf_index(const char* par_name);
extern std::int32_t get_dinf_index(const char* name);
extern std::int32_t get_double_param(Task_t task,const char* par_name,double value[1]);
extern std::int32_t get_double_param_index(const char* par_name);
extern const char* get_double_param_name(std::int32_t par_idx);
extern std::int32_t get_num_double_param();
extern void get_all_double_params(Task_t task,std::int32_t buflen,double* buf);
extern ResCode put_all_double_params(Task_t task,std::int32_t num_par,const double* params);
extern std::int32_t get_int_param_index(const char* par_name);
extern const char* get_int_param_name(std::int32_t par_idx);
extern std::int32_t get_num_int_param();
extern void get_all_int_params(Task_t task,std::int32_t buflen,std::int32_t* buf);
extern ResCode put_all_int_params(Task_t task,std::int32_t num_par,const std::int32_t* params);
extern std::int32_t get_int_param(Task_t task,const char* par_name,std::int32_t value[1]);
extern std::int32_t get_param_str_len(Task_t task,const char* par_name);
extern void get_param_str(Task_t task,const char* name,std::int32_t length,char* buf);
extern ResCode put_double_param(Task_t task,const char* par_name,double value,int ok[1]);
extern ResCode put_int_param(Task_t task,const char* par_name,std::int32_t value,int ok[1]);
extern ResCode put_param_str(Task_t task,const char* par_name,const char* value,int ok[1]);
extern std::int32_t get_task_name_len(Task_t task);
extern std::int32_t get_obj_name_len(Task_t task);
extern void get_task_name(Task_t task,std::int32_t capacity,char* buf);
extern void get_obj_name(Task_t task,std::int32_t capacity,char* buf);
extern ResCode put_task_name(Task_t task,const char* name);
extern ResCode put_obj_name(Task_t task,const char* name);
extern ResCode get_var_name_len(Task_t task,std::int32_t var_idx,std::int32_t name_len[1]);
extern std::int32_t get_var_name_len2(Task_t task,std::int32_t var_idx);
extern ResCode get_barvar_name_len(Task_t task,std::int32_t barvar_idx,std::int32_t name_len[1]);
extern std::int32_t get_barvar_name_len2(Task_t task,std::int32_t barvar_idx);
extern ResCode get_var_name(Task_t task,std::int32_t var_idx,std::int32_t capacity,char* buf);
extern ResCode get_barvar_name(Task_t task,std::int32_t barvar_idx,std::int32_t capacity,char* buf);
extern ResCode put_var_name(Task_t task,std::int32_t var_idx,const char* name);
extern ResCode put_barvar_name(Task_t task,std::int32_t barvar_idx,const char* name);
extern ResCode get_con_name_len(Task_t task,std::int64_t con_idx,std::int32_t len[1]);
extern ResCode get_djc_name_len(Task_t task,std::int64_t djc_idx,std::int32_t len[1]);
extern std::int32_t get_con_name_len2(Task_t task,std::int64_t con_idx);
extern std::int32_t get_djc_name_len2(Task_t task,std::int64_t djc_idx);
extern ResCode get_con_name(Task_t task,std::int64_t con_idx,std::int32_t capacity,char* buf);
extern ResCode get_djc_name(Task_t task,std::int64_t djc_idx,std::int32_t capacity,char* buf);
extern ResCode put_con_name(Task_t task,std::int64_t con_idx,const char* buf);
extern ResCode put_djc_name(Task_t task,std::int64_t djc_idx,const char* buf);
extern ResCode write_task_to_file(Task_t task,const char* filename);
extern ResCode write_task_to_handle(Task_t task,Format format,Compression compress,WriteHandle handle,WriteFunc func);
extern ResCode write_solution_to_file(Task_t task,const char* filename);
extern ResCode write_solution_to_handle(Task_t task,SolutionFormat format,Compression compress,WriteHandle handle,WriteFunc func);
extern ResCode read_from_file(Task_t task,const char* filename);
extern ResCode read_from_handle(Task_t task,Format format,Compression compress,ReadHandle handle,ReadFunc func);
extern ResCode put_stream_file(Task_t task,StreamType whichstream,const char* filename,int append);
extern ResCode clear_stream_file(Task_t task,StreamType whichstream);
extern ResCode put_stream_callback(Task_t task,StreamType whichstream,WriteHandle handle,StreamFunc func);
extern ResCode clear_stream_callback(Task_t task,StreamType whichstream);
extern ResCode put_error_callback(Task_t task,ErrorCallbackHandle handle,ErrorCallbackFunc func);
extern ResCode put_warning_callback(Task_t task,ErrorCallbackHandle handle,ErrorCallbackFunc func);
extern ResCode clear_error_callback(Task_t task);
extern ResCode clear_warning_callback(Task_t task);
extern void license_cleanup();
extern void shutdown_global_threadpool();
extern ResCode axpy(std::int32_t n,double alpha,const double* x,double* y);
extern ResCode dot(std::int32_t n,const double* x,const double* y,double xty[1]);
extern ResCode gemv(int transa,std::int32_t m,std::int32_t n,double alpha,const double* a,const double* x,double beta,double* y);
extern ResCode gemm(int transa,int transb,std::int32_t m,std::int32_t n,std::int32_t k,double alpha,const double* a,const double* b,double beta,double* c);
extern ResCode syrk(int is_upr,int trans,std::int32_t n,std::int32_t k,double alpha,const double* a,double beta,double* c);
extern ResCode sparse_triangular_solve_dense(int transposed,std::int32_t n,const std::int32_t* lnzc,const std::int32_t* lsubc,const double* lvalc,double* b);
extern ResCode potrf(int is_upr,std::int32_t n,double* a);
extern ResCode syeig(int is_upr,std::int32_t n,const double* a,double* w);
extern ResCode syevd(int is_upr,std::int32_t n,double* a,double* w);
extern ResCode compute_sparse_cholesky(std::int32_t num_threads,int order_method,double tol_singular,std::int32_t n,const std::int32_t* a_col_num_nonzero,const std::int32_t* a_subi,const double* a_val,std::int32_t* perm,double* diag,AllocFunc alloc,AllocHandle alloc_handle,std::int32_t* l_col_num_nonzero,std::int32_t** l_subi,double** l_val);
extern ResCode optimize_batch(int is_race,double max_time_sec,std::int32_t num_threads,std::int64_t num_task,const Task_t* tasks,TrmCode* trm_code,ResCode* res_code);
extern ResCode check_out_license(Feature feature);
extern ResCode check_in_license(Feature feature);
extern ResCode check_in_all();
extern ResCode echo_intro(int long_ver);
extern void get_version(std::int32_t major[1],std::int32_t minor[1],std::int32_t revision[1]);
extern ResCode put_license_debug(int lic_debug);
extern ResCode put_license_code(NULLABLE const std::int32_t code[21]);
extern ResCode put_license_wait(std::int32_t lic_wait);
extern ResCode put_license_path(NULLABLE const char* license_path);

int library_initialized();
int initialize_library();

} // namespace operations_research::MSK12

#endif
