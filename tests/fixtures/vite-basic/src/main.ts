import { CpSat } from 'or-tools-wasm';

const statusEl = document.getElementById('status');

type RunResult = {
  mode: 'direct' | 'worker';
  ok: boolean;
  solverStatus: unknown;
};

const model = {
  name: 'choose_one',
  variables: [
    { name: 'x', domain: [0, 1] },
    { name: 'y', domain: [0, 1] },
  ],
  constraints: [
    {
      name: 'exactly_one',
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
};

function setStatus(value: unknown) {
  if (statusEl) {
    statusEl.textContent = JSON.stringify(value, null, 2);
  }
}

async function runCase(mode: RunResult['mode']): Promise<RunResult> {
  CpSat.setWorkerBridgeEnabled(mode === 'worker');
  const modelBytes = await CpSat.createModel(model);
  const validation = await CpSat.validate(modelBytes);

  if (!validation.ok) {
    throw new Error(`${mode} validation failed: ${validation.message}`);
  }

  const result = await CpSat.solve(modelBytes, {
    numSearchWorkers: 1,
  });
  const solverStatus = result.response?.status;

  if (solverStatus !== 'OPTIMAL' && solverStatus !== 'FEASIBLE') {
    throw new Error(`${mode} solve returned ${String(solverStatus)}`);
  }

  return {
    mode,
    ok: true,
    solverStatus,
  };
}

async function main() {
  setStatus({ ok: false, phase: 'running' });
  const results = [await runCase('direct'), await runCase('worker')];
  setStatus({ ok: true, results });
}

void main();
