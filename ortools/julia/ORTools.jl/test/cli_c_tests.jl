#!/usr/bin/env julia

# Checks whether the given dynamic library has the required symbols for
# ORTools.jl.
#
# This script is super light so that it can be run easily from automated tests
# when building OR-Tools.

using Libdl

function main(ARGS)
    if length(ARGS) != 1
        error("Expected usage: ./$(PROGRAM_FILE) /path/to/libortools.so")
    end

    libortools = ARGS[1]  # Equivalent to ORTools_jll.libortools
    println("Testing shared library $(ARGS[1])...")

    lib = Libdl.dlopen(libortools)
    if lib === nothing
        # This should never happen, as dlopen is supposed to error if it fails.
        return -1
    end
    println("Successfully loaded the shared library $(ARGS[1])!")

    math_opt_fct = Libdl.dlsym(lib, :MathOptSolve)
    if math_opt_fct === nothing
        # This should never happen, as dlsym is supposed to error if it fails.
        return -1
    end
    println(
        "Successfully loaded the function MathOptSolve from the shared " *
        "library $(ARGS[1])!",
    )

    cp_sat_fct = Libdl.dlsym(lib, :SolveCpModelWithParameters)
    if cp_sat_fct === nothing
        # This should never happen, as dlsym is supposed to error if it fails.
        return -1
    end
    println(
        "Successfully loaded the function SolveCpModelWithParameters from " *
        "the shared library $(ARGS[1])!",
    )
    return 0
end

# With Julia 1.11+: @main
exit(main(ARGS))
