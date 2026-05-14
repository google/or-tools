import { CpSat } from 'or-tools-wasm';

function assert(condition: unknown, message: string): asserts condition {
  if (!condition) {
    throw new Error(message);
  }
}

async function runCase(mode: 'direct' | 'worker') {
  CpSat.setWorkerBridgeEnabled(mode === 'worker');
  assert(CpSat.isWorkerBridgeEnabled() === (mode === 'worker'), `worker bridge state mismatch for ${mode}`);
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
  assert(validation.ok, `${mode} validation failed: ${validation.message}`);

  const result = await CpSat.solve(modelBytes, {
    numSearchWorkers: 1,
  });

  assert(
    result.response?.status === 'OPTIMAL' || result.response?.status === 'FEASIBLE',
    `${mode} unexpected solver status: ${String(result.response?.status)}`,
  );
}

Deno.test('solves the README model in Deno with and without the worker bridge', async () => {
  await runCase('direct');
  await runCase('worker');
});
