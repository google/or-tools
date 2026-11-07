/* Generated interface for MOSEK Stable API 12.0. */

#include "mosekstable12_environment.h"


/* NOTES on compiling and linking:
 * To build the MOSEK Stable dymalic loader functions this file must be included in the final binary or library.
 * On Linux and OSX the final library or binary must be linked with libdl.
 */

namespace operations_research::MSK12 {


#ifdef _WIN64
    #include <libloaderapi.h>
    static const char * libname = "mosekstable12.dll";
    static const char * platformname = "win64x86";
    #define LIBHANDLE_T HINSTANCE
    #define LOADLIBRARY(name) LoadLibraryA(name)
    #define UNLOADLIBRARY(handle) FreeLibrary(handle)
    #define LOADSYM(handle,name) GetProcAddress(handle,name)
    const char     pathsep = '\\';
#else
    #include <dlfcn.h>
    #define LIBHANDLE_T void *
    #define LOADLIBRARY(name) dlopen(name,RTLD_NOW)
    #define UNLOADLIBRARY(handle) dlclose(handle)
    #define LOADSYM(handle,name) dlsym(handle,name)
    #if defined(__APPLE__)
        static const char * libname = "libmosekstable12.dylib";
        static const char * platformname = "osxaarch64";
        static const char   pathsep = '/';
    #elif defined(__linux__) && (__arm64__ || __aarch64__)
        static const char pathsep = '/';
        static const char * platformname = "linuxaarch64";
        static const char * libname = "libmosekstable12.so";
    #elif defined(__linux__) && (__x86_64__ || __amd64__)
        static const char   pathsep = '/';
        static const char * platformname = "linux64x86";
        static const char * libname = "libmosekstable12.so";
    #else
        #error "Unsupported platform for MOSEK"
    #endif
#endif

template<class F>
bool loadsym(std::function<F> & target, LIBHANDLE_T h, const char * symname) {
    void * addr = LOADSYM(h,symname);
    if (! addr)
        return false;
    target = (F*)addr;
    return true;
}

static LIBHANDLE_T libmosek_handle = nullptr;


std::function<const char*(std::int32_t)> ptr_get_callback_code_name;
/** 
 * Get the name of a callback code 

 * Arguments
 */
const char* get_callback_code_name(std::int32_t code) {
  return ptr_get_callback_code_name(code);
}

std::function<const char*(ResCode)> ptr_get_resp_name;
/** 
 * Get string representing the given response code.

 * Arguments
 */
const char* get_resp_name(ResCode r) {
  return ptr_get_resp_name(r);
}

std::function<const char*(ResCode)> ptr_get_resp_descr;
/** 
 * Get string with a description of the given response code.

 * Arguments
 */
const char* get_resp_descr(ResCode r) {
  return ptr_get_resp_descr(r);
}

std::function<ResCode(Task_t)> ptr_get_last_resp;
/** 
 * Get the last error response code 

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode get_last_resp(Task_t task) {
  return ptr_get_last_resp(task);
}

std::function<ResCode(Task_t,char*,std::size_t)> ptr_get_last_resp_msg;
/** 
 * Get a string with name of the last error message recorded in the task.

 * Arguments
 * - `task` The optimizatioj task object
 * - `buf` Last message will be copied here, truncated to `buf_len` including trailing 0.
 * - `buf_len` Length of target buffer
 */
ResCode get_last_resp_msg(Task_t task,char* buf,std::size_t buf_len) {
  return ptr_get_last_resp_msg(task,buf,buf_len);
}

std::function<std::size_t(Task_t)> ptr_get_last_resp_msg_len;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 */
std::size_t get_last_resp_msg_len(Task_t task) {
  return ptr_get_last_resp_msg_len(task);
}

std::function<const char*(TrmCode)> ptr_get_trm_name;
/** 
 * Get string representing the given termination code.

 * Arguments
 */
const char* get_trm_name(TrmCode trm) {
  return ptr_get_trm_name(trm);
}

std::function<const char*(TrmCode)> ptr_get_trm_descr;
/** 
 * Get string with a description if the given termination code.

 * Arguments
 */
const char* get_trm_descr(TrmCode trm) {
  return ptr_get_trm_descr(trm);
}

std::function<Task_t(Task_t)> ptr_new_task_from_task;
/** 
 * Create a new task with data from another task. Thus will copy
 * 
 * - All problem data,
 * - All solutions including solution status,
 * - Parameters
 * - Information items
 * 
 * What may NOT be copied
 * 
 * - Callback functions and attached log files
 * - Last error message

 * Arguments
 * - `task` The optimizatioj task object
 */
Task_t new_task_from_task(Task_t task) {
  return ptr_new_task_from_task(task);
}

std::function<Task_t()> ptr_new_task;
/** 
 * Create a new task from a file. Accepted file formats are PTF, JSON and TASK, and accepted compressions are plain, zstd and gzip.
 */
Task_t new_task() {
  return ptr_new_task();
}

std::function<void(Task_t)> ptr_delete_task;
/** 
 * Delete task. This is assumed to always succeed.

 * Arguments
 * - `task` The optimizatioj task object
 */
void delete_task(Task_t task) {
  ptr_delete_task(task);
}

std::function<ResCode(Task_t,std::int32_t)> ptr_reserve_num_var;
/** 
 * Reserve space for scalar variables. This is to be considered a _hint_, not a requirement. 

 * Arguments
 * - `task` The optimizatioj task object
 * - `add_num` Number of item to add
 */
ResCode reserve_num_var(Task_t task,std::int32_t add_num) {
  return ptr_reserve_num_var(task,add_num);
}

std::function<ResCode(Task_t,std::int32_t)> ptr_reserve_num_barvar;
/** 
 * Reserve space for semidefinite variables. This is to be considered a _hint_, not a requirement.

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_barvar` Number of variables
 */
ResCode reserve_num_barvar(Task_t task,std::int32_t num_barvar) {
  return ptr_reserve_num_barvar(task,num_barvar);
}

std::function<ResCode(Task_t,std::int32_t)> ptr_reserve_num_con;
/** 
 * Reserve space for constraints. This is to be considered a _hint_, not a requirement.

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_con` Number of constraints
 */
ResCode reserve_num_con(Task_t task,std::int32_t num_con) {
  return ptr_reserve_num_con(task,num_con);
}

std::function<ResCode(Task_t,std::int64_t)> ptr_reserve_num_row;
/** 
 * Reserve space for afes. This is to be considered a _hint_, not a requirement.

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_row` Number of affine rows
 */
ResCode reserve_num_row(Task_t task,std::int64_t num_row) {
  return ptr_reserve_num_row(task,num_row);
}

std::function<ResCode(Task_t,std::int64_t)> ptr_reserve_num_nz;
/** 
 * Reserve space for coefficient matrix non-zeros. This is to be considered a _hint_, not a requirement.

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode reserve_num_nz(Task_t task,std::int64_t num_nz) {
  return ptr_reserve_num_nz(task,num_nz);
}

std::function<ResCode(Task_t,std::int64_t)> ptr_reserve_num_dom;
/** 
 * Reserve space for domains. This is to be considered a _hint_, not a requirement.

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_dom` Number of domains
 */
ResCode reserve_num_dom(Task_t task,std::int64_t num_dom) {
  return ptr_reserve_num_dom(task,num_dom);
}

std::function<ResCode(Task_t,std::int64_t)> ptr_reserve_num_symmat;
/** 
 * Reserve space for symmetric matrixes. This is to be considered a _hint_, not a requirement.

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_symmat` Number of symmetric matrixes
 */
ResCode reserve_num_symmat(Task_t task,std::int64_t num_symmat) {
  return ptr_reserve_num_symmat(task,num_symmat);
}

std::function<ResCode(Task_t,std::int64_t)> ptr_reserve_num_symmat_nz;
/** 
 * Reserve space for symmetric matrix variables. This is to be considered a _hint_, not a requirement.

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode reserve_num_symmat_nz(Task_t task,std::int64_t num_nz) {
  return ptr_reserve_num_symmat_nz(task,num_nz);
}

std::function<std::int32_t(Task_t)> ptr_get_num_var;
/** 
 * Get number of scalar variables. Cannot fail.

 * Arguments
 * - `task` The optimizatioj task object
 */
std::int32_t get_num_var(Task_t task) {
  return ptr_get_num_var(task);
}

std::function<std::int32_t(Task_t)> ptr_get_num_barvar;
/** 
 * Get number of semidefinite variables. Cannot fail.

 * Arguments
 * - `task` The optimizatioj task object
 */
std::int32_t get_num_barvar(Task_t task) {
  return ptr_get_num_barvar(task);
}

std::function<std::int64_t(Task_t)> ptr_get_num_domain;
/** 
 * Get number of domains. Cannot fail. 

 * Arguments
 * - `task` The optimizatioj task object
 */
std::int64_t get_num_domain(Task_t task) {
  return ptr_get_num_domain(task);
}

std::function<std::int64_t(Task_t)> ptr_get_num_row;
/** 
 * Get number of affine rows. Cannot fail. 

 * Arguments
 * - `task` The optimizatioj task object
 */
std::int64_t get_num_row(Task_t task) {
  return ptr_get_num_row(task);
}

std::function<std::int64_t(Task_t)> ptr_get_num_symmat;
/** 
 * Get number of symmetric matrixes. Cannot fail. 

 * Arguments
 * - `task` The optimizatioj task object
 */
std::int64_t get_num_symmat(Task_t task) {
  return ptr_get_num_symmat(task);
}

std::function<std::int64_t(Task_t)> ptr_get_num_con;
/** 
 * Get number of constraints. Cannot fail. 

 * Arguments
 * - `task` The optimizatioj task object
 */
std::int64_t get_num_con(Task_t task) {
  return ptr_get_num_con(task);
}

std::function<std::int64_t(Task_t)> ptr_get_num_djc;
/** 
 * Get number of disjunctive constraints. Cannot fail. 

 * Arguments
 * - `task` The optimizatioj task object
 */
std::int64_t get_num_djc(Task_t task) {
  return ptr_get_num_djc(task);
}

std::function<ResCode(Task_t,std::int32_t)> ptr_append_vars;
/** 
 * Append scalar variables.

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_var` Number of variables
 */
ResCode append_vars(Task_t task,std::int32_t num_var) {
  return ptr_append_vars(task,num_var);
}

std::function<ResCode(Task_t,std::int64_t)> ptr_append_rows;
/** 
 * Append a number of empty affine rows

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_row` Number of affine rows
 */
ResCode append_rows(Task_t task,std::int64_t num_row) {
  return ptr_append_rows(task,num_row);
}

std::function<ResCode(Task_t,std::int32_t)> ptr_append_barvar;
/** 
 * Append a single positive semi-definite variable.

 * Arguments
 * - `task` The optimizatioj task object
 * - `dim` Dimension
 */
ResCode append_barvar(Task_t task,std::int32_t dim) {
  return ptr_append_barvar(task,dim);
}

std::function<ResCode(Task_t,std::int32_t,const std::int32_t*)> ptr_append_barvars;
/** 
 * Append multiple positive semi-definite variables with the given dimensions.

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_barvar` Number of variables
 * - `dims` Array of dimensionms
 */
ResCode append_barvars(Task_t task,std::int32_t num_barvar,const std::int32_t* dims) {
  return ptr_append_barvars(task,num_barvar,dims);
}

std::function<ResCode(Task_t,std::int32_t,std::int64_t,const std::int32_t*,const std::int32_t*,const double*)> ptr_append_symmat;
/** 
 * Append a single symmetric matrix.

 * Arguments
 * - `task` The optimizatioj task object
 * - `dim` Dimension
 * - `nnz` Number of nonzeros
 * - `symmat_i` Symmetric matrix row subscripts
 * - `symmat_j` Symmetric matrix column subscripts
 * - `symmat_val` Symmetric matrix values
 */
ResCode append_symmat(Task_t task,std::int32_t dim,std::int64_t nnz,const std::int32_t* symmat_i,const std::int32_t* symmat_j,const double* symmat_val) {
  return ptr_append_symmat(task,dim,nnz,symmat_i,symmat_j,symmat_val);
}

std::function<ResCode(Task_t,std::int64_t,const std::int32_t*,const std::int64_t*,const std::int32_t*,const std::int32_t*,const double*)> ptr_append_symmats;
/** 
 * Append a list of symmetric matrixes.

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_symmat` Number of symmetric matrixes
 * - `dim` Dimension
 * - `nnz` Number of nonzeros
 * - `symmat_i` Symmetric matrix row subscripts
 * - `symmat_j` Symmetric matrix column subscripts
 * - `symmat_val` Symmetric matrix values
 */
ResCode append_symmats(Task_t task,std::int64_t num_symmat,const std::int32_t* dim,const std::int64_t* nnz,const std::int32_t* symmat_i,const std::int32_t* symmat_j,const double* symmat_val) {
  return ptr_append_symmats(task,num_symmat,dim,nnz,symmat_i,symmat_j,symmat_val);
}

std::function<ResCode(Task_t,std::int64_t)> ptr_append_empty_cons;
/** 
 * Append a number of empty constraints, initially they will have domain `null`.

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_con` Number of constraints
 */
ResCode append_empty_cons(Task_t task,std::int64_t num_con) {
  return ptr_append_empty_cons(task,num_con);
}

std::function<ResCode(Task_t,std::int64_t)> ptr_append_empty_djcs;
/** 
 * Append a number of empty disjunctive constraints.

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_djc` Number of disjunctive constraints
 */
ResCode append_empty_djcs(Task_t task,std::int64_t num_djc) {
  return ptr_append_empty_djcs(task,num_djc);
}

std::function<ResCode(Task_t,std::int32_t,VariableType)> ptr_put_var_type;
/** 
 * Set variable type to integer or continuous.

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode put_var_type(Task_t task,std::int32_t j,VariableType var_type) {
  return ptr_put_var_type(task,j,var_type);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,const VariableType*)> ptr_put_var_type_slice;
/** 
 * Set variable types in a slice to integer or continuous.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_var` Variable index
 * - `num_var` Number of variables
 */
ResCode put_var_type_slice(Task_t task,std::int32_t first_var,std::int32_t num_var,const VariableType* var_types) {
  return ptr_put_var_type_slice(task,first_var,num_var,var_types);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,VariableType)> ptr_put_var_type_slice_value;
/** 
 * Set variable types for all entries in a slice to a single value.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_var` Variable index
 * - `num_var` Number of variables
 */
ResCode put_var_type_slice_value(Task_t task,std::int32_t first_var,std::int32_t num_var,VariableType var_type) {
  return ptr_put_var_type_slice_value(task,first_var,num_var,var_type);
}

std::function<ResCode(Task_t,std::int32_t,const std::int32_t*,const VariableType*)> ptr_put_var_type_list;
/** 
 * Set variable types in a list to integer or continuous.

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_var` Number of variables
 * - `var_idxs` Array of variable indexes
 */
ResCode put_var_type_list(Task_t task,std::int32_t num_var,const std::int32_t* var_idxs,const VariableType* var_types) {
  return ptr_put_var_type_list(task,num_var,var_idxs,var_types);
}

std::function<ResCode(Task_t,std::int32_t,VariableType[1])> ptr_get_var_type;
/** 
 * Get variable type as integer or continuous.

 * Arguments
 * - `task` The optimizatioj task object
 * - `var_idx` Variable index
 */
ResCode get_var_type(Task_t task,std::int32_t var_idx,VariableType var_type[1]) {
  return ptr_get_var_type(task,var_idx,var_type);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,VariableType*)> ptr_get_var_type_slice;
/** 
 * Get variable slice types as integer or continuous.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_var` Variable index
 * - `num_var` Number of variables
 */
ResCode get_var_type_slice(Task_t task,std::int32_t first_var,std::int32_t num_var,VariableType* var_types) {
  return ptr_get_var_type_slice(task,first_var,num_var,var_types);
}

std::function<ResCode(Task_t,std::int32_t,double,double)> ptr_put_var_bound;
/** 
 * Set variable bounds.

 * Arguments
 * - `task` The optimizatioj task object
 * - `var_idx` Variable index
 * - `low` Lower bound
 * - `upr` Upper bound
 */
ResCode put_var_bound(Task_t task,std::int32_t var_idx,double low,double upr) {
  return ptr_put_var_bound(task,var_idx,low,upr);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,const double*,const double*)> ptr_put_var_bound_slice;
/** 
 * Set variable bounds for a slice.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_var` Variable index
 * - `num_var` Number of variables
 * - `low` Lower bound
 * - `upr` Upper bound
 */
ResCode put_var_bound_slice(Task_t task,std::int32_t first_var,std::int32_t num_var,const double* low,const double* upr) {
  return ptr_put_var_bound_slice(task,first_var,num_var,low,upr);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,double,double)> ptr_put_var_bound_slice_value;
/** 
 * Set identical bound for a slice of variables.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_var` Variable index
 * - `num_var` Number of variables
 * - `low` Lower bound
 * - `upr` Upper bound
 */
ResCode put_var_bound_slice_value(Task_t task,std::int32_t first_var,std::int32_t num_var,double low,double upr) {
  return ptr_put_var_bound_slice_value(task,first_var,num_var,low,upr);
}

std::function<ResCode(Task_t,std::int32_t,double)> ptr_put_var_low_bound;
/** 
 * Set variable bounds.

 * Arguments
 * - `task` The optimizatioj task object
 * - `var_idx` Variable index
 * - `low` Lower bound
 */
ResCode put_var_low_bound(Task_t task,std::int32_t var_idx,double low) {
  return ptr_put_var_low_bound(task,var_idx,low);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,const double*)> ptr_put_var_low_bound_slice;
/** 
 * Set variable bounds for a slice.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_var` Variable index
 * - `num_var` Number of variables
 * - `low` Lower bound
 */
ResCode put_var_low_bound_slice(Task_t task,std::int32_t first_var,std::int32_t num_var,const double* low) {
  return ptr_put_var_low_bound_slice(task,first_var,num_var,low);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,double)> ptr_put_var_low_bound_slice_value;
/** 
 * Set identical bound for a slice of variables.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_var` Variable index
 * - `num_var` Number of variables
 * - `low` Lower bound
 */
ResCode put_var_low_bound_slice_value(Task_t task,std::int32_t first_var,std::int32_t num_var,double low) {
  return ptr_put_var_low_bound_slice_value(task,first_var,num_var,low);
}

std::function<ResCode(Task_t,std::int32_t,double)> ptr_put_var_upr_bound;
/** 
 * Set variable bounds.

 * Arguments
 * - `task` The optimizatioj task object
 * - `var_idx` Variable index
 * - `upr` Upper bound
 */
ResCode put_var_upr_bound(Task_t task,std::int32_t var_idx,double upr) {
  return ptr_put_var_upr_bound(task,var_idx,upr);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,const double*)> ptr_put_var_upr_bound_slice;
/** 
 * Set variable bounds for a slice.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_var` Variable index
 * - `num_var` Number of variables
 * - `upr` Upper bound
 */
ResCode put_var_upr_bound_slice(Task_t task,std::int32_t first_var,std::int32_t num_var,const double* upr) {
  return ptr_put_var_upr_bound_slice(task,first_var,num_var,upr);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,double)> ptr_put_var_upr_bound_slice_value;
/** 
 * Set identical bound for a slice of variables.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_var` Variable index
 * - `num_var` Number of variables
 * - `upr` Upper bound
 */
ResCode put_var_upr_bound_slice_value(Task_t task,std::int32_t first_var,std::int32_t num_var,double upr) {
  return ptr_put_var_upr_bound_slice_value(task,first_var,num_var,upr);
}

std::function<ResCode(Task_t,std::int32_t,double[1],double[1])> ptr_get_var_bound;
/** 
 * Get variable bounds.

 * Arguments
 * - `task` The optimizatioj task object
 * - `var_idx` Variable index
 * - `low` Lower bound
 * - `upr` Upper bound
 */
ResCode get_var_bound(Task_t task,std::int32_t var_idx,double low[1],double upr[1]) {
  return ptr_get_var_bound(task,var_idx,low,upr);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,double*,double*)> ptr_get_var_bound_slice;
/** 
 * Get variable bounds for a slice.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_var` Variable index
 * - `num_var` Number of variables
 * - `low` Lower bound
 * - `upr` Upper bound
 */
ResCode get_var_bound_slice(Task_t task,std::int32_t first_var,std::int32_t num_var,double* low,double* upr) {
  return ptr_get_var_bound_slice(task,first_var,num_var,low,upr);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int64_t[1])> ptr_get_barvar_slice_num_elm;
/** 
 * Compute the number of scalar elements in a slice of semidefinite variables.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_barvar` First semidefinite variable index in a slice
 * - `num_barvar` Number of variables
 * - `num_elm` Number of positive semidefinite non-zero entries
 */
ResCode get_barvar_slice_num_elm(Task_t task,std::int32_t first_barvar,std::int32_t num_barvar,std::int64_t num_elm[1]) {
  return ptr_get_barvar_slice_num_elm(task,first_barvar,num_barvar,num_elm);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t[1])> ptr_get_barvar_dim;
/** 
 * Get dimension of semidefinite variable

 * Arguments
 * - `task` The optimizatioj task object
 * - `barvar_idx` Positive semi-definite variable index
 * - `dim` Dimension
 */
ResCode get_barvar_dim(Task_t task,std::int32_t barvar_idx,std::int32_t dim[1]) {
  return ptr_get_barvar_dim(task,barvar_idx,dim);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t*)> ptr_get_barvar_slice_dims;
/** 
 * Get dimensions of a slice of semidefinite variables.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_barvar` First semidefinite variable index in a slice
 * - `num_barvar` Number of variables
 * - `dim` Dimension
 */
ResCode get_barvar_slice_dims(Task_t task,std::int32_t first_barvar,std::int32_t num_barvar,std::int32_t* dim) {
  return ptr_get_barvar_slice_dims(task,first_barvar,num_barvar,dim);
}

std::function<ResCode(Task_t,DomainType,std::int64_t,std::int32_t,double*,std::int64_t[1])> ptr_get_domain;
/** 
 * Return index of a domain of the requested type

 * Arguments
 * - `task` The optimizatioj task object
 * - `dim` Dimension
 * - `num_alpha` Number of alpha values in array
 * - `alpha` Array if alpha values for power cone domain
 * - `dom_idx` Index of the domain
 */
ResCode get_domain(Task_t task,DomainType dom_type,std::int64_t dim,std::int32_t num_alpha,double* alpha,std::int64_t dom_idx[1]) {
  return ptr_get_domain(task,dom_type,dim,num_alpha,alpha,dom_idx);
}

std::function<ResCode(Task_t,std::int64_t[1])> ptr_get_domain_empty;
/** 
 * Return index of the empty domain. Only one empty domain is created, so if one already exists, that one is returned instead of creating a new domain

 * Arguments
 * - `task` The optimizatioj task object
 * - `dom_idx` Index of the domain
 */
ResCode get_domain_empty(Task_t task,std::int64_t dom_idx[1]) {
  return ptr_get_domain_empty(task,dom_idx);
}

std::function<ResCode(Task_t,std::int64_t[1])> ptr_get_domain_rzero;
/** 
 * Return index of an `rzero` domain. Only one `rzero` domain is created, so if one already exists, that one is returned instead of creating a new domain

 * Arguments
 * - `task` The optimizatioj task object
 * - `dom_idx` Index of the domain
 */
ResCode get_domain_rzero(Task_t task,std::int64_t dom_idx[1]) {
  return ptr_get_domain_rzero(task,dom_idx);
}

std::function<ResCode(Task_t,std::int64_t[1])> ptr_get_domain_rplus;
/** 
 * Return index of an `rplus` domain. Only one `rplus` domain is created, so if one already exists, that one is returned instead of creating a new domain

 * Arguments
 * - `task` The optimizatioj task object
 * - `dom_idx` Index of the domain
 */
ResCode get_domain_rplus(Task_t task,std::int64_t dom_idx[1]) {
  return ptr_get_domain_rplus(task,dom_idx);
}

std::function<ResCode(Task_t,std::int64_t[1])> ptr_get_domain_rminus;
/** 
 * Return index of an `rminus` domain. Only one `rminus` domain is created, so if one already exists, that one is returned instead of creating a new domain

 * Arguments
 * - `task` The optimizatioj task object
 * - `dom_idx` Index of the domain
 */
ResCode get_domain_rminus(Task_t task,std::int64_t dom_idx[1]) {
  return ptr_get_domain_rminus(task,dom_idx);
}

std::function<ResCode(Task_t,std::int64_t[1])> ptr_get_domain_r;
/** 
 * Return index of an `r` domain. Only one `r` domain is created, so if one already exists, that one is returned instead of creating a new domain

 * Arguments
 * - `task` The optimizatioj task object
 * - `dom_idx` Index of the domain
 */
ResCode get_domain_r(Task_t task,std::int64_t dom_idx[1]) {
  return ptr_get_domain_r(task,dom_idx);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t[1])> ptr_get_domain_quadratic_cone;
/** 
 * Return the index of a quadratic cone domain of the given size.
 * 
 * The quadratic cone of size \\(n\\) is defined as
 * $$
 * \\left\\{x\\in\\real^n~:~x_0 \\geq \\sqrt{\\sum_{i=1}^{n-1} x_i^2}\\right\\}
 * $$

 * Arguments
 * - `task` The optimizatioj task object
 * - `dom_idx` Index of the domain
 */
ResCode get_domain_quadratic_cone(Task_t task,std::int64_t n,std::int64_t dom_idx[1]) {
  return ptr_get_domain_quadratic_cone(task,n,dom_idx);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t[1])> ptr_get_domain_rotated_quadratic_cone;
/** 
 * Return the index of a rotated quadratic cone domain of the given size.
 * 
 * The rotated quadratic cone of size \\(n\\) is defined as
 * $$
 * \\left\{ x\\in \\real^3 ~:~ x_0 \\geq x_1 e^{x_2/x_1},\\ x_0,x_1\\geq; 0 \\right\\}
 * $$

 * Arguments
 * - `task` The optimizatioj task object
 * - `dom_idx` Index of the domain
 */
ResCode get_domain_rotated_quadratic_cone(Task_t task,std::int64_t n,std::int64_t dom_idx[1]) {
  return ptr_get_domain_rotated_quadratic_cone(task,n,dom_idx);
}

std::function<ResCode(Task_t,std::int64_t[1])> ptr_get_domain_primal_exponential_cone;
/** 
 * Return index if an `primal_exponential` domain. Only one
 * `primal_exponential` domain is created, so if one already exists,
 * that one is returned instead of creating a new domain.
 * 
 * The primal exponential cone is defined as
 * $$
 * \\left\{ x\\in \\real^3 ~:~ x_0 \\geq x_1 e^{x_2/x_1},\\ x_0,x_1> 0 \\right\\}
 * $$

 * Arguments
 * - `task` The optimizatioj task object
 * - `dom_idx` Index of the domain
 */
ResCode get_domain_primal_exponential_cone(Task_t task,std::int64_t dom_idx[1]) {
  return ptr_get_domain_primal_exponential_cone(task,dom_idx);
}

std::function<ResCode(Task_t,std::int64_t[1])> ptr_get_domain_dual_exponential_cone;
/** 
 * Return index if an `dual_exponential` domain. Only one
 * `dual_exponential` domain is created, so if one already exists, that
 * one is returned instead of creating a new domain
 * 
 * The dual exponential cone is defined as
 * $$
 * \\left\\{ x\\in \\real^3 ~:~ x_0 \\geq -x_2 e^{-1} e^{x_1/x_2},\\ x_0> 0,\\ x_2< 0 \\right\\}
 * $$

 * Arguments
 * - `task` The optimizatioj task object
 * - `dom_idx` Index of the domain
 */
ResCode get_domain_dual_exponential_cone(Task_t task,std::int64_t dom_idx[1]) {
  return ptr_get_domain_dual_exponential_cone(task,dom_idx);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,const double*,std::int64_t[1])> ptr_get_domain_primal_power_cone;
/** 
 * Return the index of a new primal power cone.
 * 
 * The primal power cone domain of dimension \\(n\\), with \\(n_\\ell\\) variables appearing on the left-hand side, where \\(n_\\ell\\) is the length of \\(\\alpha\\), and with a homogenous sequence of exponents \\(\\alpha_0,\\ldots,\\alpha_{n_\\ell-1}\\).
 * 
 * Formally, let \\(s = \\sum_i \\alpha_i\\) and \\(\\beta_i = \\alpha_i / s\\), so that \\(\\sum_i \\beta_i=1\\). Then the primal power cone is defined as follows:
 * 
 * $$
 * \\left\\{ x\\in \\real^n ~:~ \\prod_{i=0}^{n_\\ell-1} x_i^{\\beta_i} \\geq \\sqrt{\\sum_{j=n_\\ell}^{n-1}x_j^2},\\ x_0\\ldots,x_{n_\\ell-1}\\geq 0 \\right\\}
 * $$

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_alpha` Number of alpha values in array
 * - `alpha` Array if alpha values for power cone domain
 * - `dom_idx` Index of the domain
 */
ResCode get_domain_primal_power_cone(Task_t task,std::int64_t n,std::int64_t num_alpha,const double* alpha,std::int64_t dom_idx[1]) {
  return ptr_get_domain_primal_power_cone(task,n,num_alpha,alpha,dom_idx);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,const double*,std::int64_t[1])> ptr_get_domain_dual_power_cone;
/** 
 * Return the index of a new dual power cone.
 * 
 * Appends the dual power cone domain of dimension \\(n\\), with \\(n_\\ell\\) variables appearing on the left-hand side, where \\(n_\\ell\\) is the length of \\(\\alpha\\), and with a homogenous sequence of exponents \\(\\alpha_0,\\ldots,\\alpha_{n_\\ell-1}\\).
 * 
 * Formally, let \\(s = \\sum_i \\alpha_i\\) and \\(\\beta_i = \\alpha_i / s\\), so that \\(\\sum_i \\beta_i=1\\). Then the dual power cone is defined as follows:
 * 
 * $$
 * \\left\\{ x\\in \\real^n ~:~ \\prod_{i=0}^{n_\\ell-1} \\left(\\frac{x_i}{\\beta_i}\\right)^{\\beta_i} \\geq \\sqrt{\\sum_{j=n_\\ell}^{n-1}x_j^2},\\ x_0\\ldots,x_{n_\\ell-1}\\geq 0 \\right\\}
 * $$

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_alpha` Number of alpha values in array
 * - `alpha` Array if alpha values for power cone domain
 * - `dom_idx` Index of the domain
 */
ResCode get_domain_dual_power_cone(Task_t task,std::int64_t n,std::int64_t num_alpha,const double* alpha,std::int64_t dom_idx[1]) {
  return ptr_get_domain_dual_power_cone(task,n,num_alpha,alpha,dom_idx);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t[1])> ptr_get_domain_primal_geometric_mean_cone;
/** 
 * Get the index of a new primal geometric mean cone.
 * 
 * The primal geometric mean cone is defined as
 * $$
 * \\left\\{ x\\in \\real^n ~:~ \\left(\\prod_{i=0}^{n-2} x_i\\right)^{1/(n-1)} \\geq |x_{n-1}|,\\ x_0\\ldots,x_{n-2}\\geq 0 \\right\\}
 * $$

 * Arguments
 * - `task` The optimizatioj task object
 * - `dom_idx` Index of the domain
 */
ResCode get_domain_primal_geometric_mean_cone(Task_t task,std::int64_t n,std::int64_t dom_idx[1]) {
  return ptr_get_domain_primal_geometric_mean_cone(task,n,dom_idx);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t[1])> ptr_get_domain_dual_geometric_mean_cone;
/** 
 * Get the index of a new dual geometric mean cone.
 * 
 * The dual geometric mean cone is defined as
 * $$
 * \\left\\{ x\\in \\real^n ~:~ (n-1) \\left(\\prod_{i=0}^{n-2} x_i\\right)^{1/(n-1)} \\geq |x_{n-1}|,\\ x_0,\\ldots,x_{n-2}\\geq 0 \\right\\}
 * $$

 * Arguments
 * - `task` The optimizatioj task object
 * - `dom_idx` Index of the domain
 */
ResCode get_domain_dual_geometric_mean_cone(Task_t task,std::int64_t n,std::int64_t dom_idx[1]) {
  return ptr_get_domain_dual_geometric_mean_cone(task,n,dom_idx);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t[1])> ptr_get_domain_svecpsd_cone;
/** 
 * Get the index of a new scaled vectorized PSD cone.
 * 
 * The domain consisting of vectors of length \\(n=d(d+1)/2\\) defined as follows
 * 
 * $$
 * \\{(x_1,\\ldots,x_{d(d+1)/2})\\in \\real^n~:~ \\mathrm{sMat}(x)\\in\\PSD^d\\} = \\{\\mathrm{sVec}(X)~:~X\\in\\PSD^d\\},
 * $$
 * 
 * where
 * 
 * $$
 * \\mathrm{sVec}(X) = (X_{11},\\sqrt{2}X_{21},\\ldots,\\sqrt{2}X_{d1},X_{22},\\sqrt{2}X_{32},\\ldots,X_{dd}),
 * $$
 * 
 * and
 * 
 * $$
 *     \\mathrm{sMat}(x) = \\left[\\begin{array}{cccc}
 *         x_1             & x_2/\\sqrt{2}      & \\cdots & x_{d}/\\sqrt{2} \\\\
 *         x_2/\\sqrt{2}   & x_{d+1}            & \\cdots & x_{2d-1}/\\sqrt{2} \\\\
 *         \\cdots         & \\cdots            & \\cdots & \\cdots \\\\
 *         x_{d}/\\sqrt{2} & x_{2d-1}/\\sqrt{2} & \\cdots & x_{d(d+1)/2}
 *     \\end{array}\\right].
 * $$
 * 
 * In other words, the domain consists of vectorizations of the lower-triangular part of a positive semidefinite matrix, with the non-diagonal elements additionally rescaled.
 * 
 * This domain is a self-dual cone.

 * Arguments
 * - `task` The optimizatioj task object
 * - `n` The cone dimension. Note that only values such that \\(n\\cdot(n+1)/2\\) for some integer \\(d\\) are valid.
 * - `dom_idx` Index of the domain
 */
ResCode get_domain_svecpsd_cone(Task_t task,std::int64_t n,std::int64_t dom_idx[1]) {
  return ptr_get_domain_svecpsd_cone(task,n,dom_idx);
}

std::function<ResCode(Task_t,std::int64_t,DomainType[1],std::int64_t[1],std::int32_t[1])> ptr_get_domain_info;
/** 
 * Get domain information

 * Arguments
 * - `task` The optimizatioj task object
 * - `dom_idx` Index of the domain
 * - `num_alpha` Number of alpha values in array
 */
ResCode get_domain_info(Task_t task,std::int64_t dom_idx,DomainType dom_type[1],std::int64_t size[1],std::int32_t num_alpha[1]) {
  return ptr_get_domain_info(task,dom_idx,dom_type,size,num_alpha);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,double*)> ptr_get_domain_alpha;
/** 
 * For primal and dual power domains, get domain alpha vector

 * Arguments
 * - `task` The optimizatioj task object
 * - `dom_idx` Index of the domain
 * - `num_alpha` Number of alpha values in array
 * - `alpha` Array if alpha values for power cone domain
 */
ResCode get_domain_alpha(Task_t task,std::int64_t dom_idx,std::int64_t num_alpha,double* alpha) {
  return ptr_get_domain_alpha(task,dom_idx,num_alpha,alpha);
}

std::function<ResCode(Task_t,std::int64_t,std::int32_t,const std::int32_t*,const double*)> ptr_put_row;
/** 
 * Input linear terms for a single affine row.

 * Arguments
 * - `task` The optimizatioj task object
 * - `row_idx` Index of the affine row
 * - `num_nz` Number of nonzeros
 * - `subj` Column indexes
 * - `cof` Coefficients
 */
ResCode put_row(Task_t task,std::int64_t row_idx,std::int32_t num_nz,const std::int32_t* subj,const double* cof) {
  return ptr_put_row(task,row_idx,num_nz,subj,cof);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,const std::int32_t*,const std::int32_t*,const double*)> ptr_put_row_slice;
/** 
 * Input linear terms for a slice of affine rows.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_row` Index of first affine row
 * - `num_row` Number of rows in the slice
 * - `row_num_nz` Number of non-zeros per row.
 * - `subj` Column subscripts
 * - `cof` Coefficients
 */
ResCode put_row_slice(Task_t task,std::int64_t first_row,std::int64_t num_row,const std::int32_t* row_num_nz,const std::int32_t* subj,const double* cof) {
  return ptr_put_row_slice(task,first_row,num_row,row_num_nz,subj,cof);
}

std::function<ResCode(Task_t,std::int64_t,const std::int64_t*,const std::int32_t*,const std::int32_t**,const double**)> ptr_put_row_list;
/** 
 * Input linear terms for a list of affine rows. This accepts subscripts and coefficients where rows are non-continuous.

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_row` Number of rows in list
 * - `row_idxs` Row indexes
 * - `row_num_nz` Number of non-zeros per row.
 * - `subj` List of pointers to subscripts.
 * - `cof` Coefficients
 */
ResCode put_row_list(Task_t task,std::int64_t num_row,const std::int64_t* row_idxs,const std::int32_t* row_num_nz,const std::int32_t** subj,const double** cof) {
  return ptr_put_row_list(task,num_row,row_idxs,row_num_nz,subj,cof);
}

std::function<ResCode(Task_t,std::int64_t,double)> ptr_put_row_g;
/** 
 * Input the constant term for a single affine row

 * Arguments
 * - `task` The optimizatioj task object
 * - `row_idx` Index of the affine row
 * - `g` Row fixed term
 */
ResCode put_row_g(Task_t task,std::int64_t row_idx,double g) {
  return ptr_put_row_g(task,row_idx,g);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,const double*)> ptr_put_row_slice_g;
/** 
 * Input the constant terms for a slice of affine rows.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_row` Index of the first affine row in a slice
 * - `num_row` Number of affine rows
 * - `g` Row fixed term
 */
ResCode put_row_slice_g(Task_t task,std::int64_t first_row,std::int64_t num_row,const double* g) {
  return ptr_put_row_slice_g(task,first_row,num_row,g);
}

std::function<ResCode(Task_t,std::int64_t,const std::int64_t*,const double*)> ptr_put_row_list_g;
/** 
 * Input the constant terms for a list of affine rows.

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_row` Number of affine rows
 * - `row_idxs` Array of row indexes
 * - `g` Row fixed term
 */
ResCode put_row_list_g(Task_t task,std::int64_t num_row,const std::int64_t* row_idxs,const double* g) {
  return ptr_put_row_list_g(task,num_row,row_idxs,g);
}

std::function<ResCode(Task_t,std::int32_t,std::int64_t,const std::int64_t*,const double*)> ptr_put_col;
/** 
 * Put a single column.

 * Arguments
 * - `task` The optimizatioj task object
 * - `col_idx` Variable index
 * - `row_idxs` Array of row indexes
 * - `cof` Coefficients
 */
ResCode put_col(Task_t task,std::int32_t col_idx,std::int64_t num_nz,const std::int64_t* row_idxs,const double* cof) {
  return ptr_put_col(task,col_idx,num_nz,row_idxs,cof);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,const std::int64_t*,const std::int64_t*,const double*)> ptr_put_col_slice;
/** 
 * Put a slice of columns.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_col` Index of the first columns index in a slice
 * - `num_col` Number of columns
 * - `row_idxs` Array of row indexes
 * - `cof` Coefficients
 */
ResCode put_col_slice(Task_t task,std::int32_t first_col,std::int32_t num_col,const std::int64_t* col_len,const std::int64_t* row_idxs,const double* cof) {
  return ptr_put_col_slice(task,first_col,num_col,col_len,row_idxs,cof);
}

std::function<ResCode(Task_t,std::int32_t,const std::int32_t*,const std::int64_t*,const std::int64_t**,const double**)> ptr_put_col_list;
/** 
 * Put a list of columns where the individual columns can be non-contiguous. 

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_col` Number of columns
 * - `col_idxs` Array of variable indexes
 * - `col_lens` Array of columns lengths
 * - `row_idxs` Array of row indexes
 * - `cof` Coefficients
 */
ResCode put_col_list(Task_t task,std::int32_t num_col,const std::int32_t* col_idxs,const std::int64_t* col_lens,const std::int64_t** row_idxs,const double** cof) {
  return ptr_put_col_list(task,num_col,col_idxs,col_lens,row_idxs,cof);
}

std::function<ResCode(Task_t,std::int64_t,std::int32_t,double)> ptr_put_ijc;
/** 
 * Put a non-zero triplet

 * Arguments
 * - `task` The optimizatioj task object
 * - `row_idx` Index of the affine row
 * - `var_idx` Variable index
 * - `cof` Coefficients
 */
ResCode put_ijc(Task_t task,std::int64_t row_idx,std::int32_t var_idx,double cof) {
  return ptr_put_ijc(task,row_idx,var_idx,cof);
}

std::function<ResCode(Task_t,std::int64_t,const std::int64_t*,const std::int32_t*,const double*)> ptr_put_ijc_list;
/** 
 * Put a list of non-zero triplets

 * Arguments
 * - `task` The optimizatioj task object
 * - `row_idxs` Array of row indexes
 * - `col_idxs` Array of variable indexes
 * - `cof` Coefficients
 */
ResCode put_ijc_list(Task_t task,std::int64_t num_nz,const std::int64_t* row_idxs,const std::int32_t* col_idxs,const double* cof) {
  return ptr_put_ijc_list(task,num_nz,row_idxs,col_idxs,cof);
}

std::function<ResCode(Task_t,std::int64_t,std::int32_t[1])> ptr_get_row_num_nz;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `row_idx` Index of the affine row
 */
ResCode get_row_num_nz(Task_t task,std::int64_t row_idx,std::int32_t num_nz[1]) {
  return ptr_get_row_num_nz(task,row_idx,num_nz);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t*)> ptr_get_row_slice_num_nz;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_row` Index of the first affine row in a slice
 * - `num_row` Number of affine rows
 */
ResCode get_row_slice_num_nz(Task_t task,std::int64_t first_row,std::int64_t num_row,std::int64_t* num_nz) {
  return ptr_get_row_slice_num_nz(task,first_row,num_row,num_nz);
}

std::function<ResCode(Task_t,std::int64_t,std::int32_t,std::int32_t*,double*)> ptr_get_row;
/** 
 * Get nonzeros from a single row.

 * Arguments
 * - `task` The optimizatioj task object
 * - `row_idx` Index of the affine row
 * - `nnz` Number of nonzeros
 * - `subj` Variable indexes
 * - `cof` Coefficients
 */
ResCode get_row(Task_t task,std::int64_t row_idx,std::int32_t nnz,std::int32_t* subj,double* cof) {
  return ptr_get_row(task,row_idx,nnz,subj,cof);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,std::int32_t*,std::int32_t*,double*)> ptr_get_row_slice;
/** 
 * Get nonzeros from a slice of rows.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_row` Index of the first affine row in a slice
 * - `num_row` Number of affine rows
 * - `nnz` Number of nonzeros
 * - `subj` Variable indexes
 * - `cof` Coefficients
 */
ResCode get_row_slice(Task_t task,std::int64_t first_row,std::int64_t num_row,std::int64_t nnz,std::int32_t* row_len,std::int32_t* subj,double* cof) {
  return ptr_get_row_slice(task,first_row,num_row,nnz,row_len,subj,cof);
}

std::function<ResCode(Task_t,std::int32_t,std::int64_t[1])> ptr_get_col_num_nz;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `col_idx` Variable index
 */
ResCode get_col_num_nz(Task_t task,std::int32_t col_idx,std::int64_t num_nz[1]) {
  return ptr_get_col_num_nz(task,col_idx,num_nz);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int64_t*)> ptr_get_col_slice_num_nz;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_col` Index of the first columns index in a slice
 * - `num_col` Number of columns
 */
ResCode get_col_slice_num_nz(Task_t task,std::int32_t first_col,std::int32_t num_col,std::int64_t* num_nz) {
  return ptr_get_col_slice_num_nz(task,first_col,num_col,num_nz);
}

std::function<ResCode(Task_t,std::int32_t,std::int64_t,std::int64_t*,double*)> ptr_get_col;
/** 
 * Get nonzeros from a single column.

 * Arguments
 * - `task` The optimizatioj task object
 * - `col_idx` Variable index
 * - `nnz` Number of nonzeros
 * - `cof` Coefficients
 */
ResCode get_col(Task_t task,std::int32_t col_idx,std::int64_t nnz,std::int64_t* subi,double* cof) {
  return ptr_get_col(task,col_idx,nnz,subi,cof);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int64_t,std::int64_t*,std::int64_t*,double*)> ptr_get_col_slice;
/** 
 * Get nonzeros from a slice of columns.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_col` Index of the first columns index in a slice
 * - `num_col` Number of columns
 * - `nnz` Number of nonzeros
 * - `cof` Coefficients
 */
ResCode get_col_slice(Task_t task,std::int32_t first_col,std::int32_t num_col,std::int64_t nnz,std::int64_t* col_len,std::int64_t* subi,double* cof) {
  return ptr_get_col_slice(task,first_col,num_col,nnz,col_len,subi,cof);
}

std::function<ResCode(Task_t,std::int64_t,std::int32_t,std::int64_t,const std::int64_t*,const double*)> ptr_put_bar_entry;
/** 
 * Input a single bar entry.

 * Arguments
 * - `task` The optimizatioj task object
 * - `row_idx` Index of the affine row
 * - `barvar_idx` Positive semi-definite variable index
 */
ResCode put_bar_entry(Task_t task,std::int64_t row_idx,std::int32_t barvar_idx,std::int64_t num_weight,const std::int64_t* matrix_idx,const double* weight) {
  return ptr_put_bar_entry(task,row_idx,barvar_idx,num_weight,matrix_idx,weight);
}

std::function<ResCode(Task_t,std::int64_t,const std::int64_t*,const std::int32_t*,const std::int64_t*,const std::int64_t*,const double*)> ptr_put_bar_entry_list;
/** 
 * Input a list of bar entries.

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_bar_entry` Number of entries
 * - `row_idx` Row index list
 * - `barvar_idx` Bar variable index list
 * - `num_weight` Per entry, the number of terms
 * - `matrix_idx` Matrix indexes
 */
ResCode put_bar_entry_list(Task_t task,std::int64_t num_bar_entry,const std::int64_t* row_idx,const std::int32_t* barvar_idx,const std::int64_t* num_weight,const std::int64_t* matrix_idx,const double* weight) {
  return ptr_put_bar_entry_list(task,num_bar_entry,row_idx,barvar_idx,num_weight,matrix_idx,weight);
}

std::function<ResCode(Task_t,std::int64_t,std::int32_t,const std::int32_t*,const std::int64_t*,const std::int64_t*,const double*)> ptr_put_bar_row;
/** 
 * Put bar entries for a single row, replacing all existing entries

 * Arguments
 * - `task` The optimizatioj task object
 * - `row_idx` Index of the affine row
 * - `barvar_idx` Positive semi-definite variable index
 */
ResCode put_bar_row(Task_t task,std::int64_t row_idx,std::int32_t num_bar_entry,const std::int32_t* barvar_idx,const std::int64_t* num_weight,const std::int64_t* matrix_idx,const double* weight) {
  return ptr_put_bar_row(task,row_idx,num_bar_entry,barvar_idx,num_weight,matrix_idx,weight);
}

std::function<ResCode(Task_t,std::int64_t,std::int32_t[1],std::int64_t[1])> ptr_get_symmat_info;
/** 
 * Get information on symmetric matrix.

 * Arguments
 * - `task` The optimizatioj task object
 * - `symmat_idx` Symmetric matrix index
 * - `dim` Dimension
 * - `nnz` Number of nonzeros
 */
ResCode get_symmat_info(Task_t task,std::int64_t symmat_idx,std::int32_t dim[1],std::int64_t nnz[1]) {
  return ptr_get_symmat_info(task,symmat_idx,dim,nnz);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int32_t*,std::int32_t*,double*)> ptr_get_symmat;
/** 
 * Get symmetric matrix data

 * Arguments
 * - `task` The optimizatioj task object
 * - `symmat_idx` Symmetric matrix index
 * - `nnz` Number of nonzeros
 * - `symmat_i` Symmetric matrix row subscripts
 * - `symmat_j` Symmetric matrix column subscripts
 * - `symmat_val` Symmetric matrix values
 */
ResCode get_symmat(Task_t task,std::int64_t symmat_idx,std::int64_t nnz,std::int32_t* symmat_i,std::int32_t* symmat_j,double* symmat_val) {
  return ptr_get_symmat(task,symmat_idx,nnz,symmat_i,symmat_j,symmat_val);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int32_t*,std::int64_t*)> ptr_get_symmat_slice_info;
/** 
 * Get information on a slice of symmetric matrixes.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_symmat` Index of the first symmetric matrix in a slice
 * - `num_symmat` Number of symmetric matrixes
 * - `dim` Dimension
 * - `nnz` Number of nonzeros
 */
ResCode get_symmat_slice_info(Task_t task,std::int64_t first_symmat,std::int64_t num_symmat,std::int32_t* dim,std::int64_t* nnz) {
  return ptr_get_symmat_slice_info(task,first_symmat,num_symmat,dim,nnz);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,std::int32_t*,std::int32_t*,double*)> ptr_get_symmat_slice;
/** 
 * Get information on a slice of symmetric matrixes.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_symmat` First symmetric matrix in slice
 * - `num_symmat` Number of symmetric matrixes in alice
 * - `total_nnz` Expected number of nonzeros
 * - `symmat_i` Symmetric matrix row subscripts
 * - `symmat_j` Symmetric matrix column subscripts
 * - `symmat_val` Symmetric matrix values
 */
ResCode get_symmat_slice(Task_t task,std::int64_t first_symmat,std::int64_t num_symmat,std::int64_t total_nnz,std::int32_t* symmat_i,std::int32_t* symmat_j,double* symmat_val) {
  return ptr_get_symmat_slice(task,first_symmat,num_symmat,total_nnz,symmat_i,symmat_j,symmat_val);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,const std::int64_t*,NULLABLE const double*)> ptr_append_con;
/** 
 * Append constraints.

 * Arguments
 * - `task` The optimizatioj task object
 * - `dom_idx` Index of the domain
 * - `num_rows` Row count for the constraint block.
 * - `row_idxs` Array of row indexes
 * - `con_offset` Constraint right-hand-side offset vector, where NULL means all zeros 
 */
ResCode append_con(Task_t task,std::int64_t dom_idx,std::int64_t num_rows,const std::int64_t* row_idxs,NULLABLE const double* con_offset) {
  return ptr_append_con(task,dom_idx,num_rows,row_idxs,con_offset);
}

std::function<ResCode(Task_t,std::int64_t,const std::int64_t*,std::int64_t,const std::int64_t*,NULLABLE const double*)> ptr_append_cons;
/** 
 * Append constraints.

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_con` Number of constraint blocks to add.
 * - `dom_idxs` Index of the domain to use. The domain's size must be exactly `num_rows`.
 * - `num_rows` Total number of rows we are using.
 * - `row_idxs` Array of row indexes
 * - `con_offset` Constraint right-hand-side offset vector, where NULL means all zeros 
 */
ResCode append_cons(Task_t task,std::int64_t num_con,const std::int64_t* dom_idxs,std::int64_t num_rows,const std::int64_t* row_idxs,NULLABLE const double* con_offset) {
  return ptr_append_cons(task,num_con,dom_idxs,num_rows,row_idxs,con_offset);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t,NULLABLE const double*)> ptr_append_cons_seq;
/** 
 * Append constraints with a range of rows and the same domain for all constraints.

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_con` Number of constraint blocks to add. 
 * - `dom_idx` Index of the domain to use. The domain's size must be exactly `num_rows/num_con`.
 * - `first_row` First row.
 * - `num_rows` Total number of rows we are using.
 * - `con_offset` Constraint right-hand-side offset vector, where NULL means all zeros 
 */
ResCode append_cons_seq(Task_t task,std::int64_t num_con,std::int64_t dom_idx,std::int64_t first_row,std::int64_t num_rows,NULLABLE const double* con_offset) {
  return ptr_append_cons_seq(task,num_con,dom_idx,first_row,num_rows,con_offset);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,const std::int64_t*,NULLABLE const double*)> ptr_put_con;
/** 
 * Put domain and row indexes for constraint `index`.

 * Arguments
 * - `task` The optimizatioj task object
 * - `con_idx` Index of the constraint to change.
 * - `num_rows` Total number of scalar rows we will input.
 * - `dom_idx` Index of the domain to use. The domain's size must be exactly `num_rows`. 
 * - `row_idxs` Array of row indexes
 * - `rhs_offset` Domain offset
 */
ResCode put_con(Task_t task,std::int64_t con_idx,std::int64_t num_rows,std::int64_t dom_idx,const std::int64_t* row_idxs,NULLABLE const double* rhs_offset) {
  return ptr_put_con(task,con_idx,num_rows,dom_idx,row_idxs,rhs_offset);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,double)> ptr_put_scalar_con;
/** 
 * Put domain and row indexes for a single scalar constraint `index`. 

 * Arguments
 * - `task` The optimizatioj task object
 * - `con_idx` Index of the constraint to change.
 * - `dom_idx` Index of the domain to use. The domain's size must be exactly 1. 
 * - `row_idx` Index of the affine row
 * - `rhs_offset` Domain offset
 */
ResCode put_scalar_con(Task_t task,std::int64_t con_idx,std::int64_t dom_idx,std::int64_t row_idx,double rhs_offset) {
  return ptr_put_scalar_con(task,con_idx,dom_idx,row_idx,rhs_offset);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,const std::int64_t*,const std::int64_t*,NULLABLE const double*)> ptr_put_con_slice;
/** 
 * Put domains and row indexes for a slice of constraints.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_con` Index of the constraint to change.
 * - `num_con` Index of the constraint to change.
 * - `num_rows` Total number of scalar rows we will input.
 * - `dom_idx` Indexes of the domains to use. The sum of domain sizees must be exactly `num_rows`. 
 * - `row_idx` Indexes of the scalar affine rows
 * - `rhs_offset` Domain offset
 */
ResCode put_con_slice(Task_t task,std::int64_t first_con,std::int64_t num_con,std::int64_t num_rows,const std::int64_t* dom_idx,const std::int64_t* row_idx,NULLABLE const double* rhs_offset) {
  return ptr_put_con_slice(task,first_con,num_con,num_rows,dom_idx,row_idx,rhs_offset);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t*)> ptr_get_con_slice_domains;
/** 
 * Get domains from a slice of constraints

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_con` First constraint
 * - `num_con` Number of constraints
 * - `dom_idx` Index of the domain
 */
ResCode get_con_slice_domains(Task_t task,std::int64_t first_con,std::int64_t num_con,std::int64_t* dom_idx) {
  return ptr_get_con_slice_domains(task,first_con,num_con,dom_idx);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t[1])> ptr_get_con_slice_num_row;
/** 
 * Get number of rows in a slice of constraints.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_con` First constraint index in a slice
 * - `num_con` Number of constraints
 * - `num_row` Number of affine rows
 */
ResCode get_con_slice_num_row(Task_t task,std::int64_t first_con,std::int64_t num_con,std::int64_t num_row[1]) {
  return ptr_get_con_slice_num_row(task,first_con,num_con,num_row);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t*,double*,std::int64_t*)> ptr_get_con_slice;
/** 
 * Get domains and indexes for a slice of constraints. Note that if the output is longer than the provided buffers, the result is silently truncated.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_con` Constraint index
 * - `num_con` Number of constraint
 * - `num_row` Length of row_index.
 * - `row_idx` Index of the affine row
 * - `rhs_offset` Domain offset
 * - `dom_idx` Index of the domain
 */
ResCode get_con_slice(Task_t task,std::int64_t first_con,std::int64_t num_con,std::int64_t num_row,std::int64_t* row_idx,double* rhs_offset,std::int64_t* dom_idx) {
  return ptr_get_con_slice(task,first_con,num_con,num_row,row_idx,rhs_offset,dom_idx);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,const std::int64_t*,const std::int64_t*,const std::int64_t*,const double*)> ptr_append_djc;
/** 
 * Put domain and row indexes for constraint `index`.

 * Arguments
 * - `task` The optimizatioj task object
 * - `num_rows` Total number of scalar rows we will input.
 * - `num_dom` Number of domains
 * - `num_terms` Number of terms
 * - `dom_idx` Index of the domains to use. The sum of the domain sizes must sum to `num_rows`.
 * - `term_size` Term sizes. Each entry denotes the number of domains in the corresponding term. These must sum to `num_domains`
 * - `row_idx` Indexes of the scalar affine rows
 * - `rhs_offset` Domain offset
 */
ResCode append_djc(Task_t task,std::int64_t num_rows,std::int64_t num_dom,std::int64_t num_terms,const std::int64_t* dom_idx,const std::int64_t* term_size,const std::int64_t* row_idx,const double* rhs_offset) {
  return ptr_append_djc(task,num_rows,num_dom,num_terms,dom_idx,term_size,row_idx,rhs_offset);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t,const std::int64_t*,const std::int64_t*,const std::int64_t*,const double*)> ptr_put_djc;
/** 
 * Put domain and row indexes for constraint `index`.

 * Arguments
 * - `task` The optimizatioj task object
 * - `djc_idx` Index of the djc to change.
 * - `num_rows` Total number of scalar rows we will input.
 * - `num_dom` Number of domains
 * - `num_terms` Number of terms
 * - `dom_idx` Index of the domains to use. The sum of the domain sizes must sum to `num_rows`.
 * - `term_size` Term sizes. Each entry denotes the number of domains in the corresponding term. These must sum to `num_domains`
 * - `row_idx` Indexes of the scalar affine rows
 * - `rhs_offset` Domain offset
 */
ResCode put_djc(Task_t task,std::int64_t djc_idx,std::int64_t num_rows,std::int64_t num_dom,std::int64_t num_terms,const std::int64_t* dom_idx,const std::int64_t* term_size,const std::int64_t* row_idx,const double* rhs_offset) {
  return ptr_put_djc(task,djc_idx,num_rows,num_dom,num_terms,dom_idx,term_size,row_idx,rhs_offset);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t,const std::int64_t*,const std::int64_t*,const std::int64_t*,const double*,const std::int64_t*)> ptr_put_djc_slice;
/** 
 * Put domains and row indexes for a slice of constraints.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_djc` Index of the constraint to change.
 * - `num_djc` Index of the constraint to change.
 * - `num_rows` Total number of scalar rows we will input.
 * - `num_dom` Number of domains
 * - `num_terms` Number of terms
 * - `dom_idx` Index of the domains to use. The sum of the domain sizes must sum to `num_rows`.
 * - `term_size` Term sizes. Each entry denotes the number of domains in the corresponding term. These must sum to `num_domains` 
 * - `row_idx` Indexes of the scalar affine rows
 * - `rhs_offset` Right-hand-side offset
 */
ResCode put_djc_slice(Task_t task,std::int64_t first_djc,std::int64_t num_djc,std::int64_t num_rows,std::int64_t num_dom,std::int64_t num_terms,const std::int64_t* dom_idx,const std::int64_t* term_size,const std::int64_t* row_idx,const double* rhs_offset,const std::int64_t* djc_numterm) {
  return ptr_put_djc_slice(task,first_djc,num_djc,num_rows,num_dom,num_terms,dom_idx,term_size,row_idx,rhs_offset,djc_numterm);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t[1],std::int64_t[1],std::int64_t[1])> ptr_get_djc_info;
/** 
 * Get information in a single DJC.

 * Arguments
 * - `task` The optimizatioj task object
 * - `djc_idx` DJC index
 * - `num_term` number of terms in DJC
 * - `num_dom` Number of domains
 * - `num_row` Number of affine rows
 */
ResCode get_djc_info(Task_t task,std::int64_t djc_idx,std::int64_t num_term[1],std::int64_t num_dom[1],std::int64_t num_row[1]) {
  return ptr_get_djc_info(task,djc_idx,num_term,num_dom,num_row);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t*,std::int64_t*,std::int64_t*,double*)> ptr_get_djc;
/** 
 * Get DJC data. Together with `MSK120_get_djc_info` this extracts DJC data.

 * Arguments
 * - `task` The optimizatioj task object
 * - `djc_idx` DJC index
 * - `num_terms` Expect number of terms
 * - `num_dom` Expect number of domain indexes
 * - `num_row` Expect number of rows
 * - `dom_idx` Index of the domain
 * - `row_idx` Index of the affine row
 * - `rhs_offset` Domain offset
 */
ResCode get_djc(Task_t task,std::int64_t djc_idx,std::int64_t num_terms,std::int64_t num_dom,std::int64_t num_row,std::int64_t* term_size,std::int64_t* dom_idx,std::int64_t* row_idx,double* rhs_offset) {
  return ptr_get_djc(task,djc_idx,num_terms,num_dom,num_row,term_size,dom_idx,row_idx,rhs_offset);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t[1],std::int64_t[1],std::int64_t[1])> ptr_get_djc_slice_info;
/** 
 * Get information on a slice of DJCs. This will return total sizes for terms, clauses and rows. To get information on the number of clauses and rows for each DJC, call `MSK120_get_djc_slice_term`.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_djc` first DJC index
 * - `num_djc` number of DJCs
 * - `num_term` total number of terms
 * - `num_dom` total number of clauses/domains
 * - `num_row` total number of rows
 */
ResCode get_djc_slice_info(Task_t task,std::int64_t first_djc,std::int64_t num_djc,std::int64_t num_term[1],std::int64_t num_dom[1],std::int64_t num_row[1]) {
  return ptr_get_djc_slice_info(task,first_djc,num_djc,num_term,num_dom,num_row);
}

std::function<ResCode(Task_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t,std::int64_t*,std::int64_t*,std::int64_t*,double*,std::int64_t*)> ptr_get_djc_slice;
/** 
 * Get information on a terms of a slice of DJCs. This will return total sizes for terms, clauses and rows. Together with `MSK120_get_djc_slice_info` this extracts DJC slice data.

 * Arguments
 * - `task` The optimizatioj task object
 * - `first_djc` first DJC index
 * - `num_djc` number of DJCs
 * - `num_term` Expect number of terms
 * - `num_dom` Expect number of domain indexes
 * - `num_row` Expect number of rows
 * - `dom_idx` Index of the domain
 * - `row_idx` Index of the affine row
 * - `rhs_offset` Domain offset
 */
ResCode get_djc_slice(Task_t task,std::int64_t first_djc,std::int64_t num_djc,std::int64_t num_term,std::int64_t num_dom,std::int64_t num_row,std::int64_t* term_size,std::int64_t* dom_idx,std::int64_t* row_idx,double* rhs_offset,std::int64_t* djc_num_term) {
  return ptr_get_djc_slice(task,first_djc,num_djc,num_term,num_dom,num_row,term_size,dom_idx,row_idx,rhs_offset,djc_num_term);
}

std::function<void(Task_t,ObjSense)> ptr_put_obj_sense;
/** 
 * Input objective sense.

 * Arguments
 * - `task` The optimizatioj task object
 */
void put_obj_sense(Task_t task,ObjSense sense) {
  ptr_put_obj_sense(task,sense);
}

std::function<ObjSense(Task_t)> ptr_get_obj_sense;
/** 
 * Get objectiev sense. This cannot fail.

 * Arguments
 * - `task` The optimizatioj task object
 */
ObjSense get_obj_sense(Task_t task) {
  return ptr_get_obj_sense(task);
}

std::function<ResCode(Task_t,std::int64_t)> ptr_put_obj_row;
/** 
 * Set the affine row to use as objective

 * Arguments
 * - `task` The optimizatioj task object
 * - `row_idx` Index of the affine row
 */
ResCode put_obj_row(Task_t task,std::int64_t row_idx) {
  return ptr_put_obj_row(task,row_idx);
}

std::function<void(Task_t,std::int64_t[1],int[1])> ptr_get_obj_row;
/** 
 * Get objective row index. If the objective row is set, `asgn[0]` will be set to 1 and `index[0]` is set to the row index, otherwise `asgn[0]` is set to 0.

 * Arguments
 * - `task` The optimizatioj task object
 * - `row_idx` Index of the affine row
 * - `asgn` Returns non-zero to indicate that a value was assigned, or zero if it was not
 */
void get_obj_row(Task_t task,std::int64_t row_idx[1],int asgn[1]) {
  ptr_get_obj_row(task,row_idx,asgn);
}

std::function<ResCode(Task_t,TrmCode[1])> ptr_optimize;
/** 
 * Call optimizer. On return, all input solutions have been cleared.

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode optimize(Task_t task,TrmCode trm[1]) {
  return ptr_optimize(task,trm);
}

std::function<ResCode(Task_t,StreamType)> ptr_solution_summary;
/** 
 * Prints a short summary of the current solutions. 

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode solution_summary(Task_t task,StreamType whichstream) {
  return ptr_solution_summary(task,whichstream);
}

std::function<ResCode(Task_t,TrmCode[1],CallbackHandle,CallbackFunc,CallbackHandle,IntSolCallbackFunc)> ptr_optimize_callback;
/** 
 * Call optimizer and provide callback functions. On return, all input solutions have been cleared.

 * Arguments
 * - `task` The optimizatioj task object
 * - `trm` Return the termination code here. Will always be set on return.
 */
ResCode optimize_callback(Task_t task,TrmCode trm[1],CallbackHandle cb_handle,CallbackFunc cb_func,CallbackHandle int_cb_handle,IntSolCallbackFunc int_cb_func) {
  return ptr_optimize_callback(task,trm,cb_handle,cb_func,int_cb_handle,int_cb_func);
}

std::function<void(Task_t,const char*,NULLABLE const char*)> ptr_put_remote_solver;
/** 
 * Specify a remote OptServer to use instead of built-in solver.

 * Arguments
 * - `task` The optimizatioj task object
 * - `server` Server name with protocol and port, e.g. `https://optserver.mydomain:9876`.
 */
void put_remote_solver(Task_t task,const char* server,NULLABLE const char* cert) {
  ptr_put_remote_solver(task,server,cert);
}

std::function<void(Task_t,NULLABLE const char*)> ptr_put_optserver_access_token;
/** 
 * Specify access token to be used for remote solving.

 * Arguments
 * - `task` The optimizatioj task object
 */
void put_optserver_access_token(Task_t task,NULLABLE const char* token) {
  ptr_put_optserver_access_token(task,token);
}

std::function<std::int32_t(Task_t)> ptr_get_num_sol;
/** 
 * Get number of solutions. This cannot fail.

 * Arguments
 * - `task` The optimizatioj task object
 */
std::int32_t get_num_sol(Task_t task) {
  return ptr_get_num_sol(task);
}

std::function<ResCode(Task_t,std::int32_t,SolType[1])> ptr_get_sol_type;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `sol_type` Returns the type of the solution requested
 */
ResCode get_sol_type(Task_t task,std::int32_t sol_idx,SolType sol_type[1]) {
  return ptr_get_sol_type(task,sol_idx,sol_type);
}

std::function<ResCode(Task_t,std::int32_t,SolSta[1],SolSta[1])> ptr_get_sol_status;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 */
ResCode get_sol_status(Task_t task,std::int32_t sol_idx,SolSta primal_sol_sta[1],SolSta dual_sol_sta[1]) {
  return ptr_get_sol_status(task,sol_idx,primal_sol_sta,dual_sol_sta);
}

std::function<ResCode(Task_t,std::int32_t,ProSta[1])> ptr_get_problem_status;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 */
ResCode get_problem_status(Task_t task,std::int32_t sol_idx,ProSta pro_sta[1]) {
  return ptr_get_problem_status(task,sol_idx,pro_sta);
}

std::function<ResCode(Task_t,std::int32_t,double[1])> ptr_get_primal_obj;
/** 
 * Get primal objective value for solution `sol_idx`.

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 */
ResCode get_primal_obj(Task_t task,std::int32_t sol_idx,double obj_val[1]) {
  return ptr_get_primal_obj(task,sol_idx,obj_val);
}

std::function<ResCode(Task_t,std::int32_t,double[1])> ptr_get_dual_obj;
/** 
 * Get dual objective value for solution `sol_idx`.

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 */
ResCode get_dual_obj(Task_t task,std::int32_t sol_idx,double obj_val[1]) {
  return ptr_get_dual_obj(task,sol_idx,obj_val);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t,double*)> ptr_get_sol_xx_slice;
/** 
 * Get primal variable solution slice. The value is always available, even if the primal solution status is unknown or undefined.

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index.
 * - `first_var` First in slice.
 * - `num_var` Number of elements in slice.
 */
ResCode get_sol_xx_slice(Task_t task,std::int32_t sol_idx,std::int32_t first_var,std::int32_t num_var,double* xx) {
  return ptr_get_sol_xx_slice(task,sol_idx,first_var,num_var,xx);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t,double*)> ptr_get_sol_slx_slice;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `first_var` Variable index
 * - `num_var` Number of variables
 */
ResCode get_sol_slx_slice(Task_t task,std::int32_t sol_idx,std::int32_t first_var,std::int32_t num_var,double* slx) {
  return ptr_get_sol_slx_slice(task,sol_idx,first_var,num_var,slx);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t,double*)> ptr_get_sol_sux_slice;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `first_var` Variable index
 * - `num_var` Number of variables
 */
ResCode get_sol_sux_slice(Task_t task,std::int32_t sol_idx,std::int32_t first_var,std::int32_t num_var,double* sux) {
  return ptr_get_sol_sux_slice(task,sol_idx,first_var,num_var,sux);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int64_t,double*)> ptr_get_sol_barxj;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `barvar_idx` Positive semi-definite variable index
 * - `num_var` Number of variables
 */
ResCode get_sol_barxj(Task_t task,std::int32_t sol_idx,std::int32_t barvar_idx,std::int64_t num_var,double* barx) {
  return ptr_get_sol_barxj(task,sol_idx,barvar_idx,num_var,barx);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int64_t,double*)> ptr_get_sol_barsj;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `barvar_idx` Positive semi-definite variable index
 * - `num_elm` Number of positive semidefinite non-zero entries
 */
ResCode get_sol_barsj(Task_t task,std::int32_t sol_idx,std::int32_t barvar_idx,std::int64_t num_elm,double* bars) {
  return ptr_get_sol_barsj(task,sol_idx,barvar_idx,num_elm,bars);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t,std::int64_t,double*)> ptr_get_sol_barx_slice;
/** 
 * Get primal semidefinite variable solution slice. This get the primal value for a slice of semidefinite variables. Note that the number of elements in the slice can be obtained with `MSK120_barvar_slice_num_elm`.

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index.
 * - `first_barvar` First in slice.
 * - `num_barvar` Number of variables in slice.
 * - `num_elm` Number of positive semidefinite non-zero entries
 */
ResCode get_sol_barx_slice(Task_t task,std::int32_t sol_idx,std::int32_t first_barvar,std::int32_t num_barvar,std::int64_t num_elm,double* barx) {
  return ptr_get_sol_barx_slice(task,sol_idx,first_barvar,num_barvar,num_elm,barx);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t,std::int64_t,double*)> ptr_get_sol_bars_slice;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `first_barvar` First semidefinite variable index in a slice
 * - `num_barvar` Number of variables
 * - `num_elm` Number of positive semidefinite non-zero entries
 */
ResCode get_sol_bars_slice(Task_t task,std::int32_t sol_idx,std::int32_t first_barvar,std::int32_t num_barvar,std::int64_t num_elm,double* bars) {
  return ptr_get_sol_bars_slice(task,sol_idx,first_barvar,num_barvar,num_elm,bars);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,int[1])> ptr_get_sol_basic_xj;
/** 
 * Get basis indicator for a single variable.

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` index of the constraint
 * - `var_idx` index of the variable
 */
ResCode get_sol_basic_xj(Task_t task,std::int32_t sol_idx,std::int32_t var_idx,int basic[1]) {
  return ptr_get_sol_basic_xj(task,sol_idx,var_idx,basic);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,int[1])> ptr_get_sol_basic_barx;
/** 
 * Get basis indicator for a single semidefinite variable. At the time of writing, it is not well-defined what a basic PSD variable is, exactly.

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` index of the constraint
 * - `barvar_idx` index of the variable
 */
ResCode get_sol_basic_barx(Task_t task,std::int32_t sol_idx,std::int32_t barvar_idx,int basic[1]) {
  return ptr_get_sol_basic_barx(task,sol_idx,barvar_idx,basic);
}

std::function<ResCode(Task_t,std::int32_t,std::int64_t,int[1])> ptr_get_sol_basic_con;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `con_idx` Constraint index 
 */
ResCode get_sol_basic_con(Task_t task,std::int32_t sol_idx,std::int64_t con_idx,int basic[1]) {
  return ptr_get_sol_basic_con(task,sol_idx,con_idx,basic);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,int[1],int[1])> ptr_get_sol_sta_var;
/** 
 * Get bound status indicator for a single variable. Return a value for upper and lower bounds indicating if they are binding or non-binding.

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Index of the constraint
 * - `var_idx` Index of the variable
 */
ResCode get_sol_sta_var(Task_t task,std::int32_t sol_idx,std::int32_t var_idx,int low_binding[1],int upr_binding[1]) {
  return ptr_get_sol_sta_var(task,sol_idx,var_idx,low_binding,upr_binding);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,int[1])> ptr_get_sol_sta_barx;
/** 
 * Get bound status indicator for a single semidefinite variable. *

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Index of the constraint
 * - `barvar_idx` Index of the variable
 */
ResCode get_sol_sta_barx(Task_t task,std::int32_t sol_idx,std::int32_t barvar_idx,int binding[1]) {
  return ptr_get_sol_sta_barx(task,sol_idx,barvar_idx,binding);
}

std::function<ResCode(Task_t,std::int32_t,std::int64_t,int[1])> ptr_get_sol_sta_con;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `con_idx` Constraint index 
 */
ResCode get_sol_sta_con(Task_t task,std::int32_t sol_idx,std::int64_t con_idx,int binding[1]) {
  return ptr_get_sol_sta_con(task,sol_idx,con_idx,binding);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t,int*)> ptr_get_sol_basic_x_slice;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `first_var` Variable index
 * - `num_var` Number of variables
 */
ResCode get_sol_basic_x_slice(Task_t task,std::int32_t sol_idx,std::int32_t first_var,std::int32_t num_var,int* basic) {
  return ptr_get_sol_basic_x_slice(task,sol_idx,first_var,num_var,basic);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t,int*)> ptr_get_sol_basic_barx_slice;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `first_var` Variable index
 * - `num_var` Number of variables
 */
ResCode get_sol_basic_barx_slice(Task_t task,std::int32_t sol_idx,std::int32_t first_var,std::int32_t num_var,int* basic) {
  return ptr_get_sol_basic_barx_slice(task,sol_idx,first_var,num_var,basic);
}

std::function<ResCode(Task_t,std::int32_t,std::int64_t,std::int64_t,int*)> ptr_get_sol_basic_con_slice;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `first_con` First constraint index in a slice
 * - `num_con` Number of constraints
 */
ResCode get_sol_basic_con_slice(Task_t task,std::int32_t sol_idx,std::int64_t first_con,std::int64_t num_con,int* basic) {
  return ptr_get_sol_basic_con_slice(task,sol_idx,first_con,num_con,basic);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t,int*,int*)> ptr_get_sol_sta_var_slice;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `first_var` Variable index
 * - `num_var` Number of variables
 */
ResCode get_sol_sta_var_slice(Task_t task,std::int32_t sol_idx,std::int32_t first_var,std::int32_t num_var,int* low_binding,int* upr_binding) {
  return ptr_get_sol_sta_var_slice(task,sol_idx,first_var,num_var,low_binding,upr_binding);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,std::int32_t,int*)> ptr_get_sol_sta_barx_slice;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `first_barvar` First semidefinite variable index in a slice
 * - `num_barvar` Number of variables
 */
ResCode get_sol_sta_barx_slice(Task_t task,std::int32_t sol_idx,std::int32_t first_barvar,std::int32_t num_barvar,int* bindnig) {
  return ptr_get_sol_sta_barx_slice(task,sol_idx,first_barvar,num_barvar,bindnig);
}

std::function<ResCode(Task_t,std::int32_t,std::int64_t,std::int64_t,int*)> ptr_get_sol_sta_con_slice;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `first_con` First constraint index in a slice
 * - `num_con` Number of constraints
 */
ResCode get_sol_sta_con_slice(Task_t task,std::int32_t sol_idx,std::int64_t first_con,std::int64_t num_con,int* binding) {
  return ptr_get_sol_sta_con_slice(task,sol_idx,first_con,num_con,binding);
}

std::function<ResCode(Task_t,std::int32_t,std::int64_t,std::int64_t,std::int64_t,double*)> ptr_get_sol_xc_slice;
/** 
 * Get primal solution for a slice of constraints, effectively the constraint expressions evaluated in solution.

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index.
 * - `first_con` First constraint.
 * - `num_con` Number of constraints.
 * - `num_elm` Total number of scalar elements in constraint slice.
 * - `xc` Constraint value at the given solution.
 */
ResCode get_sol_xc_slice(Task_t task,std::int32_t sol_idx,std::int64_t first_con,std::int64_t num_con,std::int64_t num_elm,double* xc) {
  return ptr_get_sol_xc_slice(task,sol_idx,first_con,num_con,num_elm,xc);
}

std::function<ResCode(Task_t,std::int32_t,std::int64_t,std::int64_t,std::int64_t,double*)> ptr_get_sol_y_slice;
/** 
 * Get dual solution for a slice of constraints.

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index.
 * - `first_con` First constraint.
 * - `num_con` Number of constraints.
 * - `num_elm` Total number of scalar elements in constraint slice.
 */
ResCode get_sol_y_slice(Task_t task,std::int32_t sol_idx,std::int64_t first_con,std::int64_t num_con,std::int64_t num_elm,double* y) {
  return ptr_get_sol_y_slice(task,sol_idx,first_con,num_con,num_elm,y);
}

std::function<std::int32_t(Task_t)> ptr_get_num_input_solutions;
/** 
 * Get current number of input solutions.

 * Arguments
 * - `task` The optimizatioj task object
 */
std::int32_t get_num_input_solutions(Task_t task) {
  return ptr_get_num_input_solutions(task);
}

std::function<ResCode(Task_t,std::int32_t)> ptr_copy_sol_to_input;
/** 
 * Copy an output solution to the input solutions.

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 */
ResCode copy_sol_to_input(Task_t task,std::int32_t sol_idx) {
  return ptr_copy_sol_to_input(task,sol_idx);
}

std::function<ResCode(Task_t,SolType)> ptr_append_sol;
/** 
 * Append an empty input solution.

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode append_sol(Task_t task,SolType soltype) {
  return ptr_append_sol(task,soltype);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,const double*)> ptr_put_sol_xx;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `num` Number of items
 */
ResCode put_sol_xx(Task_t task,std::int32_t sol_idx,std::int32_t num,const double* val) {
  return ptr_put_sol_xx(task,sol_idx,num,val);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,const double*)> ptr_put_sol_slx;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `num` Number of items
 */
ResCode put_sol_slx(Task_t task,std::int32_t sol_idx,std::int32_t num,const double* val) {
  return ptr_put_sol_slx(task,sol_idx,num,val);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,const double*)> ptr_put_sol_sux;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `num` Number of items
 */
ResCode put_sol_sux(Task_t task,std::int32_t sol_idx,std::int32_t num,const double* val) {
  return ptr_put_sol_sux(task,sol_idx,num,val);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,const std::int32_t*)> ptr_put_sol_basic_x;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `num` Number of items
 */
ResCode put_sol_basic_x(Task_t task,std::int32_t sol_idx,std::int32_t num,const std::int32_t* val) {
  return ptr_put_sol_basic_x(task,sol_idx,num,val);
}

std::function<ResCode(Task_t,std::int32_t,std::int64_t,const double*)> ptr_put_sol_barx;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `num` Number of items
 */
ResCode put_sol_barx(Task_t task,std::int32_t sol_idx,std::int64_t num,const double* val) {
  return ptr_put_sol_barx(task,sol_idx,num,val);
}

std::function<ResCode(Task_t,std::int32_t,std::int64_t,const double*)> ptr_put_sol_bars;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `num` Number of items
 */
ResCode put_sol_bars(Task_t task,std::int32_t sol_idx,std::int64_t num,const double* xx) {
  return ptr_put_sol_bars(task,sol_idx,num,xx);
}

std::function<ResCode(Task_t,std::int32_t,std::int64_t,std::int64_t,const double*)> ptr_put_sol_yi;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `num` Number of items
 */
ResCode put_sol_yi(Task_t task,std::int32_t sol_idx,std::int64_t i,std::int64_t num,const double* xx) {
  return ptr_put_sol_yi(task,sol_idx,i,num,xx);
}

std::function<ResCode(Task_t,std::int32_t,std::int64_t,const std::int32_t*)> ptr_put_sol_basic_c;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `sol_idx` Solution index
 * - `num` Number of items
 */
ResCode put_sol_basic_c(Task_t task,std::int32_t sol_idx,std::int64_t num,const std::int32_t* val) {
  return ptr_put_sol_basic_c(task,sol_idx,num,val);
}

std::function<std::int32_t()> ptr_get_num_iinf;
/** 
 */
std::int32_t get_num_iinf() {
  return ptr_get_num_iinf();
}

std::function<std::int32_t()> ptr_get_num_liinf;
/** 
 */
std::int32_t get_num_liinf() {
  return ptr_get_num_liinf();
}

std::function<std::int32_t()> ptr_get_num_dinf;
/** 
 */
std::int32_t get_num_dinf() {
  return ptr_get_num_dinf();
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t[1])> ptr_get_iinf;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode get_iinf(Task_t task,std::int32_t par_idx,std::int32_t value[1]) {
  return ptr_get_iinf(task,par_idx,value);
}

std::function<ResCode(Task_t,std::int32_t,std::int64_t[1])> ptr_get_liinf;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode get_liinf(Task_t task,std::int32_t par_idx,std::int64_t value[1]) {
  return ptr_get_liinf(task,par_idx,value);
}

std::function<ResCode(Task_t,std::int32_t,double[1])> ptr_get_dinf;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode get_dinf(Task_t task,std::int32_t par_idx,double value[1]) {
  return ptr_get_dinf(task,par_idx,value);
}

std::function<const char*(std::int32_t)> ptr_get_iinf_name;
/** 
 * Get the index of the integer information item corresponding to the given name. If the index is invalid, NULL is returned.

 * Arguments
 */
const char* get_iinf_name(std::int32_t par_idx) {
  return ptr_get_iinf_name(par_idx);
}

std::function<const char*(std::int32_t)> ptr_get_liinf_name;
/** 
 * Get the index of the long integer information item corresponding to the given name. If the index is invalid, NULL is returned.

 * Arguments
 */
const char* get_liinf_name(std::int32_t par_idx) {
  return ptr_get_liinf_name(par_idx);
}

std::function<const char*(std::int32_t)> ptr_get_dinf_name;
/** 
 * Get the index of the long integer information item corresponding to the given name. If the index is invalid, NULL is returned.

 * Arguments
 */
const char* get_dinf_name(std::int32_t par_idx) {
  return ptr_get_dinf_name(par_idx);
}

std::function<std::int32_t(const char*)> ptr_get_iinf_index;
/** 
 * This will retur the index of the integer item orresponding to name, or -1 if the name is not recognized.

 * Arguments
 * - `par_name` Name of the parameter
 */
std::int32_t get_iinf_index(const char* par_name) {
  return ptr_get_iinf_index(par_name);
}

std::function<std::int32_t(const char*)> ptr_get_liinf_index;
/** 
 * This will retur the index of the long integer item orresponding to name, or -1 if the name is not recognized.

 * Arguments
 * - `par_name` Name of the parameter
 */
std::int32_t get_liinf_index(const char* par_name) {
  return ptr_get_liinf_index(par_name);
}

std::function<std::int32_t(const char*)> ptr_get_dinf_index;
/** 
 * This will retur the index of the double item orresponding to name, or -1 if the name is not recognized.

 * Arguments
 */
std::int32_t get_dinf_index(const char* name) {
  return ptr_get_dinf_index(name);
}

std::function<std::int32_t(Task_t,const char*,double[1])> ptr_get_double_param;
/** 
 * Get the current value of a named parameter. 

 * Arguments
 * - `task` The optimizatioj task object
 * - `par_name` Name of the parameter
 */
std::int32_t get_double_param(Task_t task,const char* par_name,double value[1]) {
  return ptr_get_double_param(task,par_name,value);
}

std::function<std::int32_t(const char*)> ptr_get_double_param_index;
/** 
 * Get the index corresponding to a double parameter name.

 * Arguments
 * - `par_name` Name of the parameter
 */
std::int32_t get_double_param_index(const char* par_name) {
  return ptr_get_double_param_index(par_name);
}

std::function<const char*(std::int32_t)> ptr_get_double_param_name;
/** 
 * Get the index corresponding to a double parameter name.

 * Arguments
 */
const char* get_double_param_name(std::int32_t par_idx) {
  return ptr_get_double_param_name(par_idx);
}

std::function<std::int32_t()> ptr_get_num_double_param;
/** 
 * Get the index corresponding to a double parameter name.
 */
std::int32_t get_num_double_param() {
  return ptr_get_num_double_param();
}

std::function<void(Task_t,std::int32_t,double*)> ptr_get_all_double_params;
/** 
 * Get the index corresponding to a double parameter name.

 * Arguments
 * - `task` The optimizatioj task object
 * - `buf` Target buffer
 */
void get_all_double_params(Task_t task,std::int32_t buflen,double* buf) {
  ptr_get_all_double_params(task,buflen,buf);
}

std::function<ResCode(Task_t,std::int32_t,const double*)> ptr_put_all_double_params;
/** 
 * Set all double parameters.

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode put_all_double_params(Task_t task,std::int32_t num_par,const double* params) {
  return ptr_put_all_double_params(task,num_par,params);
}

std::function<std::int32_t(const char*)> ptr_get_int_param_index;
/** 
 * Get the index corresponding to a integer parameter name.

 * Arguments
 * - `par_name` Name of the parameter
 */
std::int32_t get_int_param_index(const char* par_name) {
  return ptr_get_int_param_index(par_name);
}

std::function<const char*(std::int32_t)> ptr_get_int_param_name;
/** 
 * Get the index corresponding to a integer parameter name.

 * Arguments
 */
const char* get_int_param_name(std::int32_t par_idx) {
  return ptr_get_int_param_name(par_idx);
}

std::function<std::int32_t()> ptr_get_num_int_param;
/** 
 * Get the index corresponding to a double parameter name.
 */
std::int32_t get_num_int_param() {
  return ptr_get_num_int_param();
}

std::function<void(Task_t,std::int32_t,std::int32_t*)> ptr_get_all_int_params;
/** 
 * Get the index corresponding to a double parameter name.

 * Arguments
 * - `task` The optimizatioj task object
 * - `buf` Target buffer
 */
void get_all_int_params(Task_t task,std::int32_t buflen,std::int32_t* buf) {
  ptr_get_all_int_params(task,buflen,buf);
}

std::function<ResCode(Task_t,std::int32_t,const std::int32_t*)> ptr_put_all_int_params;
/** 
 * Set all integer parameters.

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode put_all_int_params(Task_t task,std::int32_t num_par,const std::int32_t* params) {
  return ptr_put_all_int_params(task,num_par,params);
}

std::function<std::int32_t(Task_t,const char*,std::int32_t[1])> ptr_get_int_param;
/** 
 * Get the current value of a named parameter. 

 * Arguments
 * - `task` The optimizatioj task object
 * - `par_name` Name of the parameter
 */
std::int32_t get_int_param(Task_t task,const char* par_name,std::int32_t value[1]) {
  return ptr_get_int_param(task,par_name,value);
}

std::function<std::int32_t(Task_t,const char*)> ptr_get_param_str_len;
/** 
 * Get the length of the string representation of the current value of a named parameter. 

 * Arguments
 * - `task` The optimizatioj task object
 * - `par_name` Name of the parameter
 */
std::int32_t get_param_str_len(Task_t task,const char* par_name) {
  return ptr_get_param_str_len(task,par_name);
}

std::function<void(Task_t,const char*,std::int32_t,char*)> ptr_get_param_str;
/** 
 * Get the string representation of the current value of a named parameter. 

 * Arguments
 * - `task` The optimizatioj task object
 * - `buf` If the parameter is not recognized, the returned string is 0.
 */
void get_param_str(Task_t task,const char* name,std::int32_t length,char* buf) {
  ptr_get_param_str(task,name,length,buf);
}

std::function<ResCode(Task_t,const char*,double,int[1])> ptr_put_double_param;
/** 
 * Set the value of a named parameter.
 * 
 * The function will succeed if either the parameter is _not_ recognized or if the parameter is
 * recognized and the given value is within valid bounds it. If the parameter is recognized but
 * the value is invalid, it will fail.

 * Arguments
 * - `task` The optimizatioj task object
 * - `par_name` The parameter name is the lower case name of the MOSEK parameter without "MSK_" prefix
 * - `ok` Returns true if the parameter was found and successfully set, otherwise false. 
 */
ResCode put_double_param(Task_t task,const char* par_name,double value,int ok[1]) {
  return ptr_put_double_param(task,par_name,value,ok);
}

std::function<ResCode(Task_t,const char*,std::int32_t,int[1])> ptr_put_int_param;
/** 
 * Set the value of a named parameter.
 * 
 * The function will succeed if either the parameter is _not_ recognized or if the parameter is
 * recognized and the given value is within valid bounds it. If the parameter is recognized but
 * the value is invalid, it will fail.

 * Arguments
 * - `task` The optimizatioj task object
 * - `par_name` The parameter name is the lower case name of the MOSEK parameter without "MSK_" prefix
 * - `ok` Returns true if the parameter was found and successfully set, otherwise false. 
 */
ResCode put_int_param(Task_t task,const char* par_name,std::int32_t value,int ok[1]) {
  return ptr_put_int_param(task,par_name,value,ok);
}

std::function<ResCode(Task_t,const char*,const char*,int[1])> ptr_put_param_str;
/** 
 * Set the value of a named parameter as a string.
 * 
 * The function will succeed if either the parameter is _not_ recognized or if the parameter is
 * recognized and the given value is within valid bounds it. If the parameter is recognized but
 * the value is invalid, it will fail.

 * Arguments
 * - `task` The optimizatioj task object
 * - `par_name` The parameter name is the lower case name of the MOSEK parameter without "MSK_" prefix
 * - `value` The string representation of the parameter value. For double
 *   parameters, this is the ascii string representation of the
 *   floating point value. For integer parameters this can be
 *   either the ascii representation of the integer value or, for
 *   parameters that accept symbolic values, the lower case value
 *   name without "MSK_" prefix.
 * - `ok` Returns true if the parameter was found and successfully set, otherwise false. 
 */
ResCode put_param_str(Task_t task,const char* par_name,const char* value,int ok[1]) {
  return ptr_put_param_str(task,par_name,value,ok);
}

std::function<std::int32_t(Task_t)> ptr_get_task_name_len;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 */
std::int32_t get_task_name_len(Task_t task) {
  return ptr_get_task_name_len(task);
}

std::function<std::int32_t(Task_t)> ptr_get_obj_name_len;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 */
std::int32_t get_obj_name_len(Task_t task) {
  return ptr_get_obj_name_len(task);
}

std::function<void(Task_t,std::int32_t,char*)> ptr_get_task_name;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `buf` Target buffer
 */
void get_task_name(Task_t task,std::int32_t capacity,char* buf) {
  ptr_get_task_name(task,capacity,buf);
}

std::function<void(Task_t,std::int32_t,char*)> ptr_get_obj_name;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `buf` Target buffer
 */
void get_obj_name(Task_t task,std::int32_t capacity,char* buf) {
  ptr_get_obj_name(task,capacity,buf);
}

std::function<ResCode(Task_t,const char*)> ptr_put_task_name;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode put_task_name(Task_t task,const char* name) {
  return ptr_put_task_name(task,name);
}

std::function<ResCode(Task_t,const char*)> ptr_put_obj_name;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode put_obj_name(Task_t task,const char* name) {
  return ptr_put_obj_name(task,name);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t[1])> ptr_get_var_name_len;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `var_idx` Variable index
 */
ResCode get_var_name_len(Task_t task,std::int32_t var_idx,std::int32_t name_len[1]) {
  return ptr_get_var_name_len(task,var_idx,name_len);
}

std::function<std::int32_t(Task_t,std::int32_t)> ptr_get_var_name_len2;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `var_idx` Variable index
 */
std::int32_t get_var_name_len2(Task_t task,std::int32_t var_idx) {
  return ptr_get_var_name_len2(task,var_idx);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t[1])> ptr_get_barvar_name_len;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `barvar_idx` Positive semi-definite variable index
 */
ResCode get_barvar_name_len(Task_t task,std::int32_t barvar_idx,std::int32_t name_len[1]) {
  return ptr_get_barvar_name_len(task,barvar_idx,name_len);
}

std::function<std::int32_t(Task_t,std::int32_t)> ptr_get_barvar_name_len2;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `barvar_idx` Positive semi-definite variable index
 */
std::int32_t get_barvar_name_len2(Task_t task,std::int32_t barvar_idx) {
  return ptr_get_barvar_name_len2(task,barvar_idx);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,char*)> ptr_get_var_name;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `var_idx` Variable index
 * - `buf` Target buffer
 */
ResCode get_var_name(Task_t task,std::int32_t var_idx,std::int32_t capacity,char* buf) {
  return ptr_get_var_name(task,var_idx,capacity,buf);
}

std::function<ResCode(Task_t,std::int32_t,std::int32_t,char*)> ptr_get_barvar_name;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `barvar_idx` Positive semi-definite variable index
 * - `buf` Target buffer
 */
ResCode get_barvar_name(Task_t task,std::int32_t barvar_idx,std::int32_t capacity,char* buf) {
  return ptr_get_barvar_name(task,barvar_idx,capacity,buf);
}

std::function<ResCode(Task_t,std::int32_t,const char*)> ptr_put_var_name;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `var_idx` Variable index
 */
ResCode put_var_name(Task_t task,std::int32_t var_idx,const char* name) {
  return ptr_put_var_name(task,var_idx,name);
}

std::function<ResCode(Task_t,std::int32_t,const char*)> ptr_put_barvar_name;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `barvar_idx` Positive semi-definite variable index
 */
ResCode put_barvar_name(Task_t task,std::int32_t barvar_idx,const char* name) {
  return ptr_put_barvar_name(task,barvar_idx,name);
}

std::function<ResCode(Task_t,std::int64_t,std::int32_t[1])> ptr_get_con_name_len;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `con_idx` Constraint index 
 */
ResCode get_con_name_len(Task_t task,std::int64_t con_idx,std::int32_t len[1]) {
  return ptr_get_con_name_len(task,con_idx,len);
}

std::function<ResCode(Task_t,std::int64_t,std::int32_t[1])> ptr_get_djc_name_len;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `djc_idx` Disjunctive constraint index
 */
ResCode get_djc_name_len(Task_t task,std::int64_t djc_idx,std::int32_t len[1]) {
  return ptr_get_djc_name_len(task,djc_idx,len);
}

std::function<std::int32_t(Task_t,std::int64_t)> ptr_get_con_name_len2;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `con_idx` Constraint index 
 */
std::int32_t get_con_name_len2(Task_t task,std::int64_t con_idx) {
  return ptr_get_con_name_len2(task,con_idx);
}

std::function<std::int32_t(Task_t,std::int64_t)> ptr_get_djc_name_len2;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `djc_idx` Disjunctive constraint index
 */
std::int32_t get_djc_name_len2(Task_t task,std::int64_t djc_idx) {
  return ptr_get_djc_name_len2(task,djc_idx);
}

std::function<ResCode(Task_t,std::int64_t,std::int32_t,char*)> ptr_get_con_name;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `con_idx` Constraint index 
 * - `buf` Target buffer
 */
ResCode get_con_name(Task_t task,std::int64_t con_idx,std::int32_t capacity,char* buf) {
  return ptr_get_con_name(task,con_idx,capacity,buf);
}

std::function<ResCode(Task_t,std::int64_t,std::int32_t,char*)> ptr_get_djc_name;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `djc_idx` Disjunctive constraint index
 * - `buf` Target buffer
 */
ResCode get_djc_name(Task_t task,std::int64_t djc_idx,std::int32_t capacity,char* buf) {
  return ptr_get_djc_name(task,djc_idx,capacity,buf);
}

std::function<ResCode(Task_t,std::int64_t,const char*)> ptr_put_con_name;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `con_idx` Constraint index 
 * - `buf` Target buffer
 */
ResCode put_con_name(Task_t task,std::int64_t con_idx,const char* buf) {
  return ptr_put_con_name(task,con_idx,buf);
}

std::function<ResCode(Task_t,std::int64_t,const char*)> ptr_put_djc_name;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 * - `djc_idx` Disjunctive constraint index
 * - `buf` Target buffer
 */
ResCode put_djc_name(Task_t task,std::int64_t djc_idx,const char* buf) {
  return ptr_put_djc_name(task,djc_idx,buf);
}

std::function<ResCode(Task_t,const char*)> ptr_write_task_to_file;
/** 
 * Write task, base the format at on the file name extension.

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode write_task_to_file(Task_t task,const char* filename) {
  return ptr_write_task_to_file(task,filename);
}

std::function<ResCode(Task_t,Format,Compression,WriteHandle,WriteFunc)> ptr_write_task_to_handle;
/** 
 * Write task via a function and a handle.

 * Arguments
 * - `task` The optimizatioj task object
 * - `handle` Callback handle specified when inputting the callback function
 */
ResCode write_task_to_handle(Task_t task,Format format,Compression compress,WriteHandle handle,WriteFunc func) {
  return ptr_write_task_to_handle(task,format,compress,handle,func);
}

std::function<ResCode(Task_t,const char*)> ptr_write_solution_to_file;
/** 
 * Write solution, base the format at on the file name extension.

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode write_solution_to_file(Task_t task,const char* filename) {
  return ptr_write_solution_to_file(task,filename);
}

std::function<ResCode(Task_t,SolutionFormat,Compression,WriteHandle,WriteFunc)> ptr_write_solution_to_handle;
/** 
 * Write solution via a function and a handle.

 * Arguments
 * - `task` The optimizatioj task object
 * - `handle` Callback handle specified when inputting the callback function
 */
ResCode write_solution_to_handle(Task_t task,SolutionFormat format,Compression compress,WriteHandle handle,WriteFunc func) {
  return ptr_write_solution_to_handle(task,format,compress,handle,func);
}

std::function<ResCode(Task_t,const char*)> ptr_read_from_file;
/** 
 * Reset task and read data from file, base the format on the file extension.

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode read_from_file(Task_t task,const char* filename) {
  return ptr_read_from_file(task,filename);
}

std::function<ResCode(Task_t,Format,Compression,ReadHandle,ReadFunc)> ptr_read_from_handle;
/** 
 * Reset task and read data from stream.

 * Arguments
 * - `task` The optimizatioj task object
 * - `handle` Callback handle specified when inputting the callback function
 */
ResCode read_from_handle(Task_t task,Format format,Compression compress,ReadHandle handle,ReadFunc func) {
  return ptr_read_from_handle(task,format,compress,handle,func);
}

std::function<ResCode(Task_t,StreamType,const char*,int)> ptr_put_stream_file;
/** 
 * Write a stream to a file. This will open the file and either append to it or write a clear and rewrite it.

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode put_stream_file(Task_t task,StreamType whichstream,const char* filename,int append) {
  return ptr_put_stream_file(task,whichstream,filename,append);
}

std::function<ResCode(Task_t,StreamType)> ptr_clear_stream_file;
/** 
 * Close file attached to a stream.

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode clear_stream_file(Task_t task,StreamType whichstream) {
  return ptr_clear_stream_file(task,whichstream);
}

std::function<ResCode(Task_t,StreamType,WriteHandle,StreamFunc)> ptr_put_stream_callback;
/** 
 * Attach a log stream writer callback function to a stream. At most
 * one file and one callback can be attached to the same stream.
 * 
 * The callback method can be called at any point during a call to a
 * task method. It is forbidden to access the task it
 * is attached to from the callback function.

 * Arguments
 * - `task` The optimizatioj task object
 * - `handle` Callback handle specified when inputting the callback function
 */
ResCode put_stream_callback(Task_t task,StreamType whichstream,WriteHandle handle,StreamFunc func) {
  return ptr_put_stream_callback(task,whichstream,handle,func);
}

std::function<ResCode(Task_t,StreamType)> ptr_clear_stream_callback;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode clear_stream_callback(Task_t task,StreamType whichstream) {
  return ptr_clear_stream_callback(task,whichstream);
}

std::function<ResCode(Task_t,ErrorCallbackHandle,ErrorCallbackFunc)> ptr_put_error_callback;
/** 
 * Attach an error callback. This will be called whenever a task
 * function produces an error. It is forbidden to access
 * the task it is attached to from the callback function.

 * Arguments
 * - `task` The optimizatioj task object
 * - `handle` Callback handle specified when inputting the callback function
 */
ResCode put_error_callback(Task_t task,ErrorCallbackHandle handle,ErrorCallbackFunc func) {
  return ptr_put_error_callback(task,handle,func);
}

std::function<ResCode(Task_t,ErrorCallbackHandle,ErrorCallbackFunc)> ptr_put_warning_callback;
/** 
 * Attach a warning callback. This will be called whenever a task
 * function produces an error. It is forbidden to access the task it is
 * attached to from the callback function.

 * Arguments
 * - `task` The optimizatioj task object
 * - `handle` Callback handle specified when inputting the callback function
 */
ResCode put_warning_callback(Task_t task,ErrorCallbackHandle handle,ErrorCallbackFunc func) {
  return ptr_put_warning_callback(task,handle,func);
}

std::function<ResCode(Task_t)> ptr_clear_error_callback;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode clear_error_callback(Task_t task) {
  return ptr_clear_error_callback(task);
}

std::function<ResCode(Task_t)> ptr_clear_warning_callback;
/** 

 * Arguments
 * - `task` The optimizatioj task object
 */
ResCode clear_warning_callback(Task_t task) {
  return ptr_clear_warning_callback(task);
}

std::function<void()> ptr_license_cleanup;
/** 
 * Stops all threads and deletes all handles used by the license system. If this
 * function is called, it must be called as the last MOSEK API call. No other
 * MOSEK API calls are valid after this.
 */
void license_cleanup() {
  ptr_license_cleanup();
}

std::function<void()> ptr_shutdown_global_threadpool;
/** 
 * If MOSEK is using a global threadpool, attempt to shut
 * this down. If there are currently jobs running, this will do
 * nothing.
 */
void shutdown_global_threadpool() {
  ptr_shutdown_global_threadpool();
}

std::function<ResCode(std::int32_t,double,const double*,double*)> ptr_axpy;
/** 
 * Computes vector addition and multiplication by a scalar. 

 * Arguments
 * - `n` Length of the vectors. 
 * - `alpha` The scalar that multiplies x. 
 * - `x` The x vector. 
 * - `y` The y vector. 
 */
ResCode axpy(std::int32_t n,double alpha,const double* x,double* y) {
  return ptr_axpy(n,alpha,x,y);
}

std::function<ResCode(std::int32_t,const double*,const double*,double[1])> ptr_dot;
/** 
 * Computes the inner product of two vectors. 

 * Arguments
 * - `n` Length of the vectors. 
 * - `x` The x vector. 
 * - `y` The y vector. 
 * - `xty` The result of the inner product. 
 */
ResCode dot(std::int32_t n,const double* x,const double* y,double xty[1]) {
  return ptr_dot(n,x,y,xty);
}

std::function<ResCode(int,std::int32_t,std::int32_t,double,const double*,const double*,double,double*)> ptr_gemv;
/** 
 * Computes the multiplication of a scaled dense matrix times a dense vector, plus a scaled dense vector. Precisely, if `transa` is false then the update is
 * 
 * $$
 * y := \\alpha A x + \\beta y,
 * $$
 * 
 * and if `transa` is true
 * 
 * $$
 * y := \\alpha A^T x + \\beta y,
 * $$
 * 
 * where \\(\\alpha,\\beta\\) are scalar values and \\(A\\) is a matrix with \\(m\\) rows and \\(n\\) columns.
 * 
 * Note that the result is stored overwriting \\(y\\). It must not overlap with the other input arrays.

 * Arguments
 * - `transa` Indicates whether the matrix A must be transposed. 
 * - `m` Specifies the number of rows of the matrix A. 
 * - `n` Specifies the number of columns of the matrix A. 
 * - `alpha` A scalar value multiplying the matrix A. 
 * - `a` A pointer to the array storing matrix A in a column-major format. 
 * - `x` A pointer to the array storing the vector x. 
 * - `beta` A scalar value multiplying the vector y. 
 * - `y` A pointer to the array storing the vector y. 
 */
ResCode gemv(int transa,std::int32_t m,std::int32_t n,double alpha,const double* a,const double* x,double beta,double* y) {
  return ptr_gemv(transa,m,n,alpha,a,x,beta,y);
}

std::function<ResCode(int,int,std::int32_t,std::int32_t,std::int32_t,double,const double*,const double*,double,double*)> ptr_gemm;
/** 
 * Performs a matrix multiplication plus addition of dense matrices.
 * 
 * Given
 * \\(A\\), \\(B\\) and \\(C\\) of compatible dimensions, this function
 * computes
 * 
 * .. math:: C:= \\alpha \\mathrm{op}(A)\\mathrm{op}(B) + \\beta C
 * 
 * where \\(\\alpha,\\beta\\) are two scalar values. The function \\(\mathrm{op}(X)\\)
 * denotes \\(X\\) if transX is false, or \\(X^T\\) if set to true. The matrix \\(C\\) has \\(m\\) rows and \\(n\\) columns, and the other matrices must have compatible dimensions.
 * 
 * The result of this operation is stored in \\(C\\). It must not overlap with the other input arrays.

 * Arguments
 * - `transa` Indicates whether the matrix A must be transposed. 
 * - `transb` Indicates whether the matrix B must be transposed. 
 * - `m` Indicates the number of rows of matrix C. 
 * - `n` Indicates the number of columns of matrix C. 
 * - `k` Specifies the common dimension along which op(A) and op(B) are multiplied. 
 * - `alpha` A scalar value multiplying the result of the matrix multiplication. 
 * - `a` The pointer to the array storing matrix A in a column-major format. 
 * - `b` The pointer to the array storing matrix B in a column-major format.  
 * - `beta` A scalar value that multiplies C. 
 * - `c` The pointer to the array storing matrix C in a column-major format. 
 */
ResCode gemm(int transa,int transb,std::int32_t m,std::int32_t n,std::int32_t k,double alpha,const double* a,const double* b,double beta,double* c) {
  return ptr_gemm(transa,transb,m,n,k,alpha,a,b,beta,c);
}

std::function<ResCode(int,int,std::int32_t,std::int32_t,double,const double*,double,double*)> ptr_syrk;
/** 
 * Performs a symmetric rank-\\(k\\) update for a symmetric matrix.
 * 
 * Given a symmetric matrix \\(C\\in \\real^{n\\times n}\\), two scalars
 * \\(\\alpha,\\beta\\) and a matrix \\(A\\) of rank \\(k\\leq n\\), it
 * computes either
 * 
 * .. math:: C := \\alpha A A^T + \\beta C,
 * 
 * when `trans` is set to false and \\(A\\in \\real^{n\\times k}\\), or
 * 
 * .. math:: C := \\alpha A^T A + \\beta C,
 * 
 * when `trans` is set to true and \\(A\\in \\real^{k\\times n}\\).
 * 
 * Only the part of \\(C\\) indicated by `is_upr` is used and only that part is updated with the result. It must not overlap with the other input arrays.

 * Arguments
 * - `is_upr` Indicates whether the upper or lower triangular part of C is used. 
 * - `trans` Indicates whether the matrix A must be transposed. 
 * - `n` Specifies the order of \\(C\\).
 * - `k` Indicates the number of rows or columns of \\(A\\), depending on whether or not it is transposed, and its rank.
 * - `alpha` A scalar value multiplying the result of the matrix multiplication. 
 * - `a` The pointer to the array storing matrix A in a column-major format. 
 * - `beta` A scalar value that multiplies C. 
 * - `c` The pointer to the array storing matrix C in a column-major format. 
 */
ResCode syrk(int is_upr,int trans,std::int32_t n,std::int32_t k,double alpha,const double* a,double beta,double* c) {
  return ptr_syrk(is_upr,trans,n,k,alpha,a,beta,c);
}

std::function<ResCode(int,std::int32_t,const std::int32_t*,const std::int32_t*,const double*,double*)> ptr_sparse_triangular_solve_dense;
/** 
 * The function solves a triangular system of the form
 * 
 * .. math:: L x = b
 * 
 * or
 * 
 * .. math:: L^T x = b
 * 
 * where \\(L\\) is a sparse lower triangular nonsingular matrix. This implies in particular that diagonals in \\(L\\) are nonzero.

 * Arguments
 * - `transposed` Controls whether the solve is with L or the transposed L. 
 * - `n` Specifies the dimension of L. 
 * - `lnzc` `lnzc[j]` is the number of nonzeros in column j. 
 * - `lsubc` Row indexes for each column stored sequentially. 
 * - `lvalc` The value corresponding to row indexed stored lsubc. 
 * - `b` The right-hand side of linear equation system to be solved as a dense vector. 
 */
ResCode sparse_triangular_solve_dense(int transposed,std::int32_t n,const std::int32_t* lnzc,const std::int32_t* lsubc,const double* lvalc,double* b) {
  return ptr_sparse_triangular_solve_dense(transposed,n,lnzc,lsubc,lvalc,b);
}

std::function<ResCode(int,std::int32_t,double*)> ptr_potrf;
/** 
 * Computes a Cholesky factorization of a real symmetric positive definite dense matrix.

 * Arguments
 * - `is_upr` Indicates whether the upper or lower triangular part of the matrix is stored. 
 * - `n` Dimension of the symmetric matrix. 
 * - `a` A symmetric matrix stored in column-major order. 
 */
ResCode potrf(int is_upr,std::int32_t n,double* a) {
  return ptr_potrf(is_upr,n,a);
}

std::function<ResCode(int,std::int32_t,const double*,double*)> ptr_syeig;
/** 
 * Computes all eigenvalues of a real symmetric matrix \\(A\\). Given a matrix \\(A\\in\\real^{n\\times n}\\) it returns a vector \\(w\\in\\real^n\\) containing the eigenvalues of \\(A\\). 

 * Arguments
 * - `is_upr` Indicates whether the upper or lower triangular part is used. 
 * - `n` Dimension of the symmetric input matrix. 
 * - `a` Input matrix A. 
 * - `w` Array of length at least n containing the eigenvalues of A. 
 */
ResCode syeig(int is_upr,std::int32_t n,const double* a,double* w) {
  return ptr_syeig(is_upr,n,a,w);
}

std::function<ResCode(int,std::int32_t,double*,double*)> ptr_syevd;
/** 
 * Computes all the eigenvalues and eigenvectors a real symmetric matrix.
 * Given the input matrix \\(A\\in \\real^{n\\times n}\\), this function returns a
 * vector \\(w\\in \\real^n\\) containing the eigenvalues of \\(A\\) and it also computes the eigenvectors
 * of \\(A\\). Therefore, this function computes the eigenvalue decomposition of \\(A\\) as
 * 
 * .. math:: A= U V U^T,
 * 
 * where \\(V=\\diag(w)\\) and \\(U\\) contains the eigenvectors of \\(A\\).
 * 
 * Note that the matrix \\(U\\) overwrites the input data \\(A\\).

 * Arguments
 * - `is_upr` Indicates whether the upper or lower triangular part is used. 
 * - `n` Dimension of the symmetric input matrix. 
 * - `a` Input matrix A. 
 * - `w` Array of length at least n containing the eigenvalues of A. 
 */
ResCode syevd(int is_upr,std::int32_t n,double* a,double* w) {
  return ptr_syevd(is_upr,n,a,w);
}

std::function<ResCode(std::int32_t,int,double,std::int32_t,const std::int32_t*,const std::int32_t*,const double*,std::int32_t*,double*,AllocFunc,AllocHandle,std::int32_t*,std::int32_t**,double**)> ptr_compute_sparse_cholesky;
/** 
 * The function computes a Cholesky factorization of a sparse positive semidefinite matrix. Sparsity is exploited
 * during the computations to reduce the amount of space and work required. Both the input and output matrices
 * are represented using the sparse format.
 * 
 * To be precise, given a symmetric matrix \\(A \\in \\real^{n\\times n}\\) the function computes a nonsingular lower triangular matrix \\(L\\), a diagonal matrix \\(D\\) and a permutation matrix \\(P\\) such that
 * 
 * $$
 * LL^T - D = P A P^T
 * $$
 * 
 * If ``order_method`` is zero then reordering heuristics are not employed and \\(P\\) is the identity.
 * 
 * If a pivot during the computation of the Cholesky factorization is less than
 * 
 * $$
 * -\\rho\\cdot\\max((PAP^T)_{jj},1.0)
 * $$
 * 
 * then the matrix is declared negative semidefinite. On the hand if a pivot is smaller than
 * 
 * $$
 * \\rho\\cdot\\max((PAP^T)_{jj},1.0),
 * $$
 * 
 * then \\(D_{jj}\\) is increased from zero to
 * 
 * $$
 * \\rho\\cdot\\max((PAP^T)_{jj},1.0).
 * $$
 * 
 * Therefore, if \\(A\\) is sufficiently positive definite then \\(D\\) will be the zero matrix.
 * Here \\(\\rho\\) is set equal to value of ``tol_singular``.

 * Arguments
 * - `num_threads` The number threads that can be used to do the computation. 0 means the code makes the choice. 
 * - `order_method` If nonzero, then a sparsity preserving ordering will be employed. 
 * - `tol_singular` A positive parameter controlling when a pivot is declared zero. 
 * - `n` Specifies the order of \\(A\\). 
 * - `a_col_num_nonzero` `a_col_num_nonzero[j]` is a pointer to the first element in column \\(j\\). 
 * - `a_subi` Row indexes for each column stored in increasing order. 
 * - `a_val` The value corresponding to row indexed stored in asubc. 
 * - `perm` Permutation array used to specify the permutation matrix \\(P\\) computed by the function. 
 * - `diag` The diagonal elements of matrix \\(D\\). 
 * - `alloc` Memory allocation function for allocating the result. If `null`, the system `malloc` function is used. 
 * - `alloc_handle` Handle passed to the memory allocation function 
 * - `l_col_num_nonzero` `l_col_num_nonzero[j]` is the number of non zero elements in column \\(j\\) of \\(L\\). 
 * - `l_subi` Row indexes for each column stored in increasing order. The returned array is guaranteed to be allocated with the allocation function `alloc`.
 *   
 *   Notice that upon return, whether the function failed or suceeded, if a non-null value is returned here, it means that it was allocated and it must be deallocated acordingly.
 * - `l_val` The values corresponding to row indexed stored in lsubc. The returned array is guaranteed to be allocated with the allocation function `alloc`.
 *   
 *   Notice that upon return, whether the function failed or suceeded, if a non-null value is returned here, it means that it was allocated and it must be deallocated acordingly.
 */
ResCode compute_sparse_cholesky(std::int32_t num_threads,int order_method,double tol_singular,std::int32_t n,const std::int32_t* a_col_num_nonzero,const std::int32_t* a_subi,const double* a_val,std::int32_t* perm,double* diag,AllocFunc alloc,AllocHandle alloc_handle,std::int32_t* l_col_num_nonzero,std::int32_t** l_subi,double** l_val) {
  return ptr_compute_sparse_cholesky(num_threads,order_method,tol_singular,n,a_col_num_nonzero,a_subi,a_val,perm,diag,alloc,alloc_handle,l_col_num_nonzero,l_subi,l_val);
}

std::function<ResCode(int,double,std::int32_t,std::int64_t,const Task_t*,TrmCode*,ResCode*)> ptr_optimize_batch;
/** 
 * Optimize a number of tasks in parallel using a specified number of threads. All
 * callbacks and log output streams are disabled.
 * 
 * Assuming that each task takes about same time and there many more tasks than number of
 * threads then a linear speedup can be achieved, also known as strong scaling. A typical
 * application of this method is to solve many small tasks of similar type; in this case
 * it is recommended that each of them is allocated a single thread by setting parameter `ipar_num_threads` to 1.
 * 
 * If the parameters `is_race` or `max_time_sec` are used, then the result may not be deterministic, in the sense that the tasks which complete first may vary between runs.
 * 
 * The remaining behavior, including termination and response codes returned for each task, are the same as if each task was optimized separately.

 * Arguments
 * - `is_race` If nonzero, then the function is terminated after the first task has been completed.
 * - `max_time_sec` Time limit for the function in seconds. 
 * - `num_threads` Number of threads to be employed.
 * - `num_task` Number of tasks to optimize. 
 * - `tasks` An array of tasks to optimize in parallel. 
 * - `trm_code` The termination code for each task. 
 * - `res_code` The response code for each task. 
 */
ResCode optimize_batch(int is_race,double max_time_sec,std::int32_t num_threads,std::int64_t num_task,const Task_t* tasks,TrmCode* trm_code,ResCode* res_code) {
  return ptr_optimize_batch(is_race,max_time_sec,num_threads,num_task,tasks,trm_code,res_code);
}

std::function<ResCode(Feature)> ptr_check_out_license;
/** 
 * Checks out a license feature from the license server. Normally the required
 * license features will be automatically checked out the first time they are needed
 * by the function [optimize](#func-optimize). This function can be used to check out one
 * or more features ahead of time.
 * 
 * The feature will remain checked out until the environment is deleted or the function
 * [check_in_license](func:check_in_license) is called.
 * 
 * If a given feature is already checked out when this function is called, the call has no effect.

 * Arguments
 * - `feature` Feature to check out from the license system. 
 */
ResCode check_out_license(Feature feature) {
  return ptr_check_out_license(feature);
}

std::function<ResCode(Feature)> ptr_check_in_license;
/** 
 * Check in a license feature to the license server. By default all licenses
 * consumed by functions using a single environment are kept checked out for the
 * lifetime of the MOSEK environment. This function checks in a given license
 * feature back to the license server immediately.
 * 
 * If the given license feature is not checked out at all, or it is in use by a call to
 * [optimize](func:optimize), calling this function has no effect.
 * 
 * Please note that returning a license to the license server incurs a small
 * overhead, so frequent calls to this function should be avoided.

 * Arguments
 * - `feature` Feature to check in to the license system. 
 */
ResCode check_in_license(Feature feature) {
  return ptr_check_in_license(feature);
}

std::function<ResCode()> ptr_check_in_all;
/** 
 * Check in all unused license features to the license token server. 
 */
ResCode check_in_all() {
  return ptr_check_in_all();
}

std::function<ResCode(int)> ptr_echo_intro;
/** 
 * Prints an intro to message stream. 

 * Arguments
 * - `long_ver` If non-zero, then the intro is slightly longer. 
 */
ResCode echo_intro(int long_ver) {
  return ptr_echo_intro(long_ver);
}

std::function<void(std::int32_t[1],std::int32_t[1],std::int32_t[1])> ptr_get_version;
/** 
 * Obtains MOSEK version information. 

 * Arguments
 * - `major` Major version number. 
 * - `minor` Minor version number. 
 * - `revision` Revision number. 
 */
void get_version(std::int32_t major[1],std::int32_t minor[1],std::int32_t revision[1]) {
  ptr_get_version(major,minor,revision);
}

std::function<ResCode(int)> ptr_put_license_debug;
/** 
 * Enables debug information for the license system. If `lic_debug` is non-zero, then MOSEK will print debug info regarding the license checkout.  

 * Arguments
 * - `lic_debug` Whether license checkout debug info should be printed.  
 */
ResCode put_license_debug(int lic_debug) {
  return ptr_put_license_debug(lic_debug);
}

std::function<ResCode(NULLABLE const std::int32_t[21])> ptr_put_license_code;
/** 
 * Input a runtime license code.  This function has an effect only before the first optimization. 

 * Arguments
 * - `code` A license key string. 
 */
ResCode put_license_code(NULLABLE const std::int32_t code[21]) {
  return ptr_put_license_code(code);
}

std::function<ResCode(std::int32_t)> ptr_put_license_wait;
/** 
 * Control whether MOSEK should wait for an available license if no license is available. If `lic_wait` is non-zero, then MOSEK will wait for `lic_wait-1` milliseconds between each check for an available license.

 * Arguments
 * - `lic_wait` Enable waiting for a license until it is available. 
 */
ResCode put_license_wait(std::int32_t lic_wait) {
  return ptr_put_license_wait(lic_wait);
}

std::function<ResCode(NULLABLE const char*)> ptr_put_license_path;
/** 
 * Set the path to the license file. This function has an effect only before the first optimization. 

 * Arguments
 * - `license_path` A path specifying where to search for the license. 
 */
ResCode put_license_path(NULLABLE const char* license_path) {
  return ptr_put_license_path(license_path);
}

int library_initialized() {
    return libmosek_handle != NULL;
}

int initialize_library() {
    libmosek_handle = LOADLIBRARY(libname);
    if (!libmosek_handle) return 1;

    if (! loadsym(ptr_get_callback_code_name,libmosek_handle,"MSK12_get_callback_code_name")) goto END_ERROR;
    if (! loadsym(ptr_get_resp_name,libmosek_handle,"MSK12_get_resp_name")) goto END_ERROR;
    if (! loadsym(ptr_get_resp_descr,libmosek_handle,"MSK12_get_resp_descr")) goto END_ERROR;
    if (! loadsym(ptr_get_last_resp,libmosek_handle,"MSK12_get_last_resp")) goto END_ERROR;
    if (! loadsym(ptr_get_last_resp_msg,libmosek_handle,"MSK12_get_last_resp_msg")) goto END_ERROR;
    if (! loadsym(ptr_get_last_resp_msg_len,libmosek_handle,"MSK12_get_last_resp_msg_len")) goto END_ERROR;
    if (! loadsym(ptr_get_trm_name,libmosek_handle,"MSK12_get_trm_name")) goto END_ERROR;
    if (! loadsym(ptr_get_trm_descr,libmosek_handle,"MSK12_get_trm_descr")) goto END_ERROR;
    if (! loadsym(ptr_new_task_from_task,libmosek_handle,"MSK12_new_task_from_task")) goto END_ERROR;
    if (! loadsym(ptr_new_task,libmosek_handle,"MSK12_new_task")) goto END_ERROR;
    if (! loadsym(ptr_delete_task,libmosek_handle,"MSK12_delete_task")) goto END_ERROR;
    if (! loadsym(ptr_reserve_num_var,libmosek_handle,"MSK12_reserve_num_var")) goto END_ERROR;
    if (! loadsym(ptr_reserve_num_barvar,libmosek_handle,"MSK12_reserve_num_barvar")) goto END_ERROR;
    if (! loadsym(ptr_reserve_num_con,libmosek_handle,"MSK12_reserve_num_con")) goto END_ERROR;
    if (! loadsym(ptr_reserve_num_row,libmosek_handle,"MSK12_reserve_num_row")) goto END_ERROR;
    if (! loadsym(ptr_reserve_num_nz,libmosek_handle,"MSK12_reserve_num_nz")) goto END_ERROR;
    if (! loadsym(ptr_reserve_num_dom,libmosek_handle,"MSK12_reserve_num_dom")) goto END_ERROR;
    if (! loadsym(ptr_reserve_num_symmat,libmosek_handle,"MSK12_reserve_num_symmat")) goto END_ERROR;
    if (! loadsym(ptr_reserve_num_symmat_nz,libmosek_handle,"MSK12_reserve_num_symmat_nz")) goto END_ERROR;
    if (! loadsym(ptr_get_num_var,libmosek_handle,"MSK12_get_num_var")) goto END_ERROR;
    if (! loadsym(ptr_get_num_barvar,libmosek_handle,"MSK12_get_num_barvar")) goto END_ERROR;
    if (! loadsym(ptr_get_num_domain,libmosek_handle,"MSK12_get_num_domain")) goto END_ERROR;
    if (! loadsym(ptr_get_num_row,libmosek_handle,"MSK12_get_num_row")) goto END_ERROR;
    if (! loadsym(ptr_get_num_symmat,libmosek_handle,"MSK12_get_num_symmat")) goto END_ERROR;
    if (! loadsym(ptr_get_num_con,libmosek_handle,"MSK12_get_num_con")) goto END_ERROR;
    if (! loadsym(ptr_get_num_djc,libmosek_handle,"MSK12_get_num_djc")) goto END_ERROR;
    if (! loadsym(ptr_append_vars,libmosek_handle,"MSK12_append_vars")) goto END_ERROR;
    if (! loadsym(ptr_append_rows,libmosek_handle,"MSK12_append_rows")) goto END_ERROR;
    if (! loadsym(ptr_append_barvar,libmosek_handle,"MSK12_append_barvar")) goto END_ERROR;
    if (! loadsym(ptr_append_barvars,libmosek_handle,"MSK12_append_barvars")) goto END_ERROR;
    if (! loadsym(ptr_append_symmat,libmosek_handle,"MSK12_append_symmat")) goto END_ERROR;
    if (! loadsym(ptr_append_symmats,libmosek_handle,"MSK12_append_symmats")) goto END_ERROR;
    if (! loadsym(ptr_append_empty_cons,libmosek_handle,"MSK12_append_empty_cons")) goto END_ERROR;
    if (! loadsym(ptr_append_empty_djcs,libmosek_handle,"MSK12_append_empty_djcs")) goto END_ERROR;
    if (! loadsym(ptr_put_var_type,libmosek_handle,"MSK12_put_var_type")) goto END_ERROR;
    if (! loadsym(ptr_put_var_type_slice,libmosek_handle,"MSK12_put_var_type_slice")) goto END_ERROR;
    if (! loadsym(ptr_put_var_type_slice_value,libmosek_handle,"MSK12_put_var_type_slice_value")) goto END_ERROR;
    if (! loadsym(ptr_put_var_type_list,libmosek_handle,"MSK12_put_var_type_list")) goto END_ERROR;
    if (! loadsym(ptr_get_var_type,libmosek_handle,"MSK12_get_var_type")) goto END_ERROR;
    if (! loadsym(ptr_get_var_type_slice,libmosek_handle,"MSK12_get_var_type_slice")) goto END_ERROR;
    if (! loadsym(ptr_put_var_bound,libmosek_handle,"MSK12_put_var_bound")) goto END_ERROR;
    if (! loadsym(ptr_put_var_bound_slice,libmosek_handle,"MSK12_put_var_bound_slice")) goto END_ERROR;
    if (! loadsym(ptr_put_var_bound_slice_value,libmosek_handle,"MSK12_put_var_bound_slice_value")) goto END_ERROR;
    if (! loadsym(ptr_put_var_low_bound,libmosek_handle,"MSK12_put_var_low_bound")) goto END_ERROR;
    if (! loadsym(ptr_put_var_low_bound_slice,libmosek_handle,"MSK12_put_var_low_bound_slice")) goto END_ERROR;
    if (! loadsym(ptr_put_var_low_bound_slice_value,libmosek_handle,"MSK12_put_var_low_bound_slice_value")) goto END_ERROR;
    if (! loadsym(ptr_put_var_upr_bound,libmosek_handle,"MSK12_put_var_upr_bound")) goto END_ERROR;
    if (! loadsym(ptr_put_var_upr_bound_slice,libmosek_handle,"MSK12_put_var_upr_bound_slice")) goto END_ERROR;
    if (! loadsym(ptr_put_var_upr_bound_slice_value,libmosek_handle,"MSK12_put_var_upr_bound_slice_value")) goto END_ERROR;
    if (! loadsym(ptr_get_var_bound,libmosek_handle,"MSK12_get_var_bound")) goto END_ERROR;
    if (! loadsym(ptr_get_var_bound_slice,libmosek_handle,"MSK12_get_var_bound_slice")) goto END_ERROR;
    if (! loadsym(ptr_get_barvar_slice_num_elm,libmosek_handle,"MSK12_get_barvar_slice_num_elm")) goto END_ERROR;
    if (! loadsym(ptr_get_barvar_dim,libmosek_handle,"MSK12_get_barvar_dim")) goto END_ERROR;
    if (! loadsym(ptr_get_barvar_slice_dims,libmosek_handle,"MSK12_get_barvar_slice_dims")) goto END_ERROR;
    if (! loadsym(ptr_get_domain,libmosek_handle,"MSK12_get_domain")) goto END_ERROR;
    if (! loadsym(ptr_get_domain_empty,libmosek_handle,"MSK12_get_domain_empty")) goto END_ERROR;
    if (! loadsym(ptr_get_domain_rzero,libmosek_handle,"MSK12_get_domain_rzero")) goto END_ERROR;
    if (! loadsym(ptr_get_domain_rplus,libmosek_handle,"MSK12_get_domain_rplus")) goto END_ERROR;
    if (! loadsym(ptr_get_domain_rminus,libmosek_handle,"MSK12_get_domain_rminus")) goto END_ERROR;
    if (! loadsym(ptr_get_domain_r,libmosek_handle,"MSK12_get_domain_r")) goto END_ERROR;
    if (! loadsym(ptr_get_domain_quadratic_cone,libmosek_handle,"MSK12_get_domain_quadratic_cone")) goto END_ERROR;
    if (! loadsym(ptr_get_domain_rotated_quadratic_cone,libmosek_handle,"MSK12_get_domain_rotated_quadratic_cone")) goto END_ERROR;
    if (! loadsym(ptr_get_domain_primal_exponential_cone,libmosek_handle,"MSK12_get_domain_primal_exponential_cone")) goto END_ERROR;
    if (! loadsym(ptr_get_domain_dual_exponential_cone,libmosek_handle,"MSK12_get_domain_dual_exponential_cone")) goto END_ERROR;
    if (! loadsym(ptr_get_domain_primal_power_cone,libmosek_handle,"MSK12_get_domain_primal_power_cone")) goto END_ERROR;
    if (! loadsym(ptr_get_domain_dual_power_cone,libmosek_handle,"MSK12_get_domain_dual_power_cone")) goto END_ERROR;
    if (! loadsym(ptr_get_domain_primal_geometric_mean_cone,libmosek_handle,"MSK12_get_domain_primal_geometric_mean_cone")) goto END_ERROR;
    if (! loadsym(ptr_get_domain_dual_geometric_mean_cone,libmosek_handle,"MSK12_get_domain_dual_geometric_mean_cone")) goto END_ERROR;
    if (! loadsym(ptr_get_domain_svecpsd_cone,libmosek_handle,"MSK12_get_domain_svecpsd_cone")) goto END_ERROR;
    if (! loadsym(ptr_get_domain_info,libmosek_handle,"MSK12_get_domain_info")) goto END_ERROR;
    if (! loadsym(ptr_get_domain_alpha,libmosek_handle,"MSK12_get_domain_alpha")) goto END_ERROR;
    if (! loadsym(ptr_put_row,libmosek_handle,"MSK12_put_row")) goto END_ERROR;
    if (! loadsym(ptr_put_row_slice,libmosek_handle,"MSK12_put_row_slice")) goto END_ERROR;
    if (! loadsym(ptr_put_row_list,libmosek_handle,"MSK12_put_row_list")) goto END_ERROR;
    if (! loadsym(ptr_put_row_g,libmosek_handle,"MSK12_put_row_g")) goto END_ERROR;
    if (! loadsym(ptr_put_row_slice_g,libmosek_handle,"MSK12_put_row_slice_g")) goto END_ERROR;
    if (! loadsym(ptr_put_row_list_g,libmosek_handle,"MSK12_put_row_list_g")) goto END_ERROR;
    if (! loadsym(ptr_put_col,libmosek_handle,"MSK12_put_col")) goto END_ERROR;
    if (! loadsym(ptr_put_col_slice,libmosek_handle,"MSK12_put_col_slice")) goto END_ERROR;
    if (! loadsym(ptr_put_col_list,libmosek_handle,"MSK12_put_col_list")) goto END_ERROR;
    if (! loadsym(ptr_put_ijc,libmosek_handle,"MSK12_put_ijc")) goto END_ERROR;
    if (! loadsym(ptr_put_ijc_list,libmosek_handle,"MSK12_put_ijc_list")) goto END_ERROR;
    if (! loadsym(ptr_get_row_num_nz,libmosek_handle,"MSK12_get_row_num_nz")) goto END_ERROR;
    if (! loadsym(ptr_get_row_slice_num_nz,libmosek_handle,"MSK12_get_row_slice_num_nz")) goto END_ERROR;
    if (! loadsym(ptr_get_row,libmosek_handle,"MSK12_get_row")) goto END_ERROR;
    if (! loadsym(ptr_get_row_slice,libmosek_handle,"MSK12_get_row_slice")) goto END_ERROR;
    if (! loadsym(ptr_get_col_num_nz,libmosek_handle,"MSK12_get_col_num_nz")) goto END_ERROR;
    if (! loadsym(ptr_get_col_slice_num_nz,libmosek_handle,"MSK12_get_col_slice_num_nz")) goto END_ERROR;
    if (! loadsym(ptr_get_col,libmosek_handle,"MSK12_get_col")) goto END_ERROR;
    if (! loadsym(ptr_get_col_slice,libmosek_handle,"MSK12_get_col_slice")) goto END_ERROR;
    if (! loadsym(ptr_put_bar_entry,libmosek_handle,"MSK12_put_bar_entry")) goto END_ERROR;
    if (! loadsym(ptr_put_bar_entry_list,libmosek_handle,"MSK12_put_bar_entry_list")) goto END_ERROR;
    if (! loadsym(ptr_put_bar_row,libmosek_handle,"MSK12_put_bar_row")) goto END_ERROR;
    if (! loadsym(ptr_get_symmat_info,libmosek_handle,"MSK12_get_symmat_info")) goto END_ERROR;
    if (! loadsym(ptr_get_symmat,libmosek_handle,"MSK12_get_symmat")) goto END_ERROR;
    if (! loadsym(ptr_get_symmat_slice_info,libmosek_handle,"MSK12_get_symmat_slice_info")) goto END_ERROR;
    if (! loadsym(ptr_get_symmat_slice,libmosek_handle,"MSK12_get_symmat_slice")) goto END_ERROR;
    if (! loadsym(ptr_append_con,libmosek_handle,"MSK12_append_con")) goto END_ERROR;
    if (! loadsym(ptr_append_cons,libmosek_handle,"MSK12_append_cons")) goto END_ERROR;
    if (! loadsym(ptr_append_cons_seq,libmosek_handle,"MSK12_append_cons_seq")) goto END_ERROR;
    if (! loadsym(ptr_put_con,libmosek_handle,"MSK12_put_con")) goto END_ERROR;
    if (! loadsym(ptr_put_scalar_con,libmosek_handle,"MSK12_put_scalar_con")) goto END_ERROR;
    if (! loadsym(ptr_put_con_slice,libmosek_handle,"MSK12_put_con_slice")) goto END_ERROR;
    if (! loadsym(ptr_get_con_slice_domains,libmosek_handle,"MSK12_get_con_slice_domains")) goto END_ERROR;
    if (! loadsym(ptr_get_con_slice_num_row,libmosek_handle,"MSK12_get_con_slice_num_row")) goto END_ERROR;
    if (! loadsym(ptr_get_con_slice,libmosek_handle,"MSK12_get_con_slice")) goto END_ERROR;
    if (! loadsym(ptr_append_djc,libmosek_handle,"MSK12_append_djc")) goto END_ERROR;
    if (! loadsym(ptr_put_djc,libmosek_handle,"MSK12_put_djc")) goto END_ERROR;
    if (! loadsym(ptr_put_djc_slice,libmosek_handle,"MSK12_put_djc_slice")) goto END_ERROR;
    if (! loadsym(ptr_get_djc_info,libmosek_handle,"MSK12_get_djc_info")) goto END_ERROR;
    if (! loadsym(ptr_get_djc,libmosek_handle,"MSK12_get_djc")) goto END_ERROR;
    if (! loadsym(ptr_get_djc_slice_info,libmosek_handle,"MSK12_get_djc_slice_info")) goto END_ERROR;
    if (! loadsym(ptr_get_djc_slice,libmosek_handle,"MSK12_get_djc_slice")) goto END_ERROR;
    if (! loadsym(ptr_put_obj_sense,libmosek_handle,"MSK12_put_obj_sense")) goto END_ERROR;
    if (! loadsym(ptr_get_obj_sense,libmosek_handle,"MSK12_get_obj_sense")) goto END_ERROR;
    if (! loadsym(ptr_put_obj_row,libmosek_handle,"MSK12_put_obj_row")) goto END_ERROR;
    if (! loadsym(ptr_get_obj_row,libmosek_handle,"MSK12_get_obj_row")) goto END_ERROR;
    if (! loadsym(ptr_optimize,libmosek_handle,"MSK12_optimize")) goto END_ERROR;
    if (! loadsym(ptr_solution_summary,libmosek_handle,"MSK12_solution_summary")) goto END_ERROR;
    if (! loadsym(ptr_optimize_callback,libmosek_handle,"MSK12_optimize_callback")) goto END_ERROR;
    if (! loadsym(ptr_put_remote_solver,libmosek_handle,"MSK12_put_remote_solver")) goto END_ERROR;
    if (! loadsym(ptr_put_optserver_access_token,libmosek_handle,"MSK12_put_optserver_access_token")) goto END_ERROR;
    if (! loadsym(ptr_get_num_sol,libmosek_handle,"MSK12_get_num_sol")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_type,libmosek_handle,"MSK12_get_sol_type")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_status,libmosek_handle,"MSK12_get_sol_status")) goto END_ERROR;
    if (! loadsym(ptr_get_problem_status,libmosek_handle,"MSK12_get_problem_status")) goto END_ERROR;
    if (! loadsym(ptr_get_primal_obj,libmosek_handle,"MSK12_get_primal_obj")) goto END_ERROR;
    if (! loadsym(ptr_get_dual_obj,libmosek_handle,"MSK12_get_dual_obj")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_xx_slice,libmosek_handle,"MSK12_get_sol_xx_slice")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_slx_slice,libmosek_handle,"MSK12_get_sol_slx_slice")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_sux_slice,libmosek_handle,"MSK12_get_sol_sux_slice")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_barxj,libmosek_handle,"MSK12_get_sol_barxj")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_barsj,libmosek_handle,"MSK12_get_sol_barsj")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_barx_slice,libmosek_handle,"MSK12_get_sol_barx_slice")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_bars_slice,libmosek_handle,"MSK12_get_sol_bars_slice")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_basic_xj,libmosek_handle,"MSK12_get_sol_basic_xj")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_basic_barx,libmosek_handle,"MSK12_get_sol_basic_barx")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_basic_con,libmosek_handle,"MSK12_get_sol_basic_con")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_sta_var,libmosek_handle,"MSK12_get_sol_sta_var")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_sta_barx,libmosek_handle,"MSK12_get_sol_sta_barx")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_sta_con,libmosek_handle,"MSK12_get_sol_sta_con")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_basic_x_slice,libmosek_handle,"MSK12_get_sol_basic_x_slice")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_basic_barx_slice,libmosek_handle,"MSK12_get_sol_basic_barx_slice")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_basic_con_slice,libmosek_handle,"MSK12_get_sol_basic_con_slice")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_sta_var_slice,libmosek_handle,"MSK12_get_sol_sta_var_slice")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_sta_barx_slice,libmosek_handle,"MSK12_get_sol_sta_barx_slice")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_sta_con_slice,libmosek_handle,"MSK12_get_sol_sta_con_slice")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_xc_slice,libmosek_handle,"MSK12_get_sol_xc_slice")) goto END_ERROR;
    if (! loadsym(ptr_get_sol_y_slice,libmosek_handle,"MSK12_get_sol_y_slice")) goto END_ERROR;
    if (! loadsym(ptr_get_num_input_solutions,libmosek_handle,"MSK12_get_num_input_solutions")) goto END_ERROR;
    if (! loadsym(ptr_copy_sol_to_input,libmosek_handle,"MSK12_copy_sol_to_input")) goto END_ERROR;
    if (! loadsym(ptr_append_sol,libmosek_handle,"MSK12_append_sol")) goto END_ERROR;
    if (! loadsym(ptr_put_sol_xx,libmosek_handle,"MSK12_put_sol_xx")) goto END_ERROR;
    if (! loadsym(ptr_put_sol_slx,libmosek_handle,"MSK12_put_sol_slx")) goto END_ERROR;
    if (! loadsym(ptr_put_sol_sux,libmosek_handle,"MSK12_put_sol_sux")) goto END_ERROR;
    if (! loadsym(ptr_put_sol_basic_x,libmosek_handle,"MSK12_put_sol_basic_x")) goto END_ERROR;
    if (! loadsym(ptr_put_sol_barx,libmosek_handle,"MSK12_put_sol_barx")) goto END_ERROR;
    if (! loadsym(ptr_put_sol_bars,libmosek_handle,"MSK12_put_sol_bars")) goto END_ERROR;
    if (! loadsym(ptr_put_sol_yi,libmosek_handle,"MSK12_put_sol_yi")) goto END_ERROR;
    if (! loadsym(ptr_put_sol_basic_c,libmosek_handle,"MSK12_put_sol_basic_c")) goto END_ERROR;
    if (! loadsym(ptr_get_num_iinf,libmosek_handle,"MSK12_get_num_iinf")) goto END_ERROR;
    if (! loadsym(ptr_get_num_liinf,libmosek_handle,"MSK12_get_num_liinf")) goto END_ERROR;
    if (! loadsym(ptr_get_num_dinf,libmosek_handle,"MSK12_get_num_dinf")) goto END_ERROR;
    if (! loadsym(ptr_get_iinf,libmosek_handle,"MSK12_get_iinf")) goto END_ERROR;
    if (! loadsym(ptr_get_liinf,libmosek_handle,"MSK12_get_liinf")) goto END_ERROR;
    if (! loadsym(ptr_get_dinf,libmosek_handle,"MSK12_get_dinf")) goto END_ERROR;
    if (! loadsym(ptr_get_iinf_name,libmosek_handle,"MSK12_get_iinf_name")) goto END_ERROR;
    if (! loadsym(ptr_get_liinf_name,libmosek_handle,"MSK12_get_liinf_name")) goto END_ERROR;
    if (! loadsym(ptr_get_dinf_name,libmosek_handle,"MSK12_get_dinf_name")) goto END_ERROR;
    if (! loadsym(ptr_get_iinf_index,libmosek_handle,"MSK12_get_iinf_index")) goto END_ERROR;
    if (! loadsym(ptr_get_liinf_index,libmosek_handle,"MSK12_get_liinf_index")) goto END_ERROR;
    if (! loadsym(ptr_get_dinf_index,libmosek_handle,"MSK12_get_dinf_index")) goto END_ERROR;
    if (! loadsym(ptr_get_double_param,libmosek_handle,"MSK12_get_double_param")) goto END_ERROR;
    if (! loadsym(ptr_get_double_param_index,libmosek_handle,"MSK12_get_double_param_index")) goto END_ERROR;
    if (! loadsym(ptr_get_double_param_name,libmosek_handle,"MSK12_get_double_param_name")) goto END_ERROR;
    if (! loadsym(ptr_get_num_double_param,libmosek_handle,"MSK12_get_num_double_param")) goto END_ERROR;
    if (! loadsym(ptr_get_all_double_params,libmosek_handle,"MSK12_get_all_double_params")) goto END_ERROR;
    if (! loadsym(ptr_put_all_double_params,libmosek_handle,"MSK12_put_all_double_params")) goto END_ERROR;
    if (! loadsym(ptr_get_int_param_index,libmosek_handle,"MSK12_get_int_param_index")) goto END_ERROR;
    if (! loadsym(ptr_get_int_param_name,libmosek_handle,"MSK12_get_int_param_name")) goto END_ERROR;
    if (! loadsym(ptr_get_num_int_param,libmosek_handle,"MSK12_get_num_int_param")) goto END_ERROR;
    if (! loadsym(ptr_get_all_int_params,libmosek_handle,"MSK12_get_all_int_params")) goto END_ERROR;
    if (! loadsym(ptr_put_all_int_params,libmosek_handle,"MSK12_put_all_int_params")) goto END_ERROR;
    if (! loadsym(ptr_get_int_param,libmosek_handle,"MSK12_get_int_param")) goto END_ERROR;
    if (! loadsym(ptr_get_param_str_len,libmosek_handle,"MSK12_get_param_str_len")) goto END_ERROR;
    if (! loadsym(ptr_get_param_str,libmosek_handle,"MSK12_get_param_str")) goto END_ERROR;
    if (! loadsym(ptr_put_double_param,libmosek_handle,"MSK12_put_double_param")) goto END_ERROR;
    if (! loadsym(ptr_put_int_param,libmosek_handle,"MSK12_put_int_param")) goto END_ERROR;
    if (! loadsym(ptr_put_param_str,libmosek_handle,"MSK12_put_param_str")) goto END_ERROR;
    if (! loadsym(ptr_get_task_name_len,libmosek_handle,"MSK12_get_task_name_len")) goto END_ERROR;
    if (! loadsym(ptr_get_obj_name_len,libmosek_handle,"MSK12_get_obj_name_len")) goto END_ERROR;
    if (! loadsym(ptr_get_task_name,libmosek_handle,"MSK12_get_task_name")) goto END_ERROR;
    if (! loadsym(ptr_get_obj_name,libmosek_handle,"MSK12_get_obj_name")) goto END_ERROR;
    if (! loadsym(ptr_put_task_name,libmosek_handle,"MSK12_put_task_name")) goto END_ERROR;
    if (! loadsym(ptr_put_obj_name,libmosek_handle,"MSK12_put_obj_name")) goto END_ERROR;
    if (! loadsym(ptr_get_var_name_len,libmosek_handle,"MSK12_get_var_name_len")) goto END_ERROR;
    if (! loadsym(ptr_get_var_name_len2,libmosek_handle,"MSK12_get_var_name_len2")) goto END_ERROR;
    if (! loadsym(ptr_get_barvar_name_len,libmosek_handle,"MSK12_get_barvar_name_len")) goto END_ERROR;
    if (! loadsym(ptr_get_barvar_name_len2,libmosek_handle,"MSK12_get_barvar_name_len2")) goto END_ERROR;
    if (! loadsym(ptr_get_var_name,libmosek_handle,"MSK12_get_var_name")) goto END_ERROR;
    if (! loadsym(ptr_get_barvar_name,libmosek_handle,"MSK12_get_barvar_name")) goto END_ERROR;
    if (! loadsym(ptr_put_var_name,libmosek_handle,"MSK12_put_var_name")) goto END_ERROR;
    if (! loadsym(ptr_put_barvar_name,libmosek_handle,"MSK12_put_barvar_name")) goto END_ERROR;
    if (! loadsym(ptr_get_con_name_len,libmosek_handle,"MSK12_get_con_name_len")) goto END_ERROR;
    if (! loadsym(ptr_get_djc_name_len,libmosek_handle,"MSK12_get_djc_name_len")) goto END_ERROR;
    if (! loadsym(ptr_get_con_name_len2,libmosek_handle,"MSK12_get_con_name_len2")) goto END_ERROR;
    if (! loadsym(ptr_get_djc_name_len2,libmosek_handle,"MSK12_get_djc_name_len2")) goto END_ERROR;
    if (! loadsym(ptr_get_con_name,libmosek_handle,"MSK12_get_con_name")) goto END_ERROR;
    if (! loadsym(ptr_get_djc_name,libmosek_handle,"MSK12_get_djc_name")) goto END_ERROR;
    if (! loadsym(ptr_put_con_name,libmosek_handle,"MSK12_put_con_name")) goto END_ERROR;
    if (! loadsym(ptr_put_djc_name,libmosek_handle,"MSK12_put_djc_name")) goto END_ERROR;
    if (! loadsym(ptr_write_task_to_file,libmosek_handle,"MSK12_write_task_to_file")) goto END_ERROR;
    if (! loadsym(ptr_write_task_to_handle,libmosek_handle,"MSK12_write_task_to_handle")) goto END_ERROR;
    if (! loadsym(ptr_write_solution_to_file,libmosek_handle,"MSK12_write_solution_to_file")) goto END_ERROR;
    if (! loadsym(ptr_write_solution_to_handle,libmosek_handle,"MSK12_write_solution_to_handle")) goto END_ERROR;
    if (! loadsym(ptr_read_from_file,libmosek_handle,"MSK12_read_from_file")) goto END_ERROR;
    if (! loadsym(ptr_read_from_handle,libmosek_handle,"MSK12_read_from_handle")) goto END_ERROR;
    if (! loadsym(ptr_put_stream_file,libmosek_handle,"MSK12_put_stream_file")) goto END_ERROR;
    if (! loadsym(ptr_clear_stream_file,libmosek_handle,"MSK12_clear_stream_file")) goto END_ERROR;
    if (! loadsym(ptr_put_stream_callback,libmosek_handle,"MSK12_put_stream_callback")) goto END_ERROR;
    if (! loadsym(ptr_clear_stream_callback,libmosek_handle,"MSK12_clear_stream_callback")) goto END_ERROR;
    if (! loadsym(ptr_put_error_callback,libmosek_handle,"MSK12_put_error_callback")) goto END_ERROR;
    if (! loadsym(ptr_put_warning_callback,libmosek_handle,"MSK12_put_warning_callback")) goto END_ERROR;
    if (! loadsym(ptr_clear_error_callback,libmosek_handle,"MSK12_clear_error_callback")) goto END_ERROR;
    if (! loadsym(ptr_clear_warning_callback,libmosek_handle,"MSK12_clear_warning_callback")) goto END_ERROR;
    if (! loadsym(ptr_license_cleanup,libmosek_handle,"MSK12_license_cleanup")) goto END_ERROR;
    if (! loadsym(ptr_shutdown_global_threadpool,libmosek_handle,"MSK12_shutdown_global_threadpool")) goto END_ERROR;
    if (! loadsym(ptr_axpy,libmosek_handle,"MSK12_axpy")) goto END_ERROR;
    if (! loadsym(ptr_dot,libmosek_handle,"MSK12_dot")) goto END_ERROR;
    if (! loadsym(ptr_gemv,libmosek_handle,"MSK12_gemv")) goto END_ERROR;
    if (! loadsym(ptr_gemm,libmosek_handle,"MSK12_gemm")) goto END_ERROR;
    if (! loadsym(ptr_syrk,libmosek_handle,"MSK12_syrk")) goto END_ERROR;
    if (! loadsym(ptr_sparse_triangular_solve_dense,libmosek_handle,"MSK12_sparse_triangular_solve_dense")) goto END_ERROR;
    if (! loadsym(ptr_potrf,libmosek_handle,"MSK12_potrf")) goto END_ERROR;
    if (! loadsym(ptr_syeig,libmosek_handle,"MSK12_syeig")) goto END_ERROR;
    if (! loadsym(ptr_syevd,libmosek_handle,"MSK12_syevd")) goto END_ERROR;
    if (! loadsym(ptr_compute_sparse_cholesky,libmosek_handle,"MSK12_compute_sparse_cholesky")) goto END_ERROR;
    if (! loadsym(ptr_optimize_batch,libmosek_handle,"MSK12_optimize_batch")) goto END_ERROR;
    if (! loadsym(ptr_check_out_license,libmosek_handle,"MSK12_check_out_license")) goto END_ERROR;
    if (! loadsym(ptr_check_in_license,libmosek_handle,"MSK12_check_in_license")) goto END_ERROR;
    if (! loadsym(ptr_check_in_all,libmosek_handle,"MSK12_check_in_all")) goto END_ERROR;
    if (! loadsym(ptr_echo_intro,libmosek_handle,"MSK12_echo_intro")) goto END_ERROR;
    if (! loadsym(ptr_get_version,libmosek_handle,"MSK12_get_version")) goto END_ERROR;
    if (! loadsym(ptr_put_license_debug,libmosek_handle,"MSK12_put_license_debug")) goto END_ERROR;
    if (! loadsym(ptr_put_license_code,libmosek_handle,"MSK12_put_license_code")) goto END_ERROR;
    if (! loadsym(ptr_put_license_wait,libmosek_handle,"MSK12_put_license_wait")) goto END_ERROR;
    if (! loadsym(ptr_put_license_path,libmosek_handle,"MSK12_put_license_path")) goto END_ERROR;
    goto END_SUCCESS;
END_ERROR:
    UNLOADLIBRARY(libmosek_handle);
    libmosek_handle = NULL;
    return 1;
END_SUCCESS:
    return 0;
}
} // namespace operations_research::MSK12
