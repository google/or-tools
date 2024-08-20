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
	"testing"

	"google.golang.org/protobuf/proto"

	cmpb "github.com/google/or-tools/ortools/sat/proto/cpmodel"
	sppb "github.com/google/or-tools/ortools/sat/proto/satparameters"
)

func TestCpSolver_SolveIntVar(t *testing.T) {
	model := NewCpModelBuilder()

	x := model.NewIntVar(1, 10)
	y := model.NewIntVar(1, 10)

	model.AddEquality(NewLinearExpr().AddSum(x, y), NewConstant(15))
	model.Maximize(NewLinearExpr().AddTerm(x, 7).AddTerm(y, 1))

	m, err := model.Model()
	if err != nil {
		t.Fatalf("Model() returned with unexpected error %v", err)
	}

	res, err := SolveCpModel(m)
	if err != nil {
		t.Fatalf("SolveCpModel returned with unexpected err: %v", err)
	}
	wantStatus := cmpb.CpSolverStatus_OPTIMAL
	gotStatus := res.GetStatus()
	if wantStatus != gotStatus {
		t.Errorf("SolveCpModel() returned status = %v, want %v", gotStatus, wantStatus)
	}
	wantObj := float64(75)
	gotObj := res.GetObjectiveValue()
	if wantObj != gotObj {
		t.Errorf("SolveCpModel() returned objective = %v, got %v", gotObj, wantObj)
	}
	wantX := int64(10)
	wantY := int64(5)
	gotX := SolutionIntegerValue(res, x)
	gotY := SolutionIntegerValue(res, y)
	if wantX != gotX || wantY != gotY {
		t.Errorf("SolutionIntegerValue() returned (x, y) = (%v, %v), want (%v, %v)", gotX, gotY, wantX, wantY)
	}
}

func TestCpSolver_SolveBoolVar(t *testing.T) {
	model := NewCpModelBuilder()

	x := model.NewBoolVar()
	y := model.NewBoolVar()

	model.AddBoolOr(x, y.Not())
	model.Minimize(x)

	m, err := model.Model()
	if err != nil {
		t.Fatalf("Model() returned with unexpected error %v", err)
	}

	res, err := SolveCpModel(m)
	if err != nil {
		t.Fatalf("SolveCpModel returned with unexpected err: %v", err)
	}
	wantStatus := cmpb.CpSolverStatus_OPTIMAL
	gotStatus := res.GetStatus()
	if wantStatus != gotStatus {
		t.Errorf("SolveCpModel() return status = %v, want %v", gotStatus, wantStatus)
	}
	wantObj := float64(0)
	gotObj := res.GetObjectiveValue()
	if wantObj != gotObj {
		t.Errorf("SolveCpModel() returned objective = %v, want %v", gotObj, wantObj)
	}
	gotX := SolutionBooleanValue(res, x)
	gotY := SolutionBooleanValue(res, y)
	if gotX || gotY {
		t.Errorf("SolutionBooleanValue() returned (x, y) = (%v, %v), want (false, false)", gotX, gotY)
	}
	gotNotX := SolutionBooleanValue(res, x.Not())
	gotNotY := SolutionBooleanValue(res, y.Not())
	if !gotNotX || !gotNotY {
		t.Errorf("SolutionBooleanValue() returned (x.Not(), y.Not()) = (%v, %v), want (true, true)", gotX, gotY)
	}
	wantIntX := int64(0)
	wantIntY := int64(0)
	gotIntX := SolutionIntegerValue(res, x)
	gotIntY := SolutionIntegerValue(res, y)
	if wantIntX != gotIntX || wantIntY != gotIntY {
		t.Errorf("SolutionIntegerValue() returned (x, y) = (%v, %v), want (%v, %v)", gotIntX, gotIntY, wantIntX, wantIntY)
	}
	wantNotX := int64(1)
	wantNotY := int64(1)
	gotIntNotX := SolutionIntegerValue(res, x.Not())
	gotIntNotY := SolutionIntegerValue(res, y.Not())
	if wantNotX != gotIntNotX || wantNotY != gotIntNotY {
		t.Errorf("SolutionIntegerValue() returned (x, y) = (%v, %v), want (%v, %v)", gotIntNotX, gotIntNotY, wantNotX, wantNotY)
	}
}

func TestCpSolver_InvalidModel(t *testing.T) {
	model := NewCpModelBuilder()

	x := model.NewIntVar(0, -1)
	y := model.NewIntVar(0, 10)

	model.Maximize(NewLinearExpr().AddSum(x, y))

	m, err := model.Model()
	if err != nil {
		t.Fatalf("Model() returned with unexpected error %v", err)
	}

	res, err := SolveCpModel(m)
	if err != nil {
		t.Errorf("SolveCpModel returned with unexpected err: %v", err)
	}

	want := cmpb.CpSolverStatus_MODEL_INVALID
	got := res.GetStatus()
	if want != got {
		t.Errorf("SolveCpModel() returned status = %v, want %v", got, want)
	}
}

func TestCpSolver_InfeasibleModel(t *testing.T) {
	model := NewCpModelBuilder()

	x := model.NewIntVar(0, 5)
	y := model.NewIntVar(0, 5)

	// Infeasible constraint
	model.AddEquality(NewLinearExpr().AddSum(x, y), NewConstant(-5))

	m, err := model.Model()
	if err != nil {
		t.Fatalf("Model() returned with unexpected error %v", err)
	}

	res, err := SolveCpModel(m)
	if err != nil {
		t.Errorf("SolveCpModel returned with unexpected err: %v", err)
	}
	want := cmpb.CpSolverStatus_INFEASIBLE
	got := res.GetStatus()
	if want != got {
		t.Errorf("SolveCpModel() returned status = %v, want %v", got, want)
	}
}

func TestCpSolver_SolveWithParameters(t *testing.T) {
	model := NewCpModelBuilder()
	x := model.NewIntVar(0, 5)
	y := model.NewIntVar(0, 5)
	model.AddAllDifferent(x, y)
	model.Maximize(NewLinearExpr().AddTerm(x, 5).AddTerm(y, 6))

	params := &sppb.SatParameters{
		MaxTimeInSeconds: proto.Float64(-1),
	}

	m, err := model.Model()
	if err != nil {
		t.Fatalf("Model() returned with unexpected error %v", err)
	}

	res, err := SolveCpModelWithParameters(m, params)
	if err != nil {
		t.Errorf("SolveCpModel returned with unexpected err: %v", err)
	}
	want := cmpb.CpSolverStatus_MODEL_INVALID
	got := res.GetStatus()
	if want != got {
		t.Errorf("SolveCpModelWithParameters() returned status = %v, want %v", got, want)
	}
}

func TestCpSolver_SolveInterruptible(t *testing.T) {
	interrupt := make(chan struct{})
	close(interrupt)

	model := NewCpModelBuilder()
	x := model.NewIntVar(0, 5)
	y := model.NewIntVar(0, 5)
	model.AddAllDifferent(x, y)
	model.Maximize(NewLinearExpr().AddTerm(x, 5).AddTerm(y, 6))
	params := &sppb.SatParameters{
		MaxTimeInSeconds: proto.Float64(10),
	}

	m, err := model.Model()
	if err != nil {
		t.Fatalf("Model() returned with unexpected error %v", err)
	}

	res, err := SolveCpModelInterruptibleWithParameters(m, params, interrupt)
	if err != nil {
		t.Errorf("SolveCpModel returned with unexpected err: %v", err)
	}

	want := cmpb.CpSolverStatus_UNKNOWN
	got := res.GetStatus()
	if want != got {
		t.Errorf("SolveCpModelInterruptibleWithParameters() returned status = %v, want %v", got, want)
	}
}

func TestCpSolver_SolveInterruptible_NotCancelled(t *testing.T) {
	model := NewCpModelBuilder()
	x := model.NewIntVar(0, 5)
	y := model.NewIntVar(0, 5)
	model.AddAllDifferent(x, y)
	model.Maximize(NewLinearExpr().AddTerm(x, 5).AddTerm(y, 6))
	params := &sppb.SatParameters{
		MaxTimeInSeconds: proto.Float64(10),
	}

	m, err := model.Model()
	if err != nil {
		t.Fatalf("Model() returned with unexpected error %v", err)
	}

	interrupt := make(chan struct{})
	res, err := SolveCpModelInterruptibleWithParameters(m, params, interrupt)
	if err != nil {
		t.Errorf("SolveCpModel returned with unexpected err: %v", err)
	}
	want := cmpb.CpSolverStatus_OPTIMAL
	got := res.GetStatus()
	if want != got {
		t.Errorf("SolveCpModelInterruptibleWithParameters() returned status = %v, want %v", got, want)
	}
}

func TestCpSolver_SolveInterruptible_BadParameters(t *testing.T) {
	model := NewCpModelBuilder()
	x := model.NewIntVar(0, 5)
	y := model.NewIntVar(0, 5)
	model.AddAllDifferent(x, y)
	model.Maximize(NewLinearExpr().AddTerm(x, 5).AddTerm(y, 6))
	params := &sppb.SatParameters{
		MaxTimeInSeconds: proto.Float64(-1),
	}

	m, err := model.Model()
	if err != nil {
		t.Fatalf("Model() returned with unexpected error %v", err)
	}

	interrupt := make(chan struct{})
	res, err := SolveCpModelInterruptibleWithParameters(m, params, interrupt)
	if err != nil {
		t.Errorf("SolveCpModel returned with unexpected err: %v", err)
	}
	want := cmpb.CpSolverStatus_MODEL_INVALID
	got := res.GetStatus()
	if want != got {
		t.Errorf("SolveCpModelInterruptibleWithParameters() returned status = %v, want %v", got, want)
	}
}
