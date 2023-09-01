// Copyright 2010-2022 Google LLC
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

// MathOpt's C API for solving optimization models given as serialized protocol
// buffers.
//
// The MathOpt protocol buffers are used as inputs and outputs for many
// functions in this API, defined at ortools/math_opt/.*.proto.
// Protocol buffers have a language and machine independent binary format, and
// each supported language can serialize a message into this format. This API
// represents these serialized messages as void* and a size_t saying how many
// bytes long the void* is.
//
// Every language has a different mechanism for constructing a protocol buffer
// and serializing it. This API consumes the serialized proto directly, as it is
// designed for providing access to MathOpt from non-C languages that cannot
// call C++ functions directly, but can call C functions. Callers are expected
// to build protos in their language, serialize them, call these C functions,
// and then deserialize the returned bytes in their language.
//
// See cpp_example.cc for a minimal example of creating and serializing these
// protos from C++, calling the C API to solve the model, and then deserializing
// the returned protos.
#ifndef OR_TOOLS_MATH_OPT_CORE_C_API_SOLVER_H_
#define OR_TOOLS_MATH_OPT_CORE_C_API_SOLVER_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif  // __cplusplus

// Notifies MathOptSolve() if the user has requested that the solve stop early.
//
// This is passed as an argument to MathOptSolve(). From any thread, before or
// after the solve begins, you can trigger interruption with MathOptInterrupt().
//
// This is an opaque type you create with MathOptNewInterrupter(), pass by
// pointer, and then delete with MathOptFreeInterrupter() when done. You cannot
// copy or stack allocate this type.
struct MathOptInterrupter;

// Returns a new interrupter that has not been triggered. The caller must free
// this with MathOptFreeInterrupter().
struct MathOptInterrupter* MathOptNewInterrupter();

// Frees interrupter, has no effect when interrupter is NULL.
void MathOptFreeInterrupter(struct MathOptInterrupter* interrupter);

// Triggers the interrupter.
//
// Will CHECK fail if interrupter is NULL. This is threadsafe.
void MathOptInterrupt(struct MathOptInterrupter* interrupter);

// Checks if the interrupter is triggered.
//
// Will CHECK fail if interrupter is NULL. This is threadsafe.
int MathOptIsInterrupted(const struct MathOptInterrupter* interrupter);

// Solves an optimization model with MathOpt and returns the result.
//
// Arguments:
//   * model: a serialized ModelProto to solve. The function fails if this
//       cannot be parsed, or if this is NULL and model_size > 0.
//   * model_size: the size of model in bytes. Must be at most MAX_INT or the
//       the function fails.
//   * solver_type: which solver to use, see SolverTypeProto for numeric values.
//   * interrupter: ignored if NULL. If interrupted before the solve begins, or
//       from another thread while the solve is running, the solve will
//       terminate early with whatever results are available. MathOptSolve()
//       will not change the state (interrupted or not) of interrupter. It is
//       safe for concurrent calls to MathOptSolve() to share a single
//       interrupter. The interrupter must survive all calls to MathOptSolve().
//   * solve_result: an output argument, ignored if NULL. On success,
//       `*solve_result` is filled with a serialized SolveResultProto from
//       solving `model`. The caller must free `*solve_result` in this case with
//       MathOptFree(). On failure, `*solve_result` is set to NULL.
//   * solve_result_size: an output argument, ignored if NULL. On success,
//       `*solve_result_size` has the size in bytes of the serialized
//       SolveResultProto from solving `model` (the size of `*solve_result` if
//       set). On failure, `*solve_result_size` is set to zero.
//   * status_msg: an output argument. If NULL, this output is ignored. On
//       success, `*status_msg` is set to NULL. On failure, `*status_msg` is set
//       to a null terminated string describing the error. The caller must free
//       `*status_msg` with  MathOptFree() in this case.
//
// Note that `solve_result_size` holds the size of the serialized proto returned
// in `solve_result`. Typically, you should make `solve_result` and
// `solve_result_size` either both NULL or both not NULL. You cannot safely
// consume `solve_result` without `solve_result_size`.
//
// Returns 0 if successful and a nonzero value on failure (the value is an
// absl::StatusCode enum).
int MathOptSolve(const void* model, size_t model_size, int solver_type,
                 struct MathOptInterrupter* interrupter, void** solve_result,
                 size_t* solve_result_size, char** status_msg);

// Frees memory allocated by the MathOpt C API, e.g. the solve_result or
// status_msg output arguments from MathOptSolve(). If `ptr` is NULL, has no
// effect.
void MathOptFree(void* ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  // OR_TOOLS_MATH_OPT_CORE_C_API_SOLVER_H_
