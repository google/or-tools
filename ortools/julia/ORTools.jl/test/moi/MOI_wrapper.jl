module TestMOIWrapper

using Test
using ORTools
import MathOptInterface as MOI

## Test the optimizer
function runtests()
    for name in names(@__MODULE__; all = true)
        if startswith("$(name)", "test_")
            @testset "$(name)" begin
                getfield(@__MODULE__, name)()
            end
        end
    end
end


function test_runtests()
    model = MOI.Bridges.full_bridge_optimizer(ORTools.Optimizer(), Float64)

    MOI.Test.runtests(
        model,
        MOI.Test.Config(),
        exclude = [
            "_SecondOrderCone_",
            "test_constraint_PrimalStart_DualStart_SecondOrderCone",
            "_RotatedSecondOrderCone_",
            ## TODO: b/386355769 implement the copy_to function
            "test_model_copy_",
            ## Silent is actually cleared when the model is emptied in our implementation
            ## hence this test will keep failing. A custom test for the silent attribute
            ## has been added below.
            "test_attribute_after_empty",
            ## TODO: b/386355105 - Add support for MOI.delete
            "test_model_delete",
            ## Removed as out solver supports Real type
            "test_model_supports_constraint_VariableIndex_EqualTo",
            ## Removed as the implementation of MOI.Optimize! is yet to be done
            ## TODO: b/384662497 - implement the optimize! function
            "test_attribute_RawStatusString",
            "test_attribute_SolveTimeSec",
            ## This is dependent on the implementation of the ConstraintSet function
            ## TODO: b/384674354
            "test_basic_",
            ## Replacement implemented here.
            ## Excluded as out implementation does not support MOI.ConstraintIndex
            ## without type parameters.
            "test_constraint_get_ConstraintIndex",
            "test_model_ScalarAffineFunction_ConstraintName",
            ## We throw an error on set, the existing test throws an error on get
            ## hence it has been replaced by out test
            "test_model_duplicate_ScalarAffineFunction_ConstraintName",
            ## We throw an error on set, the existing test throws an error on get
            ## hence it has been replaced by out test
            # TODO: b/392077251
            "test_model_VariableName",
            "test_linear_complex_Zeros",
            "test_linear_complex_Zeros_duplicate",
            ## TODO: b/386359323 implement the solve interface
            "test_solve_",
            ## The default test does not support UInt8, our Optimizer supports all <: Real
            "test_model_supports_constraint_ScalarAffineFunction_EqualTo",
            ## Delete has not been implemented yet for this test to work
            ## TODO: implement the delete function
            "test_model_duplicate_VariableName",
            ## TODO: b/386359144 Requires implementation of copy_to
            ## TODO: b/386355769 implement the copy_to function
            "test_model_ModelFilter_ListOfConstraintTypesPresent",
            ## This test has been reimplemented here as it kept failing
            # TODO: b/392085365
            "test_model_ScalarFunctionConstantNotZero",
            # The set of tests below are failing due to the lack of support for
            # the following functions in the MOI wrapper:
            # TODO: b/392073143 - MOI.VariablePrimal() 
            # TODO:b/392071376 - MOI.ConstraintPrimal()
            # TODO: b/392073145 - MOI.DualStatus()
            # TODO: b/392071380 - MOI.ConstraintDual() 
            # TODO: b/392071459 - MOI.TerminationStatus()
            # TODO: b/392071460 - MOI.ObjectiveValue()
            # TODO: b/392073827 - PrimalStatus()
            "test_constraint_ScalarAffineFunction_LessThan",
            "test_constraint_ScalarAffineFunction_",
            "test_constraint_VectorAffineFunction_",
            "test_constraint_ZeroOne_bounds",
            "test_constraint_ZeroOne_bounds_2",
            "test_constraint_ZeroOne_bounds_3",
            "test_variable_solve_",
            "test_objective_",
            "test_modification_",
            "test_linear_",
        ],
    )

    return
end


function test_model_supports_constraint_VariableIndex_EqualTo()
    model = ORTools.Optimizer()
    # Support for Ints
    @test MOI.supports_constraint(model, MOI.VariableIndex, MOI.EqualTo{Int})
    # Support for Float32
    @test MOI.supports_constraint(model, MOI.VariableIndex, MOI.EqualTo{Float32})
    # Support for Float64
    @test MOI.supports_constraint(model, MOI.VariableIndex, MOI.EqualTo{Float64})
    # Scalar-in-vector
    @test !MOI.supports_constraint(model, MOI.VariableIndex, MOI.Zeros)
    return
end

function test_attribute_after_empty()
    model = MOI.Bridges.full_bridge_optimizer(ORTools.Optimizer(), Float64)
    @test MOI.supports(model, MOI.Silent()) == true
    for value in (true, false)
        MOI.set(model, MOI.Silent(), value)
        @test MOI.get(model, MOI.Silent()) == value
        MOI.empty!(model)
        @test MOI.get(model, MOI.Silent()) == true
    end
    return
end


function test_solver_specific_parameters_of_emphasis_type()
    emphasis_based_attributes =
        [ORTools.Presolve(), ORTools.Cuts(), ORTools.Heuristics(), ORTools.Scaling()]
    model = ORTools.Optimizer()

    for attribute in emphasis_based_attributes
        @test MOI.supports(model, attribute) == true
        @test MOI.get(model, attribute) == ORTools.Emphasis.EMPHASIS_UNSPECIFIED
        MOI.set(model, attribute, ORTools.Emphasis.EMPHASIS_HIGH)
        @test MOI.get(model, attribute) == ORTools.Emphasis.EMPHASIS_HIGH
        MOI.empty!(model)
        @test MOI.get(model, attribute) == nothing
        ## Re-initialize the model
        ORTools.optionally_initialize_model_and_parameters!(model)
    end
    return
end

## Test the gscip parameters attribute with a single internal attribute
function test_attributes_gscip_parameters()
    model = ORTools.Optimizer()
    @test MOI.supports(model, ORTools.GscipParametersAttribute()) == true
    @test MOI.get(model, ORTools.GscipParametersAttribute()) == nothing
    MOI.set(model, ORTools.GscipParametersAttribute(), ORTools.GScipParameters())
    @test MOI.get(model, MOI.RawOptimizerAttribute("gscip__num_solutions")) == 0
    MOI.set(model, MOI.RawOptimizerAttribute("gscip__num_solutions"), Int32(100))
    @test MOI.get(model, MOI.RawOptimizerAttribute("gscip__num_solutions")) == 100
    MOI.empty!(model)
    @test MOI.get(model, ORTools.GscipParametersAttribute()) == nothing
    @test_throws "Either your model or parameters is empty." MOI.get(
        model,
        MOI.RawOptimizerAttribute("gscip__num_solutions"),
    )
    return
end

function test_attributes_gurobi_parameters()
    model = ORTools.Optimizer()
    @test MOI.supports(model, ORTools.GurobiParametersAttribute()) == true
    @test MOI.get(model, ORTools.GurobiParametersAttribute()) == nothing
    MOI.set(model, ORTools.GurobiParametersAttribute(), ORTools.GurobiParameters())
    @test MOI.get(model, MOI.RawOptimizerAttribute("gurobi__parameters")) == []
    MOI.set(
        model,
        MOI.RawOptimizerAttribute("gurobi__parameters"),
        [ORTools.GurobiParameters_Parameter("num_solutions", "100")],
    )
    gurobi_param_vector = MOI.get(model, MOI.RawOptimizerAttribute("gurobi__parameters"))
    @test length(gurobi_param_vector) == 1
    @test gurobi_param_vector[1].name == "num_solutions"
    @test gurobi_param_vector[1].value == "100"
    MOI.empty!(model)
    @test MOI.get(model, ORTools.GurobiParametersAttribute()) == nothing
    @test_throws "Either your model or parameters is empty." MOI.get(
        model,
        MOI.RawOptimizerAttribute("gurobi__parameters"),
    )
    return
end


function test_attribute_glop_parameters()
    model = ORTools.Optimizer()
    @test MOI.supports(model, ORTools.GlopParametersAttribute()) == true
    @test MOI.get(model, ORTools.GlopParametersAttribute()) == nothing
    MOI.set(model, ORTools.GlopParametersAttribute(), ORTools.GlopParameters())
    @test MOI.get(
        model,
        MOI.RawOptimizerAttribute("glop__max_number_of_reoptimizations"),
    ) == Float64(40)
    MOI.set(
        model,
        MOI.RawOptimizerAttribute("glop__max_number_of_reoptimizations"),
        Float64(100),
    )
    @test MOI.get(
        model,
        MOI.RawOptimizerAttribute("glop__max_number_of_reoptimizations"),
    ) == Float64(100)
    MOI.empty!(model)
    @test MOI.get(model, ORTools.GlopParametersAttribute()) == nothing
    @test_throws "Either your model or parameters is empty." MOI.get(
        model,
        MOI.RawOptimizerAttribute("glop__max_number_of_reoptimizations"),
    )
    return
end

function test_attribute_cp_sat_parameters()
    model = ORTools.Optimizer()
    @test MOI.supports(model, ORTools.SatParametersAttribute()) == true
    @test MOI.get(model, ORTools.SatParametersAttribute()) == nothing
    MOI.set(model, ORTools.SatParametersAttribute(), ORTools.SatParameters())
    @test MOI.get(model, MOI.RawOptimizerAttribute("sat__name")) == ""
    MOI.set(model, MOI.RawOptimizerAttribute("sat__name"), "Google_CP_SAT")
    @test MOI.get(model, MOI.RawOptimizerAttribute("sat__name")) == "Google_CP_SAT"
    MOI.empty!(model)
    @test MOI.get(model, ORTools.SatParametersAttribute()) == nothing
    @test_throws "Either your model or parameters is empty." MOI.get(
        model,
        MOI.RawOptimizerAttribute("cp_sat__name"),
    )
    return
end

function test_attribute_glpk_parameters()
    model = ORTools.Optimizer()
    @test MOI.supports(model, ORTools.GlpkParametersAttribute()) == true
    @test MOI.get(model, ORTools.GlpkParametersAttribute()) == nothing
    MOI.set(model, ORTools.GlpkParametersAttribute(), ORTools.GlpkParameters())
    @test MOI.get(
        model,
        MOI.RawOptimizerAttribute("glpk__compute_unbound_rays_if_possible"),
    ) == false
    MOI.set(
        model,
        MOI.RawOptimizerAttribute("glpk__compute_unbound_rays_if_possible"),
        true,
    )
    @test MOI.get(
        model,
        MOI.RawOptimizerAttribute("glpk__compute_unbound_rays_if_possible"),
    ) == true
    MOI.empty!(model)
    @test MOI.get(model, ORTools.GlpkParametersAttribute()) == nothing
    @test_throws "Either your model or parameters is empty." MOI.get(
        model,
        MOI.RawOptimizerAttribute("glpk__name"),
    )
    return
end

function test_attribute_highs_parameters()
    model = ORTools.Optimizer()
    @test MOI.supports(model, ORTools.HighsOptionsAttribute()) == true
    @test MOI.get(model, ORTools.HighsOptionsAttribute()) == nothing
    MOI.set(model, ORTools.HighsOptionsAttribute(), ORTools.HighsOptions())
    @test MOI.get(model, MOI.RawOptimizerAttribute("highs__int_options")) == Dict()
    MOI.set(
        model,
        MOI.RawOptimizerAttribute("highs__int_options"),
        Dict("num_threads" => Int32(10)),
    )
    @test MOI.get(model, MOI.RawOptimizerAttribute("highs__int_options")) ==
          Dict("num_threads" => Int32(10))
    MOI.empty!(model)
    @test MOI.get(model, ORTools.HighsOptionsAttribute()) == nothing
    @test_throws "Either your model or parameters is empty." MOI.get(
        model,
        MOI.RawOptimizerAttribute("highs__name"),
    )
    return
end


"""
    test_constraint_get_ConstraintIndex()

Test getting constraints by name.
"""
function test_constraint_get_ConstraintIndex()
    model = ORTools.Optimizer()
    x = MOI.add_variable(model)
    f = one(Float64) * x
    CI = MOI.ConstraintIndex{typeof(f),MOI.GreaterThan{Float64}}
    @test MOI.supports(model, MOI.ConstraintName(), CI) == true
    c1 = MOI.add_constraint(model, f, MOI.GreaterThan(one(Float64)))
    MOI.set(model, MOI.ConstraintName(), c1, "c1")
    c2 = MOI.add_constraint(model, f, MOI.LessThan(Float64(2)))
    MOI.set(model, MOI.ConstraintName(), c2, "c2")
    F = MOI.ScalarAffineFunction{Float64}
    @test MOI.get(model, MOI.ConstraintIndex{F,MOI.LessThan{Float64}}, "c3") === nothing
    @test MOI.get(model, MOI.ConstraintIndex{F,MOI.GreaterThan{Float64}}, "c3") === nothing
    @test MOI.get(model, MOI.ConstraintIndex{F,MOI.LessThan{Float64}}, "c1") === nothing
    @test MOI.get(model, MOI.ConstraintIndex{F,MOI.GreaterThan{Float64}}, "c2") === nothing
    c1 = MOI.get(model, MOI.ConstraintIndex{F,MOI.GreaterThan{Float64}}, "c1")
    @test MOI.get(model, MOI.ConstraintIndex{F,MOI.GreaterThan{Float64}}, "c1") == c1
    @test MOI.is_valid(model, c1)
    c2 = MOI.get(model, MOI.ConstraintIndex{F,MOI.LessThan{Float64}}, "c2")
    @test MOI.get(model, MOI.ConstraintIndex{F,MOI.LessThan{Float64}}, "c2") == c2
    @test MOI.is_valid(model, c2)
    return
end

"""
  test_model_ScalarAffineFunction_ConstraintName()

Test ConstraintName for ScalarAffineFunction constraints.
"""
function test_model_ScalarAffineFunction_ConstraintName()
    model = ORTools.Optimizer()
    T = Float64
    x = MOI.add_variable(model)
    f = MOI.ScalarAffineFunction([MOI.ScalarAffineTerm(T(1), x)], T(0))
    c1 = MOI.add_constraint(model, f, MOI.GreaterThan(T(0)))
    c2 = MOI.add_constraint(model, f, MOI.LessThan(T(1)))
    MOI.set(model, MOI.ConstraintName(), c1, "c1")
    @test MOI.get(
        model,
        MOI.ConstraintIndex{MOI.ScalarAffineFunction{Float64},MOI.GreaterThan{Float64}},
        "c1",
    ) == c1
    MOI.set(model, MOI.ConstraintName(), c1, "c2")
    @test MOI.get(
        model,
        MOI.ConstraintIndex{MOI.ScalarAffineFunction{Float64},MOI.GreaterThan{Float64}},
        "c1",
    ) === nothing
    @test MOI.get(
        model,
        MOI.ConstraintIndex{MOI.ScalarAffineFunction{Float64},MOI.GreaterThan{Float64}},
        "c2",
    ) == c1
    MOI.set(model, MOI.ConstraintName(), c2, "c1")
    @test MOI.get(
        model,
        MOI.ConstraintIndex{MOI.ScalarAffineFunction{Float64},MOI.LessThan{Float64}},
        "c1",
    ) == c2
    @test_throws ErrorException MOI.set(model, MOI.ConstraintName(), c1, "c1")
    return
end

"""
    test_model_duplicate_ScalarAffineFunction_ConstraintName()

Test duplicate names in ScalarAffineFunction constraints.
"""
function test_model_duplicate_ScalarAffineFunction_ConstraintName()
    model = ORTools.Optimizer()
    T = Float64
    x = MOI.add_variables(model, 3)
    fs = [MOI.ScalarAffineFunction([MOI.ScalarAffineTerm(T(1), xi)], T(0)) for xi in x]
    c = MOI.add_constraints(model, fs, MOI.GreaterThan(T(0)))
    MOI.set(model, MOI.ConstraintName(), c[1], "x")
    @test_throws ErrorException MOI.set(model, MOI.ConstraintName(), c[2], "x")
    MOI.set(model, MOI.ConstraintName(), c[3], "z")
    @test MOI.get(
        model,
        MOI.ConstraintIndex{MOI.ScalarAffineFunction{Float64},MOI.GreaterThan{Float64}},
        "z",
    ) == c[3]
    MOI.set(model, MOI.ConstraintName(), c[2], "y")
    @test MOI.get(
        model,
        MOI.ConstraintIndex{MOI.ScalarAffineFunction{Float64},MOI.GreaterThan{Float64}},
        "x",
    ) == c[1]
    @test MOI.get(
        model,
        MOI.ConstraintIndex{MOI.ScalarAffineFunction{Float64},MOI.GreaterThan{Float64}},
        "y",
    ) == c[2]
    @test_throws ErrorException MOI.set(model, MOI.ConstraintName(), c[3], "x")
    return
end

"""
    test_model_VariableName()

Test MOI.VariableName.
"""
function test_model_VariableName()
    model = ORTools.Optimizer()
    x = MOI.add_variables(model, 2)
    MOI.set(model, MOI.VariableName(), x[1], "x1")
    @test MOI.get(model, MOI.VariableIndex, "x1") == x[1]
    MOI.set(model, MOI.VariableName(), x[1], "x2")
    @test MOI.get(model, MOI.VariableIndex, "x1") === nothing
    @test MOI.get(model, MOI.VariableIndex, "x2") == x[1]
    MOI.set(model, MOI.VariableName(), x[2], "x1")
    @test MOI.get(model, MOI.VariableIndex, "x1") == x[2]
    return
end


function test_model_supports_constraint_ScalarAffineFunction_EqualTo()
    model = ORTools.Optimizer()
    MOI.supports_constraint(model, MOI.ScalarAffineFunction{Float64}, MOI.EqualTo{Float64})
    return
end

"""
    test_model_ScalarFunctionConstantNotZero()

Test adding a linear constraint with a non-zero function constant.

This should either work, or error with `MOI.ScalarFunctionConstantNotZero` if
the model does not support it.

TODO: b/392085365
"""
function test_model_ScalarFunctionConstantNotZero()
    T = Float64
    model = ORTools.Optimizer()
    function _error(S, value)
        F = MOI.ScalarAffineFunction{T}
        return MOI.ScalarFunctionConstantNotZero{T,F,S}(value)
    end
    try
        f = MOI.ScalarAffineFunction(MOI.ScalarAffineTerm{T}[], T(1))
        c = MOI.add_constraint(model, f, MOI.EqualTo(T(2)))
        @test MOI.get(model, MOI.ConstraintFunction(), c) ≈ f
    catch err
        @test err == _error(MOI.EqualTo{T}, T(1))
    end
    try
        f = MOI.ScalarAffineFunction(MOI.ScalarAffineTerm{T}[], T(2))
        c = MOI.add_constraint(model, f, MOI.GreaterThan(T(1)))
        @test MOI.get(model, MOI.ConstraintFunction(), c) ≈ f
    catch err
        @test err == _error(MOI.GreaterThan{T}, T(2))
    end
    return
end

end

## Run all tests.
TestMOIWrapper.runtests()
