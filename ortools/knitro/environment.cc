// INSERT HEADER

#include "ortools/knitro/environment.h"
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/mutex.h"
#include "ortools/base/dynamic_library.h"
#include "ortools/base/logging.h"

namespace operations_research {

// 'define' section
std::function<int KNITRO_API(const int length, char * const release)> KN_get_release = nullptr;
std::function<int KNITRO_API(KN_context_ptr * kc)> KN_new = nullptr;
std::function<int KNITRO_API(KN_context_ptr * kc)> KN_free = nullptr;
std::function<int KNITRO_API(LM_context_ptr * lmc)> KN_checkout_license = nullptr;
std::function<int KNITRO_API(LM_context_ptr lmc, KN_context_ptr * kc)> KN_new_lm = nullptr;
std::function<int KNITRO_API(LM_context_ptr * lmc)> KN_release_license = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc)> KN_reset_params_to_defaults = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, const char * const filename)> KN_load_param_file = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, const char * const filename)> KN_load_tuner_file = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, const char * const filename)> KN_save_param_file = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, const char * const name, const int value)> KN_set_int_param_by_name = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, const char * const name, const char * const value)> KN_set_char_param_by_name = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, const char * const name, const double value)> KN_set_double_param_by_name = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, const char * const name, const double value)> KN_set_param_by_name = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, const int param_id, const int value)> KN_set_int_param = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, const int param_id, const char * const value)> KN_set_char_param = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, const int param_id, const double value)> KN_set_double_param = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, const char * const name, int * const value)> KN_get_int_param_by_name = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, const char * const name, double * const value)> KN_get_double_param_by_name = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, const int param_id, int * const value)> KN_get_int_param = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, const int param_id, double * const value)> KN_get_double_param = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const int param_id, char * const param_name, const size_t output_size)> KN_get_param_name = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const int param_id, char * const description, const size_t output_size)> KN_get_param_doc = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const int param_id, int * const param_type)> KN_get_param_type = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const int param_id, int * const num_param_values)> KN_get_num_param_values = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const int param_id, const int value_id, char * const param_value_string, const size_t output_size)> KN_get_param_value_doc = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const char * const name, int * const param_id)> KN_get_param_id = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nV, KNINT * const indexVars)> KN_add_vars = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, KNINT * const indexVar)> KN_add_var = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nC, KNINT * const indexCons)> KN_add_cons = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, KNINT * const indexCon)> KN_add_con = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nR, KNINT * const indexRsds)> KN_add_rsds = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, KNINT * const indexRsd)> KN_add_rsd = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, const double * const xLoBnds)> KN_set_var_lobnds = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double * const xLoBnds)> KN_set_var_lobnds_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexVar, const double xLoBnd)> KN_set_var_lobnd = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, const double * const xUpBnds)> KN_set_var_upbnds = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double * const xUpBnds)> KN_set_var_upbnds_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexVar, const double xUpBnd)> KN_set_var_upbnd = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, const double * const xFxBnds)> KN_set_var_fxbnds = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double * const xFxBnds)> KN_set_var_fxbnds_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexVar, const double xFxBnd)> KN_set_var_fxbnd = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, double * const xLoBnds)> KN_get_var_lobnds = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const xLoBnds)> KN_get_var_lobnds_all = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT indexVar, double * const xLoBnd)> KN_get_var_lobnd = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, double * const xUpBnds)> KN_get_var_upbnds = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const xUpBnds)> KN_get_var_upbnds_all = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT indexVar, double * const xUpBnd)> KN_get_var_upbnd = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, double * const xFxBnds)> KN_get_var_fxbnds = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const xFxBnds)> KN_get_var_fxbnds_all = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT indexVar, double * const xFxBnd)> KN_get_var_fxbnd = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, const int * const xTypes)> KN_set_var_types = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const int * const xTypes)> KN_set_var_types_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexVar, const int xType)> KN_set_var_type = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, int * const xTypes)> KN_get_var_types = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, int * const xTypes)> KN_get_var_types_all = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT indexVar, int * const xType)> KN_get_var_type = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, const int * const xProperties)> KN_set_var_properties = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const int * const xProperties)> KN_set_var_properties_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexVar, const int xProperty)> KN_set_var_property = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons, const double * const cLoBnds)> KN_set_con_lobnds = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double * const cLoBnds)> KN_set_con_lobnds_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexCon, const double cLoBnd)> KN_set_con_lobnd = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons, const double * const cUpBnds)> KN_set_con_upbnds = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double * const cUpBnds)> KN_set_con_upbnds_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexCon, const double cUpBnd)> KN_set_con_upbnd = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons, const double * const cEqBnds)> KN_set_con_eqbnds = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double * const cEqBnds)> KN_set_con_eqbnds_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexCon, const double cEqBnd)> KN_set_con_eqbnd = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons, double * const cLoBnds)> KN_get_con_lobnds = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const cLoBnds)> KN_get_con_lobnds_all = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT indexCon, double * const cLoBnd)> KN_get_con_lobnd = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons, double * const cUpBnds)> KN_get_con_upbnds = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const cUpBnds)> KN_get_con_upbnds_all = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT indexCon, double * const cUpBnd)> KN_get_con_upbnd = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons, double * const cEqBnds)> KN_get_con_eqbnds = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const cEqBnds)> KN_get_con_eqbnds_all = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT indexCon, double * const cEqBnd)> KN_get_con_eqbnd = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const int objProperty)> KN_set_obj_property = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons, const int * const cProperties)> KN_set_con_properties = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const int * const cProperties)> KN_set_con_properties_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexCon, const int cProperty)> KN_set_con_property = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const int objGoal)> KN_set_obj_goal = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, const double * const xInitVals)> KN_set_var_primal_init_values = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double * const xInitVals)> KN_set_var_primal_init_values_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexVar, const double xInitVal)> KN_set_var_primal_init_value = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, const double * const lambdaInitVals)> KN_set_var_dual_init_values = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double * const lambdaInitVals)> KN_set_var_dual_init_values_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexVar, const double lambdaInitVal)> KN_set_var_dual_init_value = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons, const double * const lambdaInitVals)> KN_set_con_dual_init_values = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double * const lambdaInitVals)> KN_set_con_dual_init_values_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexCon, const double lambdaInitVal)> KN_set_con_dual_init_value = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double constant)> KN_add_obj_constant = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc)> KN_del_obj_constant = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double constant)> KN_chg_obj_constant = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons, const double * const constants)> KN_add_con_constants = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double * const constants)> KN_add_con_constants_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexCon, const double constant)> KN_add_con_constant = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons)> KN_del_con_constants = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc)> KN_del_con_constants_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexCon)> KN_del_con_constant = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons, const double * const constants)> KN_chg_con_constants = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double * const constants)> KN_chg_con_constants_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexCon, const double constant)> KN_chg_con_constant = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nR, const KNINT * const indexRsds, const double * const constants)> KN_add_rsd_constants = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double * const constants)> KN_add_rsd_constants_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexRsd, const double constant)> KN_add_rsd_constant = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nnz, const KNINT * const indexVars, const double * const coefs)> KN_add_obj_linear_struct = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexVar, const double coef)> KN_add_obj_linear_term = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nnz, const KNINT * const indexVars)> KN_del_obj_linear_struct = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexVar)> KN_del_obj_linear_term = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nnz, const KNINT * const indexVars, const double * const coefs)> KN_chg_obj_linear_struct = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexVar, const double coef)> KN_chg_obj_linear_term = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNLONG nnz, const KNINT * const indexCons, const KNINT * const indexVars, const double * const coefs)> KN_add_con_linear_struct = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNLONG nnz, const KNINT indexCon, const KNINT * const indexVars, const double * const coefs)> KN_add_con_linear_struct_one = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexCon, const KNINT indexVar, const double coef)> KN_add_con_linear_term = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNLONG nnz, const KNINT * const indexCons, const KNINT * const indexVars)> KN_del_con_linear_struct = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNLONG nnz, const KNINT indexCon, const KNINT * const indexVars)> KN_del_con_linear_struct_one = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexCon, const KNINT indexVar)> KN_del_con_linear_term = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNLONG nnz, const KNINT * const indexCons, const KNINT * const indexVars, const double * const coefs)> KN_chg_con_linear_struct = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNLONG nnz, const KNINT indexCon, const KNINT * const indexVars, const double * const coefs)> KN_chg_con_linear_struct_one = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexCon, const KNINT indexVar, const double coef)> KN_chg_con_linear_term = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNLONG nnz, const KNINT * const indexRsds, const KNINT * const indexVars, const double * const coefs)> KN_add_rsd_linear_struct = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNLONG nnz, const KNINT indexRsd, const KNINT * const indexVars, const double * const coefs)> KN_add_rsd_linear_struct_one = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexRsd, const KNINT indexVar, const double coef)> KN_add_rsd_linear_term = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNLONG nnz, const KNINT * const indexVars1, const KNINT * const indexVars2, const double * const coefs)> KN_add_obj_quadratic_struct = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexVar1, const KNINT indexVar2, const double coef)> KN_add_obj_quadratic_term = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNLONG nnz, const KNINT * const indexCons, const KNINT * const indexVars1, const KNINT * const indexVars2, const double * const coefs)> KN_add_con_quadratic_struct = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNLONG nnz, const KNINT indexCon, const KNINT * const indexVars1, const KNINT * const indexVars2, const double * const coefs)> KN_add_con_quadratic_struct_one = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexCon, const KNINT indexVar1, const KNINT indexVar2, const double coef)> KN_add_con_quadratic_term = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexCon, const KNINT nCoords, const KNLONG nnz, const KNINT * const indexCoords, const KNINT * const indexVars, const double * const coefs, const double * const constants)> KN_add_con_L2norm = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nCC, const int * const ccTypes, const KNINT * const indexComps1, const KNINT * const indexComps2)> KN_set_compcons = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const char * const filename)> KN_load_mps_file = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const char * filename)> KN_write_mps_file = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNBOOL evalObj, const KNINT nC, const KNINT * const indexCons, KN_eval_callback * const funcCallback, CB_context_ptr * const cb)> KN_add_eval_callback = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, KN_eval_callback * const funcCallback, CB_context_ptr * const cb)> KN_add_eval_callback_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT index, KN_eval_callback * const funcCallback, CB_context_ptr * const cb)> KN_add_eval_callback_one = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nR, const KNINT * const indexRsds, KN_eval_callback * const rsdCallback, CB_context_ptr * const cb)> KN_add_lsq_eval_callback = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, KN_eval_callback * const rsdCallback, CB_context_ptr * const cb)> KN_add_lsq_eval_callback_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexRsd, KN_eval_callback * const rsdCallback, CB_context_ptr * const cb)> KN_add_lsq_eval_callback_one = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, CB_context_ptr cb, const KNINT nV, const KNINT * const objGradIndexVars, const KNLONG nnzJ, const KNINT * const jacIndexCons, const KNINT * const jacIndexVars, KN_eval_callback * const gradCallback)> KN_set_cb_grad = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, CB_context_ptr cb, const KNLONG nnzH, const KNINT * const hessIndexVars1, const KNINT * const hessIndexVars2, KN_eval_callback * const hessCallback)> KN_set_cb_hess = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, CB_context_ptr cb, const KNLONG nnzJ, const KNINT * const jacIndexRsds, const KNINT * const jacIndexVars, KN_eval_callback * const rsdJacCallback)> KN_set_cb_rsd_jac = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, CB_context_ptr cb, void * const userParams)> KN_set_cb_user_params = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, CB_context_ptr cb, const int gradopt)> KN_set_cb_gradopt = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, CB_context_ptr cb, const KNINT nV, const KNINT * const indexVars, const double * const xRelStepSizes)> KN_set_cb_relstepsizes = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, CB_context_ptr cb, const double * const xRelStepSizes)> KN_set_cb_relstepsizes_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, CB_context_ptr cb, const KNINT indexVar, const double xRelStepSize)> KN_set_cb_relstepsize = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const CB_context_ptr cb, KNINT * const nC)> KN_get_cb_number_cons = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const CB_context_ptr cb, KNINT * const nR)> KN_get_cb_number_rsds = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const CB_context_ptr cb, KNINT * const nnz)> KN_get_cb_objgrad_nnz = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const CB_context_ptr cb, KNLONG * const nnz)> KN_get_cb_jacobian_nnz = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const CB_context_ptr cb, KNLONG * const nnz)> KN_get_cb_rsd_jacobian_nnz = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const CB_context_ptr cb, KNLONG * const nnz)> KN_get_cb_hessian_nnz = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, KN_user_callback * const fnPtr, void * const userParams)> KN_set_newpt_callback = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, KN_user_callback * const fnPtr, void * const userParams)> KN_set_mip_node_callback = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, KN_user_callback * const fnPtr, void * const userParams)> KN_set_mip_usercuts_callback = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, KN_user_callback * const fnPtr, void * const userParams)> KN_set_mip_lazyconstraints_callback = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, KN_user_callback * const fnPtr, void * const userParams)> KN_set_ms_process_callback = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, KN_ms_initpt_callback * const fnPtr, void * const userParams)> KN_set_ms_initpt_callback = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, KN_puts * const fnPtr, void * const userParams)> KN_set_puts_callback = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc, KN_linsolver_callback * const fnPtr, void * const userParams)> KN_set_linsolver_callback = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT n, const double * const lobjCoefs, const double * const xLoBnds, const double * const xUpBnds, const KNINT m, const double * const cLoBnds, const double * const cUpBnds, const KNLONG nnzJ, const KNINT * const ljacIndexCons, const KNINT * const ljacIndexVars, const double * const ljacCoefs)> KN_load_lp = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT n, const double * const lobjCoefs, const double * const xLoBnds, const double * const xUpBnds, const KNINT m, const double * const cLoBnds, const double * const cUpBnds, const KNLONG nnzJ, const KNINT * const ljacIndexCons, const KNINT * const ljacIndexVars, const double * const ljacCoefs, const KNLONG nnzH, const KNINT * const qobjIndexVars1, const KNINT * const qobjIndexVars2, const double * const qobjCoefs)> KN_load_qp = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT n, const double * const lobjCoefs, const double * const xLoBnds, const double * const xUpBnds, const KNINT m, const double * const cLoBnds, const double * const cUpBnds, const KNLONG nnzJ, const KNINT * const ljacIndexCons, const KNINT * const ljacIndexVars, const double * const ljacCoefs, const KNLONG nnzH, const KNINT * const qobjIndexVars1, const KNINT * const qobjIndexVars2, const double * const qobjCoefs, const KNLONG nnzQ, const KNINT * const qconIndexCons, const KNINT * const qconIndexVars1, const KNINT * const qconIndexVars2, const double * const qconCoefs)> KN_load_qcqp = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, const double * const xFeasTols)> KN_set_var_feastols = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double * const xFeasTols)> KN_set_var_feastols_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexVar, const double xFeasTol)> KN_set_var_feastol = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons, const double * const cFeasTols)> KN_set_con_feastols = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double * const cFeasTols)> KN_set_con_feastols_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexCon, const double cFeasTol)> KN_set_con_feastol = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nCC, const KNINT * const indexCompCons, const double * const ccFeasTols)> KN_set_compcon_feastols = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double * const ccFeasTols)> KN_set_compcon_feastols_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexCompCon, const double ccFeasTol)> KN_set_compcon_feastol = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, const double * const xScaleFactors, const double * const xScaleCenters)> KN_set_var_scalings = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double * const xScaleFactors, const double * const xScaleCenters)> KN_set_var_scalings_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexVar, const double xScaleFactor, const double xScaleCenter)> KN_set_var_scaling = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons, const double * const cScaleFactors)> KN_set_con_scalings = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double * const cScaleFactors)> KN_set_con_scalings_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexCon, const double cScaleFactor)> KN_set_con_scaling = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nCC, const KNINT * const indexCompCons, const double * const ccScaleFactors)> KN_set_compcon_scalings = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double * const ccScaleFactors)> KN_set_compcon_scalings_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexCompCons, const double ccScaleFactor)> KN_set_compcon_scaling = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double objScaleFactor)> KN_set_obj_scaling = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, char * const xNames[])> KN_set_var_names = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, char * const xNames[])> KN_set_var_names_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexVars, char * const xName)> KN_set_var_name = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons, char * const cNames[])> KN_set_con_names = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, char * const cNames[])> KN_set_con_names_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexCon, char * const cName)> KN_set_con_name = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nCC, const KNINT * const indexCompCons, char * const ccNames[])> KN_set_compcon_names = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, char * const ccNames[])> KN_set_compcon_names_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const int indexCompCon, char * const ccName)> KN_set_compcon_name = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const char * const objName)> KN_set_obj_name = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, const KNINT nBufferSize, char * const xNames[])> KN_get_var_names = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nBufferSize, char * const xNames[])> KN_get_var_names_all = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT indexVars, const KNINT nBufferSize, char * const xName)> KN_get_var_name = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons, const KNINT nBufferSize, char * const cNames[])> KN_get_con_names = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nBufferSize, char * const cNames[])> KN_get_con_names_all = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT indexCons, const KNINT nBufferSize, char * const cName)> KN_get_con_name = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, const int * const xHonorBnds)> KN_set_var_honorbnds = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const int * const xHonorBnds)> KN_set_var_honorbnds_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexVar, const int xHonorBnd)> KN_set_var_honorbnd = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons, const int * const cHonorBnds)> KN_set_con_honorbnds = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const int * const cHonorBnds)> KN_set_con_honorbnds_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexCon, const int cHonorBnd)> KN_set_con_honorbnd = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, const double * const xInitVals)> KN_set_mip_var_primal_init_values = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const double * const xInitVals)> KN_set_mip_var_primal_init_values_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexVar, const double xInitVal)> KN_set_mip_var_primal_init_value = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, const int * const xPriorities)> KN_set_mip_branching_priorities = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const int * const xPriorities)> KN_set_mip_branching_priorities_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexVar, const int xPriority)> KN_set_mip_branching_priority = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, const int * const xStrategies)> KN_set_mip_intvar_strategies = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const int * const xStrategies)> KN_set_mip_intvar_strategies_all = nullptr;
std::function<int KNITRO_API( KN_context_ptr kc, const KNINT indexVar, const int xStrategy)> KN_set_mip_intvar_strategy = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc)> KN_solve = nullptr;
std::function<int KNITRO_API(KN_context_ptr kc)> KN_update = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, KNINT * const nV)> KN_get_number_vars = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, KNINT * const nC)> KN_get_number_cons = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, KNINT * const nCC)> KN_get_number_compcons = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, KNINT * const nR)> KN_get_number_rsds = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, int * const numFCevals)> KN_get_number_FC_evals = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, int * const numGAevals)> KN_get_number_GA_evals = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, int * const numHevals)> KN_get_number_H_evals = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, int * const numHVevals)> KN_get_number_HV_evals = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const time)> KN_get_solve_time_cpu = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const time)> KN_get_solve_time_real = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, int * const status, double * const obj, double * const x, double * const lambda)> KN_get_solution = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const obj)> KN_get_obj_value = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, int * const objType)> KN_get_obj_type = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, double * const x)> KN_get_var_primal_values = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const x)> KN_get_var_primal_values_all = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT indexVar, double * const x)> KN_get_var_primal_value = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, double * const lambda)> KN_get_var_dual_values = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const lambda)> KN_get_var_dual_values_all = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT indexVar, double * const lambda)> KN_get_var_dual_value = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons, double * const lambda)> KN_get_con_dual_values = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const lambda)> KN_get_con_dual_values_all = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT indexCons, double * const lambda)> KN_get_con_dual_value = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons, double * const c)> KN_get_con_values = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const c)> KN_get_con_values_all = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT indexCon, double * const c)> KN_get_con_value = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons, int * const cTypes)> KN_get_con_types = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, int * const cTypes)> KN_get_con_types_all = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT indexCon, int * const cType)> KN_get_con_type = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nR, const KNINT * const indexRsds, double * const r)> KN_get_rsd_values = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const r)> KN_get_rsd_values_all = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT indexRsd, double * const r)> KN_get_rsd_value = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nV, const KNINT * const indexVars, KNINT * const bndInfeas, KNINT * const intInfeas, double * const viols)> KN_get_var_viols = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, KNINT * const bndInfeas, KNINT * const intInfeas, double * const viols)> KN_get_var_viols_all = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT indexVar, KNINT * const bndInfeas, KNINT * const intInfeas, double * const viol)> KN_get_var_viol = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT nC, const KNINT * const indexCons, KNINT * const infeas, double * const viols)> KN_get_con_viols = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, KNINT * const infeas, double * const viols)> KN_get_con_viols_all = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, const KNINT indexCon, KNINT * const infeas, double * const viol)> KN_get_con_viol = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, KNINT * const component, KNINT * const index, KNINT * const error, double * const viol)> KN_get_presolve_error = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, int * const numIters)> KN_get_number_iters = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, int * const numCGiters)> KN_get_number_cg_iters = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const absFeasError)> KN_get_abs_feas_error = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const relFeasError)> KN_get_rel_feas_error = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const absOptError)> KN_get_abs_opt_error = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const relOptError)> KN_get_rel_opt_error = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, KNINT * const nnz)> KN_get_objgrad_nnz = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, KNINT * const indexVars, double * const objGrad)> KN_get_objgrad_values = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const objGrad)> KN_get_objgrad_values_all = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, KNLONG * const nnz)> KN_get_jacobian_nnz = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, KNINT * const indexCons, KNINT * const indexVars, double * const jac)> KN_get_jacobian_values = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, KNINT indexCon, KNINT * const nnz)> KN_get_jacobian_nnz_one = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, KNINT indexCon, KNINT * const indexVars, double * const jac)> KN_get_jacobian_values_one = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, KNLONG * const nnz)> KN_get_rsd_jacobian_nnz = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, KNINT * const indexRsds, KNINT * const indexVars, double * const rsdJac)> KN_get_rsd_jacobian_values = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, KNLONG * const nnz)> KN_get_hessian_nnz = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, KNINT * const indexVars1, KNINT * const indexVars2, double * const hess)> KN_get_hessian_values = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, int * const numNodes)> KN_get_mip_number_nodes = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, int * const numSolves)> KN_get_mip_number_solves = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const absGap)> KN_get_mip_abs_gap = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const relGap)> KN_get_mip_rel_gap = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const incumbentObj)> KN_get_mip_incumbent_obj = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const relaxBound)> KN_get_mip_relaxation_bnd = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const lastNodeObj)> KN_get_mip_lastnode_obj = nullptr;
std::function<int KNITRO_API(const KN_context_ptr kc, double * const x)> KN_get_mip_incumbent_x = nullptr;


//auto generated
void LoadKnitroFunctions(DynamicLibrary* knitro_dynamic_library) {
	knitro_dynamic_library->GetFunction(&KN_get_release, "KN_get_release");
	knitro_dynamic_library->GetFunction(&KN_new, "KN_new");
	knitro_dynamic_library->GetFunction(&KN_free, "KN_free");
	knitro_dynamic_library->GetFunction(&KN_checkout_license, "KN_checkout_license");
	knitro_dynamic_library->GetFunction(&KN_new_lm, "KN_new_lm");
	knitro_dynamic_library->GetFunction(&KN_release_license, "KN_release_license");
	knitro_dynamic_library->GetFunction(&KN_reset_params_to_defaults, "KN_reset_params_to_defaults");
	knitro_dynamic_library->GetFunction(&KN_load_param_file, "KN_load_param_file");
	knitro_dynamic_library->GetFunction(&KN_load_tuner_file, "KN_load_tuner_file");
	knitro_dynamic_library->GetFunction(&KN_save_param_file, "KN_save_param_file");
	knitro_dynamic_library->GetFunction(&KN_set_int_param_by_name, "KN_set_int_param_by_name");
	knitro_dynamic_library->GetFunction(&KN_set_char_param_by_name, "KN_set_char_param_by_name");
	knitro_dynamic_library->GetFunction(&KN_set_double_param_by_name, "KN_set_double_param_by_name");
	knitro_dynamic_library->GetFunction(&KN_set_param_by_name, "KN_set_param_by_name");
	knitro_dynamic_library->GetFunction(&KN_set_int_param, "KN_set_int_param");
	knitro_dynamic_library->GetFunction(&KN_set_char_param, "KN_set_char_param");
	knitro_dynamic_library->GetFunction(&KN_set_double_param, "KN_set_double_param");
	knitro_dynamic_library->GetFunction(&KN_get_int_param_by_name, "KN_get_int_param_by_name");
	knitro_dynamic_library->GetFunction(&KN_get_double_param_by_name, "KN_get_double_param_by_name");
	knitro_dynamic_library->GetFunction(&KN_get_int_param, "KN_get_int_param");
	knitro_dynamic_library->GetFunction(&KN_get_double_param, "KN_get_double_param");
	knitro_dynamic_library->GetFunction(&KN_get_param_name, "KN_get_param_name");
	knitro_dynamic_library->GetFunction(&KN_get_param_doc, "KN_get_param_doc");
	knitro_dynamic_library->GetFunction(&KN_get_param_type, "KN_get_param_type");
	knitro_dynamic_library->GetFunction(&KN_get_num_param_values, "KN_get_num_param_values");
	knitro_dynamic_library->GetFunction(&KN_get_param_value_doc, "KN_get_param_value_doc");
	knitro_dynamic_library->GetFunction(&KN_get_param_id, "KN_get_param_id");
	knitro_dynamic_library->GetFunction(&KN_add_vars, "KN_add_vars");
	knitro_dynamic_library->GetFunction(&KN_add_var, "KN_add_var");
	knitro_dynamic_library->GetFunction(&KN_add_cons, "KN_add_cons");
	knitro_dynamic_library->GetFunction(&KN_add_con, "KN_add_con");
	knitro_dynamic_library->GetFunction(&KN_add_rsds, "KN_add_rsds");
	knitro_dynamic_library->GetFunction(&KN_add_rsd, "KN_add_rsd");
	knitro_dynamic_library->GetFunction(&KN_set_var_lobnds, "KN_set_var_lobnds");
	knitro_dynamic_library->GetFunction(&KN_set_var_lobnds_all, "KN_set_var_lobnds_all");
	knitro_dynamic_library->GetFunction(&KN_set_var_lobnd, "KN_set_var_lobnd");
	knitro_dynamic_library->GetFunction(&KN_set_var_upbnds, "KN_set_var_upbnds");
	knitro_dynamic_library->GetFunction(&KN_set_var_upbnds_all, "KN_set_var_upbnds_all");
	knitro_dynamic_library->GetFunction(&KN_set_var_upbnd, "KN_set_var_upbnd");
	knitro_dynamic_library->GetFunction(&KN_set_var_fxbnds, "KN_set_var_fxbnds");
	knitro_dynamic_library->GetFunction(&KN_set_var_fxbnds_all, "KN_set_var_fxbnds_all");
	knitro_dynamic_library->GetFunction(&KN_set_var_fxbnd, "KN_set_var_fxbnd");
	knitro_dynamic_library->GetFunction(&KN_get_var_lobnds, "KN_get_var_lobnds");
	knitro_dynamic_library->GetFunction(&KN_get_var_lobnds_all, "KN_get_var_lobnds_all");
	knitro_dynamic_library->GetFunction(&KN_get_var_lobnd, "KN_get_var_lobnd");
	knitro_dynamic_library->GetFunction(&KN_get_var_upbnds, "KN_get_var_upbnds");
	knitro_dynamic_library->GetFunction(&KN_get_var_upbnds_all, "KN_get_var_upbnds_all");
	knitro_dynamic_library->GetFunction(&KN_get_var_upbnd, "KN_get_var_upbnd");
	knitro_dynamic_library->GetFunction(&KN_get_var_fxbnds, "KN_get_var_fxbnds");
	knitro_dynamic_library->GetFunction(&KN_get_var_fxbnds_all, "KN_get_var_fxbnds_all");
	knitro_dynamic_library->GetFunction(&KN_get_var_fxbnd, "KN_get_var_fxbnd");
	knitro_dynamic_library->GetFunction(&KN_set_var_types, "KN_set_var_types");
	knitro_dynamic_library->GetFunction(&KN_set_var_types_all, "KN_set_var_types_all");
	knitro_dynamic_library->GetFunction(&KN_set_var_type, "KN_set_var_type");
	knitro_dynamic_library->GetFunction(&KN_get_var_types, "KN_get_var_types");
	knitro_dynamic_library->GetFunction(&KN_get_var_types_all, "KN_get_var_types_all");
	knitro_dynamic_library->GetFunction(&KN_get_var_type, "KN_get_var_type");
	knitro_dynamic_library->GetFunction(&KN_set_var_properties, "KN_set_var_properties");
	knitro_dynamic_library->GetFunction(&KN_set_var_properties_all, "KN_set_var_properties_all");
	knitro_dynamic_library->GetFunction(&KN_set_var_property, "KN_set_var_property");
	knitro_dynamic_library->GetFunction(&KN_set_con_lobnds, "KN_set_con_lobnds");
	knitro_dynamic_library->GetFunction(&KN_set_con_lobnds_all, "KN_set_con_lobnds_all");
	knitro_dynamic_library->GetFunction(&KN_set_con_lobnd, "KN_set_con_lobnd");
	knitro_dynamic_library->GetFunction(&KN_set_con_upbnds, "KN_set_con_upbnds");
	knitro_dynamic_library->GetFunction(&KN_set_con_upbnds_all, "KN_set_con_upbnds_all");
	knitro_dynamic_library->GetFunction(&KN_set_con_upbnd, "KN_set_con_upbnd");
	knitro_dynamic_library->GetFunction(&KN_set_con_eqbnds, "KN_set_con_eqbnds");
	knitro_dynamic_library->GetFunction(&KN_set_con_eqbnds_all, "KN_set_con_eqbnds_all");
	knitro_dynamic_library->GetFunction(&KN_set_con_eqbnd, "KN_set_con_eqbnd");
	knitro_dynamic_library->GetFunction(&KN_get_con_lobnds, "KN_get_con_lobnds");
	knitro_dynamic_library->GetFunction(&KN_get_con_lobnds_all, "KN_get_con_lobnds_all");
	knitro_dynamic_library->GetFunction(&KN_get_con_lobnd, "KN_get_con_lobnd");
	knitro_dynamic_library->GetFunction(&KN_get_con_upbnds, "KN_get_con_upbnds");
	knitro_dynamic_library->GetFunction(&KN_get_con_upbnds_all, "KN_get_con_upbnds_all");
	knitro_dynamic_library->GetFunction(&KN_get_con_upbnd, "KN_get_con_upbnd");
	knitro_dynamic_library->GetFunction(&KN_get_con_eqbnds, "KN_get_con_eqbnds");
	knitro_dynamic_library->GetFunction(&KN_get_con_eqbnds_all, "KN_get_con_eqbnds_all");
	knitro_dynamic_library->GetFunction(&KN_get_con_eqbnd, "KN_get_con_eqbnd");
	knitro_dynamic_library->GetFunction(&KN_set_obj_property, "KN_set_obj_property");
	knitro_dynamic_library->GetFunction(&KN_set_con_properties, "KN_set_con_properties");
	knitro_dynamic_library->GetFunction(&KN_set_con_properties_all, "KN_set_con_properties_all");
	knitro_dynamic_library->GetFunction(&KN_set_con_property, "KN_set_con_property");
	knitro_dynamic_library->GetFunction(&KN_set_obj_goal, "KN_set_obj_goal");
	knitro_dynamic_library->GetFunction(&KN_set_var_primal_init_values, "KN_set_var_primal_init_values");
	knitro_dynamic_library->GetFunction(&KN_set_var_primal_init_values_all, "KN_set_var_primal_init_values_all");
	knitro_dynamic_library->GetFunction(&KN_set_var_primal_init_value, "KN_set_var_primal_init_value");
	knitro_dynamic_library->GetFunction(&KN_set_var_dual_init_values, "KN_set_var_dual_init_values");
	knitro_dynamic_library->GetFunction(&KN_set_var_dual_init_values_all, "KN_set_var_dual_init_values_all");
	knitro_dynamic_library->GetFunction(&KN_set_var_dual_init_value, "KN_set_var_dual_init_value");
	knitro_dynamic_library->GetFunction(&KN_set_con_dual_init_values, "KN_set_con_dual_init_values");
	knitro_dynamic_library->GetFunction(&KN_set_con_dual_init_values_all, "KN_set_con_dual_init_values_all");
	knitro_dynamic_library->GetFunction(&KN_set_con_dual_init_value, "KN_set_con_dual_init_value");
	knitro_dynamic_library->GetFunction(&KN_add_obj_constant, "KN_add_obj_constant");
	knitro_dynamic_library->GetFunction(&KN_del_obj_constant, "KN_del_obj_constant");
	knitro_dynamic_library->GetFunction(&KN_chg_obj_constant, "KN_chg_obj_constant");
	knitro_dynamic_library->GetFunction(&KN_add_con_constants, "KN_add_con_constants");
	knitro_dynamic_library->GetFunction(&KN_add_con_constants_all, "KN_add_con_constants_all");
	knitro_dynamic_library->GetFunction(&KN_add_con_constant, "KN_add_con_constant");
	knitro_dynamic_library->GetFunction(&KN_del_con_constants, "KN_del_con_constants");
	knitro_dynamic_library->GetFunction(&KN_del_con_constants_all, "KN_del_con_constants_all");
	knitro_dynamic_library->GetFunction(&KN_del_con_constant, "KN_del_con_constant");
	knitro_dynamic_library->GetFunction(&KN_chg_con_constants, "KN_chg_con_constants");
	knitro_dynamic_library->GetFunction(&KN_chg_con_constants_all, "KN_chg_con_constants_all");
	knitro_dynamic_library->GetFunction(&KN_chg_con_constant, "KN_chg_con_constant");
	knitro_dynamic_library->GetFunction(&KN_add_rsd_constants, "KN_add_rsd_constants");
	knitro_dynamic_library->GetFunction(&KN_add_rsd_constants_all, "KN_add_rsd_constants_all");
	knitro_dynamic_library->GetFunction(&KN_add_rsd_constant, "KN_add_rsd_constant");
	knitro_dynamic_library->GetFunction(&KN_add_obj_linear_struct, "KN_add_obj_linear_struct");
	knitro_dynamic_library->GetFunction(&KN_add_obj_linear_term, "KN_add_obj_linear_term");
	knitro_dynamic_library->GetFunction(&KN_del_obj_linear_struct, "KN_del_obj_linear_struct");
	knitro_dynamic_library->GetFunction(&KN_del_obj_linear_term, "KN_del_obj_linear_term");
	knitro_dynamic_library->GetFunction(&KN_chg_obj_linear_struct, "KN_chg_obj_linear_struct");
	knitro_dynamic_library->GetFunction(&KN_chg_obj_linear_term, "KN_chg_obj_linear_term");
	knitro_dynamic_library->GetFunction(&KN_add_con_linear_struct, "KN_add_con_linear_struct");
	knitro_dynamic_library->GetFunction(&KN_add_con_linear_struct_one, "KN_add_con_linear_struct_one");
	knitro_dynamic_library->GetFunction(&KN_add_con_linear_term, "KN_add_con_linear_term");
	knitro_dynamic_library->GetFunction(&KN_del_con_linear_struct, "KN_del_con_linear_struct");
	knitro_dynamic_library->GetFunction(&KN_del_con_linear_struct_one, "KN_del_con_linear_struct_one");
	knitro_dynamic_library->GetFunction(&KN_del_con_linear_term, "KN_del_con_linear_term");
	knitro_dynamic_library->GetFunction(&KN_chg_con_linear_struct, "KN_chg_con_linear_struct");
	knitro_dynamic_library->GetFunction(&KN_chg_con_linear_struct_one, "KN_chg_con_linear_struct_one");
	knitro_dynamic_library->GetFunction(&KN_chg_con_linear_term, "KN_chg_con_linear_term");
	knitro_dynamic_library->GetFunction(&KN_add_rsd_linear_struct, "KN_add_rsd_linear_struct");
	knitro_dynamic_library->GetFunction(&KN_add_rsd_linear_struct_one, "KN_add_rsd_linear_struct_one");
	knitro_dynamic_library->GetFunction(&KN_add_rsd_linear_term, "KN_add_rsd_linear_term");
	knitro_dynamic_library->GetFunction(&KN_add_obj_quadratic_struct, "KN_add_obj_quadratic_struct");
	knitro_dynamic_library->GetFunction(&KN_add_obj_quadratic_term, "KN_add_obj_quadratic_term");
	knitro_dynamic_library->GetFunction(&KN_add_con_quadratic_struct, "KN_add_con_quadratic_struct");
	knitro_dynamic_library->GetFunction(&KN_add_con_quadratic_struct_one, "KN_add_con_quadratic_struct_one");
	knitro_dynamic_library->GetFunction(&KN_add_con_quadratic_term, "KN_add_con_quadratic_term");
	knitro_dynamic_library->GetFunction(&KN_add_con_L2norm, "KN_add_con_L2norm");
	knitro_dynamic_library->GetFunction(&KN_set_compcons, "KN_set_compcons");
	knitro_dynamic_library->GetFunction(&KN_load_mps_file, "KN_load_mps_file");
	knitro_dynamic_library->GetFunction(&KN_write_mps_file, "KN_write_mps_file");
	knitro_dynamic_library->GetFunction(&KN_add_eval_callback, "KN_add_eval_callback");
	knitro_dynamic_library->GetFunction(&KN_add_eval_callback_all, "KN_add_eval_callback_all");
	knitro_dynamic_library->GetFunction(&KN_add_eval_callback_one, "KN_add_eval_callback_one");
	knitro_dynamic_library->GetFunction(&KN_add_lsq_eval_callback, "KN_add_lsq_eval_callback");
	knitro_dynamic_library->GetFunction(&KN_add_lsq_eval_callback_all, "KN_add_lsq_eval_callback_all");
	knitro_dynamic_library->GetFunction(&KN_add_lsq_eval_callback_one, "KN_add_lsq_eval_callback_one");
	knitro_dynamic_library->GetFunction(&KN_set_cb_grad, "KN_set_cb_grad");
	knitro_dynamic_library->GetFunction(&KN_set_cb_hess, "KN_set_cb_hess");
	knitro_dynamic_library->GetFunction(&KN_set_cb_rsd_jac, "KN_set_cb_rsd_jac");
	knitro_dynamic_library->GetFunction(&KN_set_cb_user_params, "KN_set_cb_user_params");
	knitro_dynamic_library->GetFunction(&KN_set_cb_gradopt, "KN_set_cb_gradopt");
	knitro_dynamic_library->GetFunction(&KN_set_cb_relstepsizes, "KN_set_cb_relstepsizes");
	knitro_dynamic_library->GetFunction(&KN_set_cb_relstepsizes_all, "KN_set_cb_relstepsizes_all");
	knitro_dynamic_library->GetFunction(&KN_set_cb_relstepsize, "KN_set_cb_relstepsize");
	knitro_dynamic_library->GetFunction(&KN_get_cb_number_cons, "KN_get_cb_number_cons");
	knitro_dynamic_library->GetFunction(&KN_get_cb_number_rsds, "KN_get_cb_number_rsds");
	knitro_dynamic_library->GetFunction(&KN_get_cb_objgrad_nnz, "KN_get_cb_objgrad_nnz");
	knitro_dynamic_library->GetFunction(&KN_get_cb_jacobian_nnz, "KN_get_cb_jacobian_nnz");
	knitro_dynamic_library->GetFunction(&KN_get_cb_rsd_jacobian_nnz, "KN_get_cb_rsd_jacobian_nnz");
	knitro_dynamic_library->GetFunction(&KN_get_cb_hessian_nnz, "KN_get_cb_hessian_nnz");
	knitro_dynamic_library->GetFunction(&KN_set_newpt_callback, "KN_set_newpt_callback");
	knitro_dynamic_library->GetFunction(&KN_set_mip_node_callback, "KN_set_mip_node_callback");
	knitro_dynamic_library->GetFunction(&KN_set_mip_usercuts_callback, "KN_set_mip_usercuts_callback");
	knitro_dynamic_library->GetFunction(&KN_set_mip_lazyconstraints_callback, "KN_set_mip_lazyconstraints_callback");
	knitro_dynamic_library->GetFunction(&KN_set_ms_process_callback, "KN_set_ms_process_callback");
	knitro_dynamic_library->GetFunction(&KN_set_ms_initpt_callback, "KN_set_ms_initpt_callback");
	knitro_dynamic_library->GetFunction(&KN_set_puts_callback, "KN_set_puts_callback");
	knitro_dynamic_library->GetFunction(&KN_set_linsolver_callback, "KN_set_linsolver_callback");
	knitro_dynamic_library->GetFunction(&KN_load_lp, "KN_load_lp");
	knitro_dynamic_library->GetFunction(&KN_load_qp, "KN_load_qp");
	knitro_dynamic_library->GetFunction(&KN_load_qcqp, "KN_load_qcqp");
	knitro_dynamic_library->GetFunction(&KN_set_var_feastols, "KN_set_var_feastols");
	knitro_dynamic_library->GetFunction(&KN_set_var_feastols_all, "KN_set_var_feastols_all");
	knitro_dynamic_library->GetFunction(&KN_set_var_feastol, "KN_set_var_feastol");
	knitro_dynamic_library->GetFunction(&KN_set_con_feastols, "KN_set_con_feastols");
	knitro_dynamic_library->GetFunction(&KN_set_con_feastols_all, "KN_set_con_feastols_all");
	knitro_dynamic_library->GetFunction(&KN_set_con_feastol, "KN_set_con_feastol");
	knitro_dynamic_library->GetFunction(&KN_set_compcon_feastols, "KN_set_compcon_feastols");
	knitro_dynamic_library->GetFunction(&KN_set_compcon_feastols_all, "KN_set_compcon_feastols_all");
	knitro_dynamic_library->GetFunction(&KN_set_compcon_feastol, "KN_set_compcon_feastol");
	knitro_dynamic_library->GetFunction(&KN_set_var_scalings, "KN_set_var_scalings");
	knitro_dynamic_library->GetFunction(&KN_set_var_scalings_all, "KN_set_var_scalings_all");
	knitro_dynamic_library->GetFunction(&KN_set_var_scaling, "KN_set_var_scaling");
	knitro_dynamic_library->GetFunction(&KN_set_con_scalings, "KN_set_con_scalings");
	knitro_dynamic_library->GetFunction(&KN_set_con_scalings_all, "KN_set_con_scalings_all");
	knitro_dynamic_library->GetFunction(&KN_set_con_scaling, "KN_set_con_scaling");
	knitro_dynamic_library->GetFunction(&KN_set_compcon_scalings, "KN_set_compcon_scalings");
	knitro_dynamic_library->GetFunction(&KN_set_compcon_scalings_all, "KN_set_compcon_scalings_all");
	knitro_dynamic_library->GetFunction(&KN_set_compcon_scaling, "KN_set_compcon_scaling");
	knitro_dynamic_library->GetFunction(&KN_set_obj_scaling, "KN_set_obj_scaling");
	knitro_dynamic_library->GetFunction(&KN_set_var_names, "KN_set_var_names");
	knitro_dynamic_library->GetFunction(&KN_set_var_names_all, "KN_set_var_names_all");
	knitro_dynamic_library->GetFunction(&KN_set_var_name, "KN_set_var_name");
	knitro_dynamic_library->GetFunction(&KN_set_con_names, "KN_set_con_names");
	knitro_dynamic_library->GetFunction(&KN_set_con_names_all, "KN_set_con_names_all");
	knitro_dynamic_library->GetFunction(&KN_set_con_name, "KN_set_con_name");
	knitro_dynamic_library->GetFunction(&KN_set_compcon_names, "KN_set_compcon_names");
	knitro_dynamic_library->GetFunction(&KN_set_compcon_names_all, "KN_set_compcon_names_all");
	knitro_dynamic_library->GetFunction(&KN_set_compcon_name, "KN_set_compcon_name");
	knitro_dynamic_library->GetFunction(&KN_set_obj_name, "KN_set_obj_name");
	knitro_dynamic_library->GetFunction(&KN_get_var_names, "KN_get_var_names");
	knitro_dynamic_library->GetFunction(&KN_get_var_names_all, "KN_get_var_names_all");
	knitro_dynamic_library->GetFunction(&KN_get_var_name, "KN_get_var_name");
	knitro_dynamic_library->GetFunction(&KN_get_con_names, "KN_get_con_names");
	knitro_dynamic_library->GetFunction(&KN_get_con_names_all, "KN_get_con_names_all");
	knitro_dynamic_library->GetFunction(&KN_get_con_name, "KN_get_con_name");
	knitro_dynamic_library->GetFunction(&KN_set_var_honorbnds, "KN_set_var_honorbnds");
	knitro_dynamic_library->GetFunction(&KN_set_var_honorbnds_all, "KN_set_var_honorbnds_all");
	knitro_dynamic_library->GetFunction(&KN_set_var_honorbnd, "KN_set_var_honorbnd");
	knitro_dynamic_library->GetFunction(&KN_set_con_honorbnds, "KN_set_con_honorbnds");
	knitro_dynamic_library->GetFunction(&KN_set_con_honorbnds_all, "KN_set_con_honorbnds_all");
	knitro_dynamic_library->GetFunction(&KN_set_con_honorbnd, "KN_set_con_honorbnd");
	knitro_dynamic_library->GetFunction(&KN_set_mip_var_primal_init_values, "KN_set_mip_var_primal_init_values");
	knitro_dynamic_library->GetFunction(&KN_set_mip_var_primal_init_values_all, "KN_set_mip_var_primal_init_values_all");
	knitro_dynamic_library->GetFunction(&KN_set_mip_var_primal_init_value, "KN_set_mip_var_primal_init_value");
	knitro_dynamic_library->GetFunction(&KN_set_mip_branching_priorities, "KN_set_mip_branching_priorities");
	knitro_dynamic_library->GetFunction(&KN_set_mip_branching_priorities_all, "KN_set_mip_branching_priorities_all");
	knitro_dynamic_library->GetFunction(&KN_set_mip_branching_priority, "KN_set_mip_branching_priority");
	knitro_dynamic_library->GetFunction(&KN_set_mip_intvar_strategies, "KN_set_mip_intvar_strategies");
	knitro_dynamic_library->GetFunction(&KN_set_mip_intvar_strategies_all, "KN_set_mip_intvar_strategies_all");
	knitro_dynamic_library->GetFunction(&KN_set_mip_intvar_strategy, "KN_set_mip_intvar_strategy");
	knitro_dynamic_library->GetFunction(&KN_solve, "KN_solve");
	knitro_dynamic_library->GetFunction(&KN_update, "KN_update");
	knitro_dynamic_library->GetFunction(&KN_get_number_vars, "KN_get_number_vars");
	knitro_dynamic_library->GetFunction(&KN_get_number_cons, "KN_get_number_cons");
	knitro_dynamic_library->GetFunction(&KN_get_number_compcons, "KN_get_number_compcons");
	knitro_dynamic_library->GetFunction(&KN_get_number_rsds, "KN_get_number_rsds");
	knitro_dynamic_library->GetFunction(&KN_get_number_FC_evals, "KN_get_number_FC_evals");
	knitro_dynamic_library->GetFunction(&KN_get_number_GA_evals, "KN_get_number_GA_evals");
	knitro_dynamic_library->GetFunction(&KN_get_number_H_evals, "KN_get_number_H_evals");
	knitro_dynamic_library->GetFunction(&KN_get_number_HV_evals, "KN_get_number_HV_evals");
	knitro_dynamic_library->GetFunction(&KN_get_solve_time_cpu, "KN_get_solve_time_cpu");
	knitro_dynamic_library->GetFunction(&KN_get_solve_time_real, "KN_get_solve_time_real");
	knitro_dynamic_library->GetFunction(&KN_get_solution, "KN_get_solution");
	knitro_dynamic_library->GetFunction(&KN_get_obj_value, "KN_get_obj_value");
	knitro_dynamic_library->GetFunction(&KN_get_obj_type, "KN_get_obj_type");
	knitro_dynamic_library->GetFunction(&KN_get_var_primal_values, "KN_get_var_primal_values");
	knitro_dynamic_library->GetFunction(&KN_get_var_primal_values_all, "KN_get_var_primal_values_all");
	knitro_dynamic_library->GetFunction(&KN_get_var_primal_value, "KN_get_var_primal_value");
	knitro_dynamic_library->GetFunction(&KN_get_var_dual_values, "KN_get_var_dual_values");
	knitro_dynamic_library->GetFunction(&KN_get_var_dual_values_all, "KN_get_var_dual_values_all");
	knitro_dynamic_library->GetFunction(&KN_get_var_dual_value, "KN_get_var_dual_value");
	knitro_dynamic_library->GetFunction(&KN_get_con_dual_values, "KN_get_con_dual_values");
	knitro_dynamic_library->GetFunction(&KN_get_con_dual_values_all, "KN_get_con_dual_values_all");
	knitro_dynamic_library->GetFunction(&KN_get_con_dual_value, "KN_get_con_dual_value");
	knitro_dynamic_library->GetFunction(&KN_get_con_values, "KN_get_con_values");
	knitro_dynamic_library->GetFunction(&KN_get_con_values_all, "KN_get_con_values_all");
	knitro_dynamic_library->GetFunction(&KN_get_con_value, "KN_get_con_value");
	knitro_dynamic_library->GetFunction(&KN_get_con_types, "KN_get_con_types");
	knitro_dynamic_library->GetFunction(&KN_get_con_types_all, "KN_get_con_types_all");
	knitro_dynamic_library->GetFunction(&KN_get_con_type, "KN_get_con_type");
	knitro_dynamic_library->GetFunction(&KN_get_rsd_values, "KN_get_rsd_values");
	knitro_dynamic_library->GetFunction(&KN_get_rsd_values_all, "KN_get_rsd_values_all");
	knitro_dynamic_library->GetFunction(&KN_get_rsd_value, "KN_get_rsd_value");
	knitro_dynamic_library->GetFunction(&KN_get_var_viols, "KN_get_var_viols");
	knitro_dynamic_library->GetFunction(&KN_get_var_viols_all, "KN_get_var_viols_all");
	knitro_dynamic_library->GetFunction(&KN_get_var_viol, "KN_get_var_viol");
	knitro_dynamic_library->GetFunction(&KN_get_con_viols, "KN_get_con_viols");
	knitro_dynamic_library->GetFunction(&KN_get_con_viols_all, "KN_get_con_viols_all");
	knitro_dynamic_library->GetFunction(&KN_get_con_viol, "KN_get_con_viol");
	knitro_dynamic_library->GetFunction(&KN_get_presolve_error, "KN_get_presolve_error");
	knitro_dynamic_library->GetFunction(&KN_get_number_iters, "KN_get_number_iters");
	knitro_dynamic_library->GetFunction(&KN_get_number_cg_iters, "KN_get_number_cg_iters");
	knitro_dynamic_library->GetFunction(&KN_get_abs_feas_error, "KN_get_abs_feas_error");
	knitro_dynamic_library->GetFunction(&KN_get_rel_feas_error, "KN_get_rel_feas_error");
	knitro_dynamic_library->GetFunction(&KN_get_abs_opt_error, "KN_get_abs_opt_error");
	knitro_dynamic_library->GetFunction(&KN_get_rel_opt_error, "KN_get_rel_opt_error");
	knitro_dynamic_library->GetFunction(&KN_get_objgrad_nnz, "KN_get_objgrad_nnz");
	knitro_dynamic_library->GetFunction(&KN_get_objgrad_values, "KN_get_objgrad_values");
	knitro_dynamic_library->GetFunction(&KN_get_objgrad_values_all, "KN_get_objgrad_values_all");
	knitro_dynamic_library->GetFunction(&KN_get_jacobian_nnz, "KN_get_jacobian_nnz");
	knitro_dynamic_library->GetFunction(&KN_get_jacobian_values, "KN_get_jacobian_values");
	knitro_dynamic_library->GetFunction(&KN_get_jacobian_nnz_one, "KN_get_jacobian_nnz_one");
	knitro_dynamic_library->GetFunction(&KN_get_jacobian_values_one, "KN_get_jacobian_values_one");
	knitro_dynamic_library->GetFunction(&KN_get_rsd_jacobian_nnz, "KN_get_rsd_jacobian_nnz");
	knitro_dynamic_library->GetFunction(&KN_get_rsd_jacobian_values, "KN_get_rsd_jacobian_values");
	knitro_dynamic_library->GetFunction(&KN_get_hessian_nnz, "KN_get_hessian_nnz");
	knitro_dynamic_library->GetFunction(&KN_get_hessian_values, "KN_get_hessian_values");
	knitro_dynamic_library->GetFunction(&KN_get_mip_number_nodes, "KN_get_mip_number_nodes");
	knitro_dynamic_library->GetFunction(&KN_get_mip_number_solves, "KN_get_mip_number_solves");
	knitro_dynamic_library->GetFunction(&KN_get_mip_abs_gap, "KN_get_mip_abs_gap");
	knitro_dynamic_library->GetFunction(&KN_get_mip_rel_gap, "KN_get_mip_rel_gap");
	knitro_dynamic_library->GetFunction(&KN_get_mip_incumbent_obj, "KN_get_mip_incumbent_obj");
	knitro_dynamic_library->GetFunction(&KN_get_mip_relaxation_bnd, "KN_get_mip_relaxation_bnd");
	knitro_dynamic_library->GetFunction(&KN_get_mip_lastnode_obj, "KN_get_mip_lastnode_obj");
	knitro_dynamic_library->GetFunction(&KN_get_mip_incumbent_x, "KN_get_mip_incumbent_x");
}

std::vector<std::string> KnitroDynamicLibraryPotentialPaths() {
	std::vector<std::string> potential_paths;
	const std::vector<std::string> KnitroVersions = {
		"14.0.0"}; // TODO : à compléter

	// Look for libraries pointed by KNITRODIR first.
	const char* knitrodir_env = getenv("KNITRODIR");
	if (knitrodir_env != nullptr) {
    LOG(INFO) << "Environment variable KNITRODIR = " << knitrodir_env;
#if defined(_MSC_VER)  // Windows
    potential_paths.push_back(
        absl::StrCat(knitrodir_env, "\\lib\\knitro.dll"));
#elif defined(__APPLE__)  // OS X (TODO : à vérifier)
    potential_paths.push_back(
        absl::StrCat(knitrodir_env, "/lib/libknitro.dylib"));
#elif defined(__GNUC__)   // Linux (TODO : à vérifier)
    potential_paths.push_back(
        absl::StrCat(knitrodir_env, "/lib/libknitro.so"));
#else
    LOG(ERROR) << "OS Not recognized by knitro/environment.cc."
               << " You won't be able to use Knitro.";
#endif
  } else {
    LOG(WARNING) << "Environment variable KNITRODIR undefined.";
  }

	// Search for canonical places.
	for (const std::string& version : KnitroVersions) {
		const std::string lib = version.substr(0, version.size() - 1);
#if defined(_MSC_VER)  // Windows
		potential_paths.push_back(absl::StrCat("C:\\Program Files\\Artelys\\Knitro ", version,
											"\\lib\\knitro.dll"));
		potential_paths.push_back(absl::StrCat(
			"C:\\Knitro ", version, "\\lib\\knitro.dll"));
		potential_paths.push_back(absl::StrCat("knitro.dll"));
#elif defined(__APPLE__)  // OS X TODO : à dzf
		potential_paths.push_back(absl::StrCat(
			"/Library/knitro", version, "/mac64/lib/libknitro", lib, ".dylib"));
		potential_paths.push_back(absl::StrCat("/Library/knitro", version,
											"/macos_universal2/lib/libknitro",
											lib, ".dylib"));
#elif defined(__GNUC__)   // Linux TODO : à def
		potential_paths.push_back(absl::StrCat(
			"/opt/knitro", version, "/linux64/lib/libknitro", lib, ".so"));
		potential_paths.push_back(absl::StrCat(
			"/opt/knitro", version, "/linux64/lib64/libknitro", lib, ".so"));
		potential_paths.push_back(
			absl::StrCat("/opt/knitro/linux64/lib/libknitro", lib, ".so"));
		potential_paths.push_back(
			absl::StrCat("/opt/knitro/linux64/lib64/libknitro", lib, ".so"));
#else
		LOG(ERROR) << "OS Not recognized by knitro/environment.cc."
				<< " You won't be able to use Knitro.";
#endif
  }
  	return potential_paths;
}

absl::Status LoadKnitroDynamicLibrary(std::string& knitropath) {
  static std::string knitro_lib_path;
  static std::once_flag knitro_loading_done;
  static absl::Status knitro_load_status;
  static DynamicLibrary knitro_library;
  static absl::Mutex mutex(absl::kConstInit);

  absl::MutexLock lock(&mutex);

  std::call_once(knitro_loading_done, []() {
    const std::vector<std::string> canonical_paths =
        KnitroDynamicLibraryPotentialPaths();
    for (const std::string& path : canonical_paths) {
      if (knitro_library.TryToLoad(path)) {
        LOG(INFO) << "Found the Knitro library in " << path << ".";
        knitro_lib_path.clear();
        std::filesystem::path p(path);
        p.remove_filename();
        knitro_lib_path.append(p.string());
        break;
      }
    }

    if (knitro_library.LibraryIsLoaded()) {
      LOG(INFO) << "Loading all Knitro functions";
      LoadKnitroFunctions(&knitro_library);
      knitro_load_status = absl::OkStatus();
    } else {
      knitro_load_status = absl::NotFoundError(
          absl::StrCat("Could not find the Knitro shared library. Looked in: [",
                       absl::StrJoin(canonical_paths, "', '"),
                       "]. Please check environment variable KNITRODIR"));
    }
  });
  knitropath.clear();
  knitropath.append(knitro_lib_path);
  return knitro_load_status;
}

bool KnitroIsCorrectlyInstalled() {
  	std::string knitro_lib_dir; // TODO : A voir si on garde
	absl::Status status = LoadKnitroDynamicLibrary(knitro_lib_dir);
	if (!status.ok()) {
		LOG(WARNING) << status << "\n";
		return false;
	}

	KN_context* kn = nullptr;
	if (KN_new(&kn) != 0 || kn == nullptr) {
		LOG(WARNING) << "Failed to Init Knitro Env";
		return false;
	}

	KN_free(&kn);

	return true;
}

} // namespace operations_research
