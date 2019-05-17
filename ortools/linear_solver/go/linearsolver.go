// Package linearsolver wraps the swigged C++ linear_solver.h interface.
package linearsolver

import (
	"C"
	"errors"
	"fmt"
	"reflect"

	wrap "google3/util/operations_research/linear_solver/go/linear_solver_wrap"
	pb "google3/util/operations_research/linear_solver/linear_solver_go_proto"
)

// LinearSolver wraps the swigged linear solver.
// See comments in ../linear_solver.h.
type LinearSolver struct {
	wrap.MPSolver // anonymous field
}

// Parameters wraps the MPSolverParameters class.
//
// Use it like this:
//     p := NewParameters()
//     defer DeleteParameters(p)
//     p.SetDoubleParam(PRIMAL_TOLERANCE, 1e-4)
//     p.SetIntegerParam(PRESOLVE, PRESOLVE_OFF)
//
// (See the block of const variables defined below.)
type Parameters struct {
	wrap.MPSolverParameters // anonymous field
}

// ExportOptions groups all options for exporting models to text formats.
// Use NewExportOptions() to create.
type ExportOptions struct {
	wrap.MPModelExportOptions
}

// Constants used in Parameters getters/setters.
var (
	// Double params.
	RELATIVE_MIP_GAP wrap.Operations_researchMPSolverParametersDoubleParam = wrap.MPSolverParametersRELATIVE_MIP_GAP
	PRIMAL_TOLERANCE wrap.Operations_researchMPSolverParametersDoubleParam = wrap.MPSolverParametersPRIMAL_TOLERANCE
	DUAL_TOLERANCE   wrap.Operations_researchMPSolverParametersDoubleParam = wrap.MPSolverParametersDUAL_TOLERANCE

	// Integer params.
	PRESOLVE       wrap.Operations_researchMPSolverParametersIntegerParam = wrap.MPSolverParametersPRESOLVE
	LP_ALGORITHM   wrap.Operations_researchMPSolverParametersIntegerParam = wrap.MPSolverParametersLP_ALGORITHM
	INCREMENTALITY wrap.Operations_researchMPSolverParametersIntegerParam = wrap.MPSolverParametersINCREMENTALITY
	SCALING        wrap.Operations_researchMPSolverParametersIntegerParam = wrap.MPSolverParametersSCALING

	// Categorical parameter's possible values.
	// Note that their types are 'int' because that is the function signature of
	// MPSolverParameters::SetIntegerParam()
	PRESOLVE_OFF = int(wrap.MPSolverParametersPRESOLVE_OFF)
	PRESOLVE_ON  = int(wrap.MPSolverParametersPRESOLVE_ON)

	DUAL    = int(wrap.MPSolverParametersDUAL)
	PRIMAL  = int(wrap.MPSolverParametersPRIMAL)
	BARRIER = int(wrap.MPSolverParametersBARRIER)

	INCREMENTALITY_OFF = int(wrap.MPSolverParametersINCREMENTALITY_OFF)
	INCREMENTALITY_ON  = int(wrap.MPSolverParametersINCREMENTALITY_ON)

	SCALING_OFF = int(wrap.MPSolverParametersSCALING_OFF)
	SCALING_ON  = int(wrap.MPSolverParametersSCALING_ON)
)

// New initializes a new linear solver, given a name and a solver
// type.
//
// The solver type uses proto's problem type enum in place of the C++ version,
// since they are consistent.
//
// Caller must make sure the appropriate linear solver implementation is linked.
//
// Note that Go will not track memory allocated on the C++ heap. The caller must
// call Delete() on the returned solver when it is done to free memory.
func New(name string, t pb.MPModelRequest_SolverType) (LinearSolver, error) {
	if !SupportsProblemTypet(t) {
		return LinearSolver{}, fmt.Errorf("problem type %v not supported."+
			" Make sure to add to your dependencies the appropriate"+
			" //util/operations_research/linear_solve/go:XYZ.", t)
	}
	return LinearSolver{
		wrap.NewMPSolver(name, wrap.Operations_researchMPSolverOptimizationProblemType(t)),
	}, nil
}

// Delete destroys the underlying C++ linear solver. All objects linked to the solver,
// including variables and constraints, are invalidated and cannot be used after this call.
func Delete(solver LinearSolver) {
	wrap.DeleteMPSolver(solver.MPSolver)
}

// NewParameters returns a new Parameters.
//
// Note that Go will not track memory allocated on the C++ heap. The caller must
// call DeleteParameters() on the returned object when it is done to free memory.
func NewParameters() Parameters {
	return Parameters{wrap.NewMPSolverParameters()}
}

// DeleteParameters destroys the underlying C++ object.
func DeleteParameters(params Parameters) {
	wrap.DeleteMPSolverParameters(params.MPSolverParameters)
}

// NewExportOptions returns a new ExportOptions.
//
// Note that Go will not track memory allocated on the C++ heap. The caller must
// call DeleteExportOptions() on the returned object when it is done to free
// memory.
func NewExportOptions() ExportOptions {
	return ExportOptions{wrap.NewMPModelExportOptions()}
}

// DeleteExportOptions destroys the underlying C++ object.
func DeleteExportOptions(opts ExportOptions) {
	wrap.DeleteMPModelExportOptions(opts.MPModelExportOptions)
}

// SupportsProblemTypet returns whether the given problem type is supported
// (which depends on the linear solver implementation targets that you linked).
func SupportsProblemTypet(t pb.MPModelRequest_SolverType) bool {
	wrapT := wrap.Operations_researchMPSolverOptimizationProblemType(t)
	return wrap.MPSolverSupportsProblemType(wrapT)
}

// LoadModelFromProto loads a model from the provided proto.
//
// If keepName is set, loading keeps original variable and constraint names.
// In that case, caller must make sure that all variable names and constraint
// names are unique, respectively.
func (ls *LinearSolver) LoadModelFromProto(m *pb.MPModelProto, keepNames bool) error {
	errMsg := make([]string, 1)
	var status pb.MPSolverResponseStatus
	if !keepNames {
		status = ls.MPSolver.LoadModelFromProto(m, errMsg)
	} else {
		status = ls.MPSolver.LoadModelFromProtoWithUniqueNamesOrDie(m, errMsg)
	}
	if len(errMsg[0]) > 0 {
		return errors.New(errMsg[0])
	}
	if status != pb.MPSolverResponseStatus_MPSOLVER_MODEL_IS_VALID {
		return fmt.Errorf("load model failed with status: %s", status)
	}
	return nil
}

// Model returns the model as a proto.
func (ls *LinearSolver) Model() *pb.MPModelProto {
	ret := &pb.MPModelProto{}
	ls.MPSolver.ExportModelToProto(ret)
	return ret
}

// Solve solves the model and returns a status.
//
// The status uses proto's result status type enum in place of the C++ version,
// since they are consistent.
func (ls *LinearSolver) Solve() pb.MPSolverResponseStatus {
	return pb.MPSolverResponseStatus(ls.MPSolver.Solve())
}

// SolveWithParameters is the same as Solve() except it takes a Parameters.
func (ls *LinearSolver) SolveWithParameters(p Parameters) pb.MPSolverResponseStatus {
	return pb.MPSolverResponseStatus(ls.MPSolver.Solve(p.MPSolverParameters))
}

// Solution returns the solution as a proto to the caller. Must be called
// after Solve() is called.
func (ls *LinearSolver) Solution() *pb.MPSolutionResponse {
	ret := &pb.MPSolutionResponse{}
	ls.MPSolver.FillSolutionResponseProto(ret)
	return ret
}

// ProblemType returns the solver type selected.
func (ls *LinearSolver) ProblemType() pb.MPModelRequest_SolverType {
	return pb.MPModelRequest_SolverType(ls.MPSolver.ProblemType())
}

// ConstraintActivities returns activities of all constraints.
func (ls *LinearSolver) ConstraintActivities() []float64 {
	return wrap.DoubleVectorToSlice(ls.MPSolver.ComputeConstraintActivities())
}

// VariableHint represents the hint value of a given variable.
type VariableHint struct {
	Variable *Variable
	Value    float64
}

// SetHint provides an hint for solution.
//
// Please refer to C++ MPSolver::SetHint documentation for details.
func (ls *LinearSolver) SetHint(hints []VariableHint) {
	v := wrap.NewVarValPairVector()
	defer wrap.DeleteVarValPairVector(v)
	// We reuse the same pointer on a pair. The call `v.Add(p)' below will make a
	// copy of the pair.
	p := wrap.NewVarValPair()
	defer wrap.DeleteVarValPair(p)
	for _, h := range hints {
		p.SetFirst(h.Variable)
		p.SetSecond(h.Value)
		v.Add(p)
	}
	ls.MPSolver.SetHint(v)
}

// Objective is the linear objective of the model.
type Objective struct {
	wrap.MPObjective
}

// Variable wraps the MPVariable object.
type Variable struct {
	wrap.MPVariable
}

// Constraint wraps the MPConstraint object.
type Constraint struct {
	wrap.MPConstraint
}

// Objective returns the model's objective.
func (ls *LinearSolver) Objective() Objective {
	return Objective{ls.MPSolver.MutableObjective()}
}

// SetCoefficient sets the coefficient on a variable in the objective.
func (o *Objective) SetCoefficient(v *Variable, coef float64) {
	o.MPObjective.SetCoefficient(v.MPVariable, coef)
}

// Coefficient gets the coefficient on a variable in the objective.
func (o *Objective) Coefficient(v *Variable) float64 {
	return o.MPObjective.GetCoefficient(v.MPVariable)
}

// MakeVar creates and returns a new variable.
//
// Make `name` an empty string if you would like a unique variable name to be
// generated. Otherwise an error is returned if the provided `name` already
// exists as a variable name.
func (ls *LinearSolver) MakeVar(lb, ub float64, integer bool, name string) (*Variable, error) {
	if name != "" && ls.LookupVar(name) != nil {
		return nil, fmt.Errorf("variable with name %s already exists", name)
	}
	// We check for unique name so that MakeVar() doesn't check fail in C++.
	return &Variable{ls.MPSolver.MakeVar(lb, ub, integer, name)}, nil
}

// LookupVar returns the variable with the given name, or nil if not found.
//
// Do not compare the returned pointer because different pointers can still
// refer to the same variable in the model. Use v.Name() or v.Index() to test
// for the same variable.
func (ls *LinearSolver) LookupVar(name string) *Variable {
	wrapVar := ls.MPSolver.LookupVariableOrNull(name)
	// wrapVar is a nullptr in C++ if name is not found.
	if reflect.ValueOf(wrapVar).Uint() == uint64(0) {
		return nil
	}
	return &Variable{wrapVar}
}

// MakeConstraint creates and returns a new constraint.
//
// Make `name` an empty string if you would like a unique constraint name to be
// generated. Otherwise an error is returned if the provided `name` already
// exists as a constraint name.
func (ls *LinearSolver) MakeConstraint(lb, ub float64, name string) (*Constraint, error) {
	if name != "" && ls.LookupConstraint(name) != nil {
		return nil, fmt.Errorf("constraint with name %s already exists", name)
	}
	// We check for unique name so that MakeRowConstraint() doesn't check fail.
	return &Constraint{ls.MPSolver.MakeRowConstraint(lb, ub, name)}, nil
}

// LookupConstraint return the constraint with the given name, or nil if not
// found.
//
// Do not compare the returned pointer because different pointers can still
// refer to the same constraint in the model. Use c.Name() or c.Index() to
// test for the same constraint.
func (ls *LinearSolver) LookupConstraint(name string) *Constraint {
	wrapCons := ls.MPSolver.LookupConstraintOrNull(name)
	// wrapVar is a nullptr in C++ if name is not found.
	if reflect.ValueOf(wrapCons).Uint() == uint64(0) {
		return nil
	}
	return &Constraint{wrapCons}
}

// SetCoefficient sets the coefficient on a variable in a constraint.
func (c *Constraint) SetCoefficient(v *Variable, coef float64) {
	c.MPConstraint.SetCoefficient(v.MPVariable, coef)
}

// Coefficient gets the coefficient on a variable in a constraint.
func (c *Constraint) Coefficient(v *Variable) float64 {
	return c.MPConstraint.GetCoefficient(v.MPVariable)
}

// This set of constants are the possible return values of Basis_status() of
// a Variable or a Constraint.
var (
	FREE           wrap.Operations_researchMPSolverBasisStatus = wrap.MPSolverFREE
	AT_LOWER_BOUND wrap.Operations_researchMPSolverBasisStatus = wrap.MPSolverAT_LOWER_BOUND
	AT_UPPER_BOUND wrap.Operations_researchMPSolverBasisStatus = wrap.MPSolverAT_UPPER_BOUND
	FIXED_VALUE    wrap.Operations_researchMPSolverBasisStatus = wrap.MPSolverFIXED_VALUE
	BASIC          wrap.Operations_researchMPSolverBasisStatus = wrap.MPSolverBASIC
)

// ExportModelAsLpFormat outputs the model as a string in LP format.
// See comments in ../model_exporter.h
//
// Usage:
//  options := NewExportOptions()
//  defer DeleteExportOptions(options)
//  options.SetObfuscate(true)
//  modelStr, err := ExportModelAsLpFormat(model, options)
func ExportModelAsLpFormat(model *pb.MPModelProto, options ExportOptions) (string, error) {
	modelStr := wrap.ExportModelAsLpFormatReturnString(model, options)
	if len(modelStr) == 0 {
		return "", errors.New("cannot export an invalid model as LP format")
	}
	return modelStr, nil
}

// ExportModelAsMpsFormat outputs the model as a string in MPS format.
// See comments in ../model_exporter.h
func ExportModelAsMpsFormat(model *pb.MPModelProto, options ExportOptions) (string, error) {
	modelStr := wrap.ExportModelAsMpsFormatReturnString(model, options)
	if len(modelStr) == 0 {
		return "", errors.New("cannot export an invalid model as MPS format")
	}
	return modelStr, nil
}
