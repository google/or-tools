using ORToolsGenerated
const OperationsResearch = ORToolsGenerated.Proto.operations_research
const MathOpt = OperationsResearch.math_opt
const PDLP = OperationsResearch.pdlp
const SolverType = MathOpt.SolverTypeProto
const SolveResultProto = MathOpt.SolveResultProto
const TerminationReasonProto = MathOpt.TerminationReasonProto
const LimitProto = MathOpt.LimitProto
const FeasibilityStatusProto = MathOpt.FeasibilityStatusProto
const BasisStatusProto = MathOpt.BasisStatusProto
const GScipOutput = OperationsResearch.GScipOutput
const PdlpOutput = MathOpt.var"SolveResultProto.PdlpOutput"
const PdlpTerminationCriteria_SimpleOptimalityCriteria =
    PDLP.var"TerminationCriteria.SimpleOptimalityCriteria"
const PdlpTerminationCriteria_DetailedOptimalityCriteria =
    PDLP.var"TerminationCriteria.DetailedOptimalityCriteria"
const PdlpHybridGradientParams_RestartStrategy =
    PDLP.var"PrimalDualHybridGradientParams.RestartStrategy"
const PdlpHybridGradientParams_PresolveOptions =
    PDLP.var"PrimalDualHybridGradientParams.PresolveOptions"
const PdlpHybridGradientParams_LinesearchRule =
    PDLP.var"PrimalDualHybridGradientParams.LinesearchRule"
const PdlpOptimalityNorm = PDLP.OptimalityNorm
const PdlpSchedulerType = PDLP.SchedulerType
const PdlpAdaptiveLinesearchParams = PDLP.AdaptiveLinesearchParams
const PdlpMalitskyPockParams = PDLP.MalitskyPockParams
const PB = MathOpt.PB

"""
Given the nature of the fields, we are using an alias for the VariablesProto struct.
"""
const VariablesProto = MathOpt.VariablesProto
const NewVariablesProto =
    () -> VariablesProto(
        Vector{Int64}(),
        Vector{Float64}(),
        Vector{Float64}(),
        Vector{Bool}(),
        Vector{String}(),
    )

"""
Given the nature of the fields, we are using an alias for the VariablesProto struct.
"""
const SparseDoubleVectorProto = MathOpt.SparseDoubleVectorProto
const NewSparseDoubleVector =
    () -> SparseDoubleVectorProto(Vector{Int64}(), Vector{Float64}())

"""
Given the nature of the fields, we are using an alias for the VariablesProto struct.
"""
const SparseDoubleMatrixProto = MathOpt.SparseDoubleMatrixProto
const NewSparseDoubleMatrix =
    () -> SparseDoubleMatrixProto(Vector{Int64}(), Vector{Int64}(), Vector{Float64}())

"""
Given the nature of the fields, we are using an alias for the VariablesProto struct.
"""
const LinearConstraintsProto = MathOpt.LinearConstraintsProto
const NewLinearConstraints =
    () -> LinearConstraintsProto(
        Vector{Int64}(),
        Vector{Float64}(),
        Vector{Float64}(),
        Vector{String}(),
    )

const Duration = MathOpt.google.protobuf.Duration
const Emphasis = MathOpt.EmphasisProto
const LPAlgorithm = MathOpt.LPAlgorithmProto

## Gscip parameters types
const GScipParameters_MetaParamValue =
    OperationsResearch.var"GScipParameters.MetaParamValue"
const GScipParameters_Emphasis = OperationsResearch.var"GScipParameters.Emphasis"

## Gurobi parameters types.
const GurobiParametersProto_Parameter = MathOpt.var"GurobiParametersProto.Parameter"

## Glop parameters types.
const GlopParameters_PricingRule = OperationsResearch.glop.var"GlopParameters.PricingRule"
const GlopParameters_SolverBehavior =
    OperationsResearch.glop.var"GlopParameters.SolverBehavior"
const GlopParameters_CostScalingAlgorithm =
    OperationsResearch.glop.var"GlopParameters.CostScalingAlgorithm"
const GlopParameters_ScalingAlgorithm =
    OperationsResearch.glop.var"GlopParameters.ScalingAlgorithm"
const GlopParameters_InitialBasisHeuristic =
    OperationsResearch.glop.var"GlopParameters.InitialBasisHeuristic"

## Sat parameter types.
const SatParameters_VariableOrder = OperationsResearch.sat.var"SatParameters.VariableOrder"
const SatParameters_Polarity = OperationsResearch.sat.var"SatParameters.Polarity"
const SatParameters_ConflictMinimizationAlgorithm =
    OperationsResearch.sat.var"SatParameters.ConflictMinimizationAlgorithm"
const SatParameters_BinaryMinizationAlgorithm =
    OperationsResearch.sat.var"SatParameters.BinaryMinizationAlgorithm"
const SatParameters_ClauseProtection =
    OperationsResearch.sat.var"SatParameters.ClauseProtection"
const SatParameters_ClauseOrdering =
    OperationsResearch.sat.var"SatParameters.ClauseOrdering"
const SatParameters_RestartAlgorithm =
    OperationsResearch.sat.var"SatParameters.RestartAlgorithm"
const SatParameters_MaxSatAssumptionOrder =
    OperationsResearch.sat.var"SatParameters.MaxSatAssumptionOrder"
const SatParameters_MaxSatStratificationAlgorithm =
    OperationsResearch.sat.var"SatParameters.MaxSatStratificationAlgorithm"
const SatParameters_SearchBranching =
    OperationsResearch.sat.var"SatParameters.SearchBranching"
const SatParameters_SharedTreeSplitStrategy =
    OperationsResearch.sat.var"SatParameters.SharedTreeSplitStrategy"
const SatParameters_FPRoundingMethod =
    OperationsResearch.sat.var"SatParameters.FPRoundingMethod"

## Scalar Set constraints with bounds
const SCALAR_SET = Union{
    MOI.GreaterThan{<:Real},
    MOI.LessThan{<:Real},
    MOI.EqualTo{<:Real},
    MOI.Interval{<:Real},
}

## Bounds of scalar set Constraints
function bounds(s::MOI.GreaterThan{T}) where {T<:Real}
    return s.lower, typemax(T)
end

function bounds(s::MOI.LessThan{T}) where {T<:Real}
    return typemin(T), s.upper
end

function bounds(s::MOI.EqualTo{T}) where {T<:Real}
    return s.value, s.value
end

function bounds(s::MOI.Interval{T}) where {T<:Real}
    return s.lower, s.upper
end


"""
  Mutable wrapper struct for the objective proto struct.
"""
mutable struct Objective
    maximize::Bool
    offset::Float64
    linear_coefficients::Union{Nothing,SparseDoubleVectorProto}
    quadratic_coefficients::Union{Nothing,SparseDoubleMatrixProto}
    name::String
    priority::Int64

    function Objective(;
        maximize = false,
        offset = zero(Float64),
        linear_coefficients = NewSparseDoubleVector(),
        quadratic_coefficients = NewSparseDoubleMatrix(),
        name = "",
        priority = zero(Int64),
    )
        new(maximize, offset, linear_coefficients, quadratic_coefficients, name, priority)
    end
end

function to_proto_struct(objective::Objective)::MathOpt.ObjectiveProto
    return MathOpt.ObjectiveProto(
        objective.maximize,
        objective.offset,
        objective.linear_coefficients,
        objective.quadratic_coefficients,
        objective.name,
        objective.priority,
    )
end


"""
  Mutable wrapper struct for the QuadraticConstraintProto struct.
"""
mutable struct QuadraticConstraint
    linear_terms::Union{Nothing,SparseDoubleVectorProto}
    quadratic_terms::Union{Nothing,SparseDoubleMatrixProto}
    lower_bound::Float64
    upper_bound::Float64
    name::String

    function QuadraticConstraint(;
        linear_terms = NewSparseDoubleVector(),
        quadratic_terms = NewSparseDoubleMatrix(),
        lower_bound = typemin(Float64),
        upper_bound = typemax(Float64),
        name = "",
    )
        new(linear_terms, quadratic_terms, lower_bound, upper_bound, name)
    end
end

function to_proto_struct(
    quadratic_constraint::QuadraticConstraint,
)::MathOpt.QuadraticConstraintProto
    return MathOpt.QuadraticConstraintProto(
        quadratic_constraint.linear_terms,
        quadratic_constraint.quadratic_terms,
        quadratic_constraint.lower_bound,
        quadratic_constraint.upper_bound,
        quadratic_constraint.name,
    )
end


"""
  Mutable wrapper struct for the LinearExpressionProto struct.
"""
mutable struct LinearExpression
    id::Vector{Int64}
    coefficients::Vector{Float64}
    offset::Float64

    function LinearExpression(;
        id = Vector{Int64}(),
        coefficients = Vector{Float64}(),
        offset = zero(Float64),
    )
        new(id, coefficients, offset)
    end
end

function to_proto_struct(linear_expression::LinearExpression)::MathOpt.LinearExpressionProto
    return MathOpt.LinearExpressionProto(
        linear_expression.id,
        linear_expression.coefficients,
        linear_expression.offset,
    )
end


"""
  Mutable wrapper struct for the SecondOrderConeConstraintProto struct.
"""
mutable struct SecondOrderConeConstraint
    upper_bound::Union{Nothing,LinearExpression}
    arguments_to_norm::Vector{LinearExpression}
    name::String

    function SecondOrderConeConstraint(;
        upper_bound = LinearExpression(),
        arguments_to_norm = Vector{LinearExpression}(),
        name = "",
    )
        new(upper_bound, arguments_to_norm, name)
    end
end

function to_proto_struct(
    second_order_cone_constraints::SecondOrderConeConstraint,
)::MathOpt.SecondOrderConeConstraintProto
    return MathOpt.SecondOrderConeConstraintProto(
        second_order_cone_constraints.upper_bound,
        second_order_cone_constraints.arguments_to_norm,
        second_order_cone_constraints.name,
    )
end


"""
  Mutable wrapper struct for the SosConstraintProto struct.
"""
mutable struct SosConstraint
    expressions::Vector{LinearExpression}
    weights::Vector{Float64}
    name::String

    function SosConstraint(;
        expressions = Vector{LinearExpression}(),
        weights = Vector{Float64}(),
        name = "",
    )
        new(expressions, weights, name)
    end
end

function to_proto_struct(sos_constraint::SosConstraint)::MathOpt.SosConstraintProto
    return MathOpt.SosConstraintProto(
        sos_constraint.expressions,
        sos_constraint.weights,
        sos_constraint.name,
    )
end


"""
  Mutable wrapper struct for the IndicatorConstraintProto struct.
"""
mutable struct IndicatorConstraint
    indicator_id::Int64
    activate_on_zero::Bool
    expression::Union{Nothing,SparseDoubleVectorProto}
    lower_bound::Float64
    upper_bound::Float64
    name::String

    function IndicatorConstraint(;
        indicator_id = zero(Int64),
        activate_on_zero = false,
        expression = NewSparseDoubleVector(),
        lower_bound = typemin(Float64),
        upper_bound = typemax(Float64),
        name = "",
    )
        new(indicator_id, activate_on_zero, expression, lower_bound, upper_bound, name)
    end
end

function to_proto_struct(
    indicator_constraint::IndicatorConstraint,
)::MathOpt.IndicatorConstraint
    return MathOpt.IndicatorConstraintProto(
        indicator_constraint.indicator_id,
        indicator_constraint.activate_on_zero,
        indicator_constraint.expression,
        indicator_constraint.lower_bound,
        indicator_constraint.upper_bound,
        indicator_constraint.name,
    )
end

"""
  Mutable wrapper struct for the ModelProto struct.
"""
mutable struct Model
    name::String
    variables::Union{Nothing,VariablesProto}
    objective::Union{Nothing,Objective}
    auxiliary_objectives::Dict{Int64,Objective}
    linear_constraints::Union{Nothing,LinearConstraintsProto}
    linear_constraint_matrix::Union{Nothing,SparseDoubleMatrixProto}
    quadratic_constraints::Dict{Int64,QuadraticConstraint}
    second_order_cone_constraints::Dict{Int64,SecondOrderConeConstraint}
    sos1_constraints::Dict{Int64,SosConstraint}
    sos2_constraints::Dict{Int64,SosConstraint}
    indicator_constraints::Dict{Int64,IndicatorConstraint}

    function Model(;
        name = "",
        variables = NewVariablesProto(),
        objective = Objective(),
        auxiliary_objectives = Dict{Int64,Objective}(),
        linear_constraints = NewLinearConstraints(),
        linear_constraint_matrix = NewSparseDoubleMatrix(),
        quadratic_constraints = Dict{Int64,QuadraticConstraint}(),
        second_order_cone_constraints = Dict{Int64,SecondOrderConeConstraint}(),
        sos1_constraints = Dict{Int64,SosConstraint}(),
        sos2_constraints = Dict{Int64,SosConstraint}(),
        indicator_constraints = Dict{Int64,IndicatorConstraint}(),
    )
        new(
            name,
            variables,
            objective,
            auxiliary_objectives,
            linear_constraints,
            linear_constraint_matrix,
            quadratic_constraints,
            second_order_cone_constraints,
            sos1_constraints,
            sos2_constraints,
            indicator_constraints,
        )
    end
end

function to_proto_struct(model::Model)::MathOpt.ModelProto
    auxiliary_objectives = Dict{Int64,MathOpt.ObjectiveProto}()
    for (id, objective) in model.auxiliary_objectives
        auxiliary_objectives[id] = to_proto_struct(objective)
    end

    quadratic_constraints = Dict{Int64,MathOpt.QuadraticConstraintProto}()
    for (id, quadratic_constraint) in model.quadratic_constraints
        quadratic_constraints[id] = to_proto_struct(quadratic_constraint)
    end

    second_order_cone_constraints = Dict{Int64,MathOpt.SecondOrderConeConstraintProto}()
    for (id, second_order_cone_constraint) in model.second_order_cone_constraints
        second_order_cone_constraints[id] = to_proto_struct(second_order_cone_constraint)
    end

    sos1_constraints = Dict{Int64,MathOpt.SosConstraintProto}()
    for (id, sos1_constraint) in model.sos1_constraints
        sos1_constraints[id] = to_proto_struct(sos1_constraint)
    end

    sos2_constraints = Dict{Int64,MathOpt.SosConstraintProto}()
    for (id, sos2_constraint) in model.sos2_constraints
        sos2_constraints[id] = to_proto_struct(sos2_constraint)
    end

    indicator_constraints = Dict{Int64,MathOpt.IndicatorConstraintProto}()
    for (id, indicator_constraint) in model.indicator_constraints
        indicator_constraints[id] = to_proto_struct(indicator_constraint)
    end

    return MathOpt.ModelProto(
        model.name,
        model.variables,
        to_proto_struct(model.objective),
        auxiliary_objectives,
        model.linear_constraints,
        model.linear_constraint_matrix,
        quadratic_constraints,
        second_order_cone_constraints,
        sos1_constraints,
        sos2_constraints,
        indicator_constraints,
    )
end

# The size of the encoded model
encoded_model_size(model::Model) = PB._encoded_size(to_proto_struct(model))

"""
  Mutable wrapper struct for the GscipParameters struct.
"""
mutable struct GScipParameters
    emphasis::GScipParameters_Emphasis.T
    heuristics::GScipParameters_MetaParamValue.T
    presolve::GScipParameters_MetaParamValue.T
    separating::GScipParameters_MetaParamValue.T
    bool_params::Dict{String,Bool}
    int_params::Dict{String,Int32}
    long_params::Dict{String,Int64}
    real_params::Dict{String,Float64}
    char_params::Dict{String,String}
    string_params::Dict{String,String}
    silence_output::Bool
    print_detailed_solving_stats::Bool
    print_scip_model::Bool
    search_logs_filename::String
    detailed_solving_stats_filename::String
    scip_model_filename::String
    num_solutions::Int32
    objective_limit::Float64

    function GScipParameters(;
        emphasis = GScipParameters_Emphasis.DEFAULT_EMPHASIS,
        heuristics = GScipParameters_MetaParamValue.DEFAULT_META_PARAM_VALUE,
        presolve = GScipParameters_MetaParamValue.DEFAULT_META_PARAM_VALUE,
        separating = GScipParameters_MetaParamValue.DEFAULT_META_PARAM_VALUE,
        bool_params = Dict{String,Bool}(),
        int_params = Dict{String,Int32}(),
        long_params = Dict{String,Int64}(),
        real_params = Dict{String,Float64}(),
        char_params = Dict{String,String}(),
        string_params = Dict{String,String}(),
        silence_output = false,
        print_detailed_solving_stats = false,
        print_scip_model = false,
        search_logs_filename = "",
        detailed_solving_stats_filename = "",
        scip_model_filename = "",
        num_solutions = zero(Int32),
        objective_limit = zero(Float64),
    )
        new(
            emphasis,
            heuristics,
            presolve,
            separating,
            bool_params,
            int_params,
            long_params,
            real_params,
            char_params,
            string_params,
            silence_output,
            print_detailed_solving_stats,
            print_scip_model,
            search_logs_filename,
            detailed_solving_stats_filename,
            scip_model_filename,
            num_solutions,
            objective_limit,
        )
    end
end

function to_proto_struct(
    gscip_parameters::GScipParameters,
)::OperationsResearch.GScipParameters
    return OperationsResearch.GScipParameters(
        gscip_parameters.emphasis,
        gscip_parameters.heuristics,
        gscip_parameters.presolve,
        gscip_parameters.separating,
        gscip_parameters.bool_params,
        gscip_parameters.int_params,
        gscip_parameters.long_params,
        gscip_parameters.real_params,
        gscip_parameters.char_params,
        gscip_parameters.string_params,
        gscip_parameters.silence_output,
        gscip_parameters.print_detailed_solving_stats,
        gscip_parameters.print_scip_model,
        gscip_parameters.search_logs_filename,
        gscip_parameters.detailed_solving_stats_filename,
        gscip_parameters.scip_model_filename,
        gscip_parameters.num_solutions,
        gscip_parameters.objective_limit,
    )
end

"""
  Mutable wrapper struct for the GurobiParametersProto.Parameter struct.
"""
mutable struct GurobiParameters_Parameter
    name::String
    value::String
end

function to_proto_struct(
    gurobi_parameters_parameter::GurobiParameters_Parameter,
)::GurobiParametersProto_Parameter
    return GurobiParametersProto_Parameter(
        gurobi_parameters_parameter.name,
        gurobi_parameters_parameter.value,
    )
end

"""
  Mutable wrapper struct for the GurobiParameters struct.
"""
mutable struct GurobiParameters
    parameters::Vector{GurobiParameters_Parameter}

    function GurobiParameters(; parameters = Vector{GurobiParameters_Parameter}())
        new(parameters)
    end
end

function to_proto_struct(gurobi_parameters::GurobiParameters)::MathOpt.GurobiParametersProto
    parameters =
        Vector{GurobiParametersProto_Parameter}(length(gurobi_parameters.parameters))
    for (index, parameter) in enumerate(gurobi_parameters.parameters)
        parameters[index] = to_proto_struct(parameter)
    end

    return MathOpt.GurobiParametersProto(gurobi_parameters.parameters)
end

"""
  Mutable wrapper struct for the GlopParameters struct.
"""
mutable struct GlopParameters
    scaling_method::GlopParameters_ScalingAlgorithm.T
    feasibility_rule::GlopParameters_PricingRule.T
    optimization_rule::GlopParameters_PricingRule.T
    refactorization_threshold::Float64
    recompute_reduced_costs_threshold::Float64
    recompute_edges_norm_threshold::Float64
    primal_feasibility_tolerance::Float64
    dual_feasibility_tolerance::Float64
    ratio_test_zero_threshold::Float64
    harris_tolerance_ratio::Float64
    small_pivot_threshold::Float64
    minimum_acceptable_pivot::Float64
    drop_tolerance::Float64
    use_scaling::Bool
    cost_scaling::GlopParameters_CostScalingAlgorithm.T
    initial_basis::GlopParameters_InitialBasisHeuristic.T
    use_transposed_matrix::Bool
    basis_refactorization_period::Int32
    dynamically_adjust_refactorization_period::Bool
    solve_dual_problem::GlopParameters_SolverBehavior.T
    dualizer_threshold::Float64
    solution_feasibility_tolerance::Float64
    provide_strong_optimal_guarantee::Bool
    change_status_to_imprecise::Bool
    max_number_of_reoptimizations::Float64
    lu_factorization_pivot_threshold::Float64
    max_time_in_seconds::Float64
    max_deterministic_time::Float64
    max_number_of_iterations::Int64
    markowitz_zlatev_parameter::Int32
    markowitz_singularity_threshold::Float64
    use_dual_simplex::Bool
    allow_simplex_algorithm_change::Bool
    devex_weights_reset_period::Int32
    use_preprocessing::Bool
    use_middle_product_form_update::Bool
    initialize_devex_with_column_norms::Bool
    exploit_singleton_column_in_initial_basis::Bool
    dual_small_pivot_threshold::Float64
    preprocessor_zero_tolerance::Float64
    objective_lower_limit::Float64
    objective_upper_limit::Float64
    degenerate_ministep_factor::Float64
    random_seed::Int32
    num_omp_threads::Int32
    perturb_costs_in_dual_simplex::Bool
    use_dedicated_dual_feasibility_algorithm::Bool
    relative_cost_perturbation::Float64
    relative_max_cost_perturbation::Float64
    initial_condition_number_threshold::Float64
    log_search_progress::Bool
    log_to_stdout::Bool
    crossover_bound_snapping_distance::Float64
    push_to_vertex::Bool
    use_implied_free_preprocessor::Bool
    max_valid_magnitude::Float64
    dual_price_prioritize_norm::Bool

    function GlopParameters(;
        scaling_method = GlopParameters_ScalingAlgorithm.EQUILIBRATION,
        feasibility_rule = GlopParameters_PricingRule.STEEPEST_EDGE,
        optimization_rule = GlopParameters_PricingRule.STEEPEST_EDGE,
        refactorization_threshold = Float64(1e-9),
        recompute_reduced_costs_threshold = Float64(1e-8),
        recompute_edges_norm_threshold = Float64(100.0),
        primal_feasibility_tolerance = Float64(1e-8),
        dual_feasibility_tolerance = Float64(1e-8),
        ratio_test_zero_threshold = Float64(1e-9),
        harris_tolerance_ratio = Float64(0.5),
        small_pivot_threshold = Float64(1e-6),
        minimum_acceptable_pivot = Float64(1e-6),
        drop_tolerance = Float64(1e-14),
        use_scaling = true,
        cost_scaling = GlopParameters_CostScalingAlgorithm.CONTAIN_ONE_COST_SCALING,
        initial_basis = GlopParameters_InitialBasisHeuristic.TRIANGULAR,
        use_transposed_matrix = true,
        basis_refactorization_period = Int32(64),
        dynamically_adjust_refactorization_period = true,
        solve_dual_problem = GlopParameters_SolverBehavior.LET_SOLVER_DECIDE,
        dualizer_threshold = Float64(1.5),
        solution_feasibility_tolerance = Float64(1e-6),
        provide_strong_optimal_guarantee = true,
        change_status_to_imprecise = true,
        max_number_of_reoptimizations = Float64(40),
        lu_factorization_pivot_threshold = Float64(0.01),
        max_time_in_seconds = Float64(Inf),
        max_deterministic_time = Float64(Inf),
        max_number_of_iterations = Int64(-1),
        markowitz_zlatev_parameter = Int32(3),
        markowitz_singularity_threshold = Float64(1e-15),
        use_dual_simplex = false,
        allow_simplex_algorithm_change = false,
        devex_weights_reset_period = Int32(150),
        use_preprocessing = true,
        use_middle_product_form_update = true,
        initialize_devex_with_column_norms = true,
        exploit_singleton_column_in_initial_basis = true,
        dual_small_pivot_threshold = Float64(1e-4),
        preprocessor_zero_tolerance = Float64(1e-9),
        objective_lower_limit = Float64(-Inf),
        objective_upper_limit = Float64(Inf),
        degenerate_ministep_factor = Float64(0.01),
        random_seed = Int32(1),
        num_omp_threads = Int32(1),
        perturb_costs_in_dual_simplex = false,
        use_dedicated_dual_feasibility_algorithm = true,
        relative_cost_perturbation = Float64(1e-5),
        relative_max_cost_perturbation = Float64(1e-7),
        initial_condition_number_threshold = Float64(1e50),
        log_search_progress = false,
        log_to_stdout = true,
        crossover_bound_snapping_distance = Float64(Inf),
        push_to_vertex = true,
        use_implied_free_preprocessor = true,
        max_valid_magnitude = Float64(1e30),
        dual_price_prioritize_norm = false,
    )
        new(
            scaling_method,
            feasibility_rule,
            optimization_rule,
            refactorization_threshold,
            recompute_reduced_costs_threshold,
            recompute_edges_norm_threshold,
            primal_feasibility_tolerance,
            dual_feasibility_tolerance,
            ratio_test_zero_threshold,
            harris_tolerance_ratio,
            small_pivot_threshold,
            minimum_acceptable_pivot,
            drop_tolerance,
            use_scaling,
            cost_scaling,
            initial_basis,
            use_transposed_matrix,
            basis_refactorization_period,
            dynamically_adjust_refactorization_period,
            solve_dual_problem,
            dualizer_threshold,
            solution_feasibility_tolerance,
            provide_strong_optimal_guarantee,
            change_status_to_imprecise,
            max_number_of_reoptimizations,
            lu_factorization_pivot_threshold,
            max_time_in_seconds,
            max_deterministic_time,
            max_number_of_iterations,
            markowitz_zlatev_parameter,
            markowitz_singularity_threshold,
            use_dual_simplex,
            allow_simplex_algorithm_change,
            devex_weights_reset_period,
            use_preprocessing,
            use_middle_product_form_update,
            initialize_devex_with_column_norms,
            exploit_singleton_column_in_initial_basis,
            dual_small_pivot_threshold,
            preprocessor_zero_tolerance,
            objective_lower_limit,
            objective_upper_limit,
            degenerate_ministep_factor,
            random_seed,
            num_omp_threads,
            perturb_costs_in_dual_simplex,
            use_dedicated_dual_feasibility_algorithm,
            relative_cost_perturbation,
            relative_max_cost_perturbation,
            initial_condition_number_threshold,
            log_search_progress,
            log_to_stdout,
            crossover_bound_snapping_distance,
            push_to_vertex,
            use_implied_free_preprocessor,
            max_valid_magnitude,
            dual_price_prioritize_norm,
        )
    end
end

function to_proto_struct(
    glop_parameters::GlopParameters,
)::OperationsResearch.glop.GlopParameters
    return OperationsResearch.glop.GlopParameters(
        glop_parameters.scaling_method,
        glop_parameters.feasibility_rule,
        glop_parameters.optimization_rule,
        glop_parameters.refactorization_threshold,
        glop_parameters.recompute_reduced_costs_threshold,
        glop_parameters.recompute_edges_norm_threshold,
        glop_parameters.primal_feasibility_tolerance,
        glop_parameters.dual_feasibility_tolerance,
        glop_parameters.ratio_test_zero_threshold,
        glop_parameters.harris_tolerance_ratio,
        glop_parameters.small_pivot_threshold,
        glop_parameters.minimum_acceptable_pivot,
        glop_parameters.drop_tolerance,
        glop_parameters.use_scaling,
        glop_parameters.cost_scaling,
        glop_parameters.initial_basis,
        glop_parameters.use_transposed_matrix,
        glop_parameters.basis_refactorization_period,
        glop_parameters.dynamically_adjust_refactorization_period,
        glop_parameters.solve_dual_problem,
        glop_parameters.dualizer_threshold,
        glop_parameters.solution_feasibility_tolerance,
        glop_parameters.provide_strong_optimal_guarantee,
        glop_parameters.change_status_to_imprecise,
        glop_parameters.max_number_of_reoptimizations,
        glop_parameters.lu_factorization_pivot_threshold,
        glop_parameters.max_time_in_seconds,
        glop_parameters.max_deterministic_time,
        glop_parameters.max_number_of_iterations,
        glop_parameters.markowitz_zlatev_parameter,
        glop_parameters.markowitz_singularity_threshold,
        glop_parameters.use_dual_simplex,
        glop_parameters.allow_simplex_algorithm_change,
        glop_parameters.devex_weights_reset_period,
        glop_parameters.use_preprocessing,
        glop_parameters.use_middle_product_form_update,
        glop_parameters.initialize_devex_with_column_norms,
        glop_parameters.exploit_singleton_column_in_initial_basis,
        glop_parameters.dual_small_pivot_threshold,
        glop_parameters.preprocessor_zero_tolerance,
        glop_parameters.objective_lower_limit,
        glop_parameters.objective_upper_limit,
        glop_parameters.degenerate_ministep_factor,
        glop_parameters.random_seed,
        glop_parameters.num_omp_threads,
        glop_parameters.perturb_costs_in_dual_simplex,
        glop_parameters.use_dedicated_dual_feasibility_algorithm,
        glop_parameters.relative_cost_perturbation,
        glop_parameters.relative_max_cost_perturbation,
        glop_parameters.initial_condition_number_threshold,
        glop_parameters.log_search_progress,
        glop_parameters.log_to_stdout,
        glop_parameters.crossover_bound_snapping_distance,
        glop_parameters.push_to_vertex,
        glop_parameters.use_implied_free_preprocessor,
        glop_parameters.max_valid_magnitude,
        glop_parameters.dual_price_prioritize_norm,
    )
end

"""
  Mutable wrapper struct for the SatParameters struct.
"""
mutable struct SatParameters
    name::String
    preferred_variable_order::SatParameters_VariableOrder.T
    initial_polarity::SatParameters_Polarity.T
    use_phase_saving::Bool
    polarity_rephase_increment::Int32
    random_polarity_ratio::Float64
    random_branches_ratio::Float64
    use_erwa_heuristic::Bool
    initial_variables_activity::Float64
    also_bump_variables_in_conflict_reasons::Bool
    minimization_algorithm::SatParameters_ConflictMinimizationAlgorithm.T
    binary_minimization_algorithm::SatParameters_BinaryMinizationAlgorithm.T
    subsumption_during_conflict_analysis::Bool
    clause_cleanup_period::Int32
    clause_cleanup_target::Int32
    clause_cleanup_ratio::Float64
    clause_cleanup_protection::SatParameters_ClauseProtection.T
    clause_cleanup_lbd_bound::Int32
    clause_cleanup_ordering::SatParameters_ClauseOrdering.T
    pb_cleanup_increment::Int32
    pb_cleanup_ratio::Float64
    minimize_with_propagation_restart_period::Int32
    minimize_with_propagation_num_decisions::Int32
    variable_activity_decay::Float64
    max_variable_activity_value::Float64
    glucose_max_decay::Float64
    glucose_decay_increment::Float64
    glucose_decay_increment_period::Int32
    clause_activity_decay::Float64
    max_clause_activity_value::Float64
    restart_algorithms::Vector{SatParameters_RestartAlgorithm.T}
    default_restart_algorithms::String
    restart_period::Int32
    restart_running_window_size::Int32
    restart_dl_average_ratio::Float64
    restart_lbd_average_ratio::Float64
    use_blocking_restart::Bool
    blocking_restart_window_size::Int32
    blocking_restart_multiplier::Float64
    num_conflicts_before_strategy_changes::Int32
    strategy_change_increase_ratio::Float64
    max_time_in_seconds::Float64
    max_deterministic_time::Float64
    max_number_of_conflicts::Int64
    max_memory_in_mb::Int64
    absolute_gap_limit::Float64
    relative_gap_limit::Float64
    random_seed::Int32
    permute_variable_randomly::Bool
    permute_presolve_constraint_order::Bool
    use_absl_random::Bool
    log_search_progress::Bool
    log_frequency_in_seconds::Float64
    model_reduction_log_frequency_in_seconds::Float64
    log_subsolver_statistics::Bool
    log_prefix::String
    log_to_stdout::Bool
    log_to_response::Bool
    use_pb_resolution::Bool
    minimize_reduction_during_pb_resolution::Bool
    count_assumption_levels_in_lbd::Bool
    presolve_bve_threshold::Int32
    presolve_bve_clause_weight::Int32
    probing_deterministic_time_limit::Float64
    presolve_probing_deterministic_time_limit::Float64
    presolve_blocked_clause::Bool
    presolve_use_bva::Bool
    presolve_bva_threshold::Int32
    max_presolve_iterations::Int32
    cp_model_presolve::Bool
    cp_model_probing_level::Int32
    cp_model_use_sat_presolve::Bool
    use_sat_inprocessing::Bool
    detect_table_with_cost::Bool
    table_compression_level::Int32
    expand_alldiff_constraints::Bool
    expand_reservoir_constraints::Bool
    disable_constraint_expansion::Bool
    encode_complex_linear_constraint_with_integer::Bool
    merge_no_overlap_work_limit::Float64
    merge_at_most_one_work_limit::Float64
    presolve_substitution_level::Int32
    presolve_extract_integer_enforcement::Bool
    presolve_inclusion_work_limit::Int64
    ignore_names::Bool
    infer_all_diffs::Bool
    find_big_linear_overlap::Bool
    num_workers::Int32
    num_search_workers::Int32
    min_num_lns_workers::Int32
    subsolvers::Vector{String}
    extra_subsolvers::Vector{String}
    ignore_subsolvers::Vector{String}
    subsolver_params::Vector{<:SatParameters}
    interleave_search::Bool
    interleave_batch_size::Int32
    share_objective_bounds::Bool
    share_level_zero_bounds::Bool
    share_binary_clauses::Bool
    debug_postsolve_with_full_solver::Bool
    debug_max_num_presolve_operations::Int32
    debug_crash_on_bad_hint::Bool
    use_optimization_hints::Bool
    minimize_core::Bool
    find_multiple_cores::Bool
    cover_optimization::Bool
    max_sat_assumption_order::SatParameters_MaxSatAssumptionOrder.T
    max_sat_reverse_assumption_order::Bool
    max_sat_stratification::SatParameters_MaxSatStratificationAlgorithm.T
    propagation_loop_detection_factor::Float64
    use_precedences_in_disjunctive_constraint::Bool
    max_size_to_create_precedence_literals_in_disjunctive::Int32
    use_strong_propagation_in_disjunctive::Bool
    use_overload_checker_in_cumulative::Bool
    use_timetable_edge_finding_in_cumulative::Bool
    use_hard_precedences_in_cumulative::Bool
    exploit_all_precedences::Bool
    use_disjunctive_constraint_in_cumulative::Bool
    use_timetabling_in_no_overlap_2d::Bool
    use_energetic_reasoning_in_no_overlap_2d::Bool
    use_pairwise_reasoning_in_no_overlap_2d::Bool
    use_dual_scheduling_heuristics::Bool
    linearization_level::Int32
    boolean_encoding_level::Int32
    max_domain_size_when_encoding_eq_neq_constraints::Int32
    max_num_cuts::Int32
    cut_level::Int32
    only_add_cuts_at_level_zero::Bool
    add_objective_cut::Bool
    add_cg_cuts::Bool
    add_mir_cuts::Bool
    add_zero_half_cuts::Bool
    add_clique_cuts::Bool
    max_all_diff_cut_size::Int32
    add_lin_max_cuts::Bool
    max_integer_rounding_scaling::Int32
    add_lp_constraints_lazily::Bool
    root_lp_iterations::Int32
    min_orthogonality_for_lp_constraints::Float64
    max_cut_rounds_at_level_zero::Int32
    max_consecutive_inactive_count::Int32
    cut_max_active_count_value::Float64
    cut_active_count_decay::Float64
    cut_cleanup_target::Int32
    new_constraints_batch_size::Int32
    search_branching::SatParameters_SearchBranching.T
    hint_conflict_limit::Int32
    repair_hint::Bool
    fix_variables_to_their_hinted_value::Bool
    exploit_integer_lp_solution::Bool
    exploit_all_lp_solution::Bool
    exploit_best_solution::Bool
    exploit_relaxation_solution::Bool
    exploit_objective::Bool
    probing_period_at_root::Int64
    use_probing_search::Bool
    use_shaving_in_probing_search::Bool
    shaving_search_deterministic_time::Float64
    use_objective_lb_search::Bool
    use_objective_shaving_search::Bool
    pseudo_cost_reliability_threshold::Int64
    optimize_with_core::Bool
    optimize_with_lb_tree_search::Bool
    binary_search_num_conflicts::Int32
    optimize_with_max_hs::Bool
    test_feasibility_jump::Bool
    feasibility_jump_max_num_values_scanned::Int64
    feasibility_jump_protect_linear_feasibility::Bool
    feasibility_jump_decay::Float64
    feasibility_jump_var_randomization_probability::Float64
    feasibility_jump_var_perburbation_range_ratio::Float64
    feasibility_jump_enable_restarts::Bool
    num_violation_ls::Int32
    violation_ls_perturbation_period::Int32
    shared_tree_num_workers::Int32
    use_shared_tree_search::Bool
    shared_tree_worker_objective_split_probability::Float64
    shared_tree_max_nodes_per_worker::Int32
    shared_tree_split_strategy::SatParameters_SharedTreeSplitStrategy.T
    enumerate_all_solutions::Bool
    keep_all_feasible_solutions_in_presolve::Bool
    fill_tightened_domains_in_response::Bool
    fill_additional_solutions_in_response::Bool
    instantiate_all_variables::Bool
    auto_detect_greater_than_at_least_one_of::Bool
    stop_after_first_solution::Bool
    stop_after_presolve::Bool
    stop_after_root_propagation::Bool
    use_lns_only::Bool
    solution_pool_size::Int32
    use_rins_lns::Bool
    use_feasibility_pump::Bool
    use_lb_relax_lns::Bool
    fp_rounding::SatParameters_FPRoundingMethod.T
    diversify_lns_params::Bool
    randomize_search::Bool
    search_randomization_tolerance::Int64
    use_optional_variables::Bool
    use_exact_lp_reason::Bool
    use_branching_in_lp::Bool
    use_combined_no_overlap::Bool
    catch_sigint_signal::Bool
    use_implied_bounds::Bool
    polish_lp_solution::Bool
    convert_intervals::Bool
    symmetry_level::Int32
    new_linear_propagation::Bool
    linear_split_size::Int32
    mip_max_bound::Float64
    mip_var_scaling::Float64
    mip_scale_large_domain::Bool
    mip_automatically_scale_variables::Bool
    only_solve_ip::Bool
    mip_wanted_precision::Float64
    mip_max_activity_exponent::Int32
    mip_check_precision::Float64
    mip_compute_true_objective_bound::Bool
    mip_max_valid_magnitude::Float64
    mip_drop_tolerance::Float64

    function SatParameters(;
        name = "",
        preferred_variable_order = SatParameters_VariableOrder.IN_ORDER,
        initial_polarity = SatParameters_Polarity.POLARITY_FALSE,
        use_phase_saving = true,
        polarity_rephase_increment = Int32(1000),
        random_polarity_ratio = Float64(0.0),
        random_branches_ratio = Float64(0.0),
        use_erwa_heuristic = false,
        initial_variables_activity = Float64(0.0),
        also_bump_variables_in_conflict_reasons = false,
        minimization_algorithm = SatParameters_ConflictMinimizationAlgorithm.RECURSIVE,
        binary_minimization_algorithm = SatParameters_BinaryMinizationAlgorithm.BINARY_MINIMIZATION_FIRST,
        subsumption_during_conflict_analysis = true,
        clause_cleanup_period = Int32(10000),
        clause_cleanup_target = Int32(0),
        clause_cleanup_ratio = Float64(0.5),
        clause_cleanup_protection = SatParameters_ClauseProtection.PROTECTION_NONE,
        clause_cleanup_lbd_bound = Int32(5),
        clause_cleanup_ordering = SatParameters_ClauseOrdering.CLAUSE_ACTIVITY,
        pb_cleanup_increment = Int32(200),
        pb_cleanup_ratio = Float64(0.5),
        minimize_with_propagation_restart_period = Int32(10),
        minimize_with_propagation_num_decisions = Int32(1000),
        variable_activity_decay = Float64(0.8),
        max_variable_activity_value = Float64(1e100),
        glucose_max_decay = Float64(0.95),
        glucose_decay_increment = Float64(0.01),
        glucose_decay_increment_period = Int32(5000),
        clause_activity_decay = Float64(0.999),
        max_clause_activity_value = Float64(1e20),
        restart_algorithms = Vector{SatParameters_RestartAlgorithm.T}(),
        default_restart_algorithms = "LUBY_RESTART,LBD_MOVING_AVERAGE_RESTART,DL_MOVING_AVERAGE_RESTART",
        restart_period = Int32(50),
        restart_running_window_size = Int32(50),
        restart_dl_average_ratio = Float64(1.0),
        restart_lbd_average_ratio = Float64(1.0),
        use_blocking_restart = false,
        blocking_restart_window_size = Int32(5000),
        blocking_restart_multiplier = Float64(1.4),
        num_conflicts_before_strategy_changes = Int32(0),
        strategy_change_increase_ratio = Float64(0.0),
        max_time_in_seconds = Float64(Inf),
        max_deterministic_time = Float64(Inf),
        max_number_of_conflicts = Int64(9223372036854775807),
        max_memory_in_mb = Int64(10000),
        absolute_gap_limit = Float64(1e-4),
        relative_gap_limit = Float64(0.0),
        random_seed = Int32(1),
        permute_variable_randomly = false,
        permute_presolve_constraint_order = false,
        use_absl_random = false,
        log_search_progress = false,
        log_frequency_in_seconds = Float64(-1),
        model_reduction_log_frequency_in_seconds = Float64(5),
        log_subsolver_statistics = false,
        log_prefix = "",
        log_to_stdout = true,
        log_to_response = false,
        use_pb_resolution = false,
        minimize_reduction_during_pb_resolution = false,
        count_assumption_levels_in_lbd = true,
        presolve_bve_threshold = Int32(500),
        presolve_bve_clause_weight = Int32(3),
        probing_deterministic_time_limit = Float64(1),
        presolve_probing_deterministic_time_limit = Float64(30.0),
        presolve_blocked_clause = true,
        presolve_use_bva = true,
        presolve_bva_threshold = Int32(1),
        max_presolve_iterations = Int32(3),
        cp_model_presolve = true,
        cp_model_probing_level = Int32(2),
        cp_model_use_sat_presolve = true,
        use_sat_inprocessing = false,
        detect_table_with_cost = false,
        table_compression_level = Int32(2),
        expand_alldiff_constraints = false,
        expand_reservoir_constraints = true,
        disable_constraint_expansion = false,
        encode_complex_linear_constraint_with_integer = false,
        merge_no_overlap_work_limit = Float64(1e12),
        merge_at_most_one_work_limit = Float64(1e8),
        presolve_substitution_level = Int32(1),
        presolve_extract_integer_enforcement = false,
        presolve_inclusion_work_limit = Int64(100000000),
        ignore_names = true,
        infer_all_diffs = true,
        find_big_linear_overlap = true,
        num_workers = Int32(0),
        num_search_workers = Int32(0),
        min_num_lns_workers = Int32(2),
        subsolvers = Vector{String}(),
        extra_subsolvers = Vector{String}(),
        ignore_subsolvers = Vector{String}(),
        subsolver_params = Vector{SatParameters}(),
        interleave_search = false,
        interleave_batch_size = Int32(0),
        share_objective_bounds = true,
        share_level_zero_bounds = true,
        share_binary_clauses = true,
        debug_postsolve_with_full_solver = false,
        debug_max_num_presolve_operations = Int32(0),
        debug_crash_on_bad_hint = false,
        use_optimization_hints = true,
        minimize_core = true,
        find_multiple_cores = true,
        cover_optimization = true,
        max_sat_assumption_order = SatParameters_MaxSatAssumptionOrder.DEFAULT_ASSUMPTION_ORDER,
        max_sat_reverse_assumption_order = false,
        max_sat_stratification = SatParameters_MaxSatStratificationAlgorithm.STRATIFICATION_DESCENT,
        propagation_loop_detection_factor = Float64(10.0),
        use_precedences_in_disjunctive_constraint = true,
        max_size_to_create_precedence_literals_in_disjunctive = Int32(60),
        use_strong_propagation_in_disjunctive = false,
        use_overload_checker_in_cumulative = false,
        use_timetable_edge_finding_in_cumulative = false,
        use_hard_precedences_in_cumulative = false,
        exploit_all_precedences = false,
        use_disjunctive_constraint_in_cumulative = true,
        use_timetabling_in_no_overlap_2d = false,
        use_energetic_reasoning_in_no_overlap_2d = false,
        use_pairwise_reasoning_in_no_overlap_2d = false,
        use_dual_scheduling_heuristics = true,
        linearization_level = Int32(1),
        boolean_encoding_level = Int32(1),
        max_domain_size_when_encoding_eq_neq_constraints = Int32(16),
        max_num_cuts = Int32(10000),
        cut_level = Int32(1),
        only_add_cuts_at_level_zero = false,
        add_objective_cut = false,
        add_cg_cuts = true,
        add_mir_cuts = true,
        add_zero_half_cuts = true,
        add_clique_cuts = true,
        max_all_diff_cut_size = Int32(64),
        add_lin_max_cuts = true,
        max_integer_rounding_scaling = Int32(600),
        add_lp_constraints_lazily = true,
        root_lp_iterations = Int32(2000),
        min_orthogonality_for_lp_constraints = Float64(0.05),
        max_cut_rounds_at_level_zero = Int32(1),
        max_consecutive_inactive_count = Int32(100),
        cut_max_active_count_value = Float64(1e10),
        cut_active_count_decay = Float64(0.8),
        cut_cleanup_target = Int32(1000),
        new_constraints_batch_size = Int32(50),
        search_branching = SatParameters_SearchBranching.AUTOMATIC_SEARCH,
        hint_conflict_limit = Int32(10),
        repair_hint = false,
        fix_variables_to_their_hinted_value = false,
        exploit_integer_lp_solution = true,
        exploit_all_lp_solution = true,
        exploit_best_solution = false,
        exploit_relaxation_solution = false,
        exploit_objective = true,
        probing_period_at_root = Int64(0),
        use_probing_search = false,
        use_shaving_in_probing_search = true,
        shaving_search_deterministic_time = Float64(0.001),
        use_objective_lb_search = false,
        use_objective_shaving_search = false,
        pseudo_cost_reliability_threshold = Int64(100),
        optimize_with_core = false,
        optimize_with_lb_tree_search = false,
        binary_search_num_conflicts = Int32(-1),
        optimize_with_max_hs = false,
        test_feasibility_jump = false,
        feasibility_jump_max_num_values_scanned = Int64(4096),
        feasibility_jump_protect_linear_feasibility = true,
        feasibility_jump_decay = Float64(1.0),
        feasibility_jump_var_randomization_probability = Float64(0.0),
        feasibility_jump_var_perburbation_range_ratio = Float64(0.2),
        feasibility_jump_enable_restarts = true,
        num_violation_ls = Int32(0),
        violation_ls_perturbation_period = Int32(100),
        shared_tree_num_workers = Int32(0),
        use_shared_tree_search = false,
        shared_tree_worker_objective_split_probability = Float64(0.5),
        shared_tree_max_nodes_per_worker = Int32(128),
        shared_tree_split_strategy = SatParameters_SharedTreeSplitStrategy.SPLIT_STRATEGY_AUTO,
        enumerate_all_solutions = false,
        keep_all_feasible_solutions_in_presolve = false,
        fill_tightened_domains_in_response = false,
        fill_additional_solutions_in_response = false,
        instantiate_all_variables = true,
        auto_detect_greater_than_at_least_one_of = true,
        stop_after_first_solution = false,
        stop_after_presolve = false,
        stop_after_root_propagation = false,
        use_lns_only = false,
        solution_pool_size = Int32(3),
        use_rins_lns = true,
        use_feasibility_pump = true,
        use_lb_relax_lns = false,
        fp_rounding = SatParameters_FPRoundingMethod.PROPAGATION_ASSISTED,
        diversify_lns_params = false,
        randomize_search = false,
        search_randomization_tolerance = Int64(0),
        use_optional_variables = false,
        use_exact_lp_reason = true,
        use_branching_in_lp = false,
        use_combined_no_overlap = false,
        catch_sigint_signal = true,
        use_implied_bounds = true,
        polish_lp_solution = false,
        convert_intervals = true,
        symmetry_level = Int32(2),
        new_linear_propagation = false,
        linear_split_size = Int32(100),
        mip_max_bound = Float64(1e7),
        mip_var_scaling = Float64(1.0),
        mip_scale_large_domain = false,
        mip_automatically_scale_variables = true,
        only_solve_ip = false,
        mip_wanted_precision = Float64(1e-6),
        mip_max_activity_exponent = Int32(53),
        mip_check_precision = Float64(1e-4),
        mip_compute_true_objective_bound = true,
        mip_max_valid_magnitude = Float64(1e30),
        mip_drop_tolerance = Float64(1e-16),
    )
        new(
            name,
            preferred_variable_order,
            initial_polarity,
            use_phase_saving,
            polarity_rephase_increment,
            random_polarity_ratio,
            random_branches_ratio,
            use_erwa_heuristic,
            initial_variables_activity,
            also_bump_variables_in_conflict_reasons,
            minimization_algorithm,
            binary_minimization_algorithm,
            subsumption_during_conflict_analysis,
            clause_cleanup_period,
            clause_cleanup_target,
            clause_cleanup_ratio,
            clause_cleanup_protection,
            clause_cleanup_lbd_bound,
            clause_cleanup_ordering,
            pb_cleanup_increment,
            pb_cleanup_ratio,
            minimize_with_propagation_restart_period,
            minimize_with_propagation_num_decisions,
            variable_activity_decay,
            max_variable_activity_value,
            glucose_max_decay,
            glucose_decay_increment,
            glucose_decay_increment_period,
            clause_activity_decay,
            max_clause_activity_value,
            restart_algorithms,
            default_restart_algorithms,
            restart_period,
            restart_running_window_size,
            restart_dl_average_ratio,
            restart_lbd_average_ratio,
            use_blocking_restart,
            blocking_restart_window_size,
            blocking_restart_multiplier,
            num_conflicts_before_strategy_changes,
            strategy_change_increase_ratio,
            max_time_in_seconds,
            max_deterministic_time,
            max_number_of_conflicts,
            max_memory_in_mb,
            absolute_gap_limit,
            relative_gap_limit,
            random_seed,
            permute_variable_randomly,
            permute_presolve_constraint_order,
            use_absl_random,
            log_search_progress,
            log_frequency_in_seconds,
            model_reduction_log_frequency_in_seconds,
            log_subsolver_statistics,
            log_prefix,
            log_to_stdout,
            log_to_response,
            use_pb_resolution,
            minimize_reduction_during_pb_resolution,
            count_assumption_levels_in_lbd,
            presolve_bve_threshold,
            presolve_bve_clause_weight,
            probing_deterministic_time_limit,
            presolve_probing_deterministic_time_limit,
            presolve_blocked_clause,
            presolve_use_bva,
            presolve_bva_threshold,
            max_presolve_iterations,
            cp_model_presolve,
            cp_model_probing_level,
            cp_model_use_sat_presolve,
            use_sat_inprocessing,
            detect_table_with_cost,
            table_compression_level,
            expand_alldiff_constraints,
            expand_reservoir_constraints,
            disable_constraint_expansion,
            encode_complex_linear_constraint_with_integer,
            merge_no_overlap_work_limit,
            merge_at_most_one_work_limit,
            presolve_substitution_level,
            presolve_extract_integer_enforcement,
            presolve_inclusion_work_limit,
            ignore_names,
            infer_all_diffs,
            find_big_linear_overlap,
            num_workers,
            num_search_workers,
            min_num_lns_workers,
            subsolvers,
            extra_subsolvers,
            ignore_subsolvers,
            subsolver_params,
            interleave_search,
            interleave_batch_size,
            share_objective_bounds,
            share_level_zero_bounds,
            share_binary_clauses,
            debug_postsolve_with_full_solver,
            debug_max_num_presolve_operations,
            debug_crash_on_bad_hint,
            use_optimization_hints,
            minimize_core,
            find_multiple_cores,
            cover_optimization,
            max_sat_assumption_order,
            max_sat_reverse_assumption_order,
            max_sat_stratification,
            propagation_loop_detection_factor,
            use_precedences_in_disjunctive_constraint,
            max_size_to_create_precedence_literals_in_disjunctive,
            use_strong_propagation_in_disjunctive,
            use_overload_checker_in_cumulative,
            use_timetable_edge_finding_in_cumulative,
            use_hard_precedences_in_cumulative,
            exploit_all_precedences,
            use_disjunctive_constraint_in_cumulative,
            use_timetabling_in_no_overlap_2d,
            use_energetic_reasoning_in_no_overlap_2d,
            use_pairwise_reasoning_in_no_overlap_2d,
            use_dual_scheduling_heuristics,
            linearization_level,
            boolean_encoding_level,
            max_domain_size_when_encoding_eq_neq_constraints,
            max_num_cuts,
            cut_level,
            only_add_cuts_at_level_zero,
            add_objective_cut,
            add_cg_cuts,
            add_mir_cuts,
            add_zero_half_cuts,
            add_clique_cuts,
            max_all_diff_cut_size,
            add_lin_max_cuts,
            max_integer_rounding_scaling,
            add_lp_constraints_lazily,
            root_lp_iterations,
            min_orthogonality_for_lp_constraints,
            max_cut_rounds_at_level_zero,
            max_consecutive_inactive_count,
            cut_max_active_count_value,
            cut_active_count_decay,
            cut_cleanup_target,
            new_constraints_batch_size,
            search_branching,
            hint_conflict_limit,
            repair_hint,
            fix_variables_to_their_hinted_value,
            exploit_integer_lp_solution,
            exploit_all_lp_solution,
            exploit_best_solution,
            exploit_relaxation_solution,
            exploit_objective,
            probing_period_at_root,
            use_probing_search,
            use_shaving_in_probing_search,
            shaving_search_deterministic_time,
            use_objective_lb_search,
            use_objective_shaving_search,
            pseudo_cost_reliability_threshold,
            optimize_with_core,
            optimize_with_lb_tree_search,
            binary_search_num_conflicts,
            optimize_with_max_hs,
            test_feasibility_jump,
            feasibility_jump_max_num_values_scanned,
            feasibility_jump_protect_linear_feasibility,
            feasibility_jump_decay,
            feasibility_jump_var_randomization_probability,
            feasibility_jump_var_perburbation_range_ratio,
            feasibility_jump_enable_restarts,
            num_violation_ls,
            violation_ls_perturbation_period,
            shared_tree_num_workers,
            use_shared_tree_search,
            shared_tree_worker_objective_split_probability,
            shared_tree_max_nodes_per_worker,
            shared_tree_split_strategy,
            enumerate_all_solutions,
            keep_all_feasible_solutions_in_presolve,
            fill_tightened_domains_in_response,
            fill_additional_solutions_in_response,
            instantiate_all_variables,
            auto_detect_greater_than_at_least_one_of,
            stop_after_first_solution,
            stop_after_presolve,
            stop_after_root_propagation,
            use_lns_only,
            solution_pool_size,
            use_rins_lns,
            use_feasibility_pump,
            use_lb_relax_lns,
            fp_rounding,
            diversify_lns_params,
            randomize_search,
            search_randomization_tolerance,
            use_optional_variables,
            use_exact_lp_reason,
            use_branching_in_lp,
            use_combined_no_overlap,
            catch_sigint_signal,
            use_implied_bounds,
            polish_lp_solution,
            convert_intervals,
            symmetry_level,
            new_linear_propagation,
            linear_split_size,
            mip_max_bound,
            mip_var_scaling,
            mip_scale_large_domain,
            mip_automatically_scale_variables,
            only_solve_ip,
            mip_wanted_precision,
            mip_max_activity_exponent,
            mip_check_precision,
            mip_compute_true_objective_bound,
            mip_max_valid_magnitude,
            mip_drop_tolerance,
        )
    end
end


function to_proto_struct(
    sat_parameters::SatParameters,
)::OperationsResearch.sat.SatParameters
    return OperationsResearch.sat.SatParameters(
        sat_parameters.name,
        sat_parameters.preferred_variable_order,
        sat_parameters.initial_polarity,
        sat_parameters.use_phase_saving,
        sat_parameters.polarity_rephase_increment,
        sat_parameters.random_polarity_ratio,
        sat_parameters.random_branches_ratio,
        sat_parameters.use_erwa_heuristic,
        sat_parameters.initial_variables_activity,
        sat_parameters.also_bump_variables_in_conflict_reasons,
        sat_parameters.minimization_algorithm,
        sat_parameters.binary_minimization_algorithm,
        sat_parameters.subsumption_during_conflict_analysis,
        sat_parameters.clause_cleanup_period,
        sat_parameters.clause_cleanup_target,
        sat_parameters.clause_cleanup_ratio,
        sat_parameters.clause_cleanup_protection,
        sat_parameters.clause_cleanup_lbd_bound,
        sat_parameters.clause_cleanup_ordering,
        sat_parameters.pb_cleanup_increment,
        sat_parameters.pb_cleanup_ratio,
        sat_parameters.minimize_with_propagation_restart_period,
        sat_parameters.minimize_with_propagation_num_decisions,
        sat_parameters.variable_activity_decay,
        sat_parameters.max_variable_activity_value,
        sat_parameters.glucose_max_decay,
        sat_parameters.glucose_decay_increment,
        sat_parameters.glucose_decay_increment_period,
        sat_parameters.clause_activity_decay,
        sat_parameters.max_clause_activity_value,
        sat_parameters.restart_algorithms,
        sat_parameters.default_restart_algorithms,
        sat_parameters.restart_period,
        sat_parameters.restart_running_window_size,
        sat_parameters.restart_dl_average_ratio,
        sat_parameters.restart_lbd_average_ratio,
        sat_parameters.use_blocking_restart,
        sat_parameters.blocking_restart_window_size,
        sat_parameters.blocking_restart_multiplier,
        sat_parameters.num_conflicts_before_strategy_changes,
        sat_parameters.strategy_change_increase_ratio,
        sat_parameters.max_time_in_seconds,
        sat_parameters.max_deterministic_time,
        sat_parameters.max_number_of_conflicts,
        sat_parameters.max_memory_in_mb,
        sat_parameters.absolute_gap_limit,
        sat_parameters.relative_gap_limit,
        sat_parameters.random_seed,
        sat_parameters.permute_variable_randomly,
        sat_parameters.permute_presolve_constraint_order,
        sat_parameters.use_absl_random,
        sat_parameters.log_search_progress,
        sat_parameters.log_frequency_in_seconds,
        sat_parameters.model_reduction_log_frequency_in_seconds,
        sat_parameters.log_subsolver_statistics,
        sat_parameters.log_prefix,
        sat_parameters.log_to_stdout,
        sat_parameters.log_to_response,
        sat_parameters.use_pb_resolution,
        sat_parameters.minimize_reduction_during_pb_resolution,
        sat_parameters.count_assumption_levels_in_lbd,
        sat_parameters.presolve_bve_threshold,
        sat_parameters.presolve_bve_clause_weight,
        sat_parameters.probing_deterministic_time_limit,
        sat_parameters.presolve_probing_deterministic_time_limit,
        sat_parameters.presolve_blocked_clause,
        sat_parameters.presolve_use_bva,
        sat_parameters.presolve_bva_threshold,
        sat_parameters.max_presolve_iterations,
        sat_parameters.cp_model_presolve,
        sat_parameters.cp_model_probing_level,
        sat_parameters.cp_model_use_sat_presolve,
        sat_parameters.use_sat_inprocessing,
        sat_parameters.detect_table_with_cost,
        sat_parameters.table_compression_level,
        sat_parameters.expand_alldiff_constraints,
        sat_parameters.expand_reservoir_constraints,
        sat_parameters.disable_constraint_expansion,
        sat_parameters.encode_complex_linear_constraint_with_integer,
        sat_parameters.merge_no_overlap_work_limit,
        sat_parameters.merge_at_most_one_work_limit,
        sat_parameters.presolve_substitution_level,
        sat_parameters.presolve_extract_integer_enforcement,
        sat_parameters.presolve_inclusion_work_limit,
        sat_parameters.ignore_names,
        sat_parameters.infer_all_diffs,
        sat_parameters.find_big_linear_overlap,
        sat_parameters.num_workers,
        sat_parameters.num_search_workers,
        sat_parameters.min_num_lns_workers,
        sat_parameters.subsolvers,
        sat_parameters.extra_subsolvers,
        sat_parameters.ignore_subsolvers,
        sat_parameters.subsolver_params,
        sat_parameters.interleave_search,
        sat_parameters.interleave_batch_size,
        sat_parameters.share_objective_bounds,
        sat_parameters.share_level_zero_bounds,
        sat_parameters.share_binary_clauses,
        sat_parameters.debug_postsolve_with_full_solver,
        sat_parameters.debug_max_num_presolve_operations,
        sat_parameters.debug_crash_on_bad_hint,
        sat_parameters.use_optimization_hints,
        sat_parameters.minimize_core,
        sat_parameters.find_multiple_cores,
        sat_parameters.cover_optimization,
        sat_parameters.max_sat_assumption_order,
        sat_parameters.max_sat_reverse_assumption_order,
        sat_parameters.max_sat_stratification,
        sat_parameters.propagation_loop_detection_factor,
        sat_parameters.use_precedences_in_disjunctive_constraint,
        sat_parameters.max_size_to_create_precedence_literals_in_disjunctive,
        sat_parameters.use_strong_propagation_in_disjunctive,
        sat_parameters.use_overload_checker_in_cumulative,
        sat_parameters.use_timetable_edge_finding_in_cumulative,
        sat_parameters.use_hard_precedences_in_cumulative,
        sat_parameters.exploit_all_precedences,
        sat_parameters.use_disjunctive_constraint_in_cumulative,
        sat_parameters.use_timetabling_in_no_overlap_2d,
        sat_parameters.use_energetic_reasoning_in_no_overlap_2d,
        sat_parameters.use_pairwise_reasoning_in_no_overlap_2d,
        sat_parameters.use_dual_scheduling_heuristics,
        sat_parameters.linearization_level,
        sat_parameters.boolean_encoding_level,
        sat_parameters.max_domain_size_when_encoding_eq_neq_constraints,
        sat_parameters.max_num_cuts,
        sat_parameters.cut_level,
        sat_parameters.only_add_cuts_at_level_zero,
        sat_parameters.add_objective_cut,
        sat_parameters.add_cg_cuts,
        sat_parameters.add_mir_cuts,
        sat_parameters.add_zero_half_cuts,
        sat_parameters.add_clique_cuts,
        sat_parameters.max_all_diff_cut_size,
        sat_parameters.add_lin_max_cuts,
        sat_parameters.max_integer_rounding_scaling,
        sat_parameters.add_lp_constraints_lazily,
        sat_parameters.root_lp_iterations,
        sat_parameters.min_orthogonality_for_lp_constraints,
        sat_parameters.max_cut_rounds_at_level_zero,
        sat_parameters.max_consecutive_inactive_count,
        sat_parameters.cut_max_active_count_value,
        sat_parameters.cut_active_count_decay,
        sat_parameters.cut_cleanup_target,
        sat_parameters.new_constraints_batch_size,
        sat_parameters.search_branching,
        sat_parameters.hint_conflict_limit,
        sat_parameters.repair_hint,
        sat_parameters.fix_variables_to_their_hinted_value,
        sat_parameters.exploit_integer_lp_solution,
        sat_parameters.exploit_all_lp_solution,
        sat_parameters.exploit_best_solution,
        sat_parameters.exploit_relaxation_solution,
        sat_parameters.exploit_objective,
        sat_parameters.probing_period_at_root,
        sat_parameters.use_probing_search,
        sat_parameters.use_shaving_in_probing_search,
        sat_parameters.shaving_search_deterministic_time,
        sat_parameters.use_objective_lb_search,
        sat_parameters.use_objective_shaving_search,
        sat_parameters.pseudo_cost_reliability_threshold,
        sat_parameters.optimize_with_core,
        sat_parameters.optimize_with_lb_tree_search,
        sat_parameters.binary_search_num_conflicts,
        sat_parameters.optimize_with_max_hs,
        sat_parameters.test_feasibility_jump,
        sat_parameters.feasibility_jump_max_num_values_scanned,
        sat_parameters.feasibility_jump_protect_linear_feasibility,
        sat_parameters.feasibility_jump_decay,
        sat_parameters.feasibility_jump_var_randomization_probability,
        sat_parameters.feasibility_jump_var_perburbation_range_ratio,
        sat_parameters.feasibility_jump_enable_restarts,
        sat_parameters.num_violation_ls,
        sat_parameters.violation_ls_perturbation_period,
        sat_parameters.shared_tree_num_workers,
        sat_parameters.use_shared_tree_search,
        sat_parameters.shared_tree_worker_objective_split_probability,
        sat_parameters.shared_tree_max_nodes_per_worker,
        sat_parameters.shared_tree_split_strategy,
        sat_parameters.enumerate_all_solutions,
        sat_parameters.keep_all_feasible_solutions_in_presolve,
        sat_parameters.fill_tightened_domains_in_response,
        sat_parameters.fill_additional_solutions_in_response,
        sat_parameters.instantiate_all_variables,
        sat_parameters.auto_detect_greater_than_at_least_one_of,
        sat_parameters.stop_after_first_solution,
        sat_parameters.stop_after_presolve,
        sat_parameters.stop_after_root_propagation,
        sat_parameters.use_lns_only,
        sat_parameters.solution_pool_size,
        sat_parameters.use_rins_lns,
        sat_parameters.use_feasibility_pump,
        sat_parameters.use_lb_relax_lns,
        sat_parameters.fp_rounding,
        sat_parameters.diversify_lns_params,
        sat_parameters.randomize_search,
        sat_parameters.search_randomization_tolerance,
        sat_parameters.use_optional_variables,
        sat_parameters.use_exact_lp_reason,
        sat_parameters.use_branching_in_lp,
        sat_parameters.use_combined_no_overlap,
        sat_parameters.catch_sigint_signal,
        sat_parameters.use_implied_bounds,
        sat_parameters.polish_lp_solution,
        sat_parameters.convert_intervals,
        sat_parameters.symmetry_level,
        sat_parameters.new_linear_propagation,
        sat_parameters.linear_split_size,
        sat_parameters.mip_max_bound,
        sat_parameters.mip_var_scaling,
        sat_parameters.mip_scale_large_domain,
        sat_parameters.mip_automatically_scale_variables,
        sat_parameters.only_solve_ip,
        sat_parameters.mip_wanted_precision,
        sat_parameters.mip_max_activity_exponent,
        sat_parameters.mip_check_precision,
        sat_parameters.mip_compute_true_objective_bound,
        sat_parameters.mip_max_valid_magnitude,
        sat_parameters.mip_drop_tolerance,
    )
end

"""
  Mutable wrapper struct for the GlpkParameters struct.
"""
mutable struct GlpkParameters
    compute_unbound_rays_if_possible::Bool

    function GlpkParameters(; compute_unbound_rays_if_possible = false)
        new(compute_unbound_rays_if_possible)
    end
end

function to_proto_struct(glpk_parameters::GlpkParameters)::MathOpt.GlpkParametersProto
    return MathOpt.GlpkParametersProto(glpk_parameters.compute_unbound_rays_if_possible)
end


"""
  Mutable wrapper struct for the HighsOptionsProto struct.
"""
mutable struct HighsOptions
    string_options::Dict{String,String}
    double_options::Dict{String,Float64}
    int_options::Dict{String,Int32}
    bool_options::Dict{String,Bool}

    function HighsOptions(;
        string_options = Dict{String,String}(),
        double_options = Dict{String,Float64}(),
        int_options = Dict{String,Int32}(),
        bool_options = Dict{String,Bool}(),
    )
        new(string_options, double_options, int_options, bool_options)
    end
end

function to_proto_struct(highs_options::HighsOptions)::MathOpt.HighsOptionsProto
    return MathOpt.HighsOptionsProto(
        highs_options.string_options,
        highs_options.double_options,
        highs_options.int_options,
        highs_options.bool_options,
    )
end

mutable struct OsqpSettings
    rho::Float64
    sigma::Float64
    scaling::Int64
    adaptive_rho::Bool
    adaptive_rho_interval::Int64
    adaptive_rho_tolerance::Float64
    adaptive_rho_fraction::Float64
    max_iter::Int64
    eps_abs::Float64
    eps_rel::Float64
    eps_prim_inf::Float64
    eps_dual_inf::Float64
    alpha::Float64
    delta::Float64
    polish::Bool
    polish_refine_iter::Int64
    verbose::Bool
    scaled_termination::Bool
    check_termination::Int64
    warm_start::Bool
    time_limit::Float64

    function OsqpSettings(;
        rho = zero(Float64),
        sigma = zero(Float64),
        scaling = zero(Int64),
        adaptive_rho = false,
        adaptive_rho_interval = zero(Int64),
        adaptive_rho_tolerance = zero(Float64),
        adaptive_rho_fraction = zero(Float64),
        max_iter = zero(Int64),
        eps_abs = zero(Float64),
        eps_rel = zero(Float64),
        eps_prim_inf = zero(Float64),
        eps_dual_inf = zero(Float64),
        alpha = zero(Float64),
        delta = zero(Float64),
        polish = false,
        polish_refine_iter = zero(Int64),
        verbose = false,
        scaled_termination = false,
        check_termination = 0,
        warm_start = false,
        time_limit = zero(Float64),
    )

        new(
            rho,
            sigma,
            scaling,
            adaptive_rho,
            adaptive_rho_interval,
            adaptive_rho_tolerance,
            adaptive_rho_fraction,
            max_iter,
            eps_abs,
            eps_rel,
            eps_prim_inf,
            eps_dual_inf,
            alpha,
            delta,
            polish,
            polish_refine_iter,
            verbose,
            scaled_termination,
            check_termination,
            warm_start,
            time_limit,
        )
    end
end

function to_proto_struct(osqp_settings::OsqpSettings)::MathOpt.OsqpSettingsProto
    return MathOpt.OsqpSettingsProto(
        osqp_settings.rho,
        osqp_settings.sigma,
        osqp_settings.scaling,
        osqp_settings.adaptive_rho,
        osqp_settings.adaptive_rho_interval,
        osqp_settings.adaptive_rho_tolerance,
        osqp_settings.adaptive_rho_fraction,
        osqp_settings.max_iter,
        osqp_settings.eps_abs,
        osqp_settings.eps_rel,
        osqp_settings.eps_prim_inf,
        osqp_settings.eps_dual_inf,
        osqp_settings.alpha,
        osqp_settings.delta,
        osqp_settings.polish,
        osqp_settings.polish_refine_iter,
        osqp_settings.verbose,
        osqp_settings.scaled_termination,
        osqp_settings.check_termination,
        osqp_settings.warm_start,
        osqp_settings.time_limit,
    )
end

mutable struct OsqpOutput
    initialized_underlying_solver::Bool

    function OsqpOutput(; initialized_underlying_solver = false)
        new(initialized_underlying_solver)
    end
end

function to_proto_struct(osqp_output::OsqpOutput)::MathOpt.OsqpOutput
    return MathOpt.OsqpOutput(osqp_output.initialized_underlying_solver)
end

mutable struct PdlpTerminationCriteria
    optimality_norm::PdlpOptimalityNorm.T
    optimality_criteria::Union{
        Nothing,
        PB.OneOf{
            <:Union{
                PdlpTerminationCriteria_SimpleOptimalityCriteria,
                PdlpTerminationCriteria_DetailedOptimalityCriteria,
            },
        },
    }
    eps_optimal_absolute::Float64
    eps_optimal_relative::Float64
    eps_primal_infeasible::Float64
    eps_dual_infeasible::Float64
    time_sec_limit::Float64
    iteration_limit::Int32
    kkt_matrix_pass_limit::Float64
end

function to_proto_struct(
    pdlp_termination_criteria::PdlpTerminationCriteria,
)::PDLP.TerminationCriteria
    return PDLP.TerminationCriteria(
        pdlp_termination_criteria.optimality_norm,
        pdlp_termination_criteria.optimality_criteria,
        pdlp_termination_criteria.eps_optimal_absolute,
        pdlp_termination_criteria.eps_optimal_relative,
        pdlp_termination_criteria.eps_primal_infeasible,
        pdlp_termination_criteria.eps_dual_infeasible,
        pdlp_termination_criteria.time_sec_limit,
        pdlp_termination_criteria.iteration_limit,
        pdlp_termination_criteria.kkt_matrix_pass_limit,
    )
end

mutable struct PdlpHybridGradientParameters
    termination_criteria::Union{Nothing,PdlpTerminationCriteria}
    num_threads::Int32
    num_shards::Int32
    scheduler_type::PdlpSchedulerType.T
    record_iteration_stats::Bool
    verbosity_level::Int32
    log_interval_seconds::Float64
    major_iteration_frequency::Int32
    termination_check_frequency::Int32
    restart_strategy::PdlpHybridGradientParams_RestartStrategy.T
    primal_weight_update_smoothing::Float64
    initial_primal_weight::Float64
    presolve_options::Union{Nothing,PdlpHybridGradientParams_PresolveOptions}
    l_inf_ruiz_iterations::Int32
    l2_norm_rescaling::Bool
    sufficient_reduction_for_restart::Float64
    necessary_reduction_for_restart::Float64
    linesearch_rule::PdlpHybridGradientParams_LinesearchRule.T
    adaptive_linesearch_parameters::Union{Nothing,PdlpAdaptiveLinesearchParams}
    malitsky_pock_parameters::Union{Nothing,PdlpMalitskyPockParams}
    initial_step_size_scaling::Float64
    random_projection_seeds::Vector{Int32}
    infinite_constraint_bound_threshold::Float64
    handle_some_primal_gradients_on_finite_bounds_as_residuals::Bool
    use_diagonal_qp_trust_region_solver::Bool
    diagonal_qp_trust_region_solver_tolerance::Float64
    use_feasibility_polishing::Bool
    apply_feasibility_polishing_after_limits_reached::Bool
    apply_feasibility_polishing_if_solver_is_interrupted::Bool
end


function to_proto_struct(
    pdlp_hybrid_gradient_parameters::PdlpHybridGradientParameters,
)::PDLP.PrimalDualHybridGradientParams
    termination_criteria = nothing
    if !isnothing(pdlp_hybrid_gradient_parameters.termination_criteria)
        termination_criteria =
            to_proto_struct(pdlp_hybrid_gradient_parameters.termination_criteria)
    end

    return PDLP.PrimalDualHybridGradientParams(
        termination_criteria,
        pdlp_hybrid_gradient_parameters.num_threads,
        pdlp_hybrid_gradient_parameters.num_shards,
        pdlp_hybrid_gradient_parameters.scheduler_type,
        pdlp_hybrid_gradient_parameters.record_iteration_stats,
        pdlp_hybrid_gradient_parameters.verbosity_level,
        pdlp_hybrid_gradient_parameters.log_interval_seconds,
        pdlp_hybrid_gradient_parameters.major_iteration_frequency,
        pdlp_hybrid_gradient_parameters.termination_check_frequency,
        pdlp_hybrid_gradient_parameters.restart_strategy,
        pdlp_hybrid_gradient_parameters.primal_weight_update_smoothing,
        pdlp_hybrid_gradient_parameters.initial_primal_weight,
        pdlp_hybrid_gradient_parameters.presolve_options,
        pdlp_hybrid_gradient_parameters.l_inf_ruiz_iterations,
        pdlp_hybrid_gradient_parameters.l2_norm_rescaling,
        pdlp_hybrid_gradient_parameters.sufficient_reduction_for_restart,
        pdlp_hybrid_gradient_parameters.necessary_reduction_for_restart,
        pdlp_hybrid_gradient_parameters.linesearch_rule,
        pdlp_hybrid_gradient_parameters.adaptive_linesearch_parameters,
        pdlp_hybrid_gradient_parameters.malitsky_pock_parameters,
        pdlp_hybrid_gradient_parameters.initial_step_size_scaling,
        pdlp_hybrid_gradient_parameters.random_projection_seeds,
        pdlp_hybrid_gradient_parameters.infinite_constraint_bound_threshold,
        pdlp_hybrid_gradient_parameters.handle_some_primal_gradients_on_finite_bounds_as_residuals,
        pdlp_hybrid_gradient_parameters.use_diagonal_qp_trust_region_solver,
        pdlp_hybrid_gradient_parameters.diagonal_qp_trust_region_solver_tolerance,
        pdlp_hybrid_gradient_parameters.use_feasibility_polishing,
        pdlp_hybrid_gradient_parameters.apply_feasibility_polishing_after_limits_reached,
        pdlp_hybrid_gradient_parameters.apply_feasibility_polishing_if_solver_is_interrupted,
    )
end

"""
  Mutable wrapper struct for the SolveParametersProto struct.
"""
mutable struct SolveParameters
    time_limit::Union{Nothing,Duration}
    iteration_limit::Int64
    node_limit::Int64
    cutoff_limit::Float64
    objective_limit::Float64
    best_bound_limit::Float64
    solution_limit::Int32
    enable_output::Bool
    threads::Int32
    random_seed::Int32
    absolute_gap_tolerance::Float64
    relative_gap_tolerance::Float64
    solution_pool_size::Int32
    lp_algorithm::LPAlgorithm.T
    presolve::Emphasis.T
    cuts::Emphasis.T
    heuristics::Emphasis.T
    scaling::Emphasis.T
    gscip::Union{Nothing,GScipParameters}
    gurobi::Union{Nothing,GurobiParameters}
    glop::Union{Nothing,GlopParameters}
    cp_sat::Union{Nothing,SatParameters}
    pdlp::Union{Nothing,PdlpHybridGradientParameters}
    osqp::Union{Nothing,OsqpSettings}
    glpk::Union{Nothing,GlpkParameters}
    highs::Union{Nothing,HighsOptions}

    function SolveParameters(;
        time_limit = nothing,
        iteration_limit = zero(Int64),
        node_limit = zero(Int64),
        cutoff_limit = zero(Float64),
        objective_limit = zero(Float64),
        best_bound_limit = zero(Float64),
        solution_limit = zero(Int32),
        enable_output = false,
        threads = zero(Int32),
        random_seed = zero(Int32),
        absolute_gap_tolerance = zero(Float64),
        relative_gap_tolerance = zero(Float64),
        solution_pool_size = zero(Int32),
        lp_algorithm = LPAlgorithm.LP_ALGORITHM_UNSPECIFIED,
        presolve = Emphasis.EMPHASIS_UNSPECIFIED,
        cuts = Emphasis.EMPHASIS_UNSPECIFIED,
        heuristics = Emphasis.EMPHASIS_UNSPECIFIED,
        scaling = Emphasis.EMPHASIS_UNSPECIFIED,
        gscip = nothing,
        gurobi = nothing,
        glop = nothing,
        cp_sat = nothing,
        pdlp = nothing,
        osqp = nothing,
        glpk = nothing,
        highs = nothing,
    )
        new(
            time_limit,
            iteration_limit,
            node_limit,
            cutoff_limit,
            objective_limit,
            best_bound_limit,
            solution_limit,
            enable_output,
            threads,
            random_seed,
            absolute_gap_tolerance,
            relative_gap_tolerance,
            solution_pool_size,
            lp_algorithm,
            presolve,
            cuts,
            heuristics,
            scaling,
            gscip,
            gurobi,
            glop,
            cp_sat,
            pdlp,
            osqp,
            glpk,
            highs,
        )
    end
end

function to_proto_struct(solve_parameters::SolveParameters)::MathOpt.SolveParametersProto
    gscip_parameters = nothing
    if !isnothing(solve_parameters.gscip)
        gscip_parameters = to_proto_struct(solve_parameters.gscip)
    end

    gurobi_parameters = nothing
    if !isnothing(solve_parameters.gurobi)
        gurobi_parameters = to_proto_struct(solve_parameters.gurobi)
    end

    glop_parameters = nothing
    if !isnothing(solve_parameters.glop)
        glop_parameters = to_proto_struct(solve_parameters.glop)
    end

    cp_sat_parameters = nothing
    if !isnothing(solve_parameters.cp_sat)
        cp_sat_parameters = to_proto_struct(solve_parameters.cp_sat)
    end

    glpk_parameters = nothing
    if !isnothing(solve_parameters.glpk)
        glpk_parameters = to_proto_struct(solve_parameters.glpk)
    end

    highs_parameters = nothing
    if !isnothing(solve_parameters.highs)
        highs_parameters = to_proto_struct(solve_parameters.highs)
    end

    pdlp_parameters = nothing
    if !isnothing(solve_parameters.pdlp)
        pdlp_parameters = to_proto_struct(solve_parameters.pdlp)
    end

    osqp_parameters = nothing
    if !isnothing(solve_parameters.osqp)
        osqp_parameters = to_proto_struct(solve_parameters.osqp)
    end

    return MathOpt.SolveParametersProto(
        solve_parameters.time_limit,
        solve_parameters.iteration_limit,
        solve_parameters.node_limit,
        solve_parameters.cutoff_limit,
        solve_parameters.objective_limit,
        solve_parameters.best_bound_limit,
        solve_parameters.solution_limit,
        solve_parameters.enable_output,
        solve_parameters.threads,
        solve_parameters.random_seed,
        solve_parameters.absolute_gap_tolerance,
        solve_parameters.relative_gap_tolerance,
        solve_parameters.solution_pool_size,
        solve_parameters.lp_algorithm,
        solve_parameters.presolve,
        solve_parameters.cuts,
        solve_parameters.heuristics,
        solve_parameters.scaling,
        gscip_parameters,
        gurobi_parameters,
        glop_parameters,
        cp_sat_parameters,
        pdlp_parameters,
        osqp_parameters,
        glpk_parameters,
        highs_parameters,
    )
end

# The size of the encoded solve parameters proto.
encoded_parameters_size(parameters::SolveParameters) =
    PB._encoded_size(to_proto_struct(parameters))
