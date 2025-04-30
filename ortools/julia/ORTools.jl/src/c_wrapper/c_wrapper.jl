using ORTools_jll

libortools = ORTools_jll.libortools

# Keep this file in sync with math_opt/core/c_api/solver.h.

# There is no need to explicitly have a `mutable struct MathOptInterrupter`
# on the Julia side, as it's only an opaque pointer for this API.

function MathOptNewInterrupter()
    return ccall((:MathOptNewInterrupter, libortools), Ptr{Cvoid}, ())
end

function MathOptFreeInterrupter(ptr)
    return ccall((:MathOptFreeInterrupter, libortools), Cvoid, (Ptr{Cvoid},), ptr)
end

function MathOptInterrupt(ptr)
    return ccall((:MathOptInterrupt, libortools), Cvoid, (Ptr{Cvoid},), ptr)
end

function MathOptIsInterrupted(ptr)
    return ccall((:MathOptIsInterrupted, libortools), Cint, (Ptr{Cvoid},), ptr)
end

function MathOptFree(ptr)
    return ccall((:MathOptFree, libortools), Cvoid, (Ptr{Cvoid},), ptr)
end

function MathOptSolve(
    model,
    model_size,
    solver_type,
    interrupter,
    solve_result,
    solve_result_size,
    status_msg,
)
    return ccall(
        (:MathOptSolve, libortools),
        Cint,
        (
            Ptr{Cvoid},
            Csize_t,
            Cint,
            Ptr{Cvoid},
            Ptr{Ptr{Cvoid}},
            Ptr{Csize_t},
            Ptr{Ptr{Cchar}},
        ),
        model,
        model_size,
        solver_type,
        interrupter,
        solve_result,
        solve_result_size,
        status_msg,
    )
end
