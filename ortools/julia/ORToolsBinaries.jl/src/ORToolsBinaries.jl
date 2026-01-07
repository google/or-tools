module ORToolsBinaries

export libortools

const _SRC_DIR = @__DIR__
const _DEPS_DIR = joinpath(_SRC_DIR, "..", "deps")
const _LIB_DIR = joinpath(_DEPS_DIR, "lib")

if Sys.islinux()
    const libortools = joinpath(_LIB_DIR, "libortools.so")
elseif Sys.isapple()
    const libortools = joinpath(_LIB_DIR, "libortools.dylib")
elseif Sys.iswindows()
    const libortools = joinpath(_LIB_DIR, "ortools.dll")
else
    @error "Platform not supported! If ORTools_jll provides binaries for your platform, prefer using them rather than this package."
end

if !isfile(libortools)
    @error "Installing ORToolsBinaries failed, the expected binary is not available at the expected path ($libortools)"
end

end
