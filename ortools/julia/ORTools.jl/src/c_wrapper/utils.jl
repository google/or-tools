function _check_lib_loaded()
  # If `libortools[]` returns an invalid path, `ccall` will fail, with an
  # error message along the lines of:
  #     ERROR: could not load library "something"
  # This should not be possible with `ORTools_jll.jl` or `ORToolsBinaries.jl`,
  # but entirely possible if the user is using `ORTools.set_library` directly.
  # Hence, no specific check for the library that is loaded.
  if libortools[] == ""
    @error(
        "No ORTools binaries loaded. Call `import ORTools_jll` or " *
        "`import ORToolsBinaries` first to load the OR-Tools binaries.",
    )
  end
end