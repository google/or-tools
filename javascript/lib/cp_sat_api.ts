import type { MainModule } from './cp_sat_module_loader';
import { loadCpSat } from './cp_sat_module_loader';
import workerScriptUrl from './cpsat_worker?worker&url';
import type { WorkerRequest, WorkerResponse } from './cpsat_worker_types';

type SchemaPair = {
  cp_model: string;
  sat_parameters: string;
};

type CpSatApi = {
  solve(model: Uint8Array, params?: Uint8Array | null): Promise<Uint8Array>;
  validate(model: Uint8Array): Promise<{ ok: boolean; message: string }>;
  getSchemas(): Promise<SchemaPair>;
  loadModule(): Promise<MainModule>;
  setWorkerBridgeEnabled(enabled: boolean): void;
  isWorkerBridgeEnabled(): boolean;
  cancelSolve(): Promise<void>;
};

const isBrowserMainThread = typeof window !== 'undefined' && typeof document !== 'undefined';
const workerCapable = typeof Worker !== 'undefined';
const workerBridgeAvailable = isBrowserMainThread && workerCapable;
const workerEntryUrl = new URL(workerScriptUrl, import.meta.url);
let worker: Worker | null = null;
let workerReadyPromise: Promise<void> | null = null;
const pendingWorkerRequests = new Map<
  number,
  {
    resolve(value: WorkerResponse): void;
    reject(reason: unknown): void;
  }
>();
let nextWorkerRequestId = 1;

function terminateWorker(reason?: string) {
  if (!worker) return;
  worker.terminate();
  worker = null;
  workerReadyPromise = null;
  const error = new Error(reason ?? 'CP-SAT worker terminated.');
  for (const pending of pendingWorkerRequests.values()) {
    pending.reject(error);
  }
  pendingWorkerRequests.clear();
}

function ensureWorker(): Worker {
  if (!workerBridgeAvailable) {
    throw new Error('Worker bridge is not available.');
  }
  if (worker) {
    return worker;
  }
  const instance = new Worker(workerEntryUrl, { type: 'module' });
  worker = instance;
  workerReadyPromise = new Promise<void>((resolve, reject) => {
    instance.onmessage = (event: MessageEvent<WorkerResponse>) => {
      const message = event.data;
      if (message.type === 'ready') {
        resolve();
        return;
      }
      const pending = pendingWorkerRequests.get(message.id);
      if (message.type === 'error') {
        const error = new Error(message.error);
        if (pending) {
          pending.reject(error);
          pendingWorkerRequests.delete(message.id);
        } else {
          reject(error);
        }
        return;
      }
      if (pending) {
        pendingWorkerRequests.delete(message.id);
        pending.resolve(message);
      }
    };
    instance.onerror = (event: ErrorEvent) => {
      const error =
        event.error ?? new Error(event.message || 'cpsat_worker error');
      reject(error);
      terminateWorker(error.message);
    };
  });
  return instance;
}

async function waitForWorkerReady() {
  if (!workerBridgeAvailable) {
    throw new Error('Worker bridge is not available.');
  }
  ensureWorker();
  if (!workerReadyPromise) {
    throw new Error('Worker ready state unavailable.');
  }
  await workerReadyPromise;
}

async function postWorkerRequest<T extends WorkerResponse>(request: WorkerRequest): Promise<T> {
  if (!workerBridgeAvailable) {
    throw new Error('Worker bridge is not available.');
  }
  const workerInstance = ensureWorker();
  await waitForWorkerReady();
  return new Promise<T>((resolve, reject) => {
    pendingWorkerRequests.set(request.id, {
      resolve: (value: WorkerResponse) => resolve(value as T),
      reject,
    });
    workerInstance.postMessage(request);
  });
}


const modulePromise = loadCpSat();

const schemaPromise: Promise<SchemaPair> = modulePromise.then((Module) => {
  const getCpSchema =
    typeof Module.getCpModelSchema === 'function'
      ? Module.getCpModelSchema.bind(Module)
      : () => Module.ccall('get_cp_model_schema', 'string', [], []) as unknown as string;
  const getSatSchema =
    typeof Module.getSatParametersSchema === 'function'
      ? Module.getSatParametersSchema.bind(Module)
      : () => Module.ccall('get_sat_parameters_schema', 'string', [], []) as unknown as string;
  return {
    cp_model: getCpSchema(),
    sat_parameters: getSatSchema(),
  };
});

const readUint32LE = (buffer: ArrayBuffer, ptr: number) =>
  new DataView(buffer, ptr, 4).getUint32(0, true);

function copyBytesToHeap(Module: MainModule, bytes: Uint8Array | null) {
  if (!bytes || !bytes.length) {
    return 0;
  }
  const ptr = Module._malloc(bytes.length);
  Module.HEAPU8.set(bytes, ptr);
  return ptr;
}

let workerBridgePreferred = workerBridgeAvailable;
let activeWorkerSolveId: number | null = null;

function shouldUseWorkerBridge() {
  return workerBridgePreferred && workerBridgeAvailable;
}

function setWorkerBridgePreferred(enabled: boolean) {
  workerBridgePreferred = Boolean(enabled);
  if (workerBridgePreferred && !workerBridgeAvailable) {
    console.warn('Worker bridge requested but no worker is initialized in this environment.');
  }
}

type WorkerSolveResponse = Extract<WorkerResponse, { type: 'solveResult' }>;
type WorkerValidateResponse = Extract<WorkerResponse, { type: 'validateResult' }>;

async function solveViaWorker(modelBytes: Uint8Array, paramsBytes: Uint8Array | null = null) {
  const id = nextWorkerRequestId++;
  activeWorkerSolveId = id;
  try {
    const response = await postWorkerRequest<WorkerSolveResponse>({
      type: 'solve',
      id,
      modelBytes,
      paramsBytes: paramsBytes ?? undefined,
    });
    return new Uint8Array(response.bytes);
  } finally {
    if (activeWorkerSolveId === id) {
      activeWorkerSolveId = null;
    }
  }
}

async function validateViaWorker(modelBytes: Uint8Array) {
  const id = nextWorkerRequestId++;
  const response = await postWorkerRequest<WorkerValidateResponse>({
    type: 'validate',
    id,
    modelBytes,
  });
  return { ok: response.ok, message: response.message };
}

async function solveDirect(modelBytes: Uint8Array, paramsBytes: Uint8Array | null = null) {
  const Module = await modulePromise;

  const lenPtr = Module._malloc(4);
  const modelPtr = copyBytesToHeap(Module, modelBytes);
  const paramsPtr = copyBytesToHeap(Module, paramsBytes);
  let responsePtr = 0;

  try {
    responsePtr = (await Module.ccall(
      'solve_model',
      'number',
      ['number', 'number', 'number', 'number', 'number'],
      [
        modelPtr,
        modelBytes.length,
        paramsPtr,
        paramsBytes ? paramsBytes.length : 0,
        lenPtr,
      ],
      { async: true },
    )) as number;
  } finally {
    if (modelPtr) Module._free(modelPtr);
    if (paramsPtr) Module._free(paramsPtr);
  }

  const len = readUint32LE(Module.HEAPU8.buffer, lenPtr);
  Module._free(lenPtr);

  let bytes = new Uint8Array();
  if (responsePtr && len) {
    bytes = Module.HEAPU8.slice(responsePtr, responsePtr + len);
    Module._free_buffer(responsePtr);
  } else if (responsePtr) {
    Module._free_buffer(responsePtr);
  }

  return new Uint8Array(bytes);
}

async function validateDirect(model: Uint8Array) {
  const Module = await modulePromise;
  const lenPtr = Module._malloc(4);
  const modelPtr = copyBytesToHeap(Module, model);
  let msgPtr = 0;

  try {
    // Note: Use modelBytes.length here? Original code had 'modelBytes' not defined in arguments.
    // Assuming 'model' argument is the Uint8Array.
    msgPtr = Module.ccall(
      'validate_model',
      'number',
      ['number', 'number', 'number'],
      [modelPtr, model.length, lenPtr],
    ) as number;
  } finally {
    if (modelPtr) Module._free(modelPtr);
  }

  const len = readUint32LE(Module.HEAPU8.buffer, lenPtr);
  Module._free(lenPtr);

  if (!msgPtr || len === 0) {
    if (msgPtr) Module._free_buffer(msgPtr);
    return { ok: true, message: '' };
  }

  const messageBytes = Module.HEAPU8.slice(msgPtr, msgPtr + len);
  Module._free_buffer(msgPtr);
  const message = new TextDecoder().decode(messageBytes);
  return { ok: false, message };
}

async function cancelSolve() {
  if (worker && activeWorkerSolveId !== null) {
    terminateWorker('Solve canceled by user.');
    activeWorkerSolveId = null;
    return;
  }
  const Module = await modulePromise;
  Module.ccall('interrupt_solve', 'void', [], []);
}

export const CpSat: CpSatApi = {
  solve: (model, params = null) =>
    (shouldUseWorkerBridge() ? solveViaWorker(model, params) : solveDirect(model, params)),
  validate: (model) => (shouldUseWorkerBridge() ? validateViaWorker(model) : validateDirect(model)),
  getSchemas: () => schemaPromise,
  loadModule: () => modulePromise,
  setWorkerBridgeEnabled: (enabled: boolean) => setWorkerBridgePreferred(enabled),
  isWorkerBridgeEnabled: () => shouldUseWorkerBridge(),
  cancelSolve,
};

if (isBrowserMainThread) {
  (window as Window & { CpSat?: CpSatApi }).CpSat = CpSat;
}

export type { CpSatApi };
export type { WorkerRequest, WorkerResponse } from './cpsat_worker_types';
export default CpSat;
