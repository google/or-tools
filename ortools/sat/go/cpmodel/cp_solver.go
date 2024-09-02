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

package cpmodel

import (
	"fmt"
	"sync"
	"unsafe"

	cmpb "github.com/google/or-tools/ortools/sat/proto/cpmodel"
	sppb "github.com/google/or-tools/ortools/sat/proto/satparameters"

	"google.golang.org/protobuf/proto"
)

/*
#include <stdlib.h> // for free
#include <stdint.h>
#include "ortools/sat/go/cpmodel/cp_solver_c.h"
*/
import "C"

// SolveCpModel solves a CP Model with the given input proto and returns a CPSolverResponse.
func SolveCpModel(input *cmpb.CpModelProto) (*cmpb.CpSolverResponse, error) {
	return SolveCpModelWithParameters(input, nil)
}

// SolveCpModelWithParameters solves a CP Model with the given input proto and solver
// parameters and returns a CPSolverResponse.
func SolveCpModelWithParameters(input *cmpb.CpModelProto, params *sppb.SatParameters) (*cmpb.CpSolverResponse, error) {
	// Transform `input` into bytes.
	bReq, err := proto.Marshal(input)
	if err != nil {
		return nil, fmt.Errorf("marshaling `input` failed: %w", err)
	}
	cReq := C.CBytes(bReq)
	defer C.free(cReq)

	// Transform `params` into bytes.
	bParams, err := proto.Marshal(params)
	if err != nil {
		return nil, fmt.Errorf("marshaling `params` failed: %w", err)
	}
	cParams := C.CBytes(bParams)
	defer C.free(cParams)

	var cRes unsafe.Pointer
	var cResLen C.int
	C.SolveCpModelWithParameters(cReq, C.int(len(bReq)), cParams, C.int(len(bParams)), &cRes, &cResLen)
	defer C.free(cRes)

	// Transform `cRes` into the Go response proto.
	bRes := C.GoBytes(cRes, cResLen)
	res := &cmpb.CpSolverResponse{}
	if err = proto.Unmarshal(bRes, res); err != nil {
		return nil, fmt.Errorf("unmarshaling `bRes` failed: %w", err)
	}

	return res, nil
}

// SolveCpModelInterruptibleWithParameters solves a CP Model with the given input proto
// and parameters and returns a CPSolverResponse. The solve can be interrupted by triggering
// the `interrupt`.
func SolveCpModelInterruptibleWithParameters(input *cmpb.CpModelProto, params *sppb.SatParameters, interrupt <-chan struct{}) (*cmpb.CpSolverResponse, error) {
	// Create the atomic bool for interrupting solves.
	limitReached := newAtomicBoolWrapper()
	defer limitReached.delete()

	// Transform `input` into bytes.
	bReq, err := proto.Marshal(input)
	if err != nil {
		return nil, err
	}
	cReq := C.CBytes(bReq)
	defer C.free(cReq)

	// Transform `params` input bytes.
	bParams, err := proto.Marshal(params)
	if err != nil {
		return nil, err
	}
	cParams := C.CBytes(bParams)
	defer C.free(cParams)

	solveDone := make(chan struct{})
	defer close(solveDone)
	// Wait for either to solve to finish or the solve to be interrupted.
	go func() {
		select {
		case <-interrupt:
			limitReached.trigger()
		case <-solveDone:
		}
	}()

	// We want to make sure we trigger the atomic Bool before we call the solver
	// if the input `interrupt` is already closed. We can't trust the
	// scheduler to execute the previous goroutine immediately, even calling
	// `runtime.Gosched()` (the unit test failed 3 out of 1000 times when doing
	// so).
	select {
	case <-interrupt:
		limitReached.trigger()
	default:
	}

	var cRes unsafe.Pointer
	var cResLen C.int
	C.SolveCpInterruptible(limitReached.ptr, cReq, C.int(len(bReq)), cParams, C.int(len(bParams)), &cRes, &cResLen)
	defer C.free(cRes)

	// Transform `cRes` into the Go response proto.
	bRes := C.GoBytes(cRes, cResLen)
	result := &cmpb.CpSolverResponse{}
	if err = proto.Unmarshal(bRes, result); err != nil {
		return nil, fmt.Errorf("unmarshaling `bRes` failed: %w", err)
	}

	return result, nil
}

// atomicBoolWrapper keeps a pointer on a C++ AtomicBool instance.
type atomicBoolWrapper struct {
	mutex sync.Mutex
	ptr   unsafe.Pointer // Guarded by mutex.
}

// newAtomicBoolWrapper returns a new instance of a C++ AtomicBool.
//
// The returned object must be destroyed with delete() for the C++ object not to
// leak.
//
// This object is thread-safe: delete() and trigger() can be called
// concurrently.
func newAtomicBoolWrapper() *atomicBoolWrapper {
	return &atomicBoolWrapper{
		ptr: C.SolveCpNewAtomicBool(),
	}
}

// trigger triggers the C++ SolveCpStopSolve method with the atomic bool.
//
// If the atomic bool has been deleted this has no effect.
func (intr *atomicBoolWrapper) trigger() {
	intr.mutex.Lock()
	defer intr.mutex.Unlock()
	if uintptr(intr.ptr) != 0 {
		C.SolveCpStopSolve(intr.ptr)
	}
}

// delete deletes the underlying C++ object.
//
// Calling it multiple times has not effect.
func (intr *atomicBoolWrapper) delete() {
	intr.mutex.Lock()
	defer intr.mutex.Unlock()
	// We don't test that intr.ptr is not nullptr here since C++ `delete` can be
	// called with nullptr.
	C.SolveCpDestroyAtomicBool(intr.ptr)
	intr.ptr = unsafe.Pointer(uintptr(0))
}

// SolutionBooleanValue returns the value of BoolVar `bv` in the response.
func SolutionBooleanValue(r *cmpb.CpSolverResponse, bv BoolVar) bool {
	return bv.evaluateSolutionValue(r) != 0
}

// SolutionIntegerValue returns the value of LinearArgument `la` in the response.
func SolutionIntegerValue(r *cmpb.CpSolverResponse, la LinearArgument) int64 {
	return la.evaluateSolutionValue(r)
}
