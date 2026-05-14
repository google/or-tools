const statusEl = document.getElementById('status');

type RunResult = {
  mode: 'direct' | 'worker';
  ok: boolean;
  solverStatus: unknown;
  workerStats: WorkerStats;
};

type WorkerStats = {
  total: number;
  pthread: number;
};

type CpSatApi = typeof import('or-tools-wasm')['CpSat'];

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

function installWorkerSpy() {
  const originalWorker = window.Worker;
  const creations: Array<{ url: string; name?: string }> = [];

  window.Worker = function WorkerSpy(scriptURL: string | URL, options?: WorkerOptions) {
    creations.push({
      url: String(scriptURL),
      name: options?.name,
    });
    return new originalWorker(scriptURL, options);
  } as unknown as typeof Worker;

  return {
    snapshot(): WorkerStats {
      return {
        total: creations.length,
        pthread: creations.filter((creation) => creation.name?.startsWith('em-pthread-')).length,
      };
    },
  };
}

function forceSmallHardwareConcurrency() {
  Object.defineProperty(navigator, 'hardwareConcurrency', {
    configurable: true,
    value: 2,
  });
}

async function runCase(CpSat: CpSatApi, mode: RunResult['mode'], getWorkerStats: () => WorkerStats): Promise<RunResult> {
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
    workerStats: getWorkerStats(),
  };
}

async function main() {
  setStatus({ ok: false, phase: 'running' });
  forceSmallHardwareConcurrency();
  const workerSpy = installWorkerSpy();
  const { CpSat } = await import('or-tools-wasm');
  const results = [
    await runCase(CpSat, 'direct', workerSpy.snapshot),
    await runCase(CpSat, 'worker', workerSpy.snapshot),
  ];
  setStatus({ ok: true, results });
}

void main();
