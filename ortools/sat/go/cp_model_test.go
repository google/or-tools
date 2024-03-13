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
	"errors"
	"fmt"
	"math"
	"sort"
	"testing"

	"github.com/golang/glog"
	"golang/cmp/cmp"
	"golang/protobuf/v2/testing/protocmp/protocmp"
	cmpb "ortools/sat/cp_model_go_proto"
)

func Example() {
	model := NewCpModelBuilder()

	x := model.NewIntVar(1, 3)
	y := model.NewIntVar(1, 3)
	b := model.NewBoolVar()

	model.AddLessOrEqual(x, NewConstant(1)).OnlyEnforceIf(b)
	model.AddLessOrEqual(y, NewConstant(1)).OnlyEnforceIf(b.Not())

	obj := NewLinearExpr().AddSum(x, b.Not()).AddTerm(y, 5)
	model.Maximize(obj)
	m, err := model.Model()
	if err != nil {
		log.Fatalf("Building model returned with error %v", err)
	}

	res, err := SolveCpModel(m)
	if err != nil {
		log.Fatalf("CP solver returned with unexpected err %v", err)
	}
	if res.GetStatus() != cmpb.CpSolverStatus_FEASIBLE && res.GetStatus() != cmpb.CpSolverStatus_OPTIMAL {
		log.Fatalf("CP solver returned with status %v", res.GetStatus())
	}

	fmt.Println("Objective:", res.GetObjectiveValue())
	fmt.Println("x:", SolutionIntegerValue(res, x))
	fmt.Println("y:", SolutionIntegerValue(res, y))
	fmt.Println("Int b:", SolutionIntegerValue(res, b))
	fmt.Println("Bool b:", SolutionBooleanValue(res, b))
	// Output:
	// Objective: 16
	// x: 1
	// y: 3
	// Int b: 1
	// Bool b: true
}

func TestBoolVar_Not(t *testing.T) {
	model := NewCpModelBuilder()

	bv1 := model.NewBoolVar().WithName("bv1")
	bv2 := bv1.Not()
	bv3 := bv2.Not()

	want := 1*bv1.Index() - 1
	if got := bv2.Index(); got != want {
		t.Errorf("Index() = %v, want %v", got, want)
	}
	want = bv1.Index()
	if got := bv3.Index(); got != want {
		t.Errorf("Index() = %v, want %v", got, want)
	}
}

func TestVar_Name(t *testing.T) {
	testCases := []struct {
		name    string
		varName func() string
		want    string
	}{
		{
			name: "IntVarName",
			varName: func() string {
				model := NewCpModelBuilder()
				iv := model.NewIntVar(0, 10).WithName("iv1")
				return iv.Name()
			},
			want: "iv1",
		},
		{
			name: "BoolVarName",
			varName: func() string {
				model := NewCpModelBuilder()
				bv := model.NewBoolVar().WithName("bv1")
				return bv.Name()
			},
			want: "bv1",
		},
		{
			name: "IntervalVarName",
			varName: func() string {
				model := NewCpModelBuilder()
				iv := model.NewIntVar(0, 10)
				interval := model.NewIntervalVar(iv, iv, iv).WithName("interval")
				return interval.Name()
			},
			want: "interval",
		},
	}

	for _, test := range testCases {
		t.Run(test.name, func(t *testing.T) {
			got := test.varName()
			if got != test.want {
				t.Errorf("test.varName() = %#v, want %#v", got, test.want)
			}
		})
	}
}

func TestVar_BoolVarDomain(t *testing.T) {
	model := NewCpModelBuilder()

	bv1 := model.NewBoolVar()

	got, err := bv1.Domain()
	want := NewDomain(0, 1)
	if err != nil {
		t.Fatalf("Domain() returned with unexpected error %v", err)
	}
	if diff := cmp.Diff(want, got, cmp.AllowUnexported(Domain{}, ClosedInterval{})); diff != "" {
		t.Errorf("Domain() = %v, want %v", got, want)
	}
}

func TestVar_IntVarDomain(t *testing.T) {
	model := NewCpModelBuilder()
	testCases := []struct {
		name   string
		intVar IntVar
		want   Domain
	}{
		{
			name:   "DomainWithSingleInterval",
			intVar: model.NewIntVar(-10, 10),
			want:   NewDomain(-10, 10),
		},
		{
			name:   "DomainWithMultipleIntervals",
			intVar: model.NewIntVarFromDomain(FromValues([]int64{1, 2, 3, 5, 4, 6, 10, 12, 11, 15, 8})),
			want:   FromValues([]int64{1, 2, 3, 5, 4, 6, 10, 12, 11, 15, 8}),
		},
	}

	for _, test := range testCases {
		t.Run(test.name, func(t *testing.T) {
			got, err := test.intVar.Domain()
			if err != nil {
				t.Fatalf("Domain() returned with unexpected error %v", err)
			}
			if diff := cmp.Diff(test.want, got, cmp.AllowUnexported(Domain{}, ClosedInterval{})); diff != "" {
				t.Errorf("Domain() returned with unexpected diff (-want+got):\n%s", diff)
			}
		})
	}
}

func TestVar_IntegerVariableProto(t *testing.T) {
	testCases := []struct {
		name     string
		varIndex func(model *Builder) VarIndex
		want     *cmpb.IntegerVariableProto
	}{
		{
			name: "BoolVar",
			varIndex: func(model *Builder) VarIndex {
				bv := model.NewBoolVar()
				return bv.Index()
			},
			want: cmpb.IntegerVariableProto_builder{
				Domain: []int64{0, 1},
			}.Build(),
		},
		{
			name: "IntVar",
			varIndex: func(model *Builder) VarIndex {
				iv := model.NewIntVar(-10, 10)
				return iv.Index()
			},
			want: cmpb.IntegerVariableProto_builder{
				Domain: []int64{-10, 10},
			}.Build(),
		},
		{
			name: "IntVarFromDomain",
			varIndex: func(model *Builder) VarIndex {
				iv := model.NewIntVarFromDomain(FromValues([]int64{1, 2, 3, 5, 4, 6, 10, 12, 11, 15, 8}))
				return iv.Index()
			},
			want: cmpb.IntegerVariableProto_builder{
				Domain: FromValues([]int64{1, 2, 3, 5, 4, 6, 10, 12, 11, 15, 8}).FlattenedIntervals(),
			}.Build(),
		},
		{
			name: "ConstVar",
			varIndex: func(model *Builder) VarIndex {
				cv := model.NewConstant(10)
				return cv.Index()
			},
			want: cmpb.IntegerVariableProto_builder{
				Domain: []int64{10, 10},
			}.Build(),
		},
		{
			name: "TrueVar",
			varIndex: func(model *Builder) VarIndex {
				tv := model.TrueVar()
				return tv.Index()
			},
			want: cmpb.IntegerVariableProto_builder{
				Domain: []int64{1, 1},
			}.Build(),
		},
		{
			name: "FalseVar",
			varIndex: func(model *Builder) VarIndex {
				fv := model.FalseVar()
				return fv.Index()
			},
			want: cmpb.IntegerVariableProto_builder{
				Domain: []int64{0, 0},
			}.Build(),
		},
	}

	for _, test := range testCases {
		t.Run(test.name, func(t *testing.T) {
			model := NewCpModelBuilder()
			varIndex := test.varIndex(model)
			m := mustModel(t, model)
			got := m.GetVariables()[varIndex]
			if diff := cmp.Diff(test.want, got, protocmp.Transform()); diff != "" {
				t.Errorf("test.variable() returned with unexpected diff (-want+got):\n%s", diff)
			}
		})
	}
}

func TestVar_EvaluateSolutionValue(t *testing.T) {
	testCases := []struct {
		name                  string
		evaluateSolutionValue func() int64_t
		want                  int64_t
	}{
		{
			name: "IntVarEvaluateSolutionValue",
			evaluateSolutionValue: func() int64_t {
				model := NewCpModelBuilder()
				iv := model.NewIntVar(0, 10)
				response := cmpb.CpSolverResponse_builder{
					Solution: []int64{5},
				}.Build()
				return iv.evaluateSolutionValue(response)
			},
			want: 5,
		},
		{
			name: "BoolVarEvaluateSolutionValue",
			evaluateSolutionValue: func() int64_t {
				model := NewCpModelBuilder()
				bv := model.NewBoolVar()
				response := cmpb.CpSolverResponse_builder{
					Solution: []int64{0},
				}.Build()
				return bv.evaluateSolutionValue(response)
			},
			want: 0,
		},
		{
			name: "BoolVarNotEvaluateSolutionValue",
			evaluateSolutionValue: func() int64_t {
				model := NewCpModelBuilder()
				bv := model.NewBoolVar()
				response := cmpb.CpSolverResponse_builder{
					Solution: []int64{0},
				}.Build()
				return bv.Not().evaluateSolutionValue(response)
			},
			want: 1,
		},
		{
			name: "AddLinExpr",
			evaluateSolutionValue: func() int64_t {
				model := NewCpModelBuilder()
				iv := model.NewIntVar(0, 10)
				bv := model.NewBoolVar()
				le := NewLinearExpr().AddTerm(iv, 10).AddTerm(bv, 20).AddConstant(5)
				response := cmpb.CpSolverResponse_builder{
					Solution: []int64{5, 1},
				}.Build()
				return le.evaluateSolutionValue(response)
			},
			want: 75,
		},
	}

	for _, test := range testCases {
		t.Run(test.name, func(t *testing.T) {
			got := test.evaluateSolutionValue()
			if got != test.want {
				t.Errorf("test.evaluateSolutionValue() = %v, want %v", got, test.want)
			}
		})
	}
}

func TestVar_AddToLinearExpr(t *testing.T) {
	model := NewCpModelBuilder()

	iv := model.NewIntVar(-10, 10)
	bv := model.NewBoolVar()
	linExpr := NewLinearExpr().AddTerm(iv, 10).AddTerm(bv, 20).AddConstant(5)

	testCases := []struct {
		name         string
		addToLinExpr func() *LinearExpr
		want         *LinearExpr
	}{
		{
			name: "AddIntVar",
			addToLinExpr: func() *LinearExpr {
				addLinExpr := NewLinearExpr()
				iv.addToLinearExpr(addLinExpr, 2)
				return addLinExpr
			},
			want: &LinearExpr{varCoeffs: []varCoeff{{ind: iv.Index(), coeff: 2}}},
		},
		{
			name: "AddBoolVar",
			addToLinExpr: func() *LinearExpr {
				addLinExpr := NewLinearExpr()
				bv.addToLinearExpr(addLinExpr, 3)
				return addLinExpr
			},
			want: &LinearExpr{varCoeffs: []varCoeff{{ind: bv.Index(), coeff: 3}}},
		},
		{
			name: "AddBoolVarNot",
			addToLinExpr: func() *LinearExpr {
				addLinExpr := NewLinearExpr()
				bv.Not().addToLinearExpr(addLinExpr, 4)
				return addLinExpr
			},
			want: &LinearExpr{varCoeffs: []varCoeff{{ind: bv.Index(), coeff: -4}}, offset: 4},
		},
		{
			name: "AddLinExpr",
			addToLinExpr: func() *LinearExpr {
				addLinExpr := NewLinearExpr()
				linExpr.addToLinearExpr(addLinExpr, 5)
				return addLinExpr
			},
			want: &LinearExpr{
				varCoeffs: []varCoeff{
					{ind: iv.Index(), coeff: 50},
					{ind: bv.Index(), coeff: 100},
				},
				offset: 25,
			},
		},
		{
			name: "AddMultipleTerms",
			addToLinExpr: func() *LinearExpr {
				addLinExpr := NewLinearExpr()
				iv.addToLinearExpr(addLinExpr, 2)
				bv.addToLinearExpr(addLinExpr, 3)
				bv.Not().addToLinearExpr(addLinExpr, 4)
				linExpr.addToLinearExpr(addLinExpr, 5)
				return addLinExpr
			},
			want: &LinearExpr{
				varCoeffs: []varCoeff{
					{ind: iv.Index(), coeff: 2},
					{ind: bv.Index(), coeff: 3},
					{ind: bv.Index(), coeff: -4},
					{ind: iv.Index(), coeff: 50},
					{ind: bv.Index(), coeff: 100},
				},
				offset: 29,
			},
		},
	}

	for _, test := range testCases {
		t.Run(test.name, func(t *testing.T) {
			got := test.addToLinExpr()
			if diff := cmp.Diff(test.want, got, cmp.AllowUnexported(LinearExpr{}, varCoeff{})); diff != "" {
				t.Errorf("test.addToLinExpr() returned with unexpected diff (-want+got):\n%s", diff)
			}
		})
	}
}

func TestVar_AsLinearExpressionProto(t *testing.T) {
	model := NewCpModelBuilder()

	iv := model.NewIntVar(-10, 10)
	bv := model.NewBoolVar()
	linExpr := NewLinearExpr().AddTerm(iv, 10).AddTerm(bv, 20).AddConstant(5)

	testCases := []struct {
		name       string
		buildProto func() *cmpb.LinearExpressionProto
		want       *cmpb.LinearExpressionProto
	}{
		{
			name: "IntVar",
			buildProto: func() *cmpb.LinearExpressionProto {
				return iv.asLinearExpressionProto()
			},
			want: cmpb.LinearExpressionProto_builder{
				Vars:   []int32{int32_t(iv.Index())},
				Coeffs: []int64{1},
			}.Build(),
		},
		{
			name: "BoolVar",
			buildProto: func() *cmpb.LinearExpressionProto {
				return bv.asLinearExpressionProto()
			},
			want: cmpb.LinearExpressionProto_builder{
				Vars:   []int32{int32_t(bv.Index())},
				Coeffs: []int64{1},
			}.Build(),
		},
		{
			name: "BoolVarNot",
			buildProto: func() *cmpb.LinearExpressionProto {
				return bv.Not().asLinearExpressionProto()
			},
			want: cmpb.LinearExpressionProto_builder{
				Vars:   []int32{int32_t(bv.Index())},
				Coeffs: []int64{-1},
				Offset: 1,
			}.Build(),
		},
		{
			name: "LinearExpr",
			buildProto: func() *cmpb.LinearExpressionProto {
				return linExpr.asLinearExpressionProto()
			},
			want: cmpb.LinearExpressionProto_builder{
				Vars:   []int32{int32_t(iv.Index()), int32_t(bv.Index())},
				Coeffs: []int64{10, 20},
				Offset: 5,
			}.Build(),
		},
	}

	for _, test := range testCases {
		t.Run(test.name, func(t *testing.T) {
			got := test.buildProto()
			if diff := cmp.Diff(test.want, got, protocmp.Transform()); diff != "" {
				t.Errorf("test.buildProto() returned with unexpected diff (-want+got):\n%s", diff)
			}
		})
	}
}

func TestVar_AsNegatedLinearExpressionProto(t *testing.T) {
	model := NewCpModelBuilder()

	// These variables are declared separately since they need to be accessed by both the
	// `buildNegatedProto` and the `want` in the tests.
	iv := model.NewIntVar(-10, 10)
	bv := model.NewBoolVar()
	linExpr := NewLinearExpr().AddTerm(iv, 10).AddTerm(bv, 20).AddConstant(5)

	testCases := []struct {
		name              string
		buildNegatedProto func() *cmpb.LinearExpressionProto
		want              *cmpb.LinearExpressionProto
	}{
		{
			name: "IntVar",
			buildNegatedProto: func() *cmpb.LinearExpressionProto {
				return asNegatedLinearExpressionProto(iv)
			},
			want: cmpb.LinearExpressionProto_builder{
				Vars:   []int32{int32_t(iv.Index())},
				Coeffs: []int64{-1},
			}.Build(),
		},
		{
			name: "BoolVar",
			buildNegatedProto: func() *cmpb.LinearExpressionProto {
				return asNegatedLinearExpressionProto(bv)
			},
			want: cmpb.LinearExpressionProto_builder{
				Vars:   []int32{int32_t(bv.Index())},
				Coeffs: []int64{-1},
			}.Build(),
		},
		{
			name: "BoolVarNot",
			buildNegatedProto: func() *cmpb.LinearExpressionProto {
				return asNegatedLinearExpressionProto(bv.Not())
			},
			want: cmpb.LinearExpressionProto_builder{
				Vars:   []int32{int32_t(bv.Index())},
				Coeffs: []int64{1},
				Offset: -1,
			}.Build(),
		},
		{
			name: "LinearExpr",
			buildNegatedProto: func() *cmpb.LinearExpressionProto {
				return asNegatedLinearExpressionProto(linExpr)
			},
			want: cmpb.LinearExpressionProto_builder{
				Vars:   []int32{int32_t(iv.Index()), int32_t(bv.Index())},
				Coeffs: []int64{-10, -20},
				Offset: -5,
			}.Build(),
		},
	}

	for _, test := range testCases {
		t.Run(test.name, func(t *testing.T) {
			got := test.buildNegatedProto()
			if diff := cmp.Diff(test.want, got, protocmp.Transform()); diff != "" {
				t.Errorf("test.buildNegatedProto() returned with unexpected diff (-want+got):\n%s", diff)
			}
		})
	}
}

func TestLinearExpr(t *testing.T) {
	model := NewCpModelBuilder()

	iv1 := model.NewIntVar(2, 8).WithName("iv1")
	iv2 := model.NewIntVar(1, 5).WithName("iv2")
	bv := model.NewBoolVar().WithName("bv1")
	lin := NewLinearExpr().AddWeightedSum([]LinearArgument{iv1, bv}, []int64{10, 20})

	testCases := []struct {
		name      string
		buildExpr func() *LinearExpr
		want      *LinearExpr
	}{
		{
			name:      "NewConstant",
			buildExpr: func() *LinearExpr { return NewConstant(42) },
			want:      &LinearExpr{varCoeffs: nil, offset: 42},
		},
		{
			name:      "AddIntVar",
			buildExpr: func() *LinearExpr { return NewLinearExpr().Add(iv1) },
			want: &LinearExpr{
				varCoeffs: []varCoeff{{ind: iv1.Index(), coeff: 1}},
				offset:    0},
		},
		{
			name:      "AddBoolVar",
			buildExpr: func() *LinearExpr { return NewLinearExpr().Add(bv) },
			want: &LinearExpr{
				varCoeffs: []varCoeff{{ind: bv.Index(), coeff: 1}},
				offset:    0},
		},
		{
			name:      "AddBoolVarNot",
			buildExpr: func() *LinearExpr { return NewLinearExpr().Add(bv.Not()) },
			want: &LinearExpr{
				varCoeffs: []varCoeff{{ind: bv.Index(), coeff: -1}},
				offset:    1},
		},
		{
			name:      "AddConstant",
			buildExpr: func() *LinearExpr { return NewLinearExpr().Add(bv).AddConstant(100) },
			want: &LinearExpr{
				varCoeffs: []varCoeff{{ind: bv.Index(), coeff: 1}},
				offset:    100},
		},
		{
			name:      "AddTerm",
			buildExpr: func() *LinearExpr { return NewLinearExpr().AddTerm(iv2, 10) },
			want: &LinearExpr{
				varCoeffs: []varCoeff{{ind: iv2.Index(), coeff: 10}},
				offset:    0},
		},
		{
			name:      "AddSameVar",
			buildExpr: func() *LinearExpr { return NewLinearExpr().Add(iv1).AddTerm(iv1, 2) },
			want: &LinearExpr{
				varCoeffs: []varCoeff{
					{ind: iv1.Index(), coeff: 1},
					{ind: iv1.Index(), coeff: 2},
				},
				offset: 0},
		},
		{
			name:      "AddSumIntVar",
			buildExpr: func() *LinearExpr { return NewLinearExpr().AddSum(iv1) },
			want: &LinearExpr{
				varCoeffs: []varCoeff{{ind: iv1.Index(), coeff: 1}},
				offset:    0},
		},
		{
			name:      "AddSumBoolVar",
			buildExpr: func() *LinearExpr { return NewLinearExpr().AddSum(bv.Not()) },
			want: &LinearExpr{
				varCoeffs: []varCoeff{{ind: bv.Index(), coeff: -1}},
				offset:    1},
		},
		{
			name:      "AddSumManyVars",
			buildExpr: func() *LinearExpr { return NewLinearExpr().AddSum(iv1, iv2, bv, bv.Not()) },
			want: &LinearExpr{
				varCoeffs: []varCoeff{
					{ind: iv1.Index(), coeff: 1},
					{ind: iv2.Index(), coeff: 1},
					{ind: bv.Index(), coeff: 1},
					{ind: bv.Index(), coeff: -1},
				},
				offset: 1,
			},
		},
		{
			name:      "AddSumLinearExpr",
			buildExpr: func() *LinearExpr { return NewLinearExpr().AddSum(lin) },
			want: &LinearExpr{
				varCoeffs: []varCoeff{
					{ind: iv1.Index(), coeff: 10},
					{ind: bv.Index(), coeff: 20},
				},
				offset: 0,
			},
		},
		{
			name: "AddWeightedSumIntVar",
			buildExpr: func() *LinearExpr {
				return NewLinearExpr().AddWeightedSum([]LinearArgument{iv1}, []int64{10})
			},
			want: &LinearExpr{
				varCoeffs: []varCoeff{{ind: iv1.Index(), coeff: 10}},
				offset:    0,
			},
		},
		{
			name: "AddWeightedSumBoolVar",
			buildExpr: func() *LinearExpr {
				return NewLinearExpr().AddWeightedSum([]LinearArgument{bv}, []int64{-10})
			},
			want: &LinearExpr{
				varCoeffs: []varCoeff{{ind: bv.Index(), coeff: -10}},
				offset:    0,
			},
		},
		{
			name: "AddWeightedSumManyVars",
			buildExpr: func() *LinearExpr {
				return NewLinearExpr().AddWeightedSum([]LinearArgument{iv1, iv2, bv, bv.Not()}, []int64{10, 20, 30, 40})
			},
			want: &LinearExpr{
				varCoeffs: []varCoeff{
					{ind: iv1.Index(), coeff: 10},
					{ind: iv2.Index(), coeff: 20},
					{ind: bv.Index(), coeff: 30},
					{ind: bv.Index(), coeff: -40},
				},
				offset: 40,
			},
		},
		{
			name: "AddWeightedSumLinearExpr",
			buildExpr: func() *LinearExpr {
				return NewLinearExpr().AddWeightedSum([]LinearArgument{lin}, []int64{3})
			},
			want: &LinearExpr{
				varCoeffs: []varCoeff{
					{ind: iv1.Index(), coeff: 30},
					{ind: bv.Index(), coeff: 60},
				},
				offset: 0,
			},
		},
	}

	for _, test := range testCases {
		t.Run(test.name, func(t *testing.T) {
			got := test.buildExpr()
			if diff := cmp.Diff(test.want, got, cmp.AllowUnexported(LinearExpr{}, varCoeff{})); diff != "" {
				t.Errorf("test.buildExpr() returned with unexpected diff (-want+got):\n%s", diff)
			}
		})
	}
}

func mustModel(t *testing.T, builder *Builder) *cmpb.CpModelProto {
	m, err := builder.Model()
	if err != nil {
		t.Fatalf("Model() returned with unexpected err %v", err)
	}
	return m
}

func TestIntervalVar(t *testing.T) {
	model := NewCpModelBuilder()
	iv1 := model.NewIntVar(0, 10)
	iv2 := model.NewIntVar(0, 10)
	bv1 := model.NewBoolVar()
	trueVar := model.TrueVar()

	testCases := []struct {
		name        string
		intervalVar func() *cmpb.ConstraintProto
		want        *cmpb.ConstraintProto
	}{
		{
			name: "NewIntervalVar",
			intervalVar: func() *cmpb.ConstraintProto {
				iv := model.NewIntervalVar(NewConstant(1), iv1, iv2)
				m := mustModel(t, model)
				return m.GetConstraints()[iv.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				EnforcementLiteral: []int32{int32_t(trueVar.Index())},
				Interval: cmpb.IntervalConstraintProto_builder{
					Start: cmpb.LinearExpressionProto_builder{Offset: 1}.Build(),
					Size: cmpb.LinearExpressionProto_builder{
						Vars:   []int32{int32_t(iv1.Index())},
						Coeffs: []int64{1},
					}.Build(),
					End: cmpb.LinearExpressionProto_builder{
						Vars:   []int32{int32_t(iv2.Index())},
						Coeffs: []int64{1},
					}.Build(),
				}.Build(),
			}.Build(),
		},
		{
			name: "NewFixedSizeIntervalVar",
			intervalVar: func() *cmpb.ConstraintProto {
				iv := model.NewFixedSizeIntervalVar(iv1, 5)
				m := mustModel(t, model)
				return m.GetConstraints()[iv.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				EnforcementLiteral: []int32{int32_t(trueVar.Index())},
				Interval: cmpb.IntervalConstraintProto_builder{
					Start: cmpb.LinearExpressionProto_builder{
						Vars:   []int32{int32_t(iv1.Index())},
						Coeffs: []int64{1},
					}.Build(),
					Size: cmpb.LinearExpressionProto_builder{Offset: 5}.Build(),
					End: cmpb.LinearExpressionProto_builder{
						Vars:   []int32{int32_t(iv1.Index())},
						Coeffs: []int64{1},
						Offset: 5,
					}.Build(),
				}.Build(),
			}.Build(),
		},
		{
			name: "NewOptionalIntervalVar",
			intervalVar: func() *cmpb.ConstraintProto {
				iv := model.NewOptionalIntervalVar(NewConstant(1), iv1, iv2, bv1)
				m := mustModel(t, model)
				return m.GetConstraints()[iv.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				EnforcementLiteral: []int32{int32_t(bv1.Index())},
				Interval: cmpb.IntervalConstraintProto_builder{
					Start: cmpb.LinearExpressionProto_builder{Offset: 1}.Build(),
					Size: cmpb.LinearExpressionProto_builder{
						Vars:   []int32{int32_t(iv1.Index())},
						Coeffs: []int64{1},
					}.Build(),
					End: cmpb.LinearExpressionProto_builder{
						Vars:   []int32{int32_t(iv2.Index())},
						Coeffs: []int64{1},
					}.Build(),
				}.Build(),
			}.Build(),
		},
		{
			name: "NewOptionalFixedSizeIntervalVar",
			intervalVar: func() *cmpb.ConstraintProto {
				iv := model.NewOptionalFixedSizeIntervalVar(iv1, 5, bv1)
				m := mustModel(t, model)
				return m.GetConstraints()[iv.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				EnforcementLiteral: []int32{int32_t(bv1.Index())},
				Interval: cmpb.IntervalConstraintProto_builder{
					Start: cmpb.LinearExpressionProto_builder{
						Vars:   []int32{int32_t(iv1.Index())},
						Coeffs: []int64{1},
					}.Build(),
					Size: cmpb.LinearExpressionProto_builder{Offset: 5}.Build(),
					End: cmpb.LinearExpressionProto_builder{
						Vars:   []int32{int32_t(iv1.Index())},
						Coeffs: []int64{1},
						Offset: 5,
					}.Build(),
				}.Build(),
			}.Build(),
		},
	}

	for _, test := range testCases {
		t.Run(test.name, func(t *testing.T) {
			got := test.intervalVar()
			if diff := cmp.Diff(test.want, got, protocmp.Transform()); diff != "" {
				t.Errorf("test.intervalVar() returned with unexpected diff (-want+got):\n%s", diff)
			}
		})
	}
}

func TestConstraint_Name(t *testing.T) {
	model := NewCpModelBuilder()

	bv1 := model.NewBoolVar()
	bv2 := model.NewBoolVar()
	ct := model.AddBoolOr(bv1, bv2).WithName("name")

	got := ct.Name()
	want := "name"
	if diff := cmp.Diff(want, got); diff != "" {
		t.Errorf("Name() = %#v, want %#v", got, want)
	}
}

func TestCpModelBuilder_Constraints(t *testing.T) {
	model := NewCpModelBuilder()

	bv1 := model.NewBoolVar()
	bv2 := model.NewBoolVar()
	bv3 := model.NewBoolVar()
	iv1 := model.NewIntVar(-10, 10)
	iv2 := model.NewIntVar(-10, 10)
	iv3 := model.NewIntVar(-10, 10)
	iv4 := model.NewIntVar(-10, 10)
	interval1 := model.NewIntervalVar(iv1, iv2, iv3)
	interval2 := model.NewIntervalVar(iv3, iv2, iv4)
	interval3 := model.NewIntervalVar(iv2, iv3, iv4)
	interval4 := model.NewIntervalVar(iv4, iv3, iv1)
	one := model.NewConstant(1)

	testCases := []struct {
		name       string
		constraint func() *cmpb.ConstraintProto
		want       *cmpb.ConstraintProto
	}{
		{
			name: "AddBoolOr",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddBoolOr(bv1, bv2.Not()).OnlyEnforceIf(bv3)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				EnforcementLiteral: []int32{int32_t(bv3.Index())},
				BoolOr: cmpb.BoolArgumentProto_builder{
					Literals: []int32{int32_t(bv1.Index()), int32_t(bv2.Not().Index())},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddBoolAnd",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddBoolAnd(bv1, bv2.Not()).OnlyEnforceIf(bv3)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				EnforcementLiteral: []int32{int32_t(bv3.Index())},
				BoolAnd: cmpb.BoolArgumentProto_builder{
					Literals: []int32{int32_t(bv1.Index()), int32_t(bv2.Not().Index())},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddBoolXor",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddBoolXor(bv1, bv2.Not()).OnlyEnforceIf(bv3)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				EnforcementLiteral: []int32{int32_t(bv3.Index())},
				BoolXor: cmpb.BoolArgumentProto_builder{
					Literals: []int32{int32_t(bv1.Index()), int32_t(bv2.Not().Index())},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddAtLeastOne",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddAtLeastOne(bv1, bv2.Not()).OnlyEnforceIf(bv3)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				EnforcementLiteral: []int32{int32_t(bv3.Index())},
				BoolOr: cmpb.BoolArgumentProto_builder{
					Literals: []int32{int32_t(bv1.Index()), int32_t(bv2.Not().Index())},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddAtMostOne",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddAtMostOne(bv1, bv2.Not()).OnlyEnforceIf(bv3)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				EnforcementLiteral: []int32{int32_t(bv3.Index())},
				AtMostOne: cmpb.BoolArgumentProto_builder{
					Literals: []int32{int32_t(bv1.Index()), int32_t(bv2.Not().Index())},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddExactlyOne",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddExactlyOne(bv1, bv2.Not()).OnlyEnforceIf(bv3)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				EnforcementLiteral: []int32{int32_t(bv3.Index())},
				ExactlyOne: cmpb.BoolArgumentProto_builder{
					Literals: []int32{int32_t(bv1.Index()), int32_t(bv2.Not().Index())},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddImplication",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddImplication(bv1, bv2.Not())
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				BoolOr: cmpb.BoolArgumentProto_builder{
					Literals: []int32{int32_t(bv1.Not().Index()), int32_t(bv2.Not().Index())},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddLinearConstraintForDomain",
			constraint: func() *cmpb.ConstraintProto {
				d := FromIntervals([]ClosedInterval{{0, 1}, {3, 4}, {11, 20}})
				c := model.AddLinearConstraintForDomain(NewLinearExpr().Add(iv1).Add(bv1).AddConstant(5), d)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				Linear: cmpb.LinearConstraintProto_builder{
					Vars:   []int32{int32_t(iv1.Index()), int32_t(bv1.Index())},
					Coeffs: []int64{1, 1},
					Domain: []int64{-5, -4, -2, -1, 6, 15},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddLinearConstraint",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddLinearConstraint(NewLinearExpr().Add(iv1).Add(bv1), 2, 6)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				Linear: cmpb.LinearConstraintProto_builder{
					Vars:   []int32{int32_t(iv1.Index()), int32_t(bv1.Index())},
					Coeffs: []int64{1, 1},
					Domain: []int64{2, 6},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddEquality",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddEquality(iv1, NewConstant(10))
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				Linear: cmpb.LinearConstraintProto_builder{
					Vars:   []int32{int32_t(iv1.Index())},
					Coeffs: []int64{1},
					Domain: []int64{10, 10},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddLessOrEqual",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddLessOrEqual(iv1, NewConstant(10))
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				Linear: cmpb.LinearConstraintProto_builder{
					Vars:   []int32{int32_t(iv1.Index())},
					Coeffs: []int64{1},
					Domain: []int64{math.MinInt64, 10},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddLessThan",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddLessThan(iv1, NewConstant(10))
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				Linear: cmpb.LinearConstraintProto_builder{
					Vars:   []int32{int32_t(iv1.Index())},
					Coeffs: []int64{1},
					Domain: []int64{math.MinInt64, 9},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddGreaterOrEqual",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddGreaterOrEqual(iv1, NewConstant(10))
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				Linear: cmpb.LinearConstraintProto_builder{
					Vars:   []int32{int32_t(iv1.Index())},
					Coeffs: []int64{1},
					Domain: []int64{10, math.MaxInt64},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddGreaterThan",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddGreaterThan(iv1, NewConstant(10))
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				Linear: cmpb.LinearConstraintProto_builder{
					Vars:   []int32{int32_t(iv1.Index())},
					Coeffs: []int64{1},
					Domain: []int64{11, math.MaxInt64},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddNotEqual",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddNotEqual(iv1, NewConstant(10))
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				Linear: cmpb.LinearConstraintProto_builder{
					Vars:   []int32{int32_t(iv1.Index())},
					Coeffs: []int64{1},
					Domain: []int64{math.MinInt64, 9, 11, math.MaxInt64},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddAllDifferent",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddAllDifferent(iv1, bv1, bv2.Not(), NewConstant(10))
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				AllDiff: cmpb.AllDifferentConstraintProto_builder{
					Exprs: []*cmpb.LinearExpressionProto{
						cmpb.LinearExpressionProto_builder{
							Vars:   []int32{int32_t(iv1.Index())},
							Coeffs: []int64{1},
						}.Build(),
						cmpb.LinearExpressionProto_builder{
							Vars:   []int32{int32_t(bv1.Index())},
							Coeffs: []int64{1},
						}.Build(),
						cmpb.LinearExpressionProto_builder{
							Vars:   []int32{int32_t(bv2.Index())},
							Coeffs: []int64{-1},
							Offset: 1,
						}.Build(),
						cmpb.LinearExpressionProto_builder{
							Vars:   []int32{},
							Coeffs: []int64{},
							Offset: 10,
						}.Build(),
					},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddVariableElement",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddVariableElement(iv1, []IntVar{iv2, iv3}, iv4)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				Element: cmpb.ElementConstraintProto_builder{
					Index:  int32_t(iv1.Index()),
					Target: int32_t(iv4.Index()),
					Vars:   []int32{int32_t(iv2.Index()), int32_t(iv3.Index())},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddElement",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddElement(iv1, []int64{10, 20}, iv4)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				Element: cmpb.ElementConstraintProto_builder{
					Index:  int32_t(iv1.Index()),
					Target: int32_t(iv4.Index()),
					Vars: []int32{
						int32_t(model.NewConstant(10).Index()),
						int32_t(model.NewConstant(20).Index()),
					},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddInverseConstraint",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddInverseConstraint([]IntVar{iv1, iv2}, []IntVar{iv3, iv4})
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				Inverse: cmpb.InverseConstraintProto_builder{
					FDirect:  []int32{int32_t(iv1.Index()), int32_t(iv2.Index())},
					FInverse: []int32{int32_t(iv3.Index()), int32_t(iv4.Index())},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddMinEquality",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddMinEquality(iv1, iv2, iv3)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				LinMax: cmpb.LinearArgumentProto_builder{
					Target: cmpb.LinearExpressionProto_builder{
						Vars:   []int32{int32_t(iv1.Index())},
						Coeffs: []int64{-1},
					}.Build(),
					Exprs: []*cmpb.LinearExpressionProto{
						cmpb.LinearExpressionProto_builder{
							Vars:   []int32{int32_t(iv2.Index())},
							Coeffs: []int64{-1},
						}.Build(),
						cmpb.LinearExpressionProto_builder{
							Vars:   []int32{int32_t(iv3.Index())},
							Coeffs: []int64{-1},
						}.Build(),
					},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddMaxEquality",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddMaxEquality(iv1, iv2, iv3)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				LinMax: cmpb.LinearArgumentProto_builder{
					Target: cmpb.LinearExpressionProto_builder{
						Vars:   []int32{int32_t(iv1.Index())},
						Coeffs: []int64{1},
					}.Build(),
					Exprs: []*cmpb.LinearExpressionProto{
						cmpb.LinearExpressionProto_builder{
							Vars:   []int32{int32_t(iv2.Index())},
							Coeffs: []int64{1},
						}.Build(),
						cmpb.LinearExpressionProto_builder{
							Vars:   []int32{int32_t(iv3.Index())},
							Coeffs: []int64{1},
						}.Build(),
					},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddMultiplicationEquality",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddMultiplicationEquality(iv1, iv2, iv3)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				IntProd: cmpb.LinearArgumentProto_builder{
					Target: cmpb.LinearExpressionProto_builder{
						Vars:   []int32{int32_t(iv1.Index())},
						Coeffs: []int64{1},
					}.Build(),
					Exprs: []*cmpb.LinearExpressionProto{
						cmpb.LinearExpressionProto_builder{
							Vars:   []int32{int32_t(iv2.Index())},
							Coeffs: []int64{1},
						}.Build(),
						cmpb.LinearExpressionProto_builder{
							Vars:   []int32{int32_t(iv3.Index())},
							Coeffs: []int64{1},
						}.Build(),
					},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddDivisionEquality",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddDivisionEquality(iv1, iv2, iv3)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				IntDiv: cmpb.LinearArgumentProto_builder{
					Target: cmpb.LinearExpressionProto_builder{
						Vars:   []int32{int32_t(iv1.Index())},
						Coeffs: []int64{1},
					}.Build(),
					Exprs: []*cmpb.LinearExpressionProto{
						cmpb.LinearExpressionProto_builder{
							Vars:   []int32{int32_t(iv2.Index())},
							Coeffs: []int64{1},
						}.Build(),
						cmpb.LinearExpressionProto_builder{
							Vars:   []int32{int32_t(iv3.Index())},
							Coeffs: []int64{1},
						}.Build(),
					},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddAbsEquality",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddAbsEquality(iv1, iv2)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				LinMax: cmpb.LinearArgumentProto_builder{
					Target: cmpb.LinearExpressionProto_builder{
						Vars:   []int32{int32_t(iv1.Index())},
						Coeffs: []int64{1},
					}.Build(),
					Exprs: []*cmpb.LinearExpressionProto{
						cmpb.LinearExpressionProto_builder{
							Vars:   []int32{int32_t(iv2.Index())},
							Coeffs: []int64{1},
						}.Build(),
						cmpb.LinearExpressionProto_builder{
							Vars:   []int32{int32_t(iv2.Index())},
							Coeffs: []int64{-1},
						}.Build(),
					},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddModuloEquality",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddModuloEquality(iv1, iv2, iv3)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				IntMod: cmpb.LinearArgumentProto_builder{
					Target: cmpb.LinearExpressionProto_builder{
						Vars:   []int32{int32_t(iv1.Index())},
						Coeffs: []int64{1},
					}.Build(),
					Exprs: []*cmpb.LinearExpressionProto{
						cmpb.LinearExpressionProto_builder{
							Vars:   []int32{int32_t(iv2.Index())},
							Coeffs: []int64{1},
						}.Build(),
						cmpb.LinearExpressionProto_builder{
							Vars:   []int32{int32_t(iv3.Index())},
							Coeffs: []int64{1},
						}.Build(),
					},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddNoOverlap",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddNoOverlap(interval1, interval2)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				NoOverlap: cmpb.NoOverlapConstraintProto_builder{
					Intervals: []int32{int32_t(interval1.Index()), int32_t(interval2.Index())},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddNoOverlap2D",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddNoOverlap2D()
				c.AddRectangle(interval1, interval2)
				c.AddRectangle(interval3, interval4)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				NoOverlap_2D: cmpb.NoOverlap2DConstraintProto_builder{
					XIntervals: []int32{int32_t(interval1.Index()), int32_t(interval3.Index())},
					YIntervals: []int32{int32_t(interval2.Index()), int32_t(interval4.Index())},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddCircuitConstraint",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddCircuitConstraint()
				c.AddArc(0, 1, bv1)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				Circuit: cmpb.CircuitConstraintProto_builder{
					Tails:    []int32{0},
					Heads:    []int32{1},
					Literals: []int32{int32_t(bv1.Index())},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddMultipleCircuitConstraint",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddMultipleCircuitConstraint()
				c.AddRoute(0, 1, bv1)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				Routes: cmpb.RoutesConstraintProto_builder{
					Tails:    []int32{0},
					Heads:    []int32{1},
					Literals: []int32{int32_t(bv1.Index())},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddAllowedAssignments",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddAllowedAssignments(iv1, iv2)
				c.AddTuple(0, 2)
				c.AddTuple(1, 3)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				Table: cmpb.TableConstraintProto_builder{
					Vars:   []int32{int32_t(iv1.Index()), int32_t(iv2.Index())},
					Values: []int64{0, 2, 1, 3},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddReservoirConstraint",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddReservoirConstraint(10, 20)
				c.AddEvent(NewLinearExpr().AddTerm(iv1, 2), NewConstant(15))
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				Reservoir: cmpb.ReservoirConstraintProto_builder{
					MinLevel: 10,
					MaxLevel: 20,
					TimeExprs: []*cmpb.LinearExpressionProto{
						cmpb.LinearExpressionProto_builder{
							Vars:   []int32{int32_t(iv1.Index())},
							Coeffs: []int64{2},
						}.Build(),
					},
					LevelChanges: []*cmpb.LinearExpressionProto{
						cmpb.LinearExpressionProto_builder{
							Offset: 15,
						}.Build(),
					},
					ActiveLiterals: []int32{int32_t(one.Index())},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddAutomaton",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddAutomaton([]IntVar{iv1, iv2}, 0, []int64{5, 10})
				c.AddTransition(0, 1, 10)
				c.AddTransition(2, 3, 15)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				Automaton: cmpb.AutomatonConstraintProto_builder{
					Vars:            []int32{int32_t(iv1.Index()), int32_t(iv2.Index())},
					StartingState:   0,
					FinalStates:     []int64{5, 10},
					TransitionTail:  []int64{0, 2},
					TransitionHead:  []int64{1, 3},
					TransitionLabel: []int64{10, 15},
				}.Build(),
			}.Build(),
		},
		{
			name: "AddCumulative",
			constraint: func() *cmpb.ConstraintProto {
				c := model.AddCumulative(iv1)
				c.AddDemand(interval1, iv2)
				m := mustModel(t, model)
				return m.GetConstraints()[c.Index()]
			},
			want: cmpb.ConstraintProto_builder{
				Cumulative: cmpb.CumulativeConstraintProto_builder{
					Capacity: cmpb.LinearExpressionProto_builder{
						Vars:   []int32{int32_t(iv1.Index())},
						Coeffs: []int64{1},
					}.Build(),
					Intervals: []int32{int32_t(interval1.Index())},
					Demands: []*cmpb.LinearExpressionProto{
						cmpb.LinearExpressionProto_builder{
							Vars:   []int32{int32_t(iv2.Index())},
							Coeffs: []int64{1},
						}.Build(),
					},
				}.Build(),
			}.Build(),
		},
	}

	for _, test := range testCases {
		t.Run(test.name, func(t *testing.T) {
			got := test.constraint()
			if diff := cmp.Diff(test.want, got, protocmp.Transform()); diff != "" {
				t.Errorf("test.buildModel() returned with unexpected diff (-want+got):\n%s", diff)
			}
		})
	}
}

func TestCpModelBuilder_Minimize(t *testing.T) {
	model := NewCpModelBuilder()

	iv1 := model.NewIntVar(-10, 10)
	iv2 := model.NewIntVar(-10, 10)

	model.Minimize(NewLinearExpr().AddTerm(iv1, 3).AddTerm(iv2, 5))

	m := mustModel(t, model)
	want := cmpb.CpObjectiveProto_builder{
		Vars:   []int32{int32_t(iv1.Index()), int32_t(iv2.Index())},
		Coeffs: []int64{3, 5},
	}.Build()
	got := m.GetObjective()

	if diff := cmp.Diff(want, got, protocmp.Transform()); diff != "" {
		t.Errorf("GetObjective() returned unexpected diff (-want+got): %v", diff)
	}
}

func TestCpModelBuilder_Maximize(t *testing.T) {
	model := NewCpModelBuilder()

	iv1 := model.NewIntVar(-10, 10)
	iv2 := model.NewIntVar(-10, 10)

	model.Maximize(NewLinearExpr().AddTerm(iv1, 3).AddTerm(iv2, 5).AddConstant(7))
	want := cmpb.CpObjectiveProto_builder{
		Vars:          []int32{int32_t(iv1.Index()), int32_t(iv2.Index())},
		Coeffs:        []int64{-3, -5},
		ScalingFactor: -1.0,
		Offset:        -7,
	}.Build()

	m := mustModel(t, model)
	got := m.GetObjective()
	if diff := cmp.Diff(want, got, protocmp.Transform()); diff != "" {
		t.Errorf("GetObjective() returned unexpected diff (-want+got): %v", diff)
	}
}

func TestCpModelBuilder_ConstantVars(t *testing.T) {
	model := NewCpModelBuilder()

	trueVar := model.TrueVar()
	falseVar := model.FalseVar()
	constVar := model.NewConstant(10)

	wantTrue := BoolVar{cpb: model, ind: trueVar.Index()}
	wantFalse := BoolVar{cpb: model, ind: falseVar.Index()}
	wantConst := IntVar{cpb: model, ind: constVar.Index()}

	if trueVar != wantTrue {
		t.Errorf("TrueVar() = %+v, want %+v", trueVar, wantTrue)
	}
	if falseVar != wantFalse {
		t.Errorf("FalseVar() = %+v, want %+v", falseVar, wantFalse)
	}
	if constVar != wantConst {
		t.Errorf("NewConstant() = %+v, want %+v", constVar, wantConst)
	}
	gotDom, err := constVar.Domain()
	wantDom := NewSingleDomain(10)
	if err != nil {
		t.Fatalf("Domain() returned with unexpected error %v", err)
	}
	if diff := cmp.Diff(wantDom, gotDom, cmp.AllowUnexported(Domain{}, ClosedInterval{})); diff != "" {
		t.Errorf("Domain() = %v, want %v", gotDom, wantDom)
	}

	m := mustModel(t, model)
	// Test that constant variables are only created once.
	want := len(m.GetVariables())

	model.TrueVar()
	model.FalseVar()
	model.NewConstant(10)

	got := len(m.GetVariables())

	if got != want {
		t.Errorf("len(GetVariables()) = %v, want %v", got, want)
	}
}

func TestCpModelBuilder_IndexValueSlices(t *testing.T) {
	indices := []int32{5, 1, 3}
	values := []int64{10, 11, 8}

	sort.Sort(indexValueSlices{indices, values})

	wantIndices := []int32{1, 3, 5}
	wantValues := []int64{11, 8, 10}

	if diff := cmp.Diff(wantIndices, indices); diff != "" {
		t.Errorf("Sort indexValueSlices return unexpected indices diff (-want+got): %v", diff)
	}
	if diff := cmp.Diff(wantValues, values); diff != "" {
		t.Errorf("Sort indexValueSlices return unexpected values diff (-want+got): %v", diff)
	}
}

func TestCpModelBuilder_SetHint(t *testing.T) {
	model := NewCpModelBuilder()

	iv := model.NewIntVar(-10, 10)
	bv1 := model.NewBoolVar()
	bv2 := model.NewBoolVar()
	hint := &Hint{
		Ints:  map[IntVar]int64{iv: 7},
		Bools: map[BoolVar]bool{bv2.Not(): false, bv1: true},
	}
	model.SetHint(hint)

	m := mustModel(t, model)
	got := m.GetSolutionHint()
	want := cmpb.PartialVariableAssignment_builder{
		Vars:   []int32{int32_t(iv.Index()), int32_t(bv1.Index()), int32_t(bv2.Index())},
		Values: []int64{7, 1, 1},
	}.Build()

	if diff := cmp.Diff(want, got, protocmp.Transform()); diff != "" {
		t.Errorf("GetSolutionHint() returned unexpected diff (-want+got): %v", diff)
	}
}

func TestCpModelBuilder_ClearHint(t *testing.T) {
	model := NewCpModelBuilder()

	iv := model.NewIntVar(-10, 10)
	bv1 := model.NewBoolVar()
	bv2 := model.NewBoolVar()
	hint := &Hint{
		Ints:  map[IntVar]int64{iv: 7},
		Bools: map[BoolVar]bool{bv1: true, bv2.Not(): false},
	}
	model.SetHint(hint)
	model.ClearHint()

	m := mustModel(t, model)
	got := m.GetSolutionHint()
	if got != nil {
		t.Errorf("ClearHint() returned %v, want nil", got)
	}
}

func TestCpModelBuilder_AddAssumption(t *testing.T) {
	model := NewCpModelBuilder()

	bv1 := model.NewBoolVar()
	bv2 := model.NewBoolVar()
	bv3 := model.NewBoolVar()

	model.AddAssumption(bv1, bv2)
	model.AddAssumption(bv3.Not())

	m := mustModel(t, model)
	got := m.GetAssumptions()
	want := []int32{int32_t(bv1.Index()), int32_t(bv2.Index()), int32_t(bv3.Not().Index())}

	if diff := cmp.Diff(want, got); diff != "" {
		t.Errorf("GetAssumptions() returned unexpected diff (-want+got): %v", diff)
	}
}

func TestCpModelBuilder_ClearAssumption(t *testing.T) {
	model := NewCpModelBuilder()

	bv1 := model.NewBoolVar()
	bv2 := model.NewBoolVar()
	bv3 := model.NewBoolVar()

	model.AddAssumption(bv1, bv2)
	model.AddAssumption(bv3.Not())

	model.ClearAssumption()

	m := mustModel(t, model)
	got := m.GetAssumptions()
	if got != nil {
		t.Errorf("ClearAssumption() returned %v, want nil", got)
	}
}

func TestCpModelBuilder_AddDecisionStrategy(t *testing.T) {
	model := NewCpModelBuilder()

	iv := model.NewIntVar(-10, 10)
	bv := model.NewBoolVar()
	model.AddDecisionStrategy([]IntVar{iv, IntVar(bv)}, cmpb.DecisionStrategyProto_CHOOSE_HIGHEST_MAX, cmpb.DecisionStrategyProto_SELECT_LOWER_HALF)
	model.AddDecisionStrategy([]IntVar{iv}, cmpb.DecisionStrategyProto_CHOOSE_LOWEST_MIN, cmpb.DecisionStrategyProto_SELECT_UPPER_HALF)

	m := mustModel(t, model)
	got := m.GetSearchStrategy()
	want := []*cmpb.DecisionStrategyProto{
		cmpb.DecisionStrategyProto_builder{
			Variables:                 []int32{int32_t(iv.Index()), int32_t(bv.Index())},
			VariableSelectionStrategy: cmpb.DecisionStrategyProto_CHOOSE_HIGHEST_MAX,
			DomainReductionStrategy:   cmpb.DecisionStrategyProto_SELECT_LOWER_HALF,
		}.Build(),
		cmpb.DecisionStrategyProto_builder{
			Variables:                 []int32{int32_t(iv.Index())},
			VariableSelectionStrategy: cmpb.DecisionStrategyProto_CHOOSE_LOWEST_MIN,
			DomainReductionStrategy:   cmpb.DecisionStrategyProto_SELECT_UPPER_HALF,
		}.Build(),
	}

	if diff := cmp.Diff(want, got, protocmp.Transform()); diff != "" {
		t.Errorf("GetSearchStrategy() returned unexpected diff (-want+got): %v", diff)
	}
}

func TestCpModelBuilder_ErrorHandling(t *testing.T) {
	testCases := []struct {
		name    string
		builder func() *Builder
	}{
		{
			name: "AddRectangle",
			builder: func() *Builder {
				model1 := NewCpModelBuilder()
				model2 := NewCpModelBuilder()
				i1 := model1.NewFixedSizeIntervalVar(NewConstant(0), 10)
				i2 := model2.NewFixedSizeIntervalVar(NewConstant(10), 11)
				c := model1.AddNoOverlap2D()
				c.AddRectangle(i1, i2)
				return model1
			},
		},
		{
			name: "AddArc",
			builder: func() *Builder {
				model1 := NewCpModelBuilder()
				model2 := NewCpModelBuilder()
				c := model1.AddCircuitConstraint()
				c.AddArc(0, 10, model2.NewBoolVar())
				return model1
			},
		},
		{
			name: "AddRoute",
			builder: func() *Builder {
				model1 := NewCpModelBuilder()
				model2 := NewCpModelBuilder()
				c := model1.AddMultipleCircuitConstraint()
				c.AddRoute(0, 1, model2.NewBoolVar())
				return model1
			},
		},
		{
			name: "AddDemand",
			builder: func() *Builder {
				model1 := NewCpModelBuilder()
				model2 := NewCpModelBuilder()
				cap := NewConstant(10)
				dem := NewConstant(11)
				c := model1.AddCumulative(cap)
				c.AddDemand(model2.NewFixedSizeIntervalVar(cap, 10), dem)
				return model1
			},
		},
		{
			name: "AddBoolOrs",
			builder: func() *Builder {
				model1 := NewCpModelBuilder()
				model2 := NewCpModelBuilder()
				model1.AddBoolOr(model2.NewBoolVar())
				return model1
			},
		},
		{
			name: "AddNoOverlap",
			builder: func() *Builder {
				model1 := NewCpModelBuilder()
				model2 := NewCpModelBuilder()
				model1.AddNoOverlap(model2.NewFixedSizeIntervalVar(NewConstant(10), 10))
				return model1
			},
		},
		{
			name: "AddAutomaton",
			builder: func() *Builder {
				model1 := NewCpModelBuilder()
				model2 := NewCpModelBuilder()
				model1.AddAutomaton([]IntVar{model2.NewIntVar(0, 10)}, 0, []int64{10})
				return model1
			},
		},
		{
			name: "AddAssumption",
			builder: func() *Builder {
				model1 := NewCpModelBuilder()
				model2 := NewCpModelBuilder()
				model1.AddAssumption(model2.NewBoolVar())
				return model1
			},
		},
		{
			name: "AddDecisionStrategy",
			builder: func() *Builder {
				model1 := NewCpModelBuilder()
				model2 := NewCpModelBuilder()
				model1.AddDecisionStrategy([]IntVar{model2.NewIntVar(0, 10)}, cmpb.DecisionStrategyProto_CHOOSE_HIGHEST_MAX, cmpb.DecisionStrategyProto_SELECT_LOWER_HALF)
				return model1
			},
		},
	}

	for _, test := range testCases {
		t.Run(test.name, func(t *testing.T) {
			got, err := test.builder().Model()
			if !errors.Is(err, ErrMixedModels) {
				t.Errorf("test.Model() returned with unexpected error %v; want ErrMixedModels error", err)
			}
			if got != nil {
				t.Errorf("test.Model() returned with unexpected model %v; want nil", got)
			}
		})
	}
}
