# Keep this file in sync with sat/c_api/cp_solver_c.h.

function SolveCpModelWithParameters(
    creq, creq_len, cparams, cparams_len, cres, cres_len
)
  _check_lib_loaded()
  return ccall(
      (:SolveCpModelWithParameters, libortools[]),
      Cvoid,
      (
          Ptr{Cvoid},
          Cint,
          Ptr{Cvoid},
          Cint,
          Ptr{Ptr{Cvoid}},
          Ptr{Cint},
      ),
      creq,
      creq_len,
      cparams,
      cparams_len,
      cres,
      cres_len,
  )
end

function SolveCpNewEnv()
  _check_lib_loaded()
  return ccall((:SolveCpNewEnv, libortools[]), Ptr{Cvoid}, ())
end

function SolveCpDestroyEnv(cenv)
  _check_lib_loaded()
  return ccall((:SolveCpDestroyEnv, libortools[]), Cvoid, (Ptr{Cvoid},), cenv)
end

function SolveCpStopSearch(cenv)
  _check_lib_loaded()
  return ccall((:SolveCpStopSearch, libortools[]), Cvoid, (Ptr{Cvoid},), cenv)
end

function SolveCpInterruptible(
    cenv, creq, creq_len, cparams, cparams_len, cres, cres_len
)
  _check_lib_loaded()
  return ccall(
      (:SolveCpInterruptible, libortools[]),
      Cvoid,
      (
          Ptr{Cvoid},
          Ptr{Cvoid},
          Cint,
          Ptr{Cvoid},
          Cint,
          Ptr{Ptr{Cvoid}},
          Ptr{Cint},
      ),
      cenv,
      creq,
      creq_len,
      cparams,
      cparams_len,
      cres,
      cres_len,
  )
end
