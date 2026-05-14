import { CpSat } from 'or-tools-wasm';

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) {
    throw new Error(message);
  }
}

Deno.test('solves the README model in Deno', async () => {
  assert(CpSat.isWorkerBridgeEnabled() === false, 'worker bridge should be disabled in Deno');

  const modelBytes = await CpSat.createModel({
    name: 'choose_one',
    variables: [
      { name: 'x', domain: [0, 1] },
      { name: 'y', domain: [0, 1] },
    ],
    constraints: [
      {
        linear: {
          vars: [0, 1],
          coeffs: [1, 1],
          domain: [1, 1],
        },
      },
    ],
    objective: {
      vars: [0, 1],
      coeffs: [1, 2],
    },
  });

  const validation = await CpSat.validate(modelBytes);
  assert(validation.ok, validation.message);

  const result = await CpSat.solve(modelBytes, {
    numSearchWorkers: 1,
  });

  assert(
    result.response?.status === 'OPTIMAL' || result.response?.status === 'FEASIBLE',
    `unexpected solver status: ${String(result.response?.status)}`,
  );
});
