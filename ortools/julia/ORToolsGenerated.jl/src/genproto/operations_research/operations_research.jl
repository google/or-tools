module operations_research

include("../google/google.jl")
import .operations_research as var"#operations_research"

include("optional_boolean_pb.jl")
include("routing_enums_pb.jl")
include("solver_parameters_pb.jl")
include("search_stats_pb.jl")
include("gscip_pb.jl")
include("search_limit_pb.jl")
include("course_scheduling_pb.jl")
include("demon_profiler_pb.jl")
include("assignment_pb.jl")
include("linear_solver_pb.jl")
include("sat/sat.jl")
include("routing_parameters_pb.jl")
include("scheduling/scheduling.jl")
include("glop/glop.jl")
include("bop/bop.jl")
include("packing/packing.jl")
include("pdlp/pdlp.jl")
include("math_opt/math_opt.jl")

end # module operations_research
