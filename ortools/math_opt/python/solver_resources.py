# Copyright 2010-2024 Google LLC
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Configures solver resources."""

import dataclasses
from typing import Optional

from ortools.math_opt import rpc_pb2


@dataclasses.dataclass
class SolverResources:
    """The hints on the resources a remote solve is expected to use.

    These parameters are hints and may be ignored by the remote server (in
    particular in case of solve in a local subprocess, for example).

    When using remote_solve() and remote_compute_infeasible_subsystem(), these
    hints are mostly optional as some defaults will be computed based on the other
    parameters.

    When using remote_streaming_solve() these hints are used to dimension the
    resources available during the execution of every action; thus it is
    recommended to set them.

    MOE:begin_intracomment_strip

    The go/uoss server will use these parameters to do a bin-packing of all
    requests. Parameter cpu is a soft-limit, the solve may still be able to use
    more CPUs.  The ram parameter is an hard-limit, an out-of-memory error will
    occur if the solve attempts to use more memory.

    MOE:end_intracomment_strip

    Attributes:
      cpu: The number of solver threads that are expected to actually execute in
        parallel. Must be finite and >0.0.  For example a value of 3.0 means that
        if the solver has 5 threads that can execute we expect at least 3 of these
        threads to be scheduled in parallel for any given time slice of the
        operating system scheduler.  A fractional value indicates that we don't
        expect the operating system to constantly schedule the solver's work. For
        example with 0.5 we would expect the solver's threads to be scheduled half
        the time.  This parameter is usually used in conjunction with
        SolveParameters.threads. For some solvers like Gurobi it makes sense to
        use SolverResources.cpu = SolveParameters.threads. For other solvers like
        CP-SAT, it may makes sense to use a value lower than the number of threads
        as not all threads may be ready to be scheduled at the same time. It is
        better to consult each solver documentation to set this parameter.  Note
        that if the SolveParameters.threads is not set then this parameter should
        also be left unset.
      ram: The limit of RAM for the solve in bytes. Must be finite and >=1.0 (even
        though it should in practice be much larger).
    """

    cpu: Optional[float] = None
    ram: Optional[float] = None

    def to_proto(self) -> rpc_pb2.SolverResourcesProto:
        return rpc_pb2.SolverResourcesProto(cpu=self.cpu, ram=self.ram)
