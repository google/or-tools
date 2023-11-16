
from absl import app
from absl import flags
from ortools.linear_solver.python import model_builder

FLAGS = flags.FLAGS

_INPUT = flags.DEFINE_string('input', '', 'Input file to load and solve.')
_PARAMS = flags.DEFINE_string('params', '', 'Solver parameters in string format.')
_SOLVER = flags.DEFINE_string('solver', 'sat', 'Solver type to solve the model with.')


def main(_):
    model = model_builder.ModelBuilder()

    # Load MPS file.
    if not model.import_from_mps_file(_INPUT.value):
        print(f'Cannot import MPS file: \'{_INPUT.value}\'')
        return

    # Create solver.
    solver = model_builder.ModelSolver(_SOLVER.value)
    if not solver.solver_is_supported():
        print(f'Cannot create solver with name \'{_SOLVER.value}\'')
        return

    # Set parameters.
    if _PARAMS.value:
        solver.set_solver_specific_parameters(_PARAMS.value)

    # Enable the output of the solver.
    solver.enable_output(True)

    # And solve.
    solver.solve(model)


if __name__ == '__main__':
    app.run(main)
