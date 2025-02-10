import MathOptInterface as MOI
include("Type_wrappers.jl")

const PARAM_SPLITTER = "__"
const PARAM_FIELD_NAME_TO_INSTANCE_DICT = Dict(
    "gscip" => GScipParameters(),
    "gurobi" => GurobiParameters(),
    "glop" => GlopParameters(),
    "cp_sat" => SatParameters(),
    "glpk" => GlpkParameters(),
    "highs" => HighsOptions(),
)

# Though these are the supported solvers, nothing in the code is really specific to them
# For other solvers with an open source implementation, like Gurobi, it is better to use their open 
# source wrapper, e.g Gurobi.jl.
const SUPPORTED_SOLVER_TYPES = [
    SolverType.SOLVER_TYPE_UNSPECIFIED,
    SolverType.SOLVER_TYPE_GLOP,
    SolverType.SOLVER_TYPE_CP_SAT,
]

const NON_GOOGLE_SOLVER_WARNING = """
You have specified a non-Google solver; the solver is unlikely to be compiled into OR-Tools and you will encounter errors.
If you'd really like to use the solver you have specified, we highly recommend that you use its
available Julia wrapper. Support for remote solves in ORTools.jl will be added in the future.
"""

# Keys to the constraint indices dict
const SCALAR_SET_WITH_VARIABLE_INDEX_CONSTRAINT_KEY = "scalar_set_with_variable_index"
const SCALAR_SET_WITH_SCALAR_FUNCTION_CONSTRAINT_KEY = "scalar_set_with_scalar_function"
const INTEGER_CONSTRAINT_KEY = "integer"
const ZERO_ONE_CONSTRAINT_KEY = "zero_one"

"""
  Optimizer()

  Create a new MathOpt optimizer.

  # Solver Type

  By default, the optimizer will use the `GLOP` solver.
"""
mutable struct Optimizer <: MOI.AbstractOptimizer
    solver_type::SolverType.T
    model::Union{Model,Nothing}
    parameters::Union{SolveParameters,Nothing}

    # Metadata added for MOI's internal use
    constraint_types_present::Set{Tuple{Type,Type}}
    constraint_indices_dict::Dict{String,Vector{MOI.ConstraintIndex}}

    # Indicator of whether an objective has been set
    objective_set::Bool

    # Constructor with optional parameters
    function Optimizer(;
        model_name::String = "",
        solver_type::SolverType.T = SolverType.SOLVER_TYPE_GLOP,
        parameters::Union{SolveParameters,Nothing} = SolveParameters(),
    )
        if !in(solver_type, SUPPORTED_SOLVER_TYPES)
            @warn NON_GOOGLE_SOLVER_WARNING
        end
        # Initialize a model instance
        model = Model(name = model_name)

        # Initialize the constraint indices dict
        # TODO: b/392072219 - replace this with member fields on the struct
        constraint_indices_dict = Dict(
            SCALAR_SET_WITH_VARIABLE_INDEX_CONSTRAINT_KEY => [],
            SCALAR_SET_WITH_SCALAR_FUNCTION_CONSTRAINT_KEY => [],
            INTEGER_CONSTRAINT_KEY => [],
            ZERO_ONE_CONSTRAINT_KEY => [],
        )

        new(
            solver_type,
            model,
            parameters,
            Set{Tuple{Type,Type}}(),
            constraint_indices_dict,
            false,
        )
    end
end

function setproperty!(model::Optimizer, field::Symbol, value)
    if field == :solver_type && !in(value, SUPPORTED_SOLVER_TYPES)
        @warn NON_GOOGLE_SOLVER_WARNING
    end

    setfield!(model, field, value)

    return nothing
end

function MOI.empty!(model::Optimizer)
    model.solver_type = SolverType.SOLVER_TYPE_UNSPECIFIED
    model.model = nothing
    model.parameters = nothing
    # Clear the related metadata
    model.constraint_types_present = Set{Tuple{Type,Type}}()
    model.constraint_indices_dict = Dict()
    model.objective_set = false

    return nothing
end

function MOI.is_empty(model::Optimizer)
    return isnothing(model.model) &&
           isnothing(model.parameters) &&
           model.solver_type == SolverType.SOLVER_TYPE_UNSPECIFIED &&
           !model.objective_set
end

"""
TODO: b/384496265 - implement Base.summary(::IO, ::Optimizer) 
to print a nice string when someone shows your model
"""

function MOI.get(model::Optimizer, ::MOI.SolverName)
    return "$(model.solver_type)"
end

function MOI.get(model::Optimizer, ::MOI.SolverVersion)
    return "1.0.0-DEV"
end

function MOI.get(model::Optimizer, ::MOI.Name)
    if !MOI.is_empty(model)
        return model.model.name
    end

    return ""
end

function MOI.set(model::Optimizer, ::MOI.Name, name::String)
    optionally_initialize_model_and_parameters!(model)

    model.model.name = name

    return nothing
end

MOI.supports(model::Optimizer, ::MOI.Name) = true


function optionally_initialize_model_and_parameters!(model::Optimizer)::Nothing
    if MOI.is_empty(model)
        model.solver_type = SolverType.SOLVER_TYPE_UNSPECIFIED
        model.model = Model()
        model.parameters = SolveParameters()
        # Re-initailize the associated metadata.
        # TODO: b/392072219 - use emtpy! to do this after resolving this bug.
        model.constraint_indices_dict = Dict(
            SCALAR_SET_WITH_VARIABLE_INDEX_CONSTRAINT_KEY => [],
            SCALAR_SET_WITH_SCALAR_FUNCTION_CONSTRAINT_KEY => [],
            INTEGER_CONSTRAINT_KEY => [],
            ZERO_ONE_CONSTRAINT_KEY => [],
        )
        model.objective_set = false
    end

    if isnothing(model.parameters)
        model.parameters = SolveParameters()
    end

    return nothing
end

function optionally_initialize_objective!(model::Optimizer)::Nothing
    if isnothing(model.model.objective)
        model.model.objective = Objective()
    end

    return nothing
end


function MOI.get(model::Optimizer, ::MOI.Silent)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        return !model.parameters.enable_output
    end

    return true
end

function MOI.set(model::Optimizer, ::MOI.Silent, silent::Bool)
    optionally_initialize_model_and_parameters!(model)

    model.parameters.enable_output = !silent

    return nothing
end

MOI.supports(model::Optimizer, ::MOI.Silent) = true

function MOI.get(model::Optimizer, ::MOI.TimeLimitSec)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        return model.parameters.time_limit
    end

    return nothing
end

function MOI.set(model::Optimizer, ::MOI.TimeLimitSec, time_limit::Union{Nothing,Duration})
    optionally_initialize_model_and_parameters!(model)
    model.parameters.time_limit = time_limit

    return nothing
end

MOI.supports(model::Optimizer, ::MOI.TimeLimitSec) = true

function MOI.get(model::Optimizer, ::MOI.ObjectiveLimit)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        objective_limit = model.parameters.objective_limit
        return objective_limit == -Inf ? nothing : objective_limit
    end

    return nothing
end

function MOI.set(
    model::Optimizer,
    ::MOI.ObjectiveLimit,
    objective_limit::Union{Nothing,Real},
)
    optionally_initialize_model_and_parameters!(model)

    if isnothing(objective_limit)
        model.parameters.objective_limit = -Inf
    else
        model.parameters.objective_limit = Float64(objective_limit)
    end

    return nothing
end

MOI.supports(model::Optimizer, ::MOI.ObjectiveLimit) = true

function MOI.get(model::Optimizer, ::MOI.SolutionLimit)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        solution_limit = model.parameters.solution_limit
        return solution_limit == 0 ? nothing : solution_limit
    end

    return nothing
end

function MOI.set(model::Optimizer, ::MOI.SolutionLimit, solution_limit::Union{Nothing,Int})
    optionally_initialize_model_and_parameters!(model)

    if isnothing(solution_limit)
        model.parameters.solution_limit = zero(Int32)
    else
        try
            model.parameters.solution_limit = Int32(solution_limit)
        catch err
            if isa(err, InexactError)
                println("Solution limit must be an Int32")
            else
                println("Setting solution limit failed with error: ", err)
            end
        end
    end

    return nothing
end

MOI.supports(model::Optimizer, ::MOI.SolutionLimit) = true

function MOI.get(model::Optimizer, ::MOI.NodeLimit)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        node_limit = model.parameters.node_limit
        return node_limit == 0 ? nothing : node_limit
    end

    return nothing
end

function MOI.set(model::Optimizer, ::MOI.NodeLimit, node_limit::Union{Nothing,Int})
    optionally_initialize_model_and_parameters!(model)

    if isnothing(node_limit)
        model.parameters.node_limit = zero(Int64)
    else
        model.parameters.node_limit = Int64(node_limit)
    end

    return nothing
end

MOI.supports(model::Optimizer, ::MOI.NodeLimit) = true

function MOI.get(model::Optimizer, ::MOI.NumberOfThreads)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        number_of_threads = model.parameters.threads
        return number_of_threads == 0 ? nothing : number_of_threads
    end

    return nothing
end

function MOI.set(
    model::Optimizer,
    ::MOI.NumberOfThreads,
    number_of_threads::Union{Nothing,Int},
)
    optionally_initialize_model_and_parameters!(model)

    if isnothing(number_of_threads)
        model.parameters.threads = zero(Int32)
    else
        model.parameters.threads = Int32(number_of_threads)
    end

    return nothing
end

MOI.supports(model::Optimizer, ::MOI.NumberOfThreads) = true

function MOI.get(model::Optimizer, ::MOI.AbsoluteGapTolerance)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        return model.parameters.absolute_gap_tolerance
    end

    return 0
end

function MOI.set(
    model::Optimizer,
    ::MOI.AbsoluteGapTolerance,
    absolute_gap_tolerance::Union{Nothing,Real},
)
    optionally_initialize_model_and_parameters!(model)

    if isnothing(absolute_gap_tolerance)
        model.parameters.absolute_gap_tolerance = zero(Float64)
    else
        model.parameters.absolute_gap_tolerance = Float64(absolute_gap_tolerance)
    end

    return nothing
end

MOI.supports(model::Optimizer, ::MOI.AbsoluteGapTolerance) = true

function MOI.get(model::Optimizer, ::MOI.RelativeGapTolerance)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        return model.parameters.relative_gap_tolerance
    end

    return 0
end

function MOI.set(
    model::Optimizer,
    ::MOI.RelativeGapTolerance,
    relative_gap_tolerance::Union{Nothing,Real},
)
    optionally_initialize_model_and_parameters!(model)

    if isnothing(relative_gap_tolerance)
        model.parameters.relative_gap_tolerance = zero(Float64)
    else
        model.parameters.relative_gap_tolerance = Float64(relative_gap_tolerance)
    end

    return nothing
end

MOI.supports(model::Optimizer, ::MOI.RelativeGapTolerance) = true

# Extra parameter attributes supported by the solver
struct CutOffLimit <: MOI.AbstractOptimizerAttribute end
MOI.attribute_value_type(::CutOffLimit) = Union{Nothing,Real}

function MOI.set(model::Optimizer, ::CutOffLimit, cutoff_limit::Union{Nothing,Real})
    optionally_initialize_model_and_parameters!(model)

    if isnothing(cutoff_limit)
        model.parameters.cutoff_limit = zero(Float64)
    else
        model.parameters.cutoff_limit = Float64(cutoff_limit)
    end

    return nothing
end

function MOI.get(model::Optimizer, ::CutOffLimit)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        return model.parameters.cutoff_limit
    end

    return nothing
end

MOI.supports(model::Optimizer, ::CutOffLimit) = true


struct BestBoundLimit <: MOI.AbstractOptimizerAttribute end
MOI.attribute_value_type(::BestBoundLimit) = Union{Nothing,Real}

function MOI.set(model::Optimizer, ::BestBoundLimit, best_bound_limit::Union{Nothing,Real})
    optionally_initialize_model_and_parameters!(model)

    if isnothing(best_bound_limit)
        model.parameters.best_bound_limit = zero(Float64)
    else
        model.parameters.best_bound_limit = Float64(best_bound_limit)
    end

    return nothing
end

function MOI.get(model::Optimizer, ::BestBoundLimit)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        return model.parameters.best_bound_limit
    end

    return nothing
end

MOI.supports(model::Optimizer, ::BestBoundLimit) = true


struct RandomSeed <: MOI.AbstractOptimizerAttribute end
MOI.attribute_value_type(::RandomSeed) = Union{Nothing,Int}

function MOI.set(model::Optimizer, ::RandomSeed, random_seed::Union{Nothing,Int})
    optionally_initialize_model_and_parameters!(model)

    if isnothing(random_seed)
        model.parameters.random_seed = zero(Int32)
    else
        model.parameters.random_seed = Int32(random_seed)
    end

    return nothing
end

function MOI.get(model::Optimizer, ::RandomSeed)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        return model.parameters.random_seed
    end

    return nothing
end

MOI.supports(model::Optimizer, ::RandomSeed) = true


struct SolutionPoolSize <: MOI.AbstractOptimizerAttribute end
MOI.attribute_value_type(::SolutionPoolSize) = Union{Nothing,Int}

function MOI.set(
    model::Optimizer,
    ::SolutionPoolSize,
    solution_pool_size::Union{Nothing,Int},
)
    optionally_initialize_model_and_parameters!(model)

    if isnothing(solution_pool_size)
        model.parameters.solution_pool_size = zero(Int32)
    else
        model.parameters.solution_pool_size = Int32(solution_pool_size)
    end

    return nothing
end

function MOI.get(model::Optimizer, ::SolutionPoolSize)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        return model.parameters.solution_pool_size
    end

    return nothing
end

MOI.supports(model::Optimizer, ::SolutionPoolSize) = true


struct LPAlgorithmType <: MOI.AbstractOptimizerAttribute end
MOI.attribute_value_type(::LPAlgorithmType) = Union{Nothing,LPAlgorithm.T}

# 
function MOI.set(
    model::Optimizer,
    ::LPAlgorithmType,
    lp_algorithm::Union{Nothing,LPAlgorithm.T},
)
    optionally_initialize_model_and_parameters!(model)

    if isnothing(lp_algorithm)
        model.parameters.lp_algorithm = LPAlgorithm.LP_ALGORITHM_UNSPECIFIED
    else
        # Glop for simplex & dual simplex, PDLP/Bisco for first order (and nothing for barrier)
        # The solvers above are Google first party solvers.
        if lp_algorithm == LPAlgorithm.LP_ALGORITHM_BARRIER
            @warn NON_GOOGLE_SOLVER_WARNING
        end

        model.parameters.lp_algorithm = lp_algorithm
    end

    return nothing
end

function MOI.get(model::Optimizer, ::LPAlgorithmType)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        return model.parameters.lp_algorithm
    end

    return nothing
end

MOI.supports(model::Optimizer, ::LPAlgorithmType) = true


struct Presolve <: MOI.AbstractOptimizerAttribute end
MOI.attribute_value_type(::Presolve) = Union{Nothing,Emphasis.T}

function MOI.set(model::Optimizer, ::Presolve, presolve::Union{Nothing,Emphasis.T})
    optionally_initialize_model_and_parameters!(model)

    if isnothing(presolve)
        model.parameters.presolve = Emphasis.EMPHASIS_UNSPECIFIED
    else
        model.parameters.presolve = presolve
    end

    return nothing
end

function MOI.get(model::Optimizer, ::Presolve)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        return model.parameters.presolve
    end

    return nothing
end

MOI.supports(model::Optimizer, ::Presolve) = true


struct Cuts <: MOI.AbstractOptimizerAttribute end
MOI.attribute_value_type(::Cuts) = Union{Nothing,Emphasis.T}

function MOI.set(model::Optimizer, ::Cuts, cuts::Union{Nothing,Emphasis.T})
    optionally_initialize_model_and_parameters!(model)

    if isnothing(cuts)
        model.parameters.cuts = Emphasis.EMPHASIS_UNSPECIFIED
    else
        model.parameters.cuts = cuts
    end

    return nothing
end

function MOI.get(model::Optimizer, ::Cuts)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        return model.parameters.cuts
    end

    return nothing
end

MOI.supports(model::Optimizer, ::Cuts) = true


struct Heuristics <: MOI.AbstractOptimizerAttribute end
MOI.attribute_value_type(::Heuristics) = Union{Nothing,Emphasis.T}

function MOI.set(model::Optimizer, ::Heuristics, heuristics::Union{Nothing,Emphasis.T})
    optionally_initialize_model_and_parameters!(model)

    if isnothing(heuristics)
        model.parameters.heuristics = Emphasis.EMPHASIS_UNSPECIFIED
    else
        model.parameters.heuristics = heuristics
    end

    return nothing
end

function MOI.get(model::Optimizer, ::Heuristics)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        return model.parameters.heuristics
    end

    return nothing
end

MOI.supports(model::Optimizer, ::Heuristics) = true

struct Scaling <: MOI.AbstractOptimizerAttribute end
MOI.attribute_value_type(::Scaling) = Union{Nothing,Emphasis.T}

function MOI.set(model::Optimizer, ::Scaling, scaling::Union{Nothing,Emphasis.T})
    optionally_initialize_model_and_parameters!(model)

    if isnothing(scaling)
        model.parameters.scaling = Emphasis.EMPHASIS_UNSPECIFIED
    else
        model.parameters.scaling = scaling
    end

    return nothing
end

function MOI.get(model::Optimizer, ::Scaling)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        return model.parameters.scaling
    end

    return nothing
end

MOI.supports(model::Optimizer, ::Scaling) = true


struct GscipParametersAttribute <: MOI.AbstractOptimizerAttribute end
MOI.attribute_value_type(::GscipParametersAttribute) = Union{Nothing,GScipParameters}

function MOI.set(
    model::Optimizer,
    ::GscipParametersAttribute,
    gscip_parameters::Union{Nothing,GScipParameters},
)
    optionally_initialize_model_and_parameters!(model)

    model.parameters.gscip = gscip_parameters

    return nothing
end

function MOI.get(model::Optimizer, ::GscipParametersAttribute)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        return model.parameters.gscip
    end

    return nothing
end

MOI.supports(model::Optimizer, ::GscipParametersAttribute) = true


struct GurobiParametersAttribute <: MOI.AbstractOptimizerAttribute end
MOI.attribute_value_type(::GurobiParametersAttribute) = Union{Nothing,GurobiParameters}

function MOI.set(
    model::Optimizer,
    ::GurobiParametersAttribute,
    gurobi_parameters::Union{Nothing,GurobiParameters},
)
    optionally_initialize_model_and_parameters!(model)

    model.parameters.gurobi = gurobi_parameters

    return nothing
end

function MOI.get(model::Optimizer, ::GurobiParametersAttribute)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        return model.parameters.gurobi
    end

    return nothing
end

MOI.supports(model::Optimizer, ::GurobiParametersAttribute) = true


struct GlopParametersAttribute <: MOI.AbstractOptimizerAttribute end
MOI.attribute_value_type(::GlopParametersAttribute) = Union{Nothing,GlopParameters}

function MOI.set(
    model::Optimizer,
    ::GlopParametersAttribute,
    glop_parameters::Union{Nothing,GlopParameters},
)
    optionally_initialize_model_and_parameters!(model)

    model.parameters.glop = glop_parameters

    return nothing
end

function MOI.get(model::Optimizer, ::GlopParametersAttribute)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        return model.parameters.glop
    end

    return nothing
end

MOI.supports(model::Optimizer, ::GlopParametersAttribute) = true


struct SatParametersAttribute <: MOI.AbstractOptimizerAttribute end
MOI.attribute_value_type(::SatParametersAttribute) = Union{Nothing,CpSatParameters}

function MOI.set(
    model::Optimizer,
    ::SatParametersAttribute,
    cp_sat_parameters::Union{Nothing,SatParameters},
)
    optionally_initialize_model_and_parameters!(model)

    model.parameters.cp_sat = cp_sat_parameters

    return nothing
end

function MOI.get(model::Optimizer, ::SatParametersAttribute)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        return model.parameters.cp_sat
    end

    return nothing
end

MOI.supports(model::Optimizer, ::SatParametersAttribute) = true


struct GlpkParametersAttribute <: MOI.AbstractOptimizerAttribute end
MOI.attribute_value_type(::GlpkParametersAttribute) = Union{Nothing,GlpkParameters}

function MOI.set(
    model::Optimizer,
    ::GlpkParametersAttribute,
    glpk_parameters::Union{Nothing,GlpkParameters},
)
    optionally_initialize_model_and_parameters!(model)

    model.parameters.glpk = glpk_parameters

    return nothing
end

function MOI.get(model::Optimizer, ::GlpkParametersAttribute)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        return model.parameters.glpk
    end

    return nothing
end

MOI.supports(model::Optimizer, ::GlpkParametersAttribute) = true


struct HighsOptionsAttribute <: MOI.AbstractOptimizerAttribute end
MOI.attribute_value_type(::HighsOptionsAttribute) = Union{Nothing,HighsOptions}

function MOI.set(
    model::Optimizer,
    ::HighsOptionsAttribute,
    highs_parameters::Union{Nothing,HighsOptions},
)
    optionally_initialize_model_and_parameters!(model)

    model.parameters.highs = highs_parameters

    return nothing
end

function MOI.get(model::Optimizer, ::HighsOptionsAttribute)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        return model.parameters.highs
    end

    return nothing
end

MOI.supports(model::Optimizer, ::HighsOptionsAttribute) = true

"""
Set the parameter of the model throught the `RawOptimizerAttribute`. All fields
are set as they appear in the `SolveParameters` struct. However, the fields that
are structs (the solver specific parameter structs) themselves have their
internal fields split by the `PARAM_SPLITTER` and the solver name. For example,
`gscip_parameters.preprocessing` should be passed as `gscip__preprocessing`.
"""
function MOI.set(model::Optimizer, param::MOI.RawOptimizerAttribute, value)
    optionally_initialize_model_and_parameters!(model)

    param_name = param.name
    if contains(param_name, PARAM_SPLITTER)
        solve_param, field = lowercase.(split(param_name, PARAM_SPLITTER))
        if solve_param == "sat"
            solve_param = "cp_sat"
        end

        if !haskey(PARAM_FIELD_NAME_TO_INSTANCE_DICT, solve_param)
            throw(
                ArgumentError(
                    "Unsupported parameter attribute: $param_name. Model has no support for \"$solve_param\"",
                ),
            )
        end

        solve_param_instance = getfield(model.parameters, Symbol(solve_param))

        # If the parameter is not set, set it to the default instance
        if isnothing(solve_param_instance)
            solve_param_instance = PARAM_FIELD_NAME_TO_INSTANCE_DICT[solve_param]
            setfield!(model.parameters, Symbol(solve_param), solve_param_instance)
        end

        # Attempt to set the parameter value
        setfield!(solve_param_instance, Symbol(field), value)
    else
        setfield!(model.parameters, Symbol(param_name), value)
    end

    return nothing
end

"""
Get the parameter of the model throught the `RawOptimizerAttribute`. All fields
are retrieved as they appear in the `SolveParameters` struct. However, the fields
that are structs (the solver specific parameter structs) themselves have their
internal fields split by the `PARAM_SPLITTER` and the solver name. For example,
`gscip_parameters.preprocessing` should be passed as `gscip__preprocessing`.
"""
function MOI.get(model::Optimizer, param::MOI.RawOptimizerAttribute)
    if !MOI.is_empty(model) && !isnothing(model.parameters)
        param_name = param.name

        if contains(param_name, PARAM_SPLITTER)
            solve_param, field = lowercase.(split(param_name, PARAM_SPLITTER))

            if solve_param == "sat"
                solve_param = "cp_sat"
            end

            solve_param_instance = getfield(model.parameters, Symbol(solve_param))

            if isnothing(solve_param_instance)
                throw(
                    error(
                        "cannot get field from parameter \"$solve_param\" as instance is currently set to \"nothing\"",
                    ),
                )
            end

            return getfield(solve_param_instance, Symbol(field))
        else
            return getfield(model.parameters, Symbol(param_name))
        end
    end

    throw(error("Either your model or parameters is empty."))
end

MOI.supports(model::Optimizer, param::MOI.RawOptimizerAttribute) = true

function MOI.get(model::Optimizer, ::MOI.ListOfModelAttributesSet)
    if MOI.is_empty(model)
        return []
    end

    model_attributes_set = []

    F = MOI.get(model, MOI.ObjectiveFunctionType())
    if !isnothing(F)
        push!(model_attributes_set, MOI.ObjectiveFunction{F}())
    end

    objective_sense = MOI.get(model, MOI.ObjectiveSense())
    push!(model_attributes_set, MOI.ObjectiveSense())

    model_name = MOI.get(model, MOI.Name())
    if !isempty(model_name)
        push!(model_attributes_set, MOI.Name())
    end

    return model_attributes_set
end


"""

  Variable overrides

"""
function MOI.add_variable(model::Optimizer)
    optionally_initialize_model_and_parameters!(model)

    # initialize variables is they were set to Nothing
    if isnothing(model.model.variables)
        # update the model
        model.model.variables = Variables()
    end

    # add a new variable to the model
    variable_index = length(model.model.variables.ids) + 1
    push!(model.model.variables.ids, variable_index)
    push!(model.model.variables.lower_bounds, -Inf)
    push!(model.model.variables.upper_bounds, Inf)
    push!(model.model.variables.integers, false)
    push!(model.model.variables.names, "")

    return MOI.VariableIndex(variable_index)
end


function MOI.get(model::Optimizer, ::MOI.ListOfVariableIndices)
    if !MOI.is_empty(model)
        return MOI.VariableIndex.(model.model.variables.ids)
    end

    return nothing
end


function MOI.get(model::Optimizer, ::MOI.NumberOfVariables)
    if !MOI.is_empty(model)
        return length(model.model.variables.ids)
    end

    return 0
end

function MOI.is_valid(model::Optimizer, vi::MOI.VariableIndex)
    if !MOI.is_empty(model)
        return 1 <= vi.value <= MOI.get(model, MOI.NumberOfVariables())
    end

    return false
end


MOI.supports(::Optimizer, ::MOI.VariableName, ::Type{MOI.VariableIndex}) = true

function MOI.set(model::Optimizer, ::MOI.VariableName, v::MOI.VariableIndex, name::String)
    model.model.variables.names[v.value] = name

    return nothing
end

function MOI.get(model::Optimizer, ::MOI.VariableName, v::MOI.VariableIndex)
    if !MOI.is_empty(model)
        return model.model.variables.names[v.value]
    end

    return ""
end

function MOI.get(model::Optimizer, ::Type{MOI.VariableIndex}, v::String)
    if !MOI.is_empty(model) && !isempty(v)
        variable_index = findfirst(x -> x == v, model.model.variables.names)
        if isnothing(variable_index)
            return nothing
        end

        return MOI.VariableIndex(variable_index)
    end

    return nothing
end


"""

  Constraints support

"""
function MOI.supports_constraint(
    ::Optimizer,
    ::Type{MOI.VariableIndex},
    ::Type{<:SCALAR_SET},
)
    return true
end

function MOI.add_constraint(
    model::Optimizer,
    vi::MOI.VariableIndex,
    c::S,
) where {S<:SCALAR_SET}
    if !MOI.is_empty(model)
        # Check if the variable is already bounded by a constraint.
        if S <: MOI.LessThan
            throw_if_upper_bound_is_already_set(model, vi, c)
        end

        if S <: MOI.GreaterThan
            throw_if_lower_bound_is_already_set(model, vi, c)
        end

        if S <: MOI.EqualTo
            throw_if_upper_bound_is_already_set(model, vi, c)
            throw_if_lower_bound_is_already_set(model, vi, c)
        end

        if S <: MOI.Interval
            throw_if_upper_bound_is_already_set(model, vi, c)
            throw_if_lower_bound_is_already_set(model, vi, c)
        end

        # Get the int value of the variable index
        index = vi.value

        # retrieve the constraint bounds
        lower_bound, upper_bound = bounds(c)

        # set the bounds on the Variable
        model.model.variables.lower_bounds[index] = lower_bound
        model.model.variables.upper_bounds[index] = upper_bound

        # update the associated metadata.
        push!(model.constraint_types_present, (MOI.VariableIndex, typeof(c)))
        push!(
            model.constraint_indices_dict[SCALAR_SET_WITH_VARIABLE_INDEX_CONSTRAINT_KEY],
            MOI.ConstraintIndex{typeof(vi),typeof(c)}(index),
        )

        return MOI.ConstraintIndex{typeof(vi),typeof(c)}(index)
    end

    return nothing
end


function throw_if_upper_bound_is_already_set(
    model::Optimizer,
    vi::MOI.VariableIndex,
    c::S,
) where {S<:SCALAR_SET}
    # Assumes type consistency across all constraints.
    T = typeof(c).parameters[1]
    less_than_idx = MOI.ConstraintIndex{typeof(vi),MOI.LessThan{T}}(vi.value)
    interval_idx = MOI.ConstraintIndex{typeof(vi),MOI.Interval{T}}(vi.value)
    equal_to_idx = MOI.ConstraintIndex{typeof(vi),MOI.EqualTo{T}}(vi.value)

    if in(
        less_than_idx,
        model.constraint_indices_dict[SCALAR_SET_WITH_VARIABLE_INDEX_CONSTRAINT_KEY],
    )
        throw(MOI.UpperBoundAlreadySet{MOI.LessThan{T},S}(vi))
    end

    if in(
        interval_idx,
        model.constraint_indices_dict[SCALAR_SET_WITH_VARIABLE_INDEX_CONSTRAINT_KEY],
    )
        throw(MOI.UpperBoundAlreadySet{MOI.Interval{T},S}(vi))
    end

    if in(
        equal_to_idx,
        model.constraint_indices_dict[SCALAR_SET_WITH_VARIABLE_INDEX_CONSTRAINT_KEY],
    )
        throw(MOI.UpperBoundAlreadySet{MOI.EqualTo{T},S}(vi))
    end

    return nothing
end

function throw_if_lower_bound_is_already_set(
    model::Optimizer,
    vi::MOI.VariableIndex,
    c::S,
) where {S<:SCALAR_SET}
    # Assumes type consistency across all constraints.
    T = typeof(c).parameters[1]
    greater_than_idx = MOI.ConstraintIndex{typeof(vi),MOI.GreaterThan{T}}(vi.value)
    interval_idx = MOI.ConstraintIndex{typeof(vi),MOI.Interval{T}}(vi.value)
    equal_to_idx = MOI.ConstraintIndex{typeof(vi),MOI.EqualTo{T}}(vi.value)

    if in(
        greater_than_idx,
        model.constraint_indices_dict[SCALAR_SET_WITH_VARIABLE_INDEX_CONSTRAINT_KEY],
    )
        throw(MOI.LowerBoundAlreadySet{MOI.GreaterThan{T},S}(vi))
    end

    if in(
        interval_idx,
        model.constraint_indices_dict[SCALAR_SET_WITH_VARIABLE_INDEX_CONSTRAINT_KEY],
    )
        throw(MOI.LowerBoundAlreadySet{MOI.Interval{T},S}(vi))
    end

    if in(
        equal_to_idx,
        model.constraint_indices_dict[SCALAR_SET_WITH_VARIABLE_INDEX_CONSTRAINT_KEY],
    )
        throw(MOI.LowerBoundAlreadySet{MOI.EqualTo{T},S}(vi))
    end

    return nothing
end

function MOI.supports_constraint(
    ::Optimizer,
    ::Type{MOI.VariableIndex},
    ::Type{MOI.ZeroOne},
)
    return true
end

function MOI.add_constraint(model::Optimizer, vi::MOI.VariableIndex, c::MOI.ZeroOne)
    if !MOI.is_empty(model)
        # Get the int value of the variable index
        index = vi.value

        # Set the variable bounds
        model.model.variables.lower_bounds[index] = 0.0
        model.model.variables.upper_bounds[index] = 1.0

        # Mark it as being an integer
        model.model.variables.integers[index] = true

        # Update the associated metadata.
        push!(model.constraint_types_present, (MOI.VariableIndex, MOI.ZeroOne))
        push!(
            model.constraint_indices_dict[ZERO_ONE_CONSTRAINT_KEY],
            MOI.ConstraintIndex{typeof(vi),typeof(c)}(index),
        )

        return MOI.ConstraintIndex{typeof(vi),typeof(c)}(index)
    end

    return nothing
end


function MOI.supports_constraint(
    ::Optimizer,
    ::Type{MOI.VariableIndex},
    ::Type{MOI.Integer},
)
    return true
end

function MOI.add_constraint(model::Optimizer, vi::MOI.VariableIndex, c::MOI.Integer)
    if !MOI.is_empty(model)
        # Get the int value of the variable index
        index = vi.value

        # Set the bounds
        model.model.variables.lower_bounds[index] = Float64(typemin(Int))
        model.model.variables.upper_bounds[index] = Float64(typemax(Int))

        # Mark it as being an integer
        model.model.variables.integers[index] = true

        # Update the associated metadata.
        push!(model.constraint_types_present, (MOI.VariableIndex, MOI.Integer))
        push!(
            model.constraint_indices_dict[INTEGER_CONSTRAINT_KEY],
            MOI.ConstraintIndex{typeof(vi),typeof(c)}(index),
        )

        return MOI.ConstraintIndex{typeof(vi),typeof(c)}(index)
    end

    return nothing
end

# Helper function to create a dictionary of terms to combine coefficients 
# of terms(variables) if they are repeated.
# For example: 3x + 5x <= 10 will be combined to 8x <= 10.
function get_terms_dict(
    terms::Vector{MOI.ScalarAffineTerm{T}},
)::Dict{Int64,Float64} where {T<:Real}
    terms_dict = Dict{Int64,Float64}()

    for term in terms
        if !haskey(terms_dict, term.variable.value)
            terms_dict[term.variable.value] = term.coefficient
        else
            terms_dict[term.variable.value] += term.coefficient
        end
    end

    return terms_dict
end

function MOI.add_constraint(
    model::Optimizer,
    f::MOI.ScalarAffineFunction{T},
    c::SCALAR_SET,
) where {T<:Real}
    if !MOI.is_empty(model)
        # Ensure that the constant is zero else throw an error.
        iszero(f.constant) ||
            throw(MOI.ScalarFunctionConstantNotZero{T,typeof(f),typeof(c)}(f.constant))

        # Retrieve the terms from `f`
        terms = f.terms

        constraint_index = length(model.model.linear_constraints.ids) + 1
        lower_bound, upper_bound = bounds(c)

        # Update the LinearConstraintProto
        push!(model.model.linear_constraints.ids, constraint_index)
        push!(model.model.linear_constraints.lower_bounds, lower_bound)
        push!(model.model.linear_constraints.upper_bounds, upper_bound)
        push!(model.model.linear_constraints.names, "")

        terms_dict = get_terms_dict(terms)

        # Update the LinearConstaintMatrix (SparseDoubleVectorProto)
        # linear_constraint_matrix.row_ids are elements of linear_constraints.ids.
        # linear_constraint_matrix.column_ids are elements of variables.ids.
        # Matrix entries not specified are zero.
        # linear_constraint_matrix.coefficients must all be finite.
        for term_index in keys(terms_dict)
            push!(model.model.linear_constraint_matrix.row_ids, constraint_index)
            push!(model.model.linear_constraint_matrix.column_ids, term_index)
            push!(model.model.linear_constraint_matrix.coefficients, terms_dict[term_index])
        end

        # Update the associated metadata.
        push!(model.constraint_types_present, (MOI.ScalarAffineFunction{T}, typeof(c)))
        push!(
            model.constraint_indices_dict[SCALAR_SET_WITH_SCALAR_FUNCTION_CONSTRAINT_KEY],
            MOI.ConstraintIndex{typeof(f),typeof(c)}(constraint_index),
        )

        return MOI.ConstraintIndex{typeof(f),typeof(c)}(constraint_index)
    end

    return nothing
end

function MOI.supports_constraint(
    ::Optimizer,
    ::Type{MOI.ScalarAffineFunction{T}},
    ::Type{<:SCALAR_SET},
) where {T<:Real}
    return true
end


function MOI.get(model::Optimizer, ::MOI.ListOfConstraintTypesPresent)
    if !MOI.is_empty(model)
        return collect(model.constraint_types_present)
    end

    return Set{Tuple{Type,Type}}[]
end


function MOI.get(
    model::Optimizer,
    ::MOI.ListOfConstraintIndices{MOI.VariableIndex,S},
) where {S<:SCALAR_SET}
    if !MOI.is_empty(model)
        return sort!(
            model.constraint_indices_dict[SCALAR_SET_WITH_VARIABLE_INDEX_CONSTRAINT_KEY],
            by = x -> x.value,
        )
    end

    return []
end


function MOI.get(
    model::Optimizer,
    ::MOI.ListOfConstraintIndices{MOI.ScalarAffineFunction{T},S},
) where {T<:Real,S<:SCALAR_SET}
    if !MOI.is_empty(model)
        return sort!(
            model.constraint_indices_dict[SCALAR_SET_WITH_SCALAR_FUNCTION_CONSTRAINT_KEY],
            by = x -> x.value,
        )
    end

    return []
end

function MOI.get(
    model::Optimizer,
    ::MOI.ListOfConstraintIndices{MOI.VariableIndex,MOI.ZeroOne},
)
    if !MOI.is_empty(model)
        return sort!(
            model.constraint_indices_dict[ZERO_ONE_CONSTRAINT_KEY],
            by = x -> x.value,
        )
    end

    return []
end

function MOI.get(
    model::Optimizer,
    ::MOI.ListOfConstraintIndices{MOI.VariableIndex,MOI.Integer},
)
    if !MOI.is_empty(model)
        return sort!(
            model.constraint_indices_dict[INTEGER_CONSTRAINT_KEY],
            by = x -> x.value,
        )
    end

    return []
end

function MOI.get(model::Optimizer, ::MOI.NumberOfConstraints{F,S}) where {F,S}
    if !MOI.is_empty(model)
        return length(MOI.get(model, MOI.ListOfConstraintIndices{F,S}()))
    end

    return 0
end

function MOI.get(
    model::Optimizer,
    ::MOI.ConstraintFunction,
    c::MOI.ConstraintIndex{MOI.VariableIndex,Any},
)
    if !MOI.is_empty(model)
        return MOI.VariableIndex(c.value)
    end

    # TODO: Replace with another error
    throw(ArgumentError("ConstraintIndex $(c.value) is not valid for this model."))
end

function MOI.set(
    ::Optimizer,
    ::MOI.ConstraintFunction,
    ::MOI.ConstraintIndex{MOI.VariableIndex,<:Any},
    ::MOI.VariableIndex,
)
    throw(MOI.SettingVariableIndexNotAllowed())
end

function MOI.get(
    model::Optimizer,
    ::MOI.ConstraintFunction,
    c::MOI.ConstraintIndex{MOI.ScalarAffineFunction{T},S},
) where {T<:Real,S<:SCALAR_SET}
    if !MOI.is_empty(model)
        # Convert the linear constraint to the ScalarAffineFunction
        # Return all indices that match the constraint index.
        column_indices =
            findall(x -> x == c.value, model.model.linear_constraint_matrix.row_ids)

        terms = MOI.ScalarAffineTerm{T}[]

        for column_index in column_indices
            push!(
                terms,
                MOI.ScalarAffineTerm{T}(
                    model.model.linear_constraint_matrix.coefficients[column_index],
                    MOI.VariableIndex(
                        model.model.linear_constraint_matrix.column_ids[column_index],
                    ),
                ),
            )
        end

        return MOI.ScalarAffineFunction{T}(terms, zero(Float64))
    end

    return nothing
end


function MOI.set(
    model::Optimizer,
    ::MOI.ConstraintFunction,
    c::MOI.ConstraintIndex{MOI.ScalarAffineFunction{T},S},
    f::MOI.ScalarAffineFunction{T},
) where {T<:Real,S<:SCALAR_SET}
    optionally_initialize_model_and_parameters!(model)

    if !iszero(f.constant)
        throw(MOI.ScalarFunctionConstantNotZero(f.constant))
    end

    previous_fn = MOI.get(model, MOI.ConstraintFunction, c)

    if isnothing(previous_fn)
        throw(ArgumentError("ConstraintIndex $(c.value) is not valid for this model."))
    end

    # Clear the column_ids and coefficients associated with the constraint index.
    # Get indices that match the constraint index.
    associated_indices =
        findall(x -> x == c.value, model.model.linear_constraint_matrix.row_ids)

    # Clear the associated indices in the column ids and coefficients.
    deleteat!(model.model.linear_constraint_matrix.row_ids, associated_indices)
    deleteat!(model.model.linear_constraint_matrix.column_ids, associated_indices)
    deleteat!(model.model.linear_constraint_matrix.coefficients, associated_indices)

    # Retrieve the terms from `f`
    terms = f.terms

    if length(terms) == 0
        throw(
            ArgumentError(
                "ScalarAffineFunction has no terms. You need to specify at least one term.",
            ),
        )
    end

    terms_dict = get_terms_dict(terms)

    for term_index in keys(terms_dict)
        push!(model.model.linear_constraint_matrix.row_ids, c.value)
        push!(model.model.linear_constraint_matrix.column_ids, term_index)
        push!(model.model.linear_constraint_matrix.coefficients, terms_dict[term_index])
    end

    return nothing
end


function MOI.get(
    model::Optimizer,
    ::MOI.ConstraintSet,
    c::MOI.ConstraintIndex{MOI.VariableIndex,MOI.LessThan{T}},
) where {T<:Real}
    if !MOI.is_empty(model)
        # Retrieve the upper bound
        return MOI.LessThan{T}(mode.model.variables.upper_bounds[c.value])
    end

    return nothing
end

function MOI.get(
    model::Optimizer,
    ::MOI.ConstraintSet,
    c::MOI.ConstraintIndex{MOI.VariableIndex,MOI.GreaterThan{T}},
) where {T<:Real}
    if !MOI.is_empty(model)
        # Retrieve the lower bound
        return MOI.GreaterThan{T}(model.model.variables.lower_bounds[c.value])
    end

    return nothing
end

function MOI.get(
    model::Optimizer,
    ::MOI.ConstraintSet,
    c::MOI.ConstraintIndex{MOI.VariableIndex,MOI.Interval{T}},
) where {T<:Real}
    if !MOI.is_empty(model)
        # Retrieve the lower and upper bounds
        return MOI.Interval{T}(
            model.model.variables.lower_bounds[c.value],
            model.model.variables.upper_bounds[c.value],
        )
    end

    return nothing
end

function MOI.get(
    model::Optimizer,
    ::MOI.ConstraintSet,
    c::MOI.ConstraintIndex{MOI.VariableIndex,MOI.EqualTo{T}},
) where {T<:Real}
    if !MOI.is_empty(model)
        # Retrieve the lower and upper bounds
        return MOI.EqualTo{T}(model.model.variables.lower_bounds[c.value])
    end

    return nothing
end

function MOI.get(
    model::Optimizer,
    ::MOI.ConstraintSet,
    c::MOI.ConstraintIndex{MOI.VariableIndex,MOI.Integer},
)
    if !MOI.is_empty(model)
        return MOI.Integer()
    end

    return nothing
end

function MOI.get(
    model::Optimizer,
    ::MOI.ConstraintSet,
    c::MOI.ConstraintIndex{MOI.VariableIndex,MOI.ZeroOne},
)
    if !MOI.is_empty(model)
        return MOI.ZeroOne()
    end

    return nothing
end

function MOI.get(
    model::Optimizer,
    ::MOI.ConstraintSet,
    c::MOI.ConstraintIndex{MOI.ScalarAffineFunction{T},MOI.GreaterThan{T}},
) where {T<:Real}
    if !MOI.is_empty(model)
        return MOI.GreaterThan{T}(model.model.linear_constraints.lower_bounds[c.value])
    end

    return nothing
end


function MOI.get(
    model::Optimizer,
    ::MOI.ConstraintSet,
    c::MOI.ConstraintIndex{MOI.ScalarAffineFunction{T},MOI.LessThan{T}},
) where {T<:Real}
    if !MOI.is_empty(model)
        return MOI.LessThan{T}(model.model.linear_constraints.upper_bounds[c.value])
    end

    return nothing
end


function MOI.get(
    model::Optimizer,
    ::MOI.ConstraintSet,
    c::MOI.ConstraintIndex{MOI.ScalarAffineFunction{T},MOI.EqualTo{T}},
) where {T<:Real}
    if !MOI.is_empty(model)
        return MOI.EqualTo{T}(model.model.linear_constraints.lower_bounds[c.value])
    end

    return nothing
end

function MOI.get(
    model::Optimizer,
    ::MOI.ConstraintSet,
    c::MOI.ConstraintIndex{MOI.ScalarAffineFunction{T},MOI.Interval{T}},
) where {T<:Real}
    if !MOI.is_empty(model)
        return MOI.Interval{T}(
            model.model.linear_constraints.lower_bounds[c.value],
            model.model.linear_constraints.upper_bounds[c.value],
        )
    end

    return nothing
end

function MOI.set(
    model::Optimizer,
    ::MOI.ConstraintSet,
    c::MOI.ConstraintIndex{MOI.VariableIndex,S},
    s::S,
) where {S<:SCALAR_SET}
    optionally_initialize_model_and_parameters!(model)

    lower_bound, upper_bound = bounds(s)

    model.model.variables.lower_bounds[c.value] = lower_bound
    model.model.variables.upper_bounds[c.value] = upper_bound

    return nothing
end

function MOI.set(
    model::Optimizer,
    ::MOI.ConstraintSet,
    c::MOI.ConstraintIndex{MOI.ScalarAffineFunction{T},S},
    s::S,
) where {T<:Real,S<:SCALAR_SET}
    optionally_initialize_model_and_parameters!(model)

    lower_bound, upper_bound = bounds(s)

    model.model.linear_constraints.lower_bounds[c.value] = lower_bound
    model.model.linear_constraints.upper_bounds[c.value] = upper_bound

    return nothing
end


function MOI.supports(
    ::Optimizer,
    ::MOI.ConstraintName,
    ::Type{<:MOI.ConstraintIndex{MOI.ScalarAffineFunction{T},<:SCALAR_SET}},
) where {T<:Real}
    return true
end

function MOI.get(
    model::Optimizer,
    ::MOI.ConstraintName,
    c::MOI.ConstraintIndex{MOI.ScalarAffineFunction{T},<:Any},
) where {T<:Real}
    if !MOI.is_empty(model)
        return model.model.linear_constraints.names[c.value]
    end

    return ""
end

function MOI.set(
    model::Optimizer,
    ::MOI.ConstraintName,
    c::MOI.ConstraintIndex{MOI.ScalarAffineFunction{T},<:Any},
    name::String,
) where {T<:Real}
    in(name, model.model.linear_constraints.names) &&
        throw(ErrorException("Constraint with name \"$(name)\" already exists."))
    model.model.linear_constraints.names[c.value] = name

    return nothing
end

function MOI.get(
    model::Optimizer,
    ::Type{MOI.ConstraintIndex{MOI.ScalarAffineFunction{T},S}},
    constraint_name::String,
) where {T<:Real,S<:SCALAR_SET}
    if !MOI.is_empty(model)
        position_index =
            findfirst(x -> x == constraint_name, model.model.linear_constraints.names)
        if position_index == nothing
            return nothing
        end
        constraint_index = MOI.ConstraintIndex{MOI.ScalarAffineFunction{T},S}(
            model.model.linear_constraints.ids[position_index],
        )

        in(
            constraint_index,
            model.constraint_indices_dict[SCALAR_SET_WITH_SCALAR_FUNCTION_CONSTRAINT_KEY],
        ) && return constraint_index
    end

    return nothing
end

function MOI.is_valid(
    model::Optimizer,
    c::MOI.ConstraintIndex{MOI.ScalarAffineFunction{T},S},
) where {T<:Real,S<:SCALAR_SET}
    if !MOI.is_empty(model)
        return in(
            c,
            model.constraint_indices_dict[SCALAR_SET_WITH_SCALAR_FUNCTION_CONSTRAINT_KEY],
        )
    end

    return false
end

function MOI.is_valid(
    model::Optimizer,
    c::MOI.ConstraintIndex{MOI.VariableIndex,MOI.ZeroOne},
)
    if !MOI.is_empty(model)
        return in(c, model.constraint_indices_dict[ZERO_ONE_CONSTRAINT_KEY])
    end

    return false
end


function MOI.is_valid(
    model::Optimizer,
    c::MOI.ConstraintIndex{MOI.VariableIndex,MOI.Integer},
)
    if !MOI.is_empty(model)
        return in(c, model.constraint_indices_dict[INTEGER_CONSTRAINT_KEY])
    end

    return false
end

function MOI.is_valid(
    model::Optimizer,
    c::MOI.ConstraintIndex{MOI.VariableIndex,S},
) where {S<:SCALAR_SET}
    if !MOI.is_empty(model)
        return in(
            c,
            model.constraint_indices_dict[SCALAR_SET_WITH_VARIABLE_INDEX_CONSTRAINT_KEY],
        )
    end

    return false
end


function MOI.get(::Optimizer, ::MOI.ListOfConstraintAttributesSet{F,S}) where {F,S}
    attributes = MOI.AbstractConstraintAttribute[]
    if F != MOI.VariableIndex
        return push!(attributes, MOI.ConstraintName())
    end
    return attributes
end

"""

  Objective Function

"""
function MOI.set(model::Optimizer, ::MOI.ObjectiveSense, sense::MOI.OptimizationSense)
    optionally_initialize_model_and_parameters!(model)

    if sense == MOI.MAX_SENSE
        optionally_initialize_objective!(model)
        model.model.objective.maximize = true
    elseif sense == MOI.MIN_SENSE
        optionally_initialize_objective!(model)
        model.model.objective.maximize = false
    else
        # ObjectiveFunction is not considered. This is a feasibility problem.
        model.model.objective = nothing
        model.objective_set = false
    end
end

function MOI.get(model::Optimizer, ::MOI.ObjectiveSense)
    if !MOI.is_empty(model) && !isnothing(model.model.objective)
        model.objective_set &&
            return model.model.objective.maximize ? MOI.MAX_SENSE : MOI.MIN_SENSE
    end

    return MOI.FEASIBILITY_SENSE
end

MOI.supports(model::Optimizer, ::MOI.ObjectiveSense) = true


function MOI.get(model::Optimizer, ::MOI.ObjectiveFunctionType)
    if !MOI.is_empty(model) && !isnothing(model.model.objective)
        if iszero(model.model.objective.offset) &&
           length(model.model.objective.linear_coefficients.ids) == 1 &&
           length(model.model.objective.quadratic_coefficients.row_ids) == 0
            return MOI.VariableIndex
        elseif length(model.model.objective.quadratic_coefficients.row_ids) > 0
            # TODO: b/386359419 Add support for quadratic objectives
            return MOI.ScalarQuadraticFunction{Real}
            # elseif length(model.model.objective.linear_coefficients.ids) > 0
            #     return MOI.ScalarAffineFunction{Real}
        else
            # throw(error("Failed to get objective function type; no objective function found."))
            # The above has been commented out in favor of returning the ScalarAffineFunction
            # by default. This function is being used in other tests and was throwing an error resulting
            # in test failures. Other solvers, such as HiGHS seem to be returning the ScalarAffineFunction
            # by default.
            return MOI.ScalarAffineFunction{Real}
        end
    end

    return nothing
end

function reset_objective!(model::Optimizer)::Nothing
    model.model.objective.offset = zero(Float64)
    model.model.objective.linear_coefficients = NewSparseDoubleVector()
    model.model.objective.quadratic_coefficients = NewSparseDoubleMatrix()
    model.model.objective.name = ""
    model.model.objective.priority = zero(Int)

    return nothing
end

# For example: Maximize `x`
function MOI.set(
    model::Optimizer,
    ::MOI.ObjectiveFunction{MOI.VariableIndex},
    objective_function::MOI.VariableIndex,
)
    optionally_initialize_model_and_parameters!(model)
    optionally_initialize_objective!(model)

    # `zero out` the existing objective function
    reset_objective!(model)

    model.objective_set = true

    push!(model.model.objective.linear_coefficients.ids, objective_function.value)
    push!(model.model.objective.linear_coefficients.values, one(Float64))

    return nothing
end

function MOI.get(model::Optimizer, ::MOI.ObjectiveFunction{MOI.VariableIndex})
    if !MOI.is_empty(model) && !isnothing(model.model.objective)
        return MOI.VariableIndex(model.model.objective.linear_coefficients.ids[1])
    end

    return nothing
end

function MOI.set(
    model::Optimizer,
    ::MOI.ObjectiveFunction{MOI.ScalarAffineFunction{T}},
    objective_function::MOI.ScalarAffineFunction{T},
) where {T<:Real}
    optionally_initialize_model_and_parameters!(model)
    optionally_initialize_objective!(model)

    # `zero out` the existing objective function
    reset_objective!(model)

    model.objective_set = true

    terms = objective_function.terms

    terms_dict = get_terms_dict(terms)

    for term in keys(terms_dict)
        push!(model.model.objective.linear_coefficients.ids, term)
        push!(model.model.objective.linear_coefficients.values, terms_dict[term])
    end

    model.model.objective.offset = Float64(objective_function.constant)

    return nothing
end

function MOI.get(
    model::Optimizer,
    ::MOI.ObjectiveFunction{MOI.ScalarAffineFunction{T}},
) where {T<:Real}
    if !MOI.is_empty(model) && !isnothing(model.model.objective)
        scalar_affine_terms = map(
            (linear_coefficient) -> MOI.ScalarAffineTerm{T}(
                linear_coefficient[1],
                MOI.VariableIndex(linear_coefficient[2]),
            ),
            zip(
                model.model.objective.linear_coefficients.values,
                model.model.objective.linear_coefficients.ids,
            ),
        )
        return MOI.ScalarAffineFunction{T}(
            scalar_affine_terms,
            model.model.objective.offset,
        )
    end

    return nothing
end


# TODO: b/386359419 Add support for quadratic objectives
function MOI.set(
    model::Optimizer,
    ::MOI.ObjectiveFunction{T},
    objective_function::MOI.ScalarQuadraticFunction{T},
) where {T<:Real}
    optionally_initialize_model_and_parameters!(model)
    optionally_initialize_objective!(model)

    return nothing
end

function MOI.supports(
    ::Optimizer,
    ::MOI.ObjectiveFunction{F},
) where {
    F<:Union{
        MOI.VariableIndex,
        MOI.ScalarAffineFunction{<:Real},
        MOI.ScalarQuadraticFunction{<:Real},
    },
}
    return true
end

function MOI.supports(
    ::Optimizer,
    ::MOI.ObjectiveFunction{F},
) where {F<:MOI.ScalarQuadraticFunction{<:Real}}
    return true
end

function MOI.optimize!(model::Optimizer)
    # TODO: b/384662497 implement this
    return nothing
end
