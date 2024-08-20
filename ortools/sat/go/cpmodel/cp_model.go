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

// Package cpmodel offers a user-friendly API to build CP-SAT models.
//
// The `Builder` struct wraps the CP-SAT model proto and provides helper methods for
// adding constraints and variables to the model.
// The `IntVar`, `BoolVar`, and `IntervalVar` structs are references to specific
// variables in the CP-SAT model proto and provide helpful methods for interacting
// with those variables.
// The `LinearExpr` struct provides helper methods for creating constraints and the
// objective from expressions with many variables and coefficients.
package cpmodel

import (
	"errors"
	"fmt"
	"math"
	"sort"

	log "github.com/golang/glog"
	cmpb "github.com/google/or-tools/ortools/sat/proto/cpmodel"
)

// ErrMixedModels holds the error when elements added to a model are different.
var ErrMixedModels = errors.New("elements are not part of the same model")

type (
	// VarIndex is the index of a variable in the CP model proto, if positive. If this value is
	// negative, it represents the negation of a Boolean variable in the position (-1*VarIndex-1).
	VarIndex int32
	// ConstrIndex is the index of a constraint in the CP model proto.
	ConstrIndex int32
)

func (v VarIndex) positiveIndex() VarIndex {
	if v >= 0 {
		return v
	}
	return -1*v - 1
}

// LinearArgument provides an interface for BoolVar, IntVar, and LinearExpr.
type LinearArgument interface {
	addToLinearExpr(e *LinearExpr, c int64)
	// asLinearExpressionProto returns the LinearArgument as a LinearExpressionProto.
	asLinearExpressionProto() *cmpb.LinearExpressionProto
	evaluateSolutionValue(r *cmpb.CpSolverResponse) int64
}

// LinearExpr is a container for a linear expression.
type LinearExpr struct {
	varCoeffs []varCoeff
	offset    int64
}

type varCoeff struct {
	ind   VarIndex
	coeff int64
}

// NewLinearExpr creates a new empty LinearExpr.
func NewLinearExpr() *LinearExpr {
	return &LinearExpr{}
}

// NewConstant creates and returns a LinearExpr containing the constant `c`.
func NewConstant(c int64) *LinearExpr {
	return &LinearExpr{offset: c}
}

// Add adds the linear argument term to the LinearExpr and returns itself.
func (l *LinearExpr) Add(la LinearArgument) *LinearExpr {
	l.AddTerm(la, 1)
	return l
}

// AddConstant adds the constant to the LinearExpr and returns itself.
func (l *LinearExpr) AddConstant(c int64) *LinearExpr {
	l.offset += c
	return l
}

// AddTerm adds the linear argument term with the given coefficient to the LinearExpr and returns itself.
func (l *LinearExpr) AddTerm(la LinearArgument, coeff int64) *LinearExpr {
	la.addToLinearExpr(l, coeff)
	return l
}

// AddSum adds the sum of the linear arguments to the LinearExpr and returns itself.
func (l *LinearExpr) AddSum(las ...LinearArgument) *LinearExpr {
	for _, la := range las {
		l.Add(la)
	}
	return l
}

// AddWeightedSum adds the linear arguments with the corresponding coefficients to the LinearExpr
// and returns itself.
func (l *LinearExpr) AddWeightedSum(las []LinearArgument, coeffs []int64) *LinearExpr {
	if len(coeffs) != len(las) {
		log.Fatalf("las and coeffs must be the same length: %v != %v", len(las), len(coeffs))
	}
	for i, la := range las {
		l.AddTerm(la, coeffs[i])
	}
	return l
}

func (l *LinearExpr) addToLinearExpr(e *LinearExpr, c int64) {
	for _, vc := range l.varCoeffs {
		e.varCoeffs = append(e.varCoeffs, varCoeff{ind: vc.ind, coeff: vc.coeff * c})
	}
	e.offset += l.offset * c
}

func (l *LinearExpr) asLinearExpressionProto() *cmpb.LinearExpressionProto {
	linExprProto := &cmpb.LinearExpressionProto{}

	for _, vc := range l.varCoeffs {
		linExprProto.Vars = append(linExprProto.GetVars(), int32(vc.ind))
		linExprProto.Coeffs = append(linExprProto.GetCoeffs(), vc.coeff)
	}
	linExprProto.Offset = l.offset

	return linExprProto
}

func (l *LinearExpr) evaluateSolutionValue(r *cmpb.CpSolverResponse) int64 {
	result := l.offset

	for _, vc := range l.varCoeffs {
		result += r.GetSolution()[vc.ind] * vc.coeff
	}

	return result
}

func int64AsLinearExpressionProto(l int64) *cmpb.LinearExpressionProto {
	linExprProto := &cmpb.LinearExpressionProto{}
	linExprProto.Offset = l

	return linExprProto
}

// IntVar is a reference to an integer variable in the CP model.
type IntVar struct {
	ind VarIndex
	cpb *Builder
}

// Name returns the name of the variable.
func (i IntVar) Name() string {
	return i.cpb.cmpb.GetVariables()[i.ind].GetName()
}

// Domain returns the domain of the variable.
func (i IntVar) Domain() (Domain, error) {
	dom := i.cpb.cmpb.GetVariables()[i.ind].GetDomain()
	return FromFlatIntervals(dom)
}

// Index returns the index of the variable.
func (i IntVar) Index() VarIndex {
	return i.ind
}

// WithName sets the name of the variable.
func (i IntVar) WithName(s string) IntVar {
	i.cpb.cmpb.GetVariables()[i.ind].Name = s
	return i
}

func (i IntVar) addToLinearExpr(e *LinearExpr, c int64) {
	e.varCoeffs = append(e.varCoeffs, varCoeff{ind: i.ind, coeff: c})
}

func (i IntVar) asLinearExpressionProto() *cmpb.LinearExpressionProto {
	linExprProto := &cmpb.LinearExpressionProto{}

	linExprProto.Vars = []int32{int32(i.ind)}
	linExprProto.Coeffs = []int64{1}

	return linExprProto
}

func (i IntVar) evaluateSolutionValue(r *cmpb.CpSolverResponse) int64 {
	return r.GetSolution()[i.ind]
}

// BoolVar is a reference to a Boolean variable or the negation of a Boolean variable in the CP
// model.
type BoolVar struct {
	ind VarIndex
	cpb *Builder
}

// Not returns the logical Not of the Boolean variable
func (b BoolVar) Not() BoolVar {
	return BoolVar{ind: -1*b.ind - 1, cpb: b.cpb}
}

// Name returns the name of the variable.
func (b BoolVar) Name() string {
	return b.cpb.cmpb.GetVariables()[b.ind.positiveIndex()].GetName()
}

// Domain returns the domain of the variable.
func (b BoolVar) Domain() (Domain, error) {
	dom := b.cpb.cmpb.GetVariables()[b.ind.positiveIndex()].GetDomain()
	return FromFlatIntervals(dom)
}

// Index returns the index of the variable. If the variable is a negation of a nother variable v,
// its index is `-1*v.index-1`.
func (b BoolVar) Index() VarIndex {
	return b.ind
}

// WithName sets the name of the variable.
func (b BoolVar) WithName(s string) BoolVar {
	b.cpb.cmpb.GetVariables()[b.ind.positiveIndex()].Name = s
	return b
}

func (b BoolVar) addToLinearExpr(e *LinearExpr, c int64) {
	if b.ind < 0 {
		e.varCoeffs = append(e.varCoeffs, varCoeff{ind: b.ind.positiveIndex(), coeff: -c})
		e.offset += c
	} else {
		e.varCoeffs = append(e.varCoeffs, varCoeff{ind: b.ind, coeff: c})
	}
}

func (b BoolVar) asLinearExpressionProto() *cmpb.LinearExpressionProto {
	linExprProto := &cmpb.LinearExpressionProto{}

	linExprProto.Vars = []int32{int32(b.ind.positiveIndex())}
	coeff := int64(1)
	var offset int64
	if b.ind < 0 {
		coeff = -1
		offset = 1
	}
	linExprProto.Coeffs = []int64{coeff}
	linExprProto.Offset = offset

	return linExprProto
}

func (b BoolVar) evaluateSolutionValue(r *cmpb.CpSolverResponse) int64 {
	if b.ind < 0 {
		return 1 - r.GetSolution()[b.ind.positiveIndex()]
	}
	return r.GetSolution()[b.ind]
}

func asNegatedLinearExpressionProto(la LinearArgument) *cmpb.LinearExpressionProto {
	result := la.asLinearExpressionProto()

	for i, c := range result.GetCoeffs() {
		result.GetCoeffs()[i] = -c
	}
	result.Offset = -result.GetOffset()

	return result
}

// IntervalVar is a reference to an interval variable in the CP model. An interval variable is both
// a variable and a constraint. It is defined by three elements (start, size, end) and enforces
// that start + size == end.
type IntervalVar struct {
	ind ConstrIndex
	cpb *Builder
}

// Name returns the name of the interval variable.
func (iv IntervalVar) Name() string {
	return iv.cpb.cmpb.GetConstraints()[iv.ind].GetName()
}

// Index returns the index of the interval variable.
func (iv IntervalVar) Index() ConstrIndex {
	return iv.ind
}

// WithName sets the name of the interval variable.
func (iv IntervalVar) WithName(s string) IntervalVar {
	iv.cpb.cmpb.GetConstraints()[iv.ind].Name = s
	return iv
}

// Constraint is a reference to a constraint in the CP model.
type Constraint struct {
	ind ConstrIndex
	cpb *Builder
}

// WithName sets the name of the constraint.
func (c Constraint) WithName(s string) Constraint {
	c.cpb.cmpb.GetConstraints()[c.ind].Name = s
	return c
}

// Name returns the name of the constraint.
func (c Constraint) Name() string {
	return c.cpb.cmpb.GetConstraints()[c.ind].GetName()
}

// Index returns the index of the constraint.
func (c Constraint) Index() ConstrIndex {
	return c.ind
}

// OnlyEnforceIf adds a condition on the constraint. This constraint is only enforced iff all
// literals given are true.
func (c Constraint) OnlyEnforceIf(bvs ...BoolVar) Constraint {
	cstrpb := c.cpb.cmpb.GetConstraints()[c.ind]
	for _, bv := range bvs {
		cstrpb.EnforcementLiteral = append(cstrpb.GetEnforcementLiteral(), int32(bv.ind))
	}
	return c
}

// NoOverlap2DConstraint is a reference to a specialized no_overlap2D constraint that allows for
// adding rectangles to the constraint incrementally.
type NoOverlap2DConstraint struct {
	Constraint
}

// AddRectangle adds a rectangle (parallel to the axis) to the constraint.
func (noc NoOverlap2DConstraint) AddRectangle(xInterval, yInterval IntervalVar) {
	if !xInterval.cpb.checkSameModelAndSetErrorf(yInterval.cpb, "invalid parameters xInterval %v and yInterval %v added to NoOverlapConstraint %v", xInterval.Index(), yInterval.Index(), noc.Index()) {
		return
	}
	noOverlapCt := noc.cpb.cmpb.GetConstraints()[noc.ind].GetNoOverlap_2D()
	noOverlapCt.XIntervals = append(noOverlapCt.GetXIntervals(), int32(xInterval.ind))
	noOverlapCt.YIntervals = append(noOverlapCt.GetYIntervals(), int32(yInterval.ind))
}

// CircuitConstraint is a reference to a specialized circuit constraint that allows for
// adding arcs to the constraint incrementally.
type CircuitConstraint struct {
	Constraint
}

// AddArc adds an arc to the circuit constraint. `tail` and `head` are the indices of the tail
// and head nodes, respectively, and `literal` is true if the arc is selected.
func (cc *CircuitConstraint) AddArc(tail, head int32, literal BoolVar) {
	if !cc.cpb.checkSameModelAndSetErrorf(literal.cpb, "invalid parameter Boolvar %v added to CircuitConstraint %v", literal.Index(), cc.Index()) {
		return
	}
	cirCt := cc.cpb.cmpb.GetConstraints()[cc.ind].GetCircuit()
	cirCt.Tails = append(cirCt.GetTails(), tail)
	cirCt.Heads = append(cirCt.GetHeads(), head)
	cirCt.Literals = append(cirCt.GetLiterals(), int32(literal.ind))
}

// MultipleCircuitConstraint is a reference to a specialized circuit constraint that allows for
// adding arcs to the constraint incrementally.
type MultipleCircuitConstraint struct {
	Constraint
}

// AddRoute adds an arc to the circuit constraint. `tail` and `head` and the indices of the tail
// and head nodes, respectively, and `literal` is true if the arc is selected.
func (mc *MultipleCircuitConstraint) AddRoute(tail, head int32, literal BoolVar) {
	if !mc.cpb.checkSameModelAndSetErrorf(literal.cpb, "invalid parameter boolvar %v added to MultipleCircuitConstraint %v", literal.Index(), mc.Index()) {
		return
	}
	multCirCt := mc.cpb.cmpb.GetConstraints()[mc.ind].GetRoutes()
	multCirCt.Tails = append(multCirCt.GetTails(), tail)
	multCirCt.Heads = append(multCirCt.GetHeads(), head)
	multCirCt.Literals = append(multCirCt.GetLiterals(), int32(literal.ind))
}

// TableConstraint is a reference to a specialized assignment constraint that allows for adding
// tuples incrementally to the allowed/forbidden assignment constraint.
type TableConstraint struct {
	Constraint
}

// AddTuple adds a tuple of possible values to the table constraint.
func (tc *TableConstraint) AddTuple(tuple ...int64) {
	ct := tc.cpb.cmpb.GetConstraints()[tc.ind].GetTable()
	if len(ct.GetVars()) != len(tuple) {
		log.Fatalf("length of vars in the proto must be the same length as the input tuple: %v != %v", len(ct.GetVars()), len(tuple))
	}

	ct.Values = append(ct.GetValues(), tuple...)
}

// ReservoirConstraint is a reference to a specialized reservoir constraint that allows for
// adding emptying/refilling events incrementally to the constraint.
type ReservoirConstraint struct {
	Constraint
	oneIndex VarIndex
}

// AddEvent adds an event to the reservoir constraint. It will increase the used capacity
// by `levelChange` at time `time`.
func (rc *ReservoirConstraint) AddEvent(time LinearArgument, levelChange LinearArgument) {
	ct := rc.cpb.cmpb.GetConstraints()[rc.ind].GetReservoir()
	ct.TimeExprs = append(ct.GetTimeExprs(), time.asLinearExpressionProto())
	ct.LevelChanges = append(ct.GetLevelChanges(), levelChange.asLinearExpressionProto())
	ct.ActiveLiterals = append(ct.GetActiveLiterals(), int32(rc.oneIndex))
}

// AutomatonConstraint is a reference to a specialized automaton constraint that allows for
// adding transitions incrementally to the constraint.
type AutomatonConstraint struct {
	Constraint
}

// AddTransition adds a transition to the constraint. Both tail and head are states, label
// is any variable value. No two outgoing transitions from the same state can have the same
// label.
func (ac *AutomatonConstraint) AddTransition(tail, head int64, label int64) {
	ct := ac.cpb.cmpb.GetConstraints()[ac.ind].GetAutomaton()
	ct.TransitionTail = append(ct.GetTransitionTail(), tail)
	ct.TransitionHead = append(ct.GetTransitionHead(), head)
	ct.TransitionLabel = append(ct.GetTransitionLabel(), label)
}

// CumulativeConstraint is a reference to a specialized cumulative constraint that allows for
// adding fixe or variable demands incrementally to the constraint.
type CumulativeConstraint struct {
	Constraint
}

// AddDemand adds the demand to the constraint for the specified interval.
func (cc *CumulativeConstraint) AddDemand(interval IntervalVar, demand LinearArgument) {
	if !cc.cpb.checkSameModelAndSetErrorf(interval.cpb, "invalid parameter intervalVar %v added to CumulativeConstraint %v", interval.Index(), cc.Index()) {
		return
	}
	ct := cc.cpb.cmpb.GetConstraints()[cc.ind].GetCumulative()
	ct.Intervals = append(ct.GetIntervals(), int32(interval.ind))
	ct.Demands = append(ct.GetDemands(), demand.asLinearExpressionProto())
}

// checkSameModelAndSetErrorf returns true if `cp` and `cp2` point to the same Builder.
// If false, an error with the error message `errString` is set on `cp` if `cp.err`
// is nil.
func (cp *Builder) checkSameModelAndSetErrorf(cp2 *Builder, format string, a ...any) bool {
	if cp == cp2 {
		return true
	}
	var args = make([]any, len(a)+1)
	copy(args, a)
	args[len(a)] = ErrMixedModels
	err := fmt.Errorf(format+": %w", args...)
	log.Errorf("%v; use `-log_backtrace_at` flag to get the error stack", err)
	if cp.err == nil {
		cp.err = err
	}
	return false
}

// Builder provides a wrapper for the CpModelProto builder.
type Builder struct {
	cmpb      *cmpb.CpModelProto
	constants map[int64]VarIndex
	// The first and only the first error is reported in Build.
	err error
}

// NewCpModelBuilder creates and returns a new CpModel Builder.
func NewCpModelBuilder() *Builder {
	return &Builder{cmpb: &cmpb.CpModelProto{}, constants: make(map[int64]VarIndex)}
}

// NewIntVar creates a new intVar in the CpModel proto.
func (cp *Builder) NewIntVar(lb, ub int64) IntVar {
	intVar := IntVar{cpb: cp, ind: VarIndex(len(cp.cmpb.GetVariables()))}

	pVar := &cmpb.IntegerVariableProto{Domain: []int64{lb, ub}}
	cp.cmpb.Variables = append(cp.cmpb.GetVariables(), pVar)

	return intVar
}

// NewIntVarFromDomain creates a new IntVar with the given domain in the CpModel proto.
func (cp *Builder) NewIntVarFromDomain(d Domain) IntVar {
	intVar := IntVar{cpb: cp, ind: VarIndex(len(cp.cmpb.GetVariables()))}

	pVar := &cmpb.IntegerVariableProto{Domain: d.FlattenedIntervals()}
	cp.cmpb.Variables = append(cp.cmpb.GetVariables(), pVar)

	return intVar
}

// NewBoolVar creates a new BoolVar in the CpModel proto.
func (cp *Builder) NewBoolVar() BoolVar {
	boolVar := BoolVar{cpb: cp, ind: VarIndex(len(cp.cmpb.GetVariables()))}

	pVar := &cmpb.IntegerVariableProto{Domain: []int64{0, 1}}
	cp.cmpb.Variables = append(cp.cmpb.GetVariables(), pVar)

	return boolVar
}

// NewConstant creates a constant variable. If this is called multiple times, the same variable will
// always be returned.
func (cp *Builder) NewConstant(v int64) IntVar {
	if i, ok := cp.constants[v]; ok {
		return IntVar{cpb: cp, ind: i}
	}

	constVar := cp.NewIntVar(v, v)
	cp.constants[v] = constVar.ind

	return constVar
}

// TrueVar creates an always true Boolean variable. If this is called multiple times, the same
// variable will always be returned.
func (cp *Builder) TrueVar() BoolVar {
	if i, ok := cp.constants[1]; ok {
		return BoolVar{cpb: cp, ind: i}
	}

	boolVar := BoolVar{cpb: cp, ind: VarIndex(len(cp.cmpb.GetVariables()))}
	pVar := &cmpb.IntegerVariableProto{Domain: []int64{1, 1}}
	cp.cmpb.Variables = append(cp.cmpb.GetVariables(), pVar)

	cp.constants[1] = boolVar.ind

	return boolVar
}

// FalseVar creates an always false Boolean variable. If this is called multiple times, the same
// variable will always be returned.
func (cp *Builder) FalseVar() BoolVar {
	if i, present := cp.constants[0]; present {
		return BoolVar{cpb: cp, ind: i}
	}

	boolVar := BoolVar{cpb: cp, ind: VarIndex(len(cp.cmpb.GetVariables()))}
	pVar := &cmpb.IntegerVariableProto{Domain: []int64{0, 0}}
	cp.cmpb.Variables = append(cp.cmpb.GetVariables(), pVar)

	cp.constants[0] = boolVar.ind

	return boolVar
}

// NewIntervalVar creates a new interval variable from the three linear arguments. The interval
// variable enforces that `start` + `size` == `end`.
func (cp *Builder) NewIntervalVar(start, size, end LinearArgument) IntervalVar {
	return cp.NewOptionalIntervalVar(start, size, end, cp.TrueVar())
}

// NewFixedSizeIntervalVar creates a new interval variable with the fixed size.
func (cp *Builder) NewFixedSizeIntervalVar(start LinearArgument, size int64) IntervalVar {
	return cp.NewOptionalFixedSizeIntervalVar(start, size, cp.TrueVar())
}

// NewOptionalIntervalVar creates an optional interval variable from the three linear arguments and
// the Boolean variable. It only enforces that `start` + `size` = `end` if the `presence` variable
// is true.
func (cp *Builder) NewOptionalIntervalVar(start, size, end LinearArgument, presence BoolVar) IntervalVar {
	cp.AddEquality(NewLinearExpr().Add(start).Add(size), end).OnlyEnforceIf(presence)

	ind := ConstrIndex(len(cp.cmpb.GetConstraints()))
	cp.cmpb.Constraints = append(cp.cmpb.GetConstraints(), &cmpb.ConstraintProto{
		EnforcementLiteral: []int32{int32(presence.ind)},
		Constraint: &cmpb.ConstraintProto_Interval{&cmpb.IntervalConstraintProto{
			Start: start.asLinearExpressionProto(),
			Size:  size.asLinearExpressionProto(),
			End:   end.asLinearExpressionProto(),
		},
		}})

	return IntervalVar{cpb: cp, ind: ind}
}

// NewOptionalFixedSizeIntervalVar creates an optional interval variable with the fixed size. It
// only enforces that the interval is of the fixed size when the `presence` variable is true.
func (cp *Builder) NewOptionalFixedSizeIntervalVar(start LinearArgument, size int64, presence BoolVar) IntervalVar {
	sizeLinExpr := NewConstant(size)
	end := NewLinearExpr().Add(start).Add(sizeLinExpr)

	return cp.NewOptionalIntervalVar(start, sizeLinExpr, end, presence)
}

func (cp *Builder) appendConstraint(ct *cmpb.ConstraintProto) Constraint {
	i := ConstrIndex(len(cp.cmpb.GetConstraints()))
	cp.cmpb.Constraints = append(cp.cmpb.GetConstraints(), ct)

	return Constraint{cpb: cp, ind: i}
}

func buildBoolArgumentProto(cp *Builder, bvs ...BoolVar) *cmpb.BoolArgumentProto {
	var literals []int32
	for _, b := range bvs {
		cp.checkSameModelAndSetErrorf(b.cpb, "BoolVar %v added to Constraint %v", b.Index(), len(cp.cmpb.GetConstraints()))
		literals = append(literals, int32(b.ind))
	}
	return &cmpb.BoolArgumentProto{Literals: literals}
}

// AddBoolOr adds the constraint that at least one of the literals must be true.
func (cp *Builder) AddBoolOr(bvs ...BoolVar) Constraint {
	return cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_BoolOr{buildBoolArgumentProto(cp, bvs...)},
	})
}

// AddBoolAnd adds the constraint that all of the literals must be true.
func (cp *Builder) AddBoolAnd(bvs ...BoolVar) Constraint {
	return cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_BoolAnd{buildBoolArgumentProto(cp, bvs...)},
	})
}

// AddBoolXor adds the constraint that an odd number of the literals must be true.
func (cp *Builder) AddBoolXor(bvs ...BoolVar) Constraint {
	return cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_BoolXor{buildBoolArgumentProto(cp, bvs...)},
	})
}

// AddAtLeastOne adds the constraint that at least one of the literals must be true.
func (cp *Builder) AddAtLeastOne(bvs ...BoolVar) Constraint {
	return cp.AddBoolOr(bvs...)
}

// AddAtMostOne adds the constraint that at most one of the literals must be true.
func (cp *Builder) AddAtMostOne(bvs ...BoolVar) Constraint {
	return cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_AtMostOne{buildBoolArgumentProto(cp, bvs...)},
	})
}

// AddExactlyOne adds the constraint that exactly one of the literals must be true.
func (cp *Builder) AddExactlyOne(bvs ...BoolVar) Constraint {
	return cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_ExactlyOne{buildBoolArgumentProto(cp, bvs...)},
	})
}

// AddImplication adds the constraint a => b.
func (cp *Builder) AddImplication(a, b BoolVar) Constraint {
	return cp.AddBoolOr(a.Not(), b)
}

// addLinearConstraint adds a linear constraint that enforces the value of `le` to be in the
// set of `intervals`. The constant offset of `le` will be subtracted from each interval. See
// `offset()` for more details. All `intervals` are assumed to be disjoint, non-empty, and
// properly sorted.
func (cp *Builder) addLinearConstraint(le *LinearExpr, intervals ...ClosedInterval) Constraint {
	var varIndices []int32
	var varCoeffs []int64
	var domain []int64
	for _, varCoeff := range le.varCoeffs {
		varIndices = append(varIndices, int32(varCoeff.ind))
		varCoeffs = append(varCoeffs, varCoeff.coeff)
	}
	for _, i := range intervals {
		iOffset := i.Offset(-le.offset)
		domain = append(domain, iOffset.Start, iOffset.End)
	}

	return cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_Linear{&cmpb.LinearConstraintProto{
			Vars: varIndices, Coeffs: varCoeffs, Domain: domain,
		}},
	})
}

// AddLinearConstraintForDomain adds the linear constraint `expr` in `domain`.
func (cp *Builder) AddLinearConstraintForDomain(expr LinearArgument, domain Domain) Constraint {
	linExpr := NewLinearExpr().Add(expr)
	return cp.addLinearConstraint(linExpr, domain.intervals...)
}

// AddLinearConstraint adds the linear constraint `lb <= expr <= ub`
func (cp *Builder) AddLinearConstraint(expr LinearArgument, lb, ub int64) Constraint {
	linExpr := NewLinearExpr().Add(expr)
	return cp.addLinearConstraint(linExpr, ClosedInterval{lb, ub})
}

// AddEquality adds the linear constraint `lhs == rhs`.
func (cp *Builder) AddEquality(lhs LinearArgument, rhs LinearArgument) Constraint {
	diff := NewLinearExpr().Add(lhs).AddTerm(rhs, -1)

	return cp.addLinearConstraint(diff, ClosedInterval{0, 0})
}

// AddLessOrEqual adds the linear constraint `lhs <= rhs`.
func (cp *Builder) AddLessOrEqual(lhs LinearArgument, rhs LinearArgument) Constraint {
	diff := NewLinearExpr().Add(lhs).AddTerm(rhs, -1)

	return cp.addLinearConstraint(diff, ClosedInterval{math.MinInt64, 0})
}

// AddLessThan adds the linear constraint `lhs < rhs`.
func (cp *Builder) AddLessThan(lhs LinearArgument, rhs LinearArgument) Constraint {
	diff := NewLinearExpr().Add(lhs).AddTerm(rhs, -1)

	return cp.addLinearConstraint(diff, ClosedInterval{math.MinInt64, -1})
}

// AddGreaterOrEqual adds the linear constraint `lhs >= rhs`.
func (cp *Builder) AddGreaterOrEqual(lhs LinearArgument, rhs LinearArgument) Constraint {
	diff := NewLinearExpr().Add(lhs).AddTerm(rhs, -1)

	return cp.addLinearConstraint(diff, ClosedInterval{0, math.MaxInt64})
}

// AddGreaterThan adds the lienar constraints `lhs > rhs`.
func (cp *Builder) AddGreaterThan(lhs LinearArgument, rhs LinearArgument) Constraint {
	diff := NewLinearExpr().Add(lhs).AddTerm(rhs, -1)

	return cp.addLinearConstraint(diff, ClosedInterval{1, math.MaxInt64})
}

// AddNotEqual adds the linear constraint `lhs != rhs`.
func (cp *Builder) AddNotEqual(lhs LinearArgument, rhs LinearArgument) Constraint {
	diff := NewLinearExpr().Add(lhs).AddTerm(rhs, -1)

	return cp.addLinearConstraint(diff, ClosedInterval{math.MinInt64, -1}, ClosedInterval{1, math.MaxInt64})
}

// AddAllDifferent adds a constraint that forces all expressions to have different values.
func (cp *Builder) AddAllDifferent(la ...LinearArgument) Constraint {
	var exprs []*cmpb.LinearExpressionProto
	for _, l := range la {
		exprs = append(exprs, l.asLinearExpressionProto())
	}

	return cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_AllDiff{&cmpb.AllDifferentConstraintProto{Exprs: exprs}},
	})
}

// AddVariableElement adds the variable element constraint: vars[ind] == target.
func (cp *Builder) AddVariableElement(ind IntVar, vars []IntVar, target IntVar) Constraint {
	var varIndices []int32
	for _, v := range vars {
		varIndices = append(varIndices, int32(v.ind))
	}

	return cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_Element{&cmpb.ElementConstraintProto{
			Index:  int32(ind.ind),
			Target: int32(target.ind),
			Vars:   varIndices,
		}},
	})
}

// AddElement adds the element constraint: values[ind] == target
func (cp *Builder) AddElement(ind IntVar, values []int64, target IntVar) Constraint {
	vars := make([]IntVar, len(values))
	for i, v := range values {
		vars[i] = cp.NewConstant(v)
	}

	return cp.AddVariableElement(ind, vars, target)
}

// AddInverseConstraint adds a constraint that enforces if `vars[i]` is assigned a value
// `j`, then `inverseVars[j]` is assigned a value `i`, and vice versa. The lengths of `vars`
// and `inverseVars` must be the same size.
func (cp *Builder) AddInverseConstraint(vars []IntVar, inverseVars []IntVar) Constraint {
	if len(vars) != len(inverseVars) {
		log.Fatalf("vars and inverseVars must be the same length: %v != %v", len(vars), len(inverseVars))
	}

	var fDirect []int32
	for _, v := range vars {
		fDirect = append(fDirect, int32(v.ind))
	}
	var fInverse []int32
	for _, v := range inverseVars {
		fInverse = append(fInverse, int32(v.ind))
	}

	return cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_Inverse{&cmpb.InverseConstraintProto{
			FDirect:  fDirect,
			FInverse: fInverse,
		}},
	})
}

// AddMinEquality adds the constraint: target == min(exprs).
func (cp *Builder) AddMinEquality(target LinearArgument, exprs ...LinearArgument) Constraint {
	var protos []*cmpb.LinearExpressionProto
	for _, e := range exprs {
		protos = append(protos, asNegatedLinearExpressionProto(e))
	}

	return cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_LinMax{
			&cmpb.LinearArgumentProto{
				Target: asNegatedLinearExpressionProto(target),
				Exprs:  protos,
			}},
	})
}

// AddMaxEquality adds the constraint: target == max(expr).
func (cp *Builder) AddMaxEquality(target LinearArgument, exprs ...LinearArgument) Constraint {
	var protos []*cmpb.LinearExpressionProto
	for _, e := range exprs {
		protos = append(protos, e.asLinearExpressionProto())
	}

	return cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_LinMax{
			&cmpb.LinearArgumentProto{
				Target: target.asLinearExpressionProto(),
				Exprs:  protos,
			}},
	})
}

// AddMultiplicationEquality adds the constraint: target == Product(exprs).
func (cp *Builder) AddMultiplicationEquality(target LinearArgument, exprs ...LinearArgument) Constraint {
	var protos []*cmpb.LinearExpressionProto
	for _, e := range exprs {
		protos = append(protos, e.asLinearExpressionProto())
	}

	return cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_IntProd{
			&cmpb.LinearArgumentProto{
				Target: target.asLinearExpressionProto(),
				Exprs:  protos,
			}},
	})
}

// AddDivisionEquality adds the constraint: target == num / denom.
func (cp *Builder) AddDivisionEquality(target, num, denom LinearArgument) Constraint {
	return cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_IntDiv{
			&cmpb.LinearArgumentProto{
				Target: target.asLinearExpressionProto(),
				Exprs: []*cmpb.LinearExpressionProto{
					num.asLinearExpressionProto(),
					denom.asLinearExpressionProto(),
				},
			}},
	})
}

// AddAbsEquality adds the constraint: target == Abs(expr).
func (cp *Builder) AddAbsEquality(target, expr LinearArgument) Constraint {
	return cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_LinMax{
			&cmpb.LinearArgumentProto{
				Target: target.asLinearExpressionProto(),
				Exprs: []*cmpb.LinearExpressionProto{
					expr.asLinearExpressionProto(),
					asNegatedLinearExpressionProto(expr),
				},
			}},
	})
}

// AddModuloEquality adds the constraint: target == v % mod.
func (cp *Builder) AddModuloEquality(target, v, mod LinearArgument) Constraint {
	return cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_IntMod{
			&cmpb.LinearArgumentProto{
				Target: target.asLinearExpressionProto(),
				Exprs: []*cmpb.LinearExpressionProto{
					v.asLinearExpressionProto(),
					mod.asLinearExpressionProto(),
				},
			}},
	})
}

// AddNoOverlap adds a constraint that ensures that all present intervals do not overlap in time.
func (cp *Builder) AddNoOverlap(vars ...IntervalVar) Constraint {
	intervals := make([]int32, len(vars))
	for i, v := range vars {
		cp.checkSameModelAndSetErrorf(v.cpb, "invalid parameter intervalVar %v added to the AddNoOverlap constraint %v", v.Index(), len(cp.cmpb.GetConstraints()))
		intervals[i] = int32(v.ind)
	}

	return cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_NoOverlap{
			&cmpb.NoOverlapConstraintProto{
				Intervals: intervals,
			}},
	})
}

// AddNoOverlap2D adds a no_overlap2D constraint that prevents a set of boxes from overlapping.
func (cp *Builder) AddNoOverlap2D() NoOverlap2DConstraint {
	return NoOverlap2DConstraint{cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_NoOverlap_2D{
			&cmpb.NoOverlap2DConstraintProto{},
		}})}
}

// AddCircuitConstraint adds a circuit constraint to the model. The circuit constraint is
// defined on a graph where the arcs are present if the corresponding literals are set to true.
func (cp *Builder) AddCircuitConstraint() CircuitConstraint {
	return CircuitConstraint{cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_Circuit{
			&cmpb.CircuitConstraintProto{},
		}})}
}

// AddMultipleCircuitConstraint adds a multiple circuit constraint to the model, aka the "VRP"
// (Vehicle Routing Problem) constraint.
func (cp *Builder) AddMultipleCircuitConstraint() MultipleCircuitConstraint {
	return MultipleCircuitConstraint{cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_Routes{
			&cmpb.RoutesConstraintProto{},
		}})}
}

// AddAllowedAssignments adds an allowed assignments constraint to the model. When all variables
// are fixed to a single value, it forces the corresponding list of values to be
// equal to one of the tuples added to the constraint.
func (cp *Builder) AddAllowedAssignments(vars ...IntVar) TableConstraint {
	var varsInd []int32
	for _, v := range vars {
		varsInd = append(varsInd, int32(v.ind))
	}

	return TableConstraint{cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_Table{
			&cmpb.TableConstraintProto{Vars: varsInd},
		}})}
}

// AddReservoirConstraint adds a reservoir constraint with optional refill/emptying events.
//
// Maintain a reservoir level within bounds. The water level starts at 0, and
// at any time, it must be within [min_level, max_level].
//
// Given an event (time, level_change, active), if active is true, and if time
// is assigned a value t, then the level of the reservoir changes by
// level_change (which is constant) at time t. Therefore, at any time t:
//
//	sum(level_changes[i] * actives[i] if times[i] <= t)
//	    in [min_level, max_level]
//
// Note that min level must be <= 0, and the max level must be >= 0.
// Please use fixed level_changes to simulate an initial state.
//
// It returns a ReservoirConstraint that allows adding optional and non
// optional events incrementally after construction.
func (cp *Builder) AddReservoirConstraint(min, max int64) ReservoirConstraint {
	return ReservoirConstraint{
		cp.appendConstraint(
			&cmpb.ConstraintProto{
				Constraint: &cmpb.ConstraintProto_Reservoir{
					&cmpb.ReservoirConstraintProto{
						MinLevel: min, MaxLevel: max,
					}}},
		), cp.NewConstant(1).Index()}
}

// AddAutomaton adds an automaton constraint.
//
// An automaton constraint takes a list of variables (of size n), an initial
// state, a set of final states, and a set of transitions. A transition is a
// triplet ('tail', 'head', 'label'), where 'tail' and 'head' are states,
// and 'label' is the label of an arc from 'head' to 'tail',
// corresponding to the value of one variable in the list of variables.
//
// This automaton will be unrolled into a flow with n + 1 phases. Each phase
// contains the possible states of the automaton. The first state contains the
// initial state. The last phase contains the final states.
//
// Between two consecutive phases i and i + 1, the automaton creates a set of
// arcs. For each transition (tail, head, label), it will add an arc from
// the state 'tail' of phase i and the state 'head' of phase i + 1. This arc
// labeled by the value 'label' of the variables 'variables[i]'. That is,
// this arc can only be selected if 'variables[i]' is assigned the value
// 'label'. A feasible solution of this constraint is an assignment of
// variables such that, starting from the initial state in phase 0, there is a
// path labeled by the values of the variables that ends in one of the final
// states in the final phase.
//
// It returns an AutomatonConstraint that allows adding transition
// incrementally after construction.
func (cp *Builder) AddAutomaton(transitionVars []IntVar, startState int64, finalStates []int64) AutomatonConstraint {
	var transitions []int32
	for _, v := range transitionVars {
		cp.checkSameModelAndSetErrorf(v.cpb, "invalid parameter intVar %v added to the AutomatonConstraint %v", v.Index(), len(cp.cmpb.GetConstraints()))
		transitions = append(transitions, int32(v.Index()))
	}
	return AutomatonConstraint{cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_Automaton{
			&cmpb.AutomatonConstraintProto{
				Vars:          transitions,
				StartingState: startState,
				FinalStates:   finalStates,
			}},
	})}
}

// AddCumulative adds a cumulative constraint to the model that ensures that for any integer
// point, the sum of the demands of the intervals containging that point does not exceed the
// capacity.
func (cp *Builder) AddCumulative(capacity LinearArgument) CumulativeConstraint {
	return CumulativeConstraint{cp.appendConstraint(&cmpb.ConstraintProto{
		Constraint: &cmpb.ConstraintProto_Cumulative{
			&cmpb.CumulativeConstraintProto{
				Capacity: capacity.asLinearExpressionProto(),
			},
		}})}
}

// Minimize adds a linear minimization objective.
func (cp *Builder) Minimize(obj LinearArgument) {
	o := NewLinearExpr().Add(obj)

	opb := &cmpb.CpObjectiveProto{}
	for _, varCoeff := range o.varCoeffs {
		opb.Vars = append(opb.GetVars(), int32(varCoeff.ind))
		opb.Coeffs = append(opb.GetCoeffs(), varCoeff.coeff)
	}
	opb.Offset = float64(o.offset)

	cp.cmpb.Objective = opb
}

// Maximize adds a linear maximization objective.
func (cp *Builder) Maximize(obj LinearArgument) {
	o := NewLinearExpr().Add(obj)

	opb := &cmpb.CpObjectiveProto{}
	for _, varCoeff := range o.varCoeffs {
		opb.Vars = append(opb.GetVars(), int32(varCoeff.ind))
		opb.Coeffs = append(opb.GetCoeffs(), -varCoeff.coeff)
	}
	opb.Offset = float64(-o.offset)
	opb.ScalingFactor = -1

	cp.cmpb.Objective = opb
}

// Hint is a container for IntVar and BoolVar hints to the CP model.
type Hint struct {
	Ints  map[IntVar]int64
	Bools map[BoolVar]bool
}

type indexValueSlices struct {
	indices []int32
	values  []int64
}

func (ivs indexValueSlices) Len() int {
	return len(ivs.indices)
}

func (ivs indexValueSlices) Less(i, j int) bool {
	return ivs.indices[i] < ivs.indices[j]
}

func (ivs indexValueSlices) Swap(i, j int) {
	ivs.indices[i], ivs.indices[j] = ivs.indices[j], ivs.indices[i]
	ivs.values[i], ivs.values[j] = ivs.values[j], ivs.values[i]
}

func (h *Hint) proto() *cmpb.PartialVariableAssignment {
	if h == nil {
		return nil
	}

	var vars []int32
	var hints []int64
	for iv, hint := range h.Ints {
		vars = append(vars, int32(iv.ind))
		hints = append(hints, hint)
	}
	for bv, hint := range h.Bools {
		var hintInt int64
		if hint {
			hintInt = 1
		}
		if bv.ind < 0 {
			hintInt = 1 - hintInt
		}
		vars = append(vars, int32(bv.ind.positiveIndex()))
		hints = append(hints, hintInt)
	}
	sort.Sort(indexValueSlices{vars, hints})

	return &cmpb.PartialVariableAssignment{Vars: vars, Values: hints}
}

// SetHint sets the hint on the model.
func (cp *Builder) SetHint(hint *Hint) {
	cp.cmpb.SolutionHint = hint.proto()
}

// ClearHint clears any hints on the model.
func (cp *Builder) ClearHint() {
	cp.cmpb.SolutionHint = nil
}

// AddAssumption adds the literals to the model as assumptions.
func (cp *Builder) AddAssumption(lits ...BoolVar) {
	for _, lit := range lits {
		if !cp.checkSameModelAndSetErrorf(lit.cpb, "BoolVar %v added as an Assumption", lit.Index()) {
			return
		}
		cp.cmpb.Assumptions = append(cp.cmpb.GetAssumptions(), int32(lit.ind))
	}
}

// ClearAssumption clears all the assumptions on the model.
func (cp *Builder) ClearAssumption() {
	cp.cmpb.Assumptions = nil
}

// AddDecisionStrategy adds a decision strategy on a list of integer variables.
func (cp *Builder) AddDecisionStrategy(vars []IntVar, vs cmpb.DecisionStrategyProto_VariableSelectionStrategy, ds cmpb.DecisionStrategyProto_DomainReductionStrategy) {
	var indices []int32
	for _, v := range vars {
		if !cp.checkSameModelAndSetErrorf(v.cpb, "invalid parameter var %v added to the DecisionStrategy", v.Index()) {
			return
		}
		indices = append(indices, int32(v.ind))
	}

	cp.cmpb.SearchStrategy = append(cp.cmpb.GetSearchStrategy(), &cmpb.DecisionStrategyProto{
		Variables:                 indices,
		VariableSelectionStrategy: vs,
		DomainReductionStrategy:   ds,
	})
}

// Model returns the built CP model proto. The proto returned is a pointer to the proto in Builder,
// and if modified, future calls to the Builder API can fail or result in an INVALID model.
//
// The model returned is mutable and modifying any variables and constraints may result in
// unexpected changes to the model builder functionality. For example, multiple calls to
// `NewConstant()` will return the same variable even if the variable's domain has been changed to
// make it no longer a constant.
//
// Model returns an error when invalid parameters have been used during model building (e.g.
// passing variables from other builders).
func (cp *Builder) Model() (*cmpb.CpModelProto, error) {
	if cp.err != nil {
		return nil, cp.err
	}
	return cp.cmpb, nil
}
