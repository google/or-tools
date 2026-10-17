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

extern std::function<const char*(std::int32_t)> get_callback_code_name;
extern std::function<const char*(ResCode)> get_resp_name;
extern std::function<const char*(ResCode)> get_resp_descr;
extern std::function<ResCode(Task_t)> get_last_resp;
extern std::function<ResCode(Task_t,char*,std::size_t)> get_last_resp_msg;
extern std::function<std::size_t(Task_t)> get_last_resp_msg_len;
extern std::function<const char*(TrmCode)> get_trm_name;
extern std::function<const char*(TrmCode)> get_trm_descr;
extern std::function<Task_t(Task_t)> new_task_from_task;
extern std::function<Task_t()> new_task;
extern std::function<void(Task_t)> delete_task;
extern std::function<ResCode(Task_t,std::int32_t)> reserve_num_var;
extern std::function<ResCode(Task_t,std::int32_t)> reserve_num_barvar;
extern std::function<ResCode(Task_t,std::int32_t)> reserve_num_con;
extern std::function<ResCode(Task_t,std::int64_t)> reserve_num_row;
extern std::function<ResCode(Task_t,std::int64_t)> reserve_num_nz;
extern std::function<ResCode(Task_t,std::int64_t)> reserve_num_dom;
extern std::function<ResCode(Task_t,std::int64_t)> reserve_num_symmat;
extern std::function<ResCode(Task_t,std::int64_t)> reserve_num_symmat_nz;
extern std::function<std::int32_t(Task_t)> get_num_var;
extern std::function<std::int32_t(Task_t)> get_num_barvar;
extern std::function<std::int64_t(Task_t)> get_num_domain;
extern std::function<std::int64_t(Task_t)> get_num_row;
extern std::function<std::int64_t(Task_t)> get_num_symmat;
extern std::function<std::int64_t(Task_t)> get_num_con;
extern std::function<std::int64_t(Task_t)> get_num_djc;
extern std::function<ResCode(Task_t,std::int32_t)> append_vars;
extern std::function<ResCode(Task_t,std::int64_t)> append_rows;
extern std::function<ResCode(Task_t,std::int32_t)> append_barvar;
extern std::function<ResCode(Task_t,std::int32_t,const std::int32_t*)> append_barvars;
extern std::function<ResCode(Task_t,std::int32_t,std::int64_t,const std::int32_t*,const std::int32_t*,const double*)> append_symmat;
extern std::function<ResCode(Task_t,std::int64_t,const std::int32_t*,const std::int64_t*,const std::int32_t*,const std::int32_t*,const double*)> append_symmats;
extern std::function<ResCode(Task_t,std::int64_t)> append_empty_cons;
extern std::function<ResCode(Task_t,std::int64_t)> append_empty_djcs;
extern std::function<ResCode(Task_t,std::int32_t,VariableType)> put_var_type;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,const VariableType*)> put_var_type_slice;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,VariableType)> put_var_type_slice_value;
extern std::function<ResCode(Task_t,std::int32_t,const std::int32_t*,const VariableType*)> put_var_type_list;
extern std::function<ResCode(Task_t,std::int32_t,VariableType[1])> get_var_type;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,VariableType*)> get_var_type_slice;
extern std::function<ResCode(Task_t,std::int32_t,double,double)> put_var_bound;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,const double*,const double*)> put_var_bound_slice;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,double,double)> put_var_bound_slice_value;
extern std::function<ResCode(Task_t,std::int32_t,double)> put_var_low_bound;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,const double*)> put_var_low_bound_slice;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,double)> put_var_low_bound_slice_value;
extern std::function<ResCode(Task_t,std::int32_t,double)> put_var_upr_bound;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,const double*)> put_var_upr_bound_slice;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,double)> put_var_upr_bound_slice_value;
extern std::function<ResCode(Task_t,std::int32_t,double[1],double[1])> get_var_bound;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,double*,double*)> get_var_bound_slice;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int64_t[1])> get_barvar_slice_num_elm;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t[1])> get_barvar_dim;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t*)> get_barvar_slice_dims;
extern std::function<ResCode(Task_t,DomainType,std::int64_t,std::int32_t,double*,std::int64_t[1])> get_domain;
extern std::function<ResCode(Task_t,std::int64_t[1])> get_domain_empty;
extern std::function<ResCode(Task_t,std::int64_t[1])> get_domain_rzero;
extern std::function<ResCode(Task_t,std::int64_t[1])> get_domain_rplus;
extern std::function<ResCode(Task_t,std::int64_t[1])> get_domain_rminus;
extern std::function<ResCode(Task_t,std::int64_t[1])> get_domain_r;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t[1])> get_domain_quadratic_cone;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t[1])> get_domain_rotated_quadratic_cone;
extern std::function<ResCode(Task_t,std::int64_t[1])> get_domain_primal_exponential_cone;
extern std::function<ResCode(Task_t,std::int64_t[1])> get_domain_dual_exponential_cone;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,const double*,std::int64_t[1])> get_domain_primal_power_cone;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,const double*,std::int64_t[1])> get_domain_dual_power_cone;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t[1])> get_domain_primal_geometric_mean_cone;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t[1])> get_domain_dual_geometric_mean_cone;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t[1])> get_domain_svecpsd_cone;
extern std::function<ResCode(Task_t,std::int64_t,DomainType[1],std::int64_t[1],std::int32_t[1])> get_domain_info;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,double*)> get_domain_alpha;
extern std::function<ResCode(Task_t,std::int64_t,std::int32_t,const std::int32_t*,const double*)> put_row;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,const std::int32_t*,const std::int32_t*,const double*)> put_row_slice;
extern std::function<ResCode(Task_t,std::int64_t,const std::int64_t*,const std::int32_t*,const std::int32_t**,const double**)> put_row_list;
extern std::function<ResCode(Task_t,std::int64_t,double)> put_row_g;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,const double*)> put_row_slice_g;
extern std::function<ResCode(Task_t,std::int64_t,const std::int64_t*,const double*)> put_row_list_g;
extern std::function<ResCode(Task_t,std::int32_t,std::int64_t,const std::int64_t*,const double*)> put_col;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,const std::int64_t*,const std::int64_t*,const double*)> put_col_slice;
extern std::function<ResCode(Task_t,std::int32_t,const std::int32_t*,const std::int64_t*,const std::int64_t**,const double**)> put_col_list;
extern std::function<ResCode(Task_t,std::int64_t,std::int32_t,double)> put_ijc;
extern std::function<ResCode(Task_t,std::int64_t,const std::int64_t*,const std::int32_t*,const double*)> put_ijc_list;
extern std::function<ResCode(Task_t,std::int64_t,std::int32_t[1])> get_row_num_nz;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t*)> get_row_slice_num_nz;
extern std::function<ResCode(Task_t,std::int64_t,std::int32_t,std::int32_t*,double*)> get_row;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,std::int32_t*,std::int32_t*,double*)> get_row_slice;
extern std::function<ResCode(Task_t,std::int32_t,std::int64_t[1])> get_col_num_nz;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int64_t*)> get_col_slice_num_nz;
extern std::function<ResCode(Task_t,std::int32_t,std::int64_t,std::int64_t*,double*)> get_col;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int64_t,std::int64_t*,std::int64_t*,double*)> get_col_slice;
extern std::function<ResCode(Task_t,std::int64_t,std::int32_t,std::int64_t,const std::int64_t*,const double*)> put_bar_entry;
extern std::function<ResCode(Task_t,std::int64_t,const std::int64_t*,const std::int32_t*,const std::int64_t*,const std::int64_t*,const double*)> put_bar_entry_list;
extern std::function<ResCode(Task_t,std::int64_t,std::int32_t,const std::int32_t*,const std::int64_t*,const std::int64_t*,const double*)> put_bar_row;
extern std::function<ResCode(Task_t,std::int64_t,std::int32_t[1],std::int64_t[1])> get_symmat_info;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int32_t*,std::int32_t*,double*)> get_symmat;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int32_t*,std::int64_t*)> get_symmat_slice_info;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,std::int32_t*,std::int32_t*,double*)> get_symmat_slice;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,const std::int64_t*,NULLABLE const double*)> append_con;
extern std::function<ResCode(Task_t,std::int64_t,const std::int64_t*,std::int64_t,const std::int64_t*,NULLABLE const double*)> append_cons;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t,NULLABLE const double*)> append_cons_seq;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,const std::int64_t*,NULLABLE const double*)> put_con;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,double)> put_scalar_con;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,const std::int64_t*,const std::int64_t*,NULLABLE const double*)> put_con_slice;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t*)> get_con_slice_domains;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t[1])> get_con_slice_num_row;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t*,double*,std::int64_t*)> get_con_slice;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,const std::int64_t*,const std::int64_t*,const std::int64_t*,const double*)> append_djc;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t,const std::int64_t*,const std::int64_t*,const std::int64_t*,const double*)> put_djc;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t,const std::int64_t*,const std::int64_t*,const std::int64_t*,const double*,const std::int64_t*)> put_djc_slice;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t[1],std::int64_t[1],std::int64_t[1])> get_djc_info;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t*,std::int64_t*,std::int64_t*,double*)> get_djc;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t[1],std::int64_t[1],std::int64_t[1])> get_djc_slice_info;
extern std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t*,std::int64_t*,std::int64_t*,double*,std::int64_t*)> get_djc_slice;
extern std::function<void(Task_t,ObjSense)> put_obj_sense;
extern std::function<ObjSense(Task_t)> get_obj_sense;
extern std::function<ResCode(Task_t,std::int64_t)> put_obj_row;
extern std::function<void(Task_t,std::int64_t[1],int[1])> get_obj_row;
extern std::function<ResCode(Task_t,TrmCode[1])> optimize;
extern std::function<ResCode(Task_t,StreamType)> solution_summary;
extern std::function<ResCode(Task_t,TrmCode[1],CallbackHandle,CallbackFunc,CallbackHandle,IntSolCallbackFunc)> optimize_callback;
extern std::function<void(Task_t,const char*,NULLABLE const char*)> put_remote_solver;
extern std::function<void(Task_t,NULLABLE const char*)> put_optserver_access_token;
extern std::function<std::int32_t(Task_t)> get_num_sol;
extern std::function<ResCode(Task_t,std::int32_t,SolType[1])> get_sol_type;
extern std::function<ResCode(Task_t,std::int32_t,SolSta[1],SolSta[1])> get_sol_status;
extern std::function<ResCode(Task_t,std::int32_t,ProSta[1])> get_problem_status;
extern std::function<ResCode(Task_t,std::int32_t,double[1])> get_primal_obj;
extern std::function<ResCode(Task_t,std::int32_t,double[1])> get_dual_obj;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t,double*)> get_sol_xx_slice;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t,double*)> get_sol_slx_slice;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t,double*)> get_sol_sux_slice;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int64_t,double*)> get_sol_barxj;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int64_t,double*)> get_sol_barsj;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t,std::int64_t,double*)> get_sol_barx_slice;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t,std::int64_t,double*)> get_sol_bars_slice;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,int[1])> get_sol_basic_xj;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,int[1])> get_sol_basic_barx;
extern std::function<ResCode(Task_t,std::int32_t,std::int64_t,int[1])> get_sol_basic_con;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,int[1],int[1])> get_sol_sta_var;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,int[1])> get_sol_sta_barx;
extern std::function<ResCode(Task_t,std::int32_t,std::int64_t,int[1])> get_sol_sta_con;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t,int*)> get_sol_basic_x_slice;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t,int*)> get_sol_basic_barx_slice;
extern std::function<ResCode(Task_t,std::int32_t,std::int64_t,std::int64_t,int*)> get_sol_basic_con_slice;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t,int*,int*)> get_sol_sta_var_slice;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t,int*)> get_sol_sta_barx_slice;
extern std::function<ResCode(Task_t,std::int32_t,std::int64_t,std::int64_t,int*)> get_sol_sta_con_slice;
extern std::function<ResCode(Task_t,std::int32_t,std::int64_t,std::int64_t,std::int64_t,double*)> get_sol_xc_slice;
extern std::function<ResCode(Task_t,std::int32_t,std::int64_t,std::int64_t,std::int64_t,double*)> get_sol_y_slice;
extern std::function<std::int32_t(Task_t)> get_num_input_solutions;
extern std::function<ResCode(Task_t,std::int32_t)> copy_sol_to_input;
extern std::function<ResCode(Task_t,SolType)> append_sol;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,const double*)> put_sol_xx;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,const double*)> put_sol_slx;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,const double*)> put_sol_sux;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,const std::int32_t*)> put_sol_basic_x;
extern std::function<ResCode(Task_t,std::int32_t,std::int64_t,const double*)> put_sol_barx;
extern std::function<ResCode(Task_t,std::int32_t,std::int64_t,const double*)> put_sol_bars;
extern std::function<ResCode(Task_t,std::int32_t,std::int64_t,std::int64_t,const double*)> put_sol_yi;
extern std::function<ResCode(Task_t,std::int32_t,std::int64_t,const std::int32_t*)> put_sol_basic_c;
extern std::function<std::int32_t()> get_num_iinf;
extern std::function<std::int32_t()> get_num_liinf;
extern std::function<std::int32_t()> get_num_dinf;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t[1])> get_iinf;
extern std::function<ResCode(Task_t,std::int32_t,std::int64_t[1])> get_liinf;
extern std::function<ResCode(Task_t,std::int32_t,double[1])> get_dinf;
extern std::function<const char*(std::int32_t)> get_iinf_name;
extern std::function<const char*(std::int32_t)> get_liinf_name;
extern std::function<const char*(std::int32_t)> get_dinf_name;
extern std::function<std::int32_t(const char*)> get_iinf_index;
extern std::function<std::int32_t(const char*)> get_liinf_index;
extern std::function<std::int32_t(const char*)> get_dinf_index;
extern std::function<std::int32_t(Task_t,const char*,double[1])> get_double_param;
extern std::function<std::int32_t(const char*)> get_double_param_index;
extern std::function<const char*(std::int32_t)> get_double_param_name;
extern std::function<std::int32_t()> get_num_double_param;
extern std::function<void(Task_t,std::int32_t,double*)> get_all_double_params;
extern std::function<ResCode(Task_t,std::int32_t,const double*)> put_all_double_params;
extern std::function<std::int32_t(const char*)> get_int_param_index;
extern std::function<const char*(std::int32_t)> get_int_param_name;
extern std::function<std::int32_t()> get_num_int_param;
extern std::function<void(Task_t,std::int32_t,std::int32_t*)> get_all_int_params;
extern std::function<ResCode(Task_t,std::int32_t,const std::int32_t*)> put_all_int_params;
extern std::function<std::int32_t(Task_t,const char*,std::int32_t[1])> get_int_param;
extern std::function<std::int32_t(Task_t,const char*)> get_param_str_len;
extern std::function<void(Task_t,const char*,std::int32_t,char*)> get_param_str;
extern std::function<ResCode(Task_t,const char*,double,int[1])> put_double_param;
extern std::function<ResCode(Task_t,const char*,std::int32_t,int[1])> put_int_param;
extern std::function<ResCode(Task_t,const char*,const char*,int[1])> put_param_str;
extern std::function<std::int32_t(Task_t)> get_task_name_len;
extern std::function<std::int32_t(Task_t)> get_obj_name_len;
extern std::function<void(Task_t,std::int32_t,char*)> get_task_name;
extern std::function<void(Task_t,std::int32_t,char*)> get_obj_name;
extern std::function<ResCode(Task_t,const char*)> put_task_name;
extern std::function<ResCode(Task_t,const char*)> put_obj_name;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t[1])> get_var_name_len;
extern std::function<std::int32_t(Task_t,std::int32_t)> get_var_name_len2;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t[1])> get_barvar_name_len;
extern std::function<std::int32_t(Task_t,std::int32_t)> get_barvar_name_len2;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,char*)> get_var_name;
extern std::function<ResCode(Task_t,std::int32_t,std::int32_t,char*)> get_barvar_name;
extern std::function<ResCode(Task_t,std::int32_t,const char*)> put_var_name;
extern std::function<ResCode(Task_t,std::int32_t,const char*)> put_barvar_name;
extern std::function<ResCode(Task_t,std::int64_t,std::int32_t[1])> get_con_name_len;
extern std::function<ResCode(Task_t,std::int64_t,std::int32_t[1])> get_djc_name_len;
extern std::function<std::int32_t(Task_t,std::int64_t)> get_con_name_len2;
extern std::function<std::int32_t(Task_t,std::int64_t)> get_djc_name_len2;
extern std::function<ResCode(Task_t,std::int64_t,std::int32_t,char*)> get_con_name;
extern std::function<ResCode(Task_t,std::int64_t,std::int32_t,char*)> get_djc_name;
extern std::function<ResCode(Task_t,std::int64_t,const char*)> put_con_name;
extern std::function<ResCode(Task_t,std::int64_t,const char*)> put_djc_name;
extern std::function<ResCode(Task_t,const char*)> write_task_to_file;
extern std::function<ResCode(Task_t,Format,Compression,WriteHandle,WriteFunc)> write_task_to_handle;
extern std::function<ResCode(Task_t,const char*)> write_solution_to_file;
extern std::function<ResCode(Task_t,SolutionFormat,Compression,WriteHandle,WriteFunc)> write_solution_to_handle;
extern std::function<ResCode(Task_t,const char*)> read_from_file;
extern std::function<ResCode(Task_t,Format,Compression,ReadHandle,ReadFunc)> read_from_handle;
extern std::function<ResCode(Task_t,StreamType,const char*,int)> put_stream_file;
extern std::function<ResCode(Task_t,StreamType)> clear_stream_file;
extern std::function<ResCode(Task_t,StreamType,WriteHandle,StreamFunc)> put_stream_callback;
extern std::function<ResCode(Task_t,StreamType)> clear_stream_callback;
extern std::function<ResCode(Task_t,ErrorCallbackHandle,ErrorCallbackFunc)> put_error_callback;
extern std::function<ResCode(Task_t,ErrorCallbackHandle,ErrorCallbackFunc)> put_warning_callback;
extern std::function<ResCode(Task_t)> clear_error_callback;
extern std::function<ResCode(Task_t)> clear_warning_callback;
extern std::function<void()> license_cleanup;
extern std::function<void()> shutdown_global_threadpool;
extern std::function<ResCode(std::int32_t,double,const double*,double*)> axpy;
extern std::function<ResCode(std::int32_t,const double*,const double*,double[1])> dot;
extern std::function<ResCode(int,std::int32_t,std::int32_t,double,const double*,const double*,double,double*)> gemv;
extern std::function<ResCode(int,int,std::int32_t,std::int32_t,std::int32_t,double,const double*,const double*,double,double*)> gemm;
extern std::function<ResCode(int,int,std::int32_t,std::int32_t,double,const double*,double,double*)> syrk;
extern std::function<ResCode(int,std::int32_t,const std::int32_t*,const std::int32_t*,const double*,double*)> sparse_triangular_solve_dense;
extern std::function<ResCode(int,std::int32_t,double*)> potrf;
extern std::function<ResCode(int,std::int32_t,const double*,double*)> syeig;
extern std::function<ResCode(int,std::int32_t,double*,double*)> syevd;
extern std::function<ResCode(std::int32_t,int,double,std::int32_t,const std::int32_t*,const std::int32_t*,const double*,std::int32_t*,double*,AllocFunc,AllocHandle,std::int32_t*,std::int32_t**,double**)> compute_sparse_cholesky;
extern std::function<ResCode(int,double,std::int32_t,std::int64_t,const Task_t*,TrmCode*,ResCode*)> optimize_batch;
extern std::function<ResCode(Feature)> check_out_license;
extern std::function<ResCode(Feature)> check_in_license;
extern std::function<ResCode()> check_in_all;
extern std::function<ResCode(int)> echo_intro;
extern std::function<void(std::int32_t[1],std::int32_t[1],std::int32_t[1])> get_version;
extern std::function<ResCode(int)> put_license_debug;
extern std::function<ResCode(NULLABLE const std::int32_t[21])> put_license_code;
extern std::function<ResCode(std::int32_t)> put_license_wait;
extern std::function<ResCode(NULLABLE const char*)> put_license_path;

int library_initialized();
int initialize_library();

} // namespace operations_research::MSK12

#endif
