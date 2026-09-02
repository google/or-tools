module TestMOIWrapper

using Test
using ORTools
import ORToolsBinaries
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
            ## test_solve_* tests failing due to unsupported solver features
            # Unbounded integer variable domain in CP-SAT
            "test_solve_ObjectiveBound_MAX_SENSE_IP",
            "test_solve_ObjectiveBound_MIN_SENSE_IP",
            # Unsupported SOS2 constraints
            "test_solve_SOS2_add_and_delete",
            # Variable-level reduced costs vs MOI split bound duals
            "test_solve_VariableIndex_ConstraintDual_MAX_SENSE",
            "test_solve_VariableIndex_ConstraintDual_MIN_SENSE",
            "test_constraint_ScalarAffineFunction_",
            # test_variable_solve
            "test_variable_solve_Integer_with_lower_bound",
            "test_variable_solve_Integer_with_upper_bound",
            "test_variable_solve_ZeroOne_with_",
            "test_variable_solve_with_lowerbound",
            "test_variable_solve_with_upperbound",
            ## test_linear_* tests that are failing
            "test_linear_DUAL_INFEASIBLE",
            "test_linear_DUAL_INFEASIBLE_2",
            "test_linear_FEASIBILITY_SENSE",
            "test_linear_Indicator_ON_ONE",
            "test_linear_Indicator_ON_ZERO",
            "test_linear_Indicator_constant_term",
            "test_linear_Indicator_integration",
            "test_linear_Interval_inactive",
            "test_linear_SOS1_integration",
            "test_linear_SOS2_integration",
            "test_linear_Semicontinuous_integration",
            "test_linear_Semiinteger_integration",
            "test_linear_VariablePrimalStart_partial",
            "test_linear_integer_integration",
            "test_linear_integer_knapsack",
            "test_linear_integer_solve_twice",
            "test_linear_integration",
            "test_linear_integration_2",
            "test_linear_integration_Interval",
            "test_linear_integration_delete_variables",
            "test_linear_open_intervals",
            "test_linear_variable_open_intervals",
            # Conic norm cone tests: VectorAffineFunction & VectorOfVariables fail because ConstraintDual returns opposite sign for bridged equality constraints
            "test_conic_NormInfinityCone_VectorAffineFunction",
            "test_conic_NormInfinityCone_VectorOfVariables",
            "test_conic_NormOneCone_VectorAffineFunction",
            "test_conic_NormOneCone_VectorOfVariables",
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

function test_solver_specific_parameters_of_emphasis_type()
    emphasis_based_attributes =
        [ORTools.Presolve(), ORTools.Cuts(), ORTools.Heuristics(), ORTools.Scaling()]

    for attribute in emphasis_based_attributes
        model = ORTools.Optimizer()
        @test MOI.supports(model, attribute) == true
        @test isnothing(MOI.get(model, attribute))
        MOI.set(model, attribute, ORTools.Emphasis.EMPHASIS_HIGH)
        @test MOI.get(model, attribute) == ORTools.Emphasis.EMPHASIS_HIGH
        MOI.empty!(model)
        @test MOI.get(model, attribute) == ORTools.Emphasis.EMPHASIS_HIGH
    end
    return
end


function test_variable_solve_with_lowerbound()
    model = MOI.Bridges.full_bridge_optimizer(ORTools.Optimizer(), Float64)
    x = MOI.add_variable(model)
    c1 = MOI.add_constraint(model, x, MOI.GreaterThan(1.0))
    c2 = MOI.add_constraint(model, x, MOI.LessThan(2.0))
    f = MOI.ScalarAffineFunction([MOI.ScalarAffineTerm(2.0, x)], 0.0)
    MOI.set(model, MOI.ObjectiveFunction{typeof(f)}(), f)
    MOI.set(model, MOI.ObjectiveSense(), MOI.MIN_SENSE)
    MOI.optimize!(model)
    @test MOI.get(model, MOI.TerminationStatus()) == MOI.OPTIMAL
    @test MOI.get(model, MOI.PrimalStatus()) == MOI.FEASIBLE_POINT
    @test MOI.get(model, MOI.ObjectiveValue()) ≈ 2.0
    @test MOI.get(model, MOI.VariablePrimal(), x) ≈ 1.0
    return
end

function test_variable_solve_with_upperbound()
    model = MOI.Bridges.full_bridge_optimizer(ORTools.Optimizer(), Float64)
    x = MOI.add_variable(model)
    c1 = MOI.add_constraint(model, x, MOI.LessThan(1.0))
    c2 = MOI.add_constraint(model, x, MOI.GreaterThan(0.0))
    f = MOI.ScalarAffineFunction([MOI.ScalarAffineTerm(2.0, x)], 0.0)
    MOI.set(model, MOI.ObjectiveFunction{typeof(f)}(), f)
    MOI.set(model, MOI.ObjectiveSense(), MOI.MAX_SENSE)
    MOI.optimize!(model)
    @test MOI.get(model, MOI.TerminationStatus()) == MOI.OPTIMAL
    @test MOI.get(model, MOI.PrimalStatus()) == MOI.FEASIBLE_POINT
    @test MOI.get(model, MOI.ObjectiveValue()) ≈ 2.0
    @test MOI.get(model, MOI.VariablePrimal(), x) ≈ 1.0
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
    @test MOI.get(model, ORTools.GscipParametersAttribute()) !== nothing
    @test MOI.get(model, MOI.RawOptimizerAttribute("gscip__num_solutions")) == 100
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
    @test MOI.get(model, ORTools.GurobiParametersAttribute()) !== nothing
    gurobi_param_vector_after = MOI.get(model, MOI.RawOptimizerAttribute("gurobi__parameters"))
    @test length(gurobi_param_vector_after) == 1
    @test gurobi_param_vector_after[1].name == "num_solutions"
    @test gurobi_param_vector_after[1].value == "100"
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
    @test MOI.get(model, ORTools.GlopParametersAttribute()) !== nothing
    @test MOI.get(
        model,
        MOI.RawOptimizerAttribute("glop__max_number_of_reoptimizations"),
    ) == Float64(100)
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
    @test MOI.get(model, ORTools.SatParametersAttribute()) !== nothing
    @test MOI.get(model, MOI.RawOptimizerAttribute("sat__name")) == "Google_CP_SAT"
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
    @test MOI.get(model, ORTools.GlpkParametersAttribute()) !== nothing
    @test MOI.get(
        model,
        MOI.RawOptimizerAttribute("glpk__compute_unbound_rays_if_possible"),
    ) == true
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
    @test MOI.get(model, ORTools.HighsOptionsAttribute()) !== nothing
    @test MOI.get(model, MOI.RawOptimizerAttribute("highs__int_options")) ==
          Dict("num_threads" => Int32(10))
    return
end

end

## Run all tests.
TestMOIWrapper.runtests()
