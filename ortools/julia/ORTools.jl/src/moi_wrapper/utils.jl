"""
  A set of shared utility/helper functions for the MOI wrappers.
"""

"""
  Helper function to create a dictionary of terms to combine coefficients 
  of terms(variables) if they are repeated.
  For example: 3x + 5x <= 10 will be combined to 8x <= 10.
"""
function get_terms_pairs(
    terms::Vector{MOI.ScalarAffineTerm{T}},
)::Vector{Pair{Int64,Float64}} where {T<:Real}
    terms_pairs = Dict{Int64,Float64}()

    for term in terms
        if !haskey(terms_pairs, term.variable.value)
            terms_pairs[term.variable.value] = term.coefficient
        else
            terms_pairs[term.variable.value] += term.coefficient
        end
    end

    sorted_pairs = sort(collect(terms_pairs), by = x -> x[1])
    return sorted_pairs
end
