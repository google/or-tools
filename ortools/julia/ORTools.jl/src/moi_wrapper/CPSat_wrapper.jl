"""
MOI implementation for the CP-Sat solver
"""

"""
  CPSATOptimizer()

  An instance of a CPSATOptimizer.
"""
mutable struct CPSATOptimizer <: MOI.AbstractOptimizer
    model::Union{Nothing,CpModel}
    parameters::Union{Nothing,SatParameters}
    # This structure is updated by the optimize! function.
    solve_response::Union{Nothing,CpSolverResponse}

    function CPSATOptimizer(; name::String = "")
        model = CpModel(name = name)
        parameters = SatParameters()

        new(model, parameters, nothing)
    end
end

function MOI.get(optimizer::CPSATOptimizer, ::MOI.SolverName)
    return string(SolverType.SOLVER_TYPE_CP_SAT)
end

# TODO: b/448345078- Revisit this once versioning has been agreed upon.
function MOI.get(model::CPSATOptimizer, ::MOI.SolverVersion)
    return "1.0.0-DEV"
end

function MOI.empty!(optimizer::CPSATOptimizer)
    optimizer.model = CpModel()
    optimizer.solve_response = nothing

    return nothing
end

function MOI.is_empty(optimizer::CPSATOptimizer)
    return isempty(optimizer.model) && isnothing(optimizer.solve_response)
end

function Base.isempty(model::CpModel)
    # A model with default size is considered empty.
    # TODO: (b/452115117) - inline the encoded_model_size for defaule CpModel.
    return isnothing(model) || encoded_model_size(model) == encoded_model_size(CpModel())
end

function MOI.get(optimizer::CPSATOptimizer, ::MOI.Name)
    return optimizer.model.name
end

function MOI.set(optimizer::CPSATOptimizer, ::MOI.Name, name::String)
    optimizer.model.name = name

    return nothing
end

MOI.supports(optimizer::CPSATOptimizer, ::MOI.Name) = true

function MOI.get(optimizer::CPSATOptimizer, ::MOI.Silent)
    return !model.parameters.log_search_progress
end

function MOI.set(optimizer::CPSATOptimizer, ::MOI.Silent, silent::Bool)
    model.parameters.log_search_progress = !silent

    return nothing
end

MOI.supports(optimizer::CPSATOptimizer, ::MOI.Silent) = true

function MOI.get(optimizer::CPSATOptimizer, ::MOI.TimeLimitSec)
    return optimizer.parameters.max_time_in_seconds
end

function MOI.set(
    model::CPSATOptimizer,
    ::MOI.TimeLimitSec,
    time_limit::Union{Nothing,Float64},
)
    optimizer.parameters.max_time_in_seconds = time_limit

    return nothing
end

MOI.supports(optimizer::CPSATOptimizer, ::MOI.TimeLimitSec) = true

function MOI.get(optimizer::CPSATOptimizer, ::MOI.NumberOfThreads)
    number_of_threads = model.parameters.num_workers
    return number_of_threads == 0 ? nothing : number_of_threads
end

function MOI.set(
    optimizer::CPSATOptimizer,
    ::MOI.NumberOfThreads,
    number_of_threads::Union{Nothing,Int},
)
    model.parameters.num_workers =
        isnothing(number_of_threads) ? zero(Int32) : Int32(number_of_threads)

    return nothing
end

MOI.supports(optimizer::CPSATOptimizer, ::MOI.NumberOfThreads) = true

function MOI.get(optimizer::CPSATOptimizer, ::MOI.AbsoluteGapTolerance)
    return optimizer.parameters.absolute_gap_limit
end

function MOI.set(
    optimizer::CPSATOptimizer,
    ::MOI.AbsoluteGapTolerance,
    absolute_gap_tolerance::Union{Nothing,Real},
)
    optimizer.parameters.absolute_gap_limit =
        isnothing(absolute_gap_tolerance) ? zero(Float64) : Float64(absolute_gap_tolerance)

    return nothing
end

MOI.supports(optimizer::CPSATOptimizer, ::MOI.AbsoluteGapTolerance) = true

function MOI.get(optimizer::CPSATOptimizer, ::MOI.RelativeGapTolerance)
    return optimizer.parameters.relative_gap_limit
end

function MOI.set(
    optimizer::CPSATOptimizer,
    ::MOI.RelativeGapTolerance,
    relative_gap_tolerance::Union{Nothing,Real},
)
    optimizer.parameters.relative_gap_limit =
        isnothing(relative_gap_tolerance) ? zero(Float64) : Float64(relative_gap_tolerance)

    return nothing
end

MOI.supports(optimizer::CPSATOptimizer, ::MOI.RelativeGapTolerance) = true

function MOI.set(optimizer::CPSATOptimizer, param::MOI.RawOptimizerAttribute, value)
    setfield!(optimizer.parameters, Symbol(param.name), value)
end

function MOI.get(optimizer::CPSATOptimizer, param::MOI.RawOptimizerAttribute)
    return getfield!(optimizer.parameters, Symbol(param.name))
end

MOI.supports(model::Optimizer, param::MOI.RawOptimizerAttribute) = true


"""

    Variable overrides.

"""

function MOI.add_variable(optimizer::CPSATOptimizer)
    push!(optimizer.model.variables, IntegerVariable())

    return MOI.VariableIndex(length(optimizer.model.variables))
end

function MOI.get(optimizer::CPSATOptimizer, ::MOI.ListOfVariableIndices)
    return collect(1:length(optimizer.model.variables))
end

function MOI.get(optimizer::CPSATOptimizer, ::MOI.NumberOfVariables)
    return length(optimizer.model.variables)
end

function MOI.is_valid(optimizer::CPSATOptimizer, vi::MOI.VariableIndex)
    return 1 <= vi.value <= MOI.get(optimizer, MOI.NumberOfVariables())
end

MOI.supports(::CPSATOptimizer, ::MOI.VariableName, ::Type{MOI.VariableIndex}) = true

function MOI.set(
    optimizer::CPSATOptimizer,
    ::MOI.VariableName,
    v::MOI.VariableIndex,
    name::String,
)
    optimizer.model.variables[v.value].name = name

    return nothing
end

function MOI.get(optimizer::CPSATOptimizer, ::MOI.VariableName, v::MOI.VariableIndex)
    if MOI.is_valid(optimizer, v)
        return optimizer.model.variables[v.value].name
    end

    return ""
end

function MOI.get(optimizer::CPSATOptimizer, ::Type{MOI.VariableIndex}, name::String)
    variable_index = findfirst(x -> x.name == name, model.model.variables)
    if !isnothing(variable_index)
        return MOI.VariableIndex(variable_index)
    end

    return nothing
end
