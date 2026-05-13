import { CpSat, type SatParameters } from 'ortools-cpsat-wasm';

const sampleModel = {
  name: 'exactly_one_bool',
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
};

const sampleParams: SatParameters = {
  maxTimeInSeconds: 5,
  numWorkers: 1,
};

const modelInput = document.getElementById('model-input') as HTMLTextAreaElement | null;
const paramsInput = document.getElementById('params-input') as HTMLTextAreaElement | null;
const resultOutput = document.getElementById('result-output') as HTMLPreElement | null;
const statusEl = document.getElementById('status') as HTMLElement | null;
const loadSampleButton = document.getElementById('load-sample') as HTMLButtonElement | null;
const validateButton = document.getElementById('validate') as HTMLButtonElement | null;
const solveButton = document.getElementById('solve') as HTMLButtonElement | null;
const cancelButton = document.getElementById('cancel') as HTMLButtonElement | null;
const workerBridgeToggle = document.getElementById('use-worker-bridge') as HTMLInputElement | null;

function setStatus(message: string) {
  if (statusEl) {
    statusEl.textContent = message;
  }
}

function setResult(value: unknown) {
  if (resultOutput) {
    resultOutput.textContent =
      typeof value === 'string' ? value : JSON.stringify(value, null, 2);
  }
}

function setRunning(running: boolean) {
  if (validateButton) validateButton.disabled = running;
  if (solveButton) solveButton.disabled = running;
  if (loadSampleButton) loadSampleButton.disabled = running;
  if (cancelButton) cancelButton.disabled = !running;
}

function loadSample() {
  if (modelInput) {
    modelInput.value = JSON.stringify(sampleModel, null, 2);
  }
  if (paramsInput) {
    paramsInput.value = JSON.stringify(sampleParams, null, 2);
  }
  setStatus('Sample loaded.');
  setResult('');
}

function parseJsonObject(input: HTMLTextAreaElement | null, label: string) {
  if (!input) {
    throw new Error(`${label} input is missing.`);
  }
  const text = input.value.trim();
  if (!text) {
    throw new Error(`${label} is empty.`);
  }
  const parsed = JSON.parse(text) as unknown;
  if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) {
    throw new Error(`${label} must be a JSON object.`);
  }
  return parsed as Record<string, unknown>;
}

function parseParams() {
  if (!paramsInput || !paramsInput.value.trim()) {
    return null;
  }
  return parseJsonObject(paramsInput, 'SAT parameters') as SatParameters;
}

async function buildModelBytes() {
  const model = parseJsonObject(modelInput, 'Model');
  return CpSat.createModel(model);
}

async function validateModel() {
  setRunning(true);
  setStatus('Building model...');
  try {
    const modelBytes = await buildModelBytes();
    setStatus('Validating model...');
    const validation = await CpSat.validate(modelBytes);
    setResult(validation);
    setStatus(validation.ok ? 'Model is valid.' : 'Model is invalid.');
  } catch (error) {
    setStatus('Validation failed.');
    setResult((error as Error).message);
  } finally {
    setRunning(false);
  }
}

async function solveModel() {
  setRunning(true);
  setStatus('Building model...');
  try {
    const modelBytes = await buildModelBytes();
    const params = parseParams();
    setStatus('Solving...');
    const result = await CpSat.solve(modelBytes, params);
    setResult(result.response ?? { bytes: Array.from(result.bytes) });
    setStatus('Solve finished.');
  } catch (error) {
    setStatus('Solve failed.');
    setResult((error as Error).message);
  } finally {
    setRunning(false);
  }
}

if (workerBridgeToggle) {
  workerBridgeToggle.checked = true;
  CpSat.setWorkerBridgeEnabled(workerBridgeToggle.checked);
  workerBridgeToggle.addEventListener('change', () => {
    CpSat.setWorkerBridgeEnabled(workerBridgeToggle.checked);
  });
}

loadSampleButton?.addEventListener('click', loadSample);
validateButton?.addEventListener('click', () => {
  void validateModel();
});
solveButton?.addEventListener('click', () => {
  void solveModel();
});
cancelButton?.addEventListener('click', () => {
  void CpSat.cancelSolve();
  setStatus('Cancel requested.');
});

loadSample();
