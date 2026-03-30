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
    linear_expressions_constraints::Set{MOI.ConstraintIndex}
    boolean_argument_constraints::Set{MOI.ConstraintIndex}
    linear_argument_constraints::Set{MOI.ConstraintIndex}
    global_constraints::Set{MOI.ConstraintIndex}
    constraint_types_present::Set{Tuple{Type,Type}}
    # This structure is updated by the optimize! function.
    solve_response::Union{Nothing,CpSolverResponse}

    function CPSATOptimizer(; name::String = "")
        model = CpModel(name = name)
        parameters = SatParameters()
        variables_constraints = Set{MOI.ConstraintIndex}()
        linear_expressions_constraints = Set{MOI.ConstraintIndex}()
        boolean_argument_constraints = Set{MOI.ConstraintIndex}()
        linear_argument_constraints = Set{MOI.ConstraintIndex}()
        global_constraints = Set{MOI.ConstraintIndex}()
        constraint_types_present = Set{Tuple{Type,Type}}()

        new(
            model,
            parameters,
            variables_constraints,
            linear_expressions_constraints,
            boolean_argument_constraints,
            linear_argument_constraints,
            global_constraints,
            constraint_types_present,
            nothing,
        )
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
    optimizer.linear_expressions_constraints = Set{MOI.ConstraintIndex}()
    optimizer.boolean_argument_constraints = Set{MOI.ConstraintIndex}()
    optimizer.linear_argument_constraints = Set{MOI.ConstraintIndex}()
    optimizer.global_constraints = Set{MOI.ConstraintIndex}()
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

MOI.supports(model::CPSATOptimizer, param::MOI.RawOptimizerAttribute) = true


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
    linear_constraint = NewCPSatLinearConstraint()

    push!(linear_constraint.vars, variable_index)
    # In this case, we set the coefficient to 1.
    push!(linear_constraint.coeffs, 1)
    # Update the domain
    push!(linear_constraint.domain, lower_bound)
    push!(linear_constraint.domain, upper_bound)

    constraint = CPSATConstraint()
    constraint.constraint = (; linear = linear_constraint)

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

    if in(less_than_idx, optimizer.variables_constraints) && S <: MOI.LessThan
        throw(MOI.UpperBoundAlreadySet{MOI.LessThan{T},S}(vi))
    end

    if in(interval_idx, optimizer.variables_constraints) && S <: MOI.Interval
        throw(MOI.UpperBoundAlreadySet{MOI.Interval{T},S}(vi))
    end

    if in(equal_to_idx, optimizer.variables_constraints) && S <: MOI.EqualTo
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

    if in(greater_than_idx, optimizer.variables_constraints) && S <: MOI.GreaterThan
        throw(MOI.LowerBoundAlreadySet{MOI.GreaterThan{T},S}(vi))
    end

    if in(interval_idx, optimizer.variables_constraints) && S <: MOI.Interval
        throw(MOI.LowerBoundAlreadySet{MOI.Interval{T},S}(vi))
    end

    if in(equal_to_idx, optimizer.variables_constraints) && S <: MOI.EqualTo
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

function MOI.supports_constraint(
    ::CPSATOptimizer,
    ::Type{MOI.VariableIndex},
    ::Type{MOI.Integer},
)
    return true
end

# TODO: (b/452914572) - Add support for MOI.add_constrained_variable.
function MOI.add_constraint(
    optimizer::CPSATOptimizer,
    vi::MOI.VariableIndex,
    c::MOI.Integer,
)
    # Get the int value of the variable index
    index = vi.value

    # Internally, the domain of each variable is a vector of integers.
    # Update the associated metadata.
    push!(optimizer.constraint_types_present, (MOI.VariableIndex, MOI.Integer))
    push!(
        optimizer.variables_constraints,
        MOI.ConstraintIndex{MOI.VariableIndex,MOI.Integer}(index),
    )

    return MOI.ConstraintIndex{MOI.VariableIndex,MOI.Integer}(index)
end

function MOI.add_constraint(
    optimizer::CPSATOptimizer,
    f::MOI.ScalarAffineFunction{T},
    c::SCALAR_SET,
) where {T<:Real}
    # Ensure that the constant is zero else throw an error.
    MOI.throw_if_scalar_and_constant_not_zero(f, typeof(c))

    # Retrieve the terms from `f`
    terms = f.terms

    constraint_index = length(optimizer.model.constraints) + 1
    lower_bound, upper_bound = bounds(c)

    linear_constraint = NewCPSatLinearConstraint()
    # Update the domain
    push!(linear_constraint.domain, lower_bound)
    push!(linear_constraint.domain, upper_bound)

    terms_pairs = get_terms_pairs(terms)

    for (variable_index, coefficient) in terms_pairs
        push!(linear_constraint.vars, variable_index)
        push!(linear_constraint.coeffs, coefficient)
    end

    constraint = CPSATConstraint()
    constraint.constraint = (; linear = linear_constraint)

    push!(optimizer.model.constraints, constraint)

    # Update the associated metadata.
    push!(optimizer.constraint_types_present, (MOI.ScalarAffineFunction{T}, typeof(c)))
    push!(
        optimizer.linear_expressions_constraints,
        MOI.ConstraintIndex{MOI.ScalarAffineFunction{T},typeof(c)}(constraint_index),
    )

    return MOI.ConstraintIndex{MOI.ScalarAffineFunction{T},typeof(c)}(constraint_index)
end

function MOI.supports_constraint(
    ::CPSATOptimizer,
    ::Type{MOI.ScalarAffineFunction{T}},
    ::Type{<:SCALAR_SET},
) where {T<:Real}
    return true
end

function MOI.get(optimizer::CPSATOptimizer, ::MOI.ListOfConstraintTypesPresent)
    return collect(optimizer.constraint_types_present)
end

function MOI.get(
    optimizer::CPSATOptimizer,
    ::MOI.ListOfConstraintIndices{MOI.VariableIndex,S},
) where {S<:Union{MOI.ZeroOne,MOI.Integer,<:SCALAR_SET}}
    return sort!(optimizer.variables_constraints, by = x -> x.value)
end

function MOI.get(
    optimizer::CPSATOptimizer,
    ::MOI.ListOfConstraintIndices{MOI.ScalarAffineFunction{T},S},
) where {T<:Real,S<:SCALAR_SET}
    return sort!(optimizer.linear_expressions_constraints, by = x -> x.value)
end

function MOI.get(optimizer::CPSATOptimizer, ::MOI.NumberOfConstraints{F,S}) where {F,S}
    return length(MOI.get(optimizer, MOI.ListOfConstraintIndices{F,S}()))
end

function MOI.get(
    optimizer::CPSATOptimizer,
    ::MOI.ConstraintFunction,
    c::MOI.ConstraintIndex{MOI.VariableIndex,<:Any},
)
    if !MOI.is_empty(optimizer)
        return MOI.VariableIndex(c.value)
    end

    throw(ArgumentError("ConstraintIndex $(c.value) is not valid for this model."))
end

function MOI.set(
    ::CPSATOptimizer,
    ::MOI.ConstraintFunction,
    ::MOI.ConstraintIndex{MOI.VariableIndex,<:Any},
    ::MOI.VariableIndex,
)
    throw(MOI.SettingVariableIndexNotAllowed())
end

function MOI.get(
    optimizer::CPSATOptimizer,
    ::MOI.ConstraintFunction,
    c::MOI.ConstraintIndex{MOI.ScalarAffineFunction{T},S},
) where {T<:Real,S<:SCALAR_SET}
    if MOI.is_empty(optimizer)
        return nothing
    end
    # Convert the linear constraint to the ScalarAffineFunction
    # Return all indices that match the constraint index.
    linear_constraint = optimizer.model.constraints[c.value].value

    terms = MOI.ScalarAffineTerm{T}[]

    for index in linear_constraint.vars
        push!(
            terms,
            MOI.ScalarAffineTerm{T}(
                linear_constraint.coeffs[index],
                MOI.VariableIndex(linear_constraint.vars[index]),
            ),
        )
    end

    return MOI.ScalarAffineFunction{T}(terms, zero(Int))
end

function MOI.set(
    optimizer::CPSATOptimizer,
    ::MOI.ConstraintFunction,
    c::MOI.ConstraintIndex{MOI.ScalarAffineFunction{T},S},
    f::MOI.ScalarAffineFunction{T},
) where {T<:Real,S<:SCALAR_SET}
    MOI.throw_if_scalar_and_constant_not_zero(f, typeof(c))

    # Retrieve the terms from `f`
    terms = f.terms

    if length(terms) == 0
        throw(
            ArgumentError(
                "ScalarAffineFunction has no terms. You need to specify at least one term.",
            ),
        )
    end

    previous_fn = MOI.get(model, MOI.ConstraintFunction, c)

    if isnothing(previous_fn)
        throw(ArgumentError("There is no constraint with this index: $(c.value)."))
    end

    linear_constraint = optimizer.model.constraints[c.value].value

    # Clear the associated indices in the column ids and coefficients.
    empty!(linear_constraint.vars)
    empty!(linear_constraint.coeffs)

    terms_pairs = get_terms_pairs(terms)

    for (variable_index, coefficient) in term_pairs
        push!(linear_constraint.vars, variable_index)
        push!(linear_constraint.coeffs, coefficient)
    end

    return nothing
end

"""
    Abstract type for Boolean argument constraints.
"""
abstract type BoolArgumentConstraint <: MOI.AbstractVectorSet end

function MOI.supports_constraint(
    ::CPSATOptimizer,
    ::Type{MOI.VectorOfVariables},
    ::Type{BoolArgumentConstraint},
)
    return true
end

"""
    BoolOr(dimension::Int)

The bool_or constraint forces at least one literal to be true.

This constraint is a special case of `MOI.CountAtLeast`. It's equivalent to this:

    MOI.CountAtLeast(1, [d], Set([1]))
"""
struct BoolOr <: BoolArgumentConstraint
    dimension::Int

    BoolOr(dimension::Int) = new(dimension)
end

# The tag used to identify the constraint type in CPSAT's ConstraintProto.
constraint_tag(::BoolOr) = :bool_or

"""
    BoolAnd(dimension::Int)

The bool_and constraint forces all of the literals to be true.
"""
struct BoolAnd <: BoolArgumentConstraint
    dimension::Int

    BoolAnd(dimension::Int) = new(dimension)
end

# The tag used to identify the constraint type in CPSAT's ConstraintProto.
constraint_tag(::BoolAnd) = :bool_and

"""
    BoolXor(dimension::Int)

The bool_xor constraint forces an odd number of the literals to be true.
"""
struct BoolXor <: BoolArgumentConstraint
    dimension::Int

    BoolXor(dimension::Int) = new(dimension)
end

constraint_tag(::BoolXor) = :bool_xor

"""
    AtMostOne(dimension::Int)

The at_most_one constraint enforces that no more than one literal is
true at the same time.
"""
struct AtMostOne <: BoolArgumentConstraint
    dimension::Int

    AtMostOne(dimension::Int) = new(dimension)
end

# The tag used to identify the constraint type in CPSAT's ConstraintProto.
constraint_tag(::AtMostOne) = :at_most_one

"""
    ExactlyOne(dimension::Int)

The exactly_one constraint enforces that exactly one literal is
true at the same time.
"""
struct ExactlyOne <: BoolArgumentConstraint
    dimension::Int

    ExactlyOne(dimension::Int) = new(dimension)
end

# The tag used to identify the constraint type in CPSAT's ConstraintProto.
constraint_tag(::ExactlyOne) = :exactly_one

function MOI.add_constraint(
    optimizer::CPSATOptimizer,
    vi::MOI.VectorOfVariables,
    c::S,
) where {S<:BoolArgumentConstraint}
    if c.dimension != length(vi.variables)
        throw(
            ArgumentError(
                "The dimension of the constraint must match the number of variables.",
            ),
        )
    end

    constraint_index = length(optimizer.model.constraints) + 1

    literals = Int32.(map(v -> v.value, vi.variables))
    bool_argument_constraint = BoolArgument(literals)

    constraint = CPSATConstraint()
    constraint.constraint = NamedTuple{(constraint_tag(c),)}((bool_argument_constraint,))

    push!(optimizer.model.constraints, constraint)

    # Update the associated metadata.
    push!(optimizer.constraint_types_present, (MOI.VectorOfVariables, typeof(c)))
    push!(
        optimizer.boolean_argument_constraints,
        MOI.ConstraintIndex{MOI.VectorOfVariables,typeof(c)}(constraint_index),
    )

    return MOI.ConstraintIndex{MOI.VectorOfVariables,typeof(c)}(constraint_index)
end


"""
    Added support for the `LinearArgumentProto` constraint.
"""
abstract type LinearArgumentConstraint <: MOI.AbstractVectorSet end

function MOI.supports_constraint(
    ::CPSATOptimizer,
    ::Type{MOI.VectorOfVariables},
    ::Type{<:LinearArgumentConstraint},
)
    return true
end

# A LinearExpression object can be derived from a ScalarAffineFunction or from
# a VariableIndex.
function build_linear_expression(
    f::MOI.ScalarAffineFunction{T},
)::CPSatLinearExpression where {T<:Int}
    MOI.throw_if_scalar_and_constant_not_zero(f, typeof(c))

    terms = f.terms
    terms_pairs = get_terms_pairs(terms)

    linear_expression = CPSatLinearExpression()

    for (variable_index, coefficient) in terms_pairs
        push!(linear_expression.vars, variable_index)
        push!(linear_expression.coeffs, coefficient)
    end

    linear_expression.offset = f.offset

    return linear_expression
end

function build_linear_expression(vi::MOI.VariableIndex)::CPSatLinearExpression
    linear_expression = CPSatLinearExpression()
    push!(linear_expression.vars, vi.value)
    push!(linear_expression.coeffs, 1)

    return linear_expression
end

function build_linear_expressions(vi::MOI.VectorOfVariables)::Vector{CPSatLinearExpression}
    return build_linear_expression.(vi.variables)
end

function build_linear_expressions(
    vi::MOI.VectorOfVariables,
    start_index::Int,
    end_index::Int,
)::Vector{CPSatLinearExpression}
    return build_linear_expression.(vi.variables[start_index:end_index])
end

function build_linear_expressions(
    vi::MOI.VectorAffineFunction,
)::Vector{CPSatLinearExpression}
    return build_linear_expression.(MOI.scalar_function.(vi, 1:MOI.output_dimension(vi)))
end

function build_linear_expressions(
    vi::MOI.VectorAffineFunction,
    start_index::Int,
    end_index::Int,
)::Vector{CPSatLinearExpression}
    return build_linear_expression.(MOI.scalar_function.(vi, start_index:end_index))
end

function get_variable_indices(
    vi::MOI.VectorOfVariables,
    start_index::Int,
    end_index::Int,
)::Vector{Int32}
    return Int32.(map(v -> v.value, vi.variables[start_index:end_index]))
end

"""
    Get the variable indices from a VectorAffineFunction.
    This checks if each of the scalar functions have a co-efficient of 1 and a constant of 0.
    The number of terms in each scalar function is 1, for example 1*x.

    If any of the ScalarFunctions in the range is not of the form 1*x + 0, then an error is thrown.
"""
function get_variable_indices(
    vi::MOI.VectorAffineFunction,
    start_index::Int,
    end_index::Int,
)::Vector{Int32}
    variable_indices = Vector{Int32}(undef, end_index - start_index + 1)

    for i = start_index:end_index
        if MOI.scalar_function(vi, i).constant != 0 ||
           length(MOI.scalar_function(vi, i).terms) != 1 ||
           MOI.scalar_function(vi, i).terms[1].coefficient != 1
            throw(
                ArgumentError(
                    "The ScalarFunction at index $(i) is not of the form 1*x + 0.",
                ),
            )
        end
        variable_indices[i-start_index+1] =
            MOI.scalar_function(vi, i).terms[1].variable.value
    end

    return variable_indices
end

"""
    DivisionEquality(dimension::Int)

The DivisionEquality constraint forces the target to equal exprs[0] / exprs[1].
"""
struct DivisionEquality <: LinearArgumentConstraint
    dimension::Int
    target::Union{MOI.VariableIndex,MOI.ScalarAffineFunction{Int}}

    # The dimension is always set to 2.
    DivisionEquality(target::Union{MOI.VariableIndex,MOI.ScalarAffineFunction{Int}}) =
        new(2, target)
end

function MOI.add_constraint(
    optimizer::CPSATOptimizer,
    vi::Union{MOI.VectorOfVariables,MOI.VectorAffineFunction},
    c::DivisionEquality,
)
    if c.dimension != MOI.output_dimension(vi)
        throw(
            ArgumentError(
                "A 2D vector function comprising of variables or linear/affine expressions is required for the DivisionEquality constraint.",
            ),
        )
    end

    target_expr = build_linear_expression(c.target)
    expr_0, expr_1 = build_linear_expressions(vi)

    int_div_linear_argument = LinearArgument(target_expr, [expr_0, expr_1])

    constraint_index = length(optimizer.model.constraints) + 1

    constraint = CPSATConstraint()
    constraint.constraint = (; int_div = int_div_linear_argument)

    push!(optimizer.model.constraints, constraint)

    # Update the associated metadata.
    push!(optimizer.constraint_types_present, (MOI.VectorOfVariables, typeof(c)))
    push!(
        optimizer.linear_argument_constraints,
        MOI.ConstraintIndex{MOI.VectorOfVariables,typeof(c)}(constraint_index),
    )

    return MOI.ConstraintIndex{MOI.VectorOfVariables,typeof(c)}(constraint_index)
end

"""
    ModuloEquality(dimension::Int, target::Union{MOI.VariableIndex,MOI.ScalarAffineFunction{Int}})

The ModuloEquality constraint constraint forces the target to equal exprs[0] % exprs[1].
"""
struct ModuloEquality <: LinearArgumentConstraint
    dimension::Int
    target::Union{MOI.VariableIndex,MOI.ScalarAffineFunction{Int}}

    # The dimension is always set to 2.
    ModuloEquality(target::Union{MOI.VariableIndex,MOI.ScalarAffineFunction{Int}}) =
        new(2, target)
end

function MOI.add_constraint(
    optimizer::CPSATOptimizer,
    vi::Union{MOI.VectorOfVariables,MOI.VectorAffineFunction},
    c::ModuloEquality,
)
    if c.dimension != MOI.output_dimension(vi)
        throw(
            ArgumentError(
                "A 2D vector function comprising of variables or linear/affine expressions is required for the ModuloEquality constraint.",
            ),
        )
    end

    target_expr = build_linear_expression(c.target)
    expr_0, expr_1 = build_linear_expressions(vi)

    int_mod_linear_argument = LinearArgument(target_expr, [expr_0, expr_1])

    constraint_index = length(optimizer.model.constraints) + 1

    constraint = CPSATConstraint()
    constraint.constraint = (; int_mod = int_mod_linear_argument)

    push!(optimizer.model.constraints, constraint)

    # Update the associated metadata.
    push!(optimizer.constraint_types_present, (MOI.VectorOfVariables, typeof(c)))
    push!(
        optimizer.linear_argument_constraints,
        MOI.ConstraintIndex{typeof(vi),typeof(c)}(constraint_index),
    )

    return MOI.ConstraintIndex{typeof(vi),typeof(c)}(constraint_index)
end

"""
    ProductEquality(dimension::Int, target::Union{MOI.VariableIndex,MOI.ScalarAffineFunction{Int}})

The ProductEquality constraint constraint forces the target to equal the product of all
variables.
"""
struct ProductEquality <: LinearArgumentConstraint
    dimension::Int
    target::Union{MOI.VariableIndex,MOI.ScalarAffineFunction{Int}}

    ProductEquality(
        dimenson::Int,
        target::Union{MOI.VariableIndex,MOI.ScalarAffineFunction{Int}},
    ) = new(dimension, target)
end

function MOI.add_constraint(
    optimizer::CPSATOptimizer,
    vi::Union{MOI.VectorOfVariables,MOI.VectorAffineFunction},
    c::ProductEquality,
)
    if c.dimension != MOI.output_dimension(vi)
        throw(
            ArgumentError(
                "Number of variables in the ProductEquality constraint must match the dimension.",
            ),
        )
    end

    target_expr = build_linear_expression(c.target)
    expressions = build_linear_expressions(vi)

    int_product_linear_argument = LinearArgument(target_expr, expressions)

    constraint_index = length(optimizer.model.constraints) + 1

    constraint = CPSATConstraint()
    constraint.constraint = (; int_prod = int_product_linear_argument)

    push!(optimizer.model.constraints, constraint)

    # Update the associated metadata.
    push!(optimizer.constraint_types_present, (MOI.VectorOfVariables, typeof(c)))
    push!(
        optimizer.linear_argument_constraints,
        MOI.ConstraintIndex{typeof(vi),typeof(c)}(constraint_index),
    )

    return MOI.ConstraintIndex{typeof(vi),typeof(c)}(constraint_index)
end

"""
    LinMax(dimension::Int)

The LinMax constraint constraint forces the target to equal the maximum of all
linear expressions.
"""
struct LinMax <: LinearArgumentConstraint
    dimension::Int

    function LinMax(dimenson::Int)
        dimension < 2 && throw(ArgumentError("Dimension must be at least 2."))
        new(dimension, target)
    end
end

function MOI.add_constraint(
    optimizer::CPSATOptimizer,
    vi::Union{MOI.VectorOfVariables,MOI.VectorAffineFunction},
    c::LinMax,
)
    if c.dimension != MOI.output_dimension(vi)
        throw(
            ArgumentError(
                "Number of variables in the LinMax constraint must match the dimension.",
            ),
        )
    end

    expressions = build_linear_expressions(vi)
    target_expr = expressions[1]

    lin_max_linear_argument = LinearArgument(target_expr, expressions[2:end])

    constraint_index = length(optimizer.model.constraints) + 1

    constraint = CPSATConstraint()
    constraint.constraint = (; lin_max = lin_max_linear_argument)

    push!(optimizer.model.constraints, constraint)

    # Update the associated metadata.
    push!(optimizer.constraint_types_present, (MOI.VectorOfVariables, typeof(c)))
    push!(
        optimizer.linear_argument_constraints,
        MOI.ConstraintIndex{typeof(vi),typeof(c)}(constraint_index),
    )

    return MOI.ConstraintIndex{typeof(vi),typeof(c)}(constraint_index)
end


"""

    Global constraints.

"""
function MOI.supports_constraint(
    ::CPSATOptimizer,
    ::Type{<:Union{MOI.VectorOfVariables,MOI.VectorAffineFunction}},
    ::Type{MOI.AllDifferent},
)
    return true
end

function MOI.add_constraint(
    optimizer::CPSATOptimizer,
    vi::Union{MOI.VectorOfVariables,MOI.VectorAffineFunction},
    c::MOI.AllDifferent,
)
    if c.dimension != MOI.output_dimension(vi)
        throw(
            ArgumentError(
                "Number of variables in the AllDifferent constraint must match the dimension.",
            ),
        )
    end

    constraint_index = length(optimizer.model.constraints) + 1

    linear_expressions = build_linear_expressions(vi)
    all_different_constraint = AllDifferentConstraint(exprs = linear_expressions)

    constraint = CPSATConstraint()
    constraint.constraint = (; all_diff = all_different_constraint)

    push!(optimizer.model.constraints, constraint)

    # Update the associated metadata.
    push!(optimizer.constraint_types_present, (MOI.VectorOfVariables, typeof(c)))
    push!(
        optimizer.global_constraints,
        MOI.ConstraintIndex{typeof(vi),typeof(c)}(constraint_index),
    )

    return MOI.ConstraintIndex{typeof(vi),typeof(c)}(constraint_index)
end


"""
    Circuit(edge_set::Vector{Tuple{Int, Int}})

The dimension of the Circuit constraint is equal to the number of edges in the
edge_set.
"""
struct Circuit <: MOI.AbstractVectorSet
    dimension::Int
    edge_set::Vector{Tuple{Int,Int}}

    Circuit(edge_set::Vector{Tuple{Int,Int}}) = new(length(edge_set), edge_set)
end

function MOI.supports_constraint(
    ::CPSATOptimizer,
    ::Type{MOI.VectorOfVariables},
    ::Type{Circuit},
)
    return true
end

"""
    The VectorOfVariables assumes that all the variables are Binary Variables.
    Make sure to apply the MOI.ZeroOne constraint to the variables before adding
    them to the VectorOfVariables.

    The length of the VectorOfVariables must match the dimension of the Circuit.
    In short, it should be equal to the size of the edge_set in the Circuit.

    TODO: b/493237559 - Add support for MOI.VectorAffineFunction.
    TODO: b/493240012 - Add check for MOI.ZeroOne constraint or verify that CP-Sat
    errors if MOI.ZeroOne constraint is not specified for variables.
"""
function MOI.add_constraint(
    optimizer::CPSATOptimizer,
    vi::MOI.VectorOfVariables,
    c::Circuit,
)
    if c.dimension != length(vi.variables)
        throw(
            ArgumentError(
                "The dimension of the Circuit constraint must match the number of variables.",
            ),
        )
    end

    constraint_index = length(optimizer.model.constraints) + 1

    circuit_constraint = NewCircuitConstraint()

    for i = 1:c.dimension
        push!(circuit_constraint.heads, c.edge_set[i][1])
        push!(circuit_constraint.tails, c.edge_set[i][2])
        push!(circuit_constraint.literals, vi.variables[i].value)
    end

    constraint = CPSATConstraint()
    constraint.constraint = (; circuit = circuit_constraint)

    push!(optimizer.model.constraints, constraint)

    # Update the associated metadata.
    push!(optimizer.constraint_types_present, (MOI.VectorOfVariables, typeof(c)))
    push!(
        optimizer.global_constraints,
        MOI.ConstraintIndex{MOI.VectorOfVariables,Circuit}(constraint_index),
    )

    return MOI.ConstraintIndex{typeof(vi),typeof(c)}(constraint_index)
end

function supports_constraint(
    ::CPSATOptimizer,
    ::Type{MOI.VectorOfVariables},
    ::Type{MOI.Circuit},
)
    return true
end

"""
    The parameter `vi` is a vector of Integer Variables upon which the Circuit
    constraint is applied.

    In this case too, the dimension of the Circuit constraint must match the number
    of variables in the vector of Integer Variables. It should be specifid as part
    of initializing the Circuit object.
"""
function MOI.add_constraint(
    optimizer::CPSATOptimizer,
    vi::MOI.VectorOfVariables,
    c::MOI.Circuit,
)
    if c.dimension != length(vi.variables)
        throw(
            ArgumentError(
                "The dimension of the Circuit constraint must match the number of variables.",
            ),
        )
    end

    circuit_arc_literals = []
    edge_set = Vector{Tuple{Int,Int}}()

    for i = 1:c.dimension
        # Outgoing edge literal for the current node `i`.
        outgoing_edge_literals = []
        for j = 1:c.dimension
            # Boolean variable to indicate if the arc `i` -> `j` is present in the circuit.
            b_ij = MOI.add_constrained_variable(optimizer, MOI.ZeroOne())[1]
            push!(outgoing_edge_literals, b_ij)
            push!(circuit_arc_literals, b_ij)
            push!(edge_set, (i, j))

            # We add a reified constraint to ensure that if v[i] = j, then b_ij is true. vi.variables[i]
            # NOTE: The variable is passed as a ScalarAffineFunction with a co-efficient 1.
            # This is a work-around; given that the variable is subjected to multiple EqualTo constraints within the loop,
            # dispatching the `add_constraint` to the same VariableIndex(i) will result in `throw_if_upper[lower]_bound_is_already_set`
            # error. Working with ScalarAffineFunction makes the constraint index independent of the VariableIndex(i); it is unique for each constraint.
            MOI.add_constraint(optimizer, 1 * vi.variables[i], MOI.EqualTo(j))
            # Update the added constraint's `enforcement literal`
            push!(optimizer.model.constraints[end].enforcement_literal, b_ij.value)
        end
        # Only one outgoing edge is allowed.
        # TODO: b/493245890 - Remove this constraint enforcement and a bridge between MOI.Circuit and CPSAT's Circuit constraint.
        MOI.add_constraint(
            optimizer,
            MOI.VectorOfVariables(outgoing_edge_literals),
            ExactlyOne(c.dimension),
        )
    end

    circuit_arc_literals_vector = MOI.VectorOfVariables(circuit_arc_literals)

    return MOI.add_constraint(optimizer, circuit_arc_literals_vector, Circuit(edge_set))
end

struct Element <: MOI.AbstractVectorSet
    dimension::Int

    Element(dimension::Int) = new(dimension)
end

function MOI.supports_constraint(
    ::CPSATOptimizer,
    ::Type{MOI.VectorOfVariables},
    ::Type{Element},
)
    return true
end

function MOI.add_constraint(
    optimizer::CPSATOptimizer,
    vars::Union{MOI.VectorOfVariables,MOI.VectorAffineFunction},
    c::Element,
)
    if c.dimension <= 2
        throw(
            ArgumentError(
                """
                The expression must have at least 3 variables. The expression to be indexed occupies the first n - 2 variables, where n is the dimension.
                Then the index and target variables are the last 2 variables the first variable as the index and the second variable as the target.
                """,
            ),
        )
    end

    if c.dimension != MOI.output_dimension(vars)
        throw(
            ArgumentError(
                "The dimension of the Element constraint must match the number of variables.",
            ),
        )
    end

    exprs = build_linear_expressions(vars, 1, c.dimension - 2)
    linear_index, linear_target =
        build_linear_expressions(vars, (c.dimension - 2) + 1, c.dimension)

    element_constraint = ElementConstraint(
        linear_index = linear_index,
        linear_target = linear_target,
        exprs = exprs,
    )

    constraint_index = length(optimizer.model.constraints) + 1

    constraint = CPSATConstraint()
    constraint.constraint = (; element = element_constraint)

    push!(optimizer.model.constraints, constraint)

    push!(optimizer.constraint_types_present, (typeof(exprs), Element))
    push!(
        optimizer.global_constraints,
        MOI.ConstraintIndex{typeof(exprs),Element}(constraint_index),
    )

    return MOI.ConstraintIndex{typeof(exprs),Element}(constraint_index)
end

struct Routes <: MOI.AbstractVectorSet
    dimension::Int
    tails::Vector{Int}
    heads::Vector{Int}
    # TODO: b/458704567 - Does not include Node Expressions. Will be included in Routing support.

    function Routes(tails::Vector{Int}, heads::Vector{Int})
        if length(tails) != length(heads)
            throw(ArgumentError("The number of tails and heads must match."))
        end
        new(length(tails), tails, heads)
    end
end

function MOI.supports_constraint(
    ::CPSATOptimizer,
    ::Type{MOI.VectorOfVariables},
    ::Type{Routes},
)
    return true
end


"""
    The VectorOfVariables should be a vector of Boolean/Binary Variables.
"""
function MOI.add_constraint(optimizer::CPSATOptimizer, vi::MOI.VectorOfVariables, c::Routes)
    if c.dimension != MOI.output_dimension(vi)
        throw(
            ArgumentError(
                "The dimension of the Routes constraint must match the number of variables.",
            ),
        )
    end

    variable_indices = map(v -> v.value, vi.variables)

    routes_constraint =
        RoutesConstraint(tails = c.tails, heads = c.heads, literals = variable_indices)

    constraint_index = length(optimizer.model.constraints) + 1

    constraint = CPSATConstraint()
    constraint.constraint = (; routes = routes_constraint)

    push!(optimizer.model.constraints, constraint)

    push!(optimizer.constraint_types_present, (MOI.VectorOfVariables, Routes))
    push!(
        optimizer.global_constraints,
        MOI.ConstraintIndex{MOI.VectorOfVariables,Routes}(constraint_index),
    )

    return MOI.ConstraintIndex{MOI.VectorOfVariables,Routes}(constraint_index)
end


function MOI.supports_constraint(
    ::CPSATOptimizer,
    ::Type{Union{MOI.VectorOfVariables,MOI.VectorAffineFunction}},
    ::Type{MOI.Table},
)
    return true
end

function MOI.add_constraint(
    optimizer::CPSATOptimizer,
    vi::Union{MOI.VectorOfVariables,MOI.VectorAffineFunction},
    c::MOI.Table,
)
    # Flatten the table to a vector that's Row-Major first.
    flattened_table = collect(c.table'[:])
    linear_expressions = build_linear_expressions(vi)

    table_constraint = TableConstraint(values = flattened_table, exprs = linear_expressions)
    constraint_index = length(optimizer.model.constraints) + 1

    constraint = CPSATConstraint()
    constraint.constraint = (; table = table_constraint)

    push!(optimizer.model.constraints, constraint)

    push!(optimizer.constraint_types_present, (typeof(vi), MOI.Table))
    push!(
        optimizer.global_constraints,
        MOI.ConstraintIndex{typeof(vi),MOI.Table}(constraint_index),
    )

    return MOI.ConstraintIndex{typeof(vi),MOI.Table}(constraint_index)
end

"""
    Table(table::Matrix{Int}, negated::Bool)

    Contains support for the `negated` field.
"""
struct Table <: MOI.AbstractVectorSet
    dimension::Int
    table::AbstractMatrix{Int}
    negated::Bool

    function Table(table::AbstractMatrix{Int}; negated::Bool = false)
        if ndims(table) != 2
            throw(ArgumentError("The table must be a 2D matrix."))
        end
        new(size(table)[2], table, negated)
    end
end

function MOI.supports_constraint(
    ::CPSATOptimizer,
    ::Type{Union{MOI.VectorOfVariables,MOI.VectorAffineFunction}},
    ::Type{Table},
)
    return true
end

function MOI.add_constraint(
    optimizer::CPSATOptimizer,
    vi::Union{MOI.VectorOfVariables,MOI.VectorAffineFunction},
    c::Table,
)
    if c.dimension != MOI.output_dimension(vi)
        throw(
            ArgumentError(
                "The number of columns of the Table constraint must match the number of variables.",
            ),
        )
    end

    # Flatten the table to a vector that's Row-Major first.
    flattened_table = collect(c.table'[:])
    linear_expressions = build_linear_expressions(vi)
    table_constraint = TableConstraint(
        values = flattened_table,
        exprs = linear_expressions,
        negated = c.negated,
    )
    constraint_index = length(optimizer.model.constraints) + 1

    constraint = CPSATConstraint()
    constraint.constraint = (; table = table_constraint)

    push!(optimizer.model.constraints, constraint)

    push!(optimizer.constraint_types_present, (typeof(vi), Table))
    push!(
        optimizer.global_constraints,
        MOI.ConstraintIndex{typeof(vi),Table}(constraint_index),
    )

    return MOI.ConstraintIndex{typeof(vi),Table}(constraint_index)
end

"""
    The `dimension` has to be defined as part of the Automaton object.
    It is the length of the input vector of variables.
"""
struct Automaton <: MOI.AbstractVectorSet
    dimension::Int
    starting_state::Int
    final_states::Vector{Int}

    # State transition matrix.
    transitions::AbstractMatrix{Int}

    function Automaton(
        dimension::Int,
        starting_state::Int,
        final_states::Vector{Int},
        transitions::AbstractMatrix{Int},
    )
        if ndims(transitions) != 2
            throw(ArgumentError("The transitions matrix must be a 2D matrix."))
        end

        if size(transitions)[2] != 3
            throw(
                ArgumentError(
                    "The transitions matrix must have 3 columns [from_state, to_state, token_value]",
                ),
            )
        end

        if isempty(final_states)
            throw(ArgumentError("The final states must not be empty."))
        end

        new(dimension, starting_state, final_states, transitions)
    end
end

function MOI.supports_constraint(
    ::CPSATOptimizer,
    ::Type{Union{MOI.VectorOfVariables,MOI.VectorAffineFunction}},
    ::Type{Automaton},
)
    return true
end

function MOI.add_constraint(
    optimizer::CPSATOptimizer,
    vi::Union{MOI.VectorOfVariables,MOI.VectorAffineFunction},
    c::Automaton,
)
    if c.dimension != MOI.output_dimension(vi)
        throw(
            ArgumentError(
                "The dimension of the Automaton constraint must match the number of variables.",
            ),
        )
    end

    automaton_constraint = AutomatonConstraint(
        starting_state = c.starting_state,
        final_states = c.final_states,
        transition_tail = c.transitions[:, 1],
        transition_head = c.transitions[:, 2],
        transition_label = c.transitions[:, 3],
        exprs = build_linear_expressions(vi),
    )

    constraint_index = length(optimizer.model.constraints) + 1

    constraint = CPSATConstraint()
    constraint.constraint = (; automaton = automaton_constraint)

    push!(optimizer.model.constraints, constraint)

    push!(optimizer.constraint_types_present, (typeof(vi), Automaton))
    push!(
        optimizer.global_constraints,
        MOI.ConstraintIndex{typeof(vi),Automaton}(constraint_index),
    )

    return MOI.ConstraintIndex{typeof(vi),Automaton}(constraint_index)
end

"""
    The `dimension` is the number of variables in the inverse function.
    This number should be the same as the dimension of the VectorOfVariables 
    upon which the Inverse constraint is applied.
"""
struct Inverse <: MOI.AbstractVectorSet
    dimension::Int

    function Inverse(dimension::Int)
        new(dimension)
    end
end

function MOI.supports_constraint(
    ::CPSATOptimizer,
    ::Type{MOI.VectorOfVariables},
    ::Type{Inverse},
)
    return true
end

function MOI.add_constraint(
    optimizer::CPSATOptimizer,
    vi::MOI.VectorOfVariables,
    c::Inverse,
)
    if c.dimension != MOI.output_dimension(vi)
        throw(
            ArgumentError(
                "The dimension of the Inverse constraint must match the number of variables.",
            ),
        )
    end

    if c.dimension % 2 == 1
        throw(
            ArgumentError(
                """The dimension of the Inverse constraint must be even; the first half should contain the direct function (variables) and
                the second half the inverse function (variables).
                """
            ),
        )
    end

    inverse_constraint = InverseConstraint()
    var_size = Int(c.dimension / 2)
    f_direct_variable_indices = Vector{Int}(undef, var_size)
    f_inverse_variable_indices = Vector{Int}(undef, var_size)

    for i = 1:var_size
        f_direct_variable_indices[i] = vi.variables[i].value
        f_inverse_variable_indices[i] = vi.variables[i + var_size].value
    end

    append!(inverse_constraint.f_direct, f_direct_variable_indices)
    append!(inverse_constraint.f_inverse, f_inverse_variable_indices)

    constraint_index = length(optimizer.model.constraints) + 1

    constraint = CPSATConstraint()
    constraint.constraint = (; inverse = inverse_constraint)

    push!(optimizer.model.constraints, constraint)

    push!(optimizer.constraint_types_present, (MOI.VectorOfVariables, Inverse))
    push!(
        optimizer.global_constraints,
        MOI.ConstraintIndex{MOI.VectorOfVariables,Inverse}(constraint_index),
    )

    return MOI.ConstraintIndex{MOI.VectorOfVariables,Inverse}(constraint_index)
end

"""
    The `dimension` is the number of variables in the reservoir function.
    This number should be the same as the dimension of the VectorOfVariables 
    upon which the Reservoir constraint is applied.
    
    When the `active_literals` is empty, the number of variables must be 
    2 * dimension in order to include active literals.

    When the `active_literals` is not empty, the number of variables must be 
    3 * dimension in order to include active literals.

    TODO: b/493267118 - split constraint into 2; one with active literals and one without.
"""
struct Reservoir <: MOI.AbstractVectorSet
    dimension::Int
    min_level::Int
    max_level::Int

    function Reservoir(dimension::Int, min_level::Int, max_level::Int)
        new(dimension, min_level, max_level)
    end
end

function MOI.supports_constraint(
    ::CPSATOptimizer,
    ::Type{Union{MOI.VectorOfVariables,MOI.VectorAffineFunction}},
    ::Type{Reservoir},
)
    return true
end

function MOI.add_constraint(
    optimizer::CPSATOptimizer,
    vi::Union{MOI.VectorOfVariables,MOI.VectorAffineFunction},
    c::Reservoir,
)
    if 2 * c.dimension != MOI.output_dimension(vi)
        if 3 * c.dimension != MOI.output_dimension(vi)
            throw(
                ArgumentError(
                    """
                    The number of variables must be 2 * dimension = $(2 * c.dimension) or
                    3 * dimension = $(3 * c.dimension) in order to include active literals.
                    The length of the variables passed is $(MOI.output_dimension(vi)). 
                    This does not meet any of the required conditions.
                    """,
                ),
            )
        end
    end

    exprs = build_linear_expressions(vi, 1, 2 * c.dimension)

    time_exprs = exprs[1:c.dimension]
    level_changes = exprs[(c.dimension+1):(2*c.dimension)]

    active_literals = []

    # Contains active literals.
    if 3 * c.dimension == MOI.output_dimension(vi)
        try
            active_literals = get_variable_indices(vi, 2 * c.dimension + 1, 3 * c.dimension)
        catch error
            throw(error)
        end
    end

    reservoir_constraint = ReservoirConstraint(
        min_level = c.min_level,
        max_level = c.max_level,
        time_exprs = time_exprs,
        level_changes = level_changes,
        active_literals = active_literals,
    )

    constraint_index = length(optimizer.model.constraints) + 1

    constraint = CPSATConstraint()
    constraint.constraint = (; reservoir = reservoir_constraint)

    push!(optimizer.model.constraints, constraint)

    push!(optimizer.constraint_types_present, (typeof(vi), Reservoir))
    push!(
        optimizer.global_constraints,
        MOI.ConstraintIndex{typeof(vi),Reservoir}(constraint_index),
    )

    return MOI.ConstraintIndex{typeof(vi),Reservoir}(constraint_index)
end

"""

  Objective Function Support

"""

# Like maximize `x`
function MOI.set(
    optimizer::CPSATOptimizer,
    ::MOI.ObjectiveFunction{MOI.VariableIndex},
    objective_function::MOI.VariableIndex,
)
    cp_objective = CpObjective(vars = [objective_function.value], coeffs = [Int64(1)])

    optimizer.model.objective = cp_objective

    return nothing
end

function MOI.get(optimizer::CPSATOptimizer, ::MOI.ObjectiveFunction{MOI.VariableIndex})
    if isnothing(optimizer.model.objective)
        return nothing
    end

    return MOI.VariableIndex(optimizer.model.objective.vars[1])
end

function MOI.supports(::CPSATOptimizer, ::MOI.ObjectiveFunction{MOI.VariableIndex})
    return true
end

function MOI.set(
    optimizer::CPSATOptimizer,
    ::MOI.ObjectiveFunction{MOI.ScalarAffineFunction{T}},
    objective_function::MOI.ScalarAffineFunction{T},
) where {T<:Integer}
    cp_objective = CpObjective()
    # Set the floating_point_objective to `nothing`.
    optimizer.model.floating_point_objective = nothing

    terms = objective_function.terms

    terms_pairs = get_terms_pairs(terms)

    for term in terms_pairs
        push!(cp_objective.vars, term[1])
        push!(cp_objective.coeffs, term[2])
    end

    cp_objective.offset = objective_function.constant

    optimizer.model.objective = cp_objective

    return nothing
end

function MOI.get(
    optimizer::CPSATOptimizer,
    ::MOI.ObjectiveFunction{MOI.ScalarAffineFunction{T}},
) where {T<:Integer}
    if isnothing(optimizer.model.objective)
        return nothing
    end

    scalar_affine_terms = map(
        i -> MOI.ScalarAffineTerm{T}(
            optimizer.model.objective.coeffs[i],
            MOI.VariableIndex(optimizer.model.objective.vars[i]),
        ),
        1:length(optimizer.model.objective.vars),
    )

    return MOI.ScalarAffineFunction{T}(
        scalar_affine_terms,
        optimizer.model.objective.offset,
    )
end

function MOI.supports(
    ::CPSATOptimizer,
    ::MOI.ObjectiveFunction{MOI.ScalarAffineFunction{T}},
) where {T<:Real}
    return true
end

function optionally_initialize_objective!(optimizer::CPSATOptimizer)::Nothing
    if isnothing(optimizer.model.objective)
        optimizer.model.objective = CpObjective()
    end

    return nothing
end

function MOI.set(
    optimizer::CPSATOptimizer,
    ::MOI.ObjectiveSense,
    sense::MOI.OptimizationSense,
)
    if sense == MOI.MAX_SENSE
        if !isnothing(optimizer.model.objective)
            optimizer.model.objective.scaling_factor = Float64(-1)
            # For ineger objectives, we also negate the coefficients.
            optimizer.model.objective.coeffs .= .-optimizer.model.objective.coeffs
        elseif !isnothing(optimizer.model.floating_point_objective)
            optimizer.model.floating_point_objective.maximize = true
        end
    elseif sense == MOI.MIN_SENSE
        if !isnothing(optimizer.model.objective)
            optimizer.model.objective.scaling_factor = Float64(1)
        elseif !isnothing(optimizer.model.floating_point_objective)
            optimizer.model.floating_point_objective.maximize = false
        end
    else
        # ObjectiveFunction is not considered. This is a feasibility problem.
        optimizer.model.objective = nothing
        optimizer.model.floating_point_objective = nothing
    end

    return nothing
end

function MOI.get(optimizer::CPSATOptimizer, ::MOI.ObjectiveSense)
    if !isnothing(optimizer.model.objective)
        return optimizer.model.objective.scaling_factor == Float64(-1) ? MOI.MAX_SENSE :
               MOI.MIN_SENSE
    elseif !isnothing(optimizer.model.floating_point_objective)
        return optimizer.model.floating_point_objective.maximize ? MOI.MAX_SENSE :
               MOI.MIN_SENSE
    else
        return MOI.FEASIBILITY_SENSE
    end
end

MOI.supports(optimizer::CPSATOptimizer, ::MOI.ObjectiveSense) = true

function MOI.get(optimizer::CPSATOptimizer, ::MOI.ObjectiveFunctionType)
    if !isnothing(optimizer.model.objective)
        # Check if the objective function is just a single variable
        if optimizer.model.objective.offset == Float64(0) &&
           length(optimizer.model.objective.coeffs) == 1 &&
           optimizer.model.objective.coeffs[1] == Int64(1) &&
           length(optimizer.model.objective.vars) == 1

            return MOI.VariableIndex
        end

        return MOI.ScalarAffineFunction{Integer}
    elseif !isnothing(optimizer.model.floating_point_objective)
        # If this is set, it's a scalar affine function by default.
        return MOI.ScalarAffineFunction{Float64}
    else
        return nothing
    end
end

"""
This specifies to the solver that it should only look for an objective value in the given domain;
the domain on the sum of the objective terms only.
"""
struct ObjectiveFunctionSumDomain <: MOI.AbstractModelAttribute end
MOI.attribute_value_type(::ObjectiveFunctionSumDomain) = Union{Nothing,Vector{Int64}}

function MOI.supports(::CPSATOptimizer, ::Type{ObjectiveFunctionSumDomain})
    return true
end

function MOI.set(optimizer::CPSATOptimizer, ::ObjectiveFunctionSumDomain, vi::Vector{Int64})
    if isnothing(optimizer.model.objective)
        throw(
            ArgumentError("The objective function must be set before setting the domain."),
        )
    end

    optimizer.model.objective.domain = vi

    return nothing
end

function MOI.get(
    optimizer::CPSATOptimizer,
    ::ObjectiveFunctionSumDomain,
)::Union{Nothing,Vector{Int64}}
    if isnothing(optimizer.model.objective)
        return nothing
    end

    return optimizer.model.objective.domain
end

function MOI.set(
    optimizer::CPSATOptimizer,
    ::MOI.ObjectiveFunction{MOI.ScalarAffineFunction{T}},
    objective_function::MOI.ScalarAffineFunction{T},
) where {T<:Real}
    # Override the integer objective function
    optimizer.model.objective = nothing

    optimizer.model.floating_point_objective = FloatObjective()

    terms = objective_function.terms

    terms_pairs = get_terms_pairs(terms)

    for term in terms_pairs
        push!(optimizer.model.floating_point_objective.vars, term[1])
        push!(optimizer.model.floating_point_objective.coeffs, term[2])
    end

    optimizer.model.floating_point_objective.offset = objective_function.constant

    return nothing
end

function MOI.get(
    optimizer::CPSATOptimizer,
    ::MOI.ObjectiveFunction{MOI.ScalarAffineFunction{T}},
) where {T<:Real}
    if isnothing(optimizer.model.floating_point_objective)
        return nothing
    end

    scalar_affine_terms = map(
        i -> MOI.ScalarAffineTerm{T}(
            optimizer.model.floating_point_objective.coeffs[i],
            MOI.VariableIndex(optimizer.model.floating_point_objective.vars[i]),
        ),
        1:length(optimizer.model.floating_point_objective.vars),
    )

    return MOI.ScalarAffineFunction{T}(
        scalar_affine_terms,
        optimizer.model.floating_point_objective.offset,
    )
end

function MOI.get(
    optimizer::CPSATOptimizer,
    ::MOI.ObjectiveFunction{MOI.ScalarAffineFunction},
)
    try
        return something(
            MOI.get(optimizer, MOI.ObjectiveFunction{MOI.ScalarAffineFunction{Int64}}()),
            MOI.get(optimizer, MOI.ObjectiveFunction{MOI.ScalarAffineFunction{Float64}}()),
        )
    catch error
        throw(error)
        # Argument error thrown when both objectives are not set.
        return nothing
    end
end

"""
  Define the strategy to follow when the solver needs to take a new decision.
  The strategy is only defined on a subset of variables.
"""
struct DecisionStrategy
    vars::Union{MOI.VectorOfVariables,MOI.VectorAffineFunction}
    variable_selection_strategy::CPSATVariableSelectionStrategy.T
    domain_reduction_strategy::CPSATDomainReductionStrategy.T

    function DecisionStrategy(
        vars::Union{MOI.VectorOfVariables,MOI.VectorAffineFunction},
        variable_selection_strategy::CPSATVariableSelectionStrategy.T,
        domain_reduction_strategy::CPSATDomainReductionStrategy.T,
    )
        new(vars, variable_selection_strategy, domain_reduction_strategy)
    end
end

function to_cp_sat_decision_strategy(
    decision_stategy::DecisionStrategy,
)::CPSATDecisionStrategy
    cp_sat_decision_strategy = CPSATDecisionStrategy()
    if isa(decision_stategy.vars, MOI.VectorOfVariables)
        cp_sat_decision_strategy.variables = get_variable_indices(
            decision_stategy.vars,
            1,
            MOI.output_dimension(decision_stategy.vars),
        )
        empty!(cp_sat_decision_strategy.exprs)
    else
        # VectorAffineFunction is converted to a LinearExpression proto.
        cp_sat_decision_strategy.exprs = build_linear_expressions(decision_stategy.vars)
        empty!(cp_sat_decision_strategy.vars)
    end
    cp_sat_decision_strategy.variable_selection_strategy =
        decision_stategy.variable_selection_strategy
    cp_sat_decision_strategy.domain_reduction_strategy =
        decision_stategy.domain_reduction_strategy
    return cp_sat_decision_strategy
end

struct DecisionStrategyList <: MOI.AbstractModelAttribute end
MOI.attribute_value_type(::DecisionStrategyList) = Vector{DecisionStrategy}

function MOI.supports(::CPSATOptimizer, ::Type{DecisionStrategyList})
    return true
end

function MOI.set(optimizer::CPSATOptimizer, ::DecisionStrategyList, decision_strategy_list::Vector{DecisionStrategy})
    optimizer.model.search_strategy = to_cp_sat_decision_strategy.(decision_strategy_list)
    return nothing
end

function MOI.get(optimizer::CPSATOptimizer, ::DecisionStrategyList)::Vector{DecisionStrategy}
    decision_strategies = Vector{DecisionStrategy}()

    for strategy in optimizer.model.search_strategy
        vars = []
        if !isempty(strategy.variables)
            vars = MOI.VectorOfVariables(map(v -> MOI.VariableIndex(v), strategy.variables))
        elseif !isempty(strategy.exprs)
            terms = map(
                i -> MOI.VectorAffineTerm(
                    i,
                    MOI.ScalarAffineTerm{Int64}(
                        strategy.exprs.coeffs[i],
                        MOI.VariableIndex(strategy.exprs.vars[i]),
                    ),
                ),
                1:length(strategy.exprs.vars),
            )
            vars = MOI.VectorAffineFunction(terms, map(v -> v.offset, strategy.exprs))
        end

        push!(
            decision_strategies,
            DecisionStrategy(
                vars,
                strategy.variable_selection_strategy,
                strategy.domain_reduction_strategy,
            ),
        )
    end

    return decision_strategies
end
