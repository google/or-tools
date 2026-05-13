# Copyright 2010-2025 Google LLC
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

"""Configures the instantiation of the underlying solver."""

import dataclasses
from typing import Optional

from ortools.math_opt import parameters_pb2
from ortools.math_opt.solvers import gurobi_pb2


@dataclasses.dataclass
class StreamableGScipInitArguments:
    """Streamable GScip specific parameters for solver instantiation."""


@dataclasses.dataclass(frozen=True)
class GurobiISVKey:
    """The Gurobi ISV key, an alternative to license files.

    Contact Gurobi for details.

    Attributes:
      name: A string, typically a company/organization.
      application_name: A string, typically a project.
      expiration: An int, a value of 0 indicates no expiration.
      key: A string, the secret.
    """

    name: str = ""
    application_name: str = ""
    expiration: int = 0
    key: str = ""

    def to_proto(self) -> gurobi_pb2.GurobiInitializerProto.ISVKey:
        """Returns a protocol buffer equivalent of this."""
        return gurobi_pb2.GurobiInitializerProto.ISVKey(
            name=self.name,
            application_name=self.application_name,
            expiration=self.expiration,
            key=self.key,
        )


def gurobi_isv_key_from_proto(
    proto: gurobi_pb2.GurobiInitializerProto.ISVKey,
) -> GurobiISVKey:
    """Returns an equivalent GurobiISVKey to the input proto."""
    return GurobiISVKey(
        name=proto.name,
        application_name=proto.application_name,
        expiration=proto.expiration,
        key=proto.key,
    )


@dataclasses.dataclass
class StreamableGurobiInitArguments:
    """Streamable Gurobi specific parameters for solver instantiation."""

    isv_key: Optional[GurobiISVKey] = None

    def to_proto(self) -> gurobi_pb2.GurobiInitializerProto:
        """Returns a protocol buffer equivalent of this."""
        return gurobi_pb2.GurobiInitializerProto(
            isv_key=self.isv_key.to_proto() if self.isv_key else None
        )


def streamable_gurobi_init_arguments_from_proto(
    proto: gurobi_pb2.GurobiInitializerProto,
) -> StreamableGurobiInitArguments:
    """Returns an equivalent StreamableGurobiInitArguments to the input proto."""
    result = StreamableGurobiInitArguments()
    if proto.HasField("isv_key"):
        result.isv_key = gurobi_isv_key_from_proto(proto.isv_key)
    return result


@dataclasses.dataclass
class StreamableGlopInitArguments:
    """Streamable Glop specific parameters for solver instantiation."""


@dataclasses.dataclass
class StreamableCpSatInitArguments:
    """Streamable CP-SAT specific parameters for solver instantiation."""


@dataclasses.dataclass
class StreamablePdlpInitArguments:
    """Streamable Pdlp specific parameters for solver instantiation."""


@dataclasses.dataclass
class StreamableGlpkInitArguments:
    """Streamable GLPK specific parameters for solver instantiation."""


@dataclasses.dataclass
class StreamableOsqpInitArguments:
    """Streamable OSQP specific parameters for solver instantiation."""


@dataclasses.dataclass
class StreamableEcosInitArguments:
    """Streamable Ecos specific parameters for solver instantiation."""


@dataclasses.dataclass
class StreamableScsInitArguments:
    """Streamable Scs specific parameters for solver instantiation."""


@dataclasses.dataclass
class StreamableHighsInitArguments:
    """Streamable Highs specific parameters for solver instantiation."""


@dataclasses.dataclass
class StreamableSantoriniInitArguments:
    """Streamable Santorini specific parameters for solver instantiation."""


@dataclasses.dataclass
class StreamableSolverInitArguments:
    """Solver initialization parameters that can be sent to another process.

    Attributes:
      gscip: Initialization parameters specific to GScip.
      gurobi: Initialization parameters specific to Gurobi.
      glop: Initialization parameters specific to GLOP.
      cp_sat: Initialization parameters specific to CP-SAT.
      pdlp: Initialization parameters specific to PDLP.
      glpk: Initialization parameters specific to GLPK.
      osqp: Initialization parameters specific to OSQP.
      ecos: Initialization parameters specific to ECOS.
      scs: Initialization parameters specific to SCS.
      highs: Initialization parameters specific to HiGHS.
      santorini: Initialization parameters specific to Santorini.
    """

    gscip: Optional[StreamableGScipInitArguments] = None
    gurobi: Optional[StreamableGurobiInitArguments] = None
    glop: Optional[StreamableGlopInitArguments] = None
    cp_sat: Optional[StreamableCpSatInitArguments] = None
    pdlp: Optional[StreamablePdlpInitArguments] = None
    glpk: Optional[StreamableGlpkInitArguments] = None
    osqp: Optional[StreamableOsqpInitArguments] = None
    ecos: Optional[StreamableEcosInitArguments] = None
    scs: Optional[StreamableScsInitArguments] = None
    highs: Optional[StreamableHighsInitArguments] = None
    santorini: Optional[StreamableSantoriniInitArguments] = None

    def to_proto(self) -> parameters_pb2.SolverInitializerProto:
        """Returns a protocol buffer equivalent of this."""
        return parameters_pb2.SolverInitializerProto(
            gurobi=self.gurobi.to_proto() if self.gurobi else None
        )


def streamable_solver_init_arguments_from_proto(
    proto: parameters_pb2.SolverInitializerProto,
) -> StreamableSolverInitArguments:
    """Returns an equivalent StreamableSolverInitArguments to the input proto."""
    result = StreamableSolverInitArguments()
    if proto.HasField("gurobi"):
        result.gurobi = streamable_gurobi_init_arguments_from_proto(proto.gurobi)
    return result
