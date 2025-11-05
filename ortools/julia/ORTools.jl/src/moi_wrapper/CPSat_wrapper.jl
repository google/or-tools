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
    # Set of constraints in variable indices
    variables_constraints::Set{MOI.ConstraintIndex}
    constraint_types_present::Set{Tuple{Type,Type}}
    # This structure is updated by the optimize! function.
    solve_response::Union{Nothing,CpSolverResponse}

    function CPSATOptimizer(; name::String = "")
        model = CpModel(name = name)
        parameters = SatParameters()
        variables_constraints = Set{MOI.ConstraintIndex}()
        constraint_types_present = Set{Tuple{Type,Type}}()

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
    optimizer.variables_constraints = Set{MOI.ConstraintIndex}()
    optimizer.constraint_types_present = Set{Tuple{Type,Type}}()
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


"""

    Constraint overrides.

"""

function MOI.supports_constraint(
    ::CPSATOptimizer,
    ::Type{MOI.VariableIndex},
    ::Type{<:SCALAR_SET},
)
    return true
end

function MOI.add_constraint(
    optimizer::CPSATOptimizer,
    vi::MOI.VariableIndex,
    c::S,
) where {S<:SCALAR_SET}
    if S <: MOI.LessThan
        throw_if_upper_bound_is_already_set(optimizer, vi, c)
    end

    if S <: MOI.GreaterThan
        throw_if_lower_bound_is_already_set(optimizer, vi, c)
    end

    if S <: MOI.Interval
        throw_if_upper_bound_is_already_set(optimizer, vi, c)
        throw_if_lower_bound_is_already_set(optimizer, vi, c)
    end

    if S <: MOI.EqualTo
        throw_if_upper_bound_is_already_set(optimizer, vi, c)
        throw_if_lower_bound_is_already_set(optimizer, vi, c)
    end

    variable_index = vi.value

    # retrieve the constraint bounds
    lower_bound, upper_bound = bounds(c)
    
    # TODO: (b/452908268) - Update variable's domain instead of creating a new linear constraint.
    linear_constraint = CPSatLinearConstraintProto()

    push!(linear_constraint.vars, variable_index)
    # In this case, we set the coefficient to 1.
    push!(linear_constraint.coeffs, 1)
    # Update the domain
    push!(linear_constraint.domain, lower_bound)
    push!(linear_constraint.domain, upper_bound)

    constraint = CPSATConstraint()
    constraint.name = :linear
    constraint.value = linear_constraint

    push!(optimizer.model.constraints, constraint)

    push!(
        optimizer.variables_constraints,
        MOI.ConstraintIndex{MOI.VariableIndex,typeof(c)}(variable_index),
    )
    push!(optimizer.constraint_types_present, (MOI.VariableIndex, typeof(c)))

    return MOI.ConstraintIndex{MOI.VariableIndex,typeof(c)}(variable_index)
end

function throw_if_upper_bound_is_already_set(
    optimizer::CPSATOptimizer,
    vi::MOI.VariableIndex,
    c::S,
) where {S<:SCALAR_SET}
    # Assumes type consistency across all constraints.
    T = typeof(c).parameters[1]
    less_than_idx = MOI.ConstraintIndex{MOI.VariableIndex,MOI.LessThan{T}}(vi.value)
    interval_idx = MOI.ConstraintIndex{MOI.VariableIndex,MOI.Interval{T}}(vi.value)
    equal_to_idx = MOI.ConstraintIndex{MOI.VariableIndex,MOI.EqualTo{T}}(vi.value)

    if in(less_than_idx, optimizer.variables_constraints)
        throw(MOI.UpperBoundAlreadySet{MOI.LessThan{T},S}(vi))
    end

    if in(interval_idx, optimizer.variables_constraints)
        throw(MOI.UpperBoundAlreadySet{MOI.Interval{T},S}(vi))
    end

    if in(equal_to_idx, optimizer.variables_constraints)
        throw(MOI.UpperBoundAlreadySet{MOI.EqualTo{T},S}(vi))
    end

    return nothing
end

function throw_if_lower_bound_is_already_set(
    optimizer::CPSATOptimizer,
    vi::MOI.VariableIndex,
    c::S,
) where {S<:SCALAR_SET}
    # Assumes type consistency across all constraints.
    T = typeof(c).parameters[1]
    greater_than_idx = MOI.ConstraintIndex{typeof(vi),MOI.GreaterThan{T}}(vi.value)
    interval_idx = MOI.ConstraintIndex{typeof(vi),MOI.Interval{T}}(vi.value)
    equal_to_idx = MOI.ConstraintIndex{typeof(vi),MOI.EqualTo{T}}(vi.value)

    if in(greater_than_idx, optimizer.variables_constraints)
        throw(MOI.LowerBoundAlreadySet{MOI.GreaterThan{T},S}(vi))
    end

    if in(interval_idx, optimizer.variables_constraints)
        throw(MOI.LowerBoundAlreadySet{MOI.Interval{T},S}(vi))
    end

    if in(equal_to_idx, optimizer.variables_constraints)
        throw(MOI.LowerBoundAlreadySet{MOI.EqualTo{T},S}(vi))
    end

    return nothing
end

function MOI.supports_constraint(
    ::CPSATOptimizer,
    ::Type{MOI.VariableIndex},
    ::Type{MOI.ZeroOne},
)
    return true
end

function MOI.add_constraint(
    optimizer::CPSATOptimizer,
    vi::MOI.VariableIndex,
    c::MOI.ZeroOne,
)
    # Get the int value of the variable index
    index = vi.value

    # Set the variable bounds to [0, 1] (override the existing domain)
    optimizer.model.variables[index].domain = [zero(Int), one(Int)]

    # Update the associated metadata.
    # TODO: (b/452416646) - Fetch constraint types dynamically at runtime
    push!(optimizer.constraint_types_present, (MOI.VariableIndex, MOI.ZeroOne))
    push!(
        optimizer.variables_constraints,
        MOI.ConstraintIndex{MOI.VariableIndex,MOI.ZeroOne}(index),
    )

    return MOI.ConstraintIndex{MOI.VariableIndex,MOI.ZeroOne}(index)
end
