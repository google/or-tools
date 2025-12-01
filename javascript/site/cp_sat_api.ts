import type { MainModule } from './cp_sat_runtime.js';
import createCpSatModule from './cp_sat_runtime.js';
import cpSatWasmUrl from './cp_sat_runtime.wasm?url';
import CpSatWorker from './cpsat_worker?worker';
import type { WorkerRequest, WorkerResponse } from './cpsat_worker_types';

type SchemaPair = {
  cp_model: string;
  sat_parameters: string;
};

export type CpSatModelInstance = {
  toBinary(): Uint8Array;
};

type CpSatModelConstructor = new (
  bytes: Uint8Array,
  metadata?: Record<string, unknown>,
) => CpSatModelInstance;

interface ProtobufType<T = unknown> {
  verify(obj: unknown): string | null;
  encode(obj: T): { finish(): Uint8Array };
  decode(bytes: Uint8Array): unknown;
  toObject(message: unknown, options?: { longs?: StringConstructor }): unknown;
}

type CpSatApi = {
  CpSatModel: CpSatModelConstructor;
  createModel(proto: Record<string, unknown>): Promise<CpSatModelInstance>;
  solve(
    model: CpSatModelInstance | Uint8Array,
    params?: Record<string, unknown>,
  ): Promise<{ bytes: Uint8Array; response: unknown }>;
  validate(
    model: CpSatModelInstance | Uint8Array,
  ): Promise<{ ok: boolean; message: string }>;
  getSchemas(): Promise<SchemaPair>;
  loadModule(): Promise<MainModule>;
  loadTypes(): Promise<{
    CpModelProto: ProtobufType;
    CpSolverResponse: ProtobufType;
    SatParameters: ProtobufType;
  }>;
  setWorkerBridgeEnabled(enabled: boolean): void;
  isWorkerBridgeEnabled(): boolean;
  cancelSolve(): Promise<void>;
};

type ProtobufModule = typeof import('protobufjs');
let protobufModule: ProtobufModule | null = null;

const loadProtobuf = async (): Promise<ProtobufModule> => {
  if (protobufModule) return protobufModule;
  try {
    const mod = await import('protobufjs');
    protobufModule = mod;
    return mod;
  } catch (error) {
    throw new Error(
      'protobufjs is required to parse the embedded schemas in the demos. Install it optionally with `npm install protobufjs`.',
    );
  }
};

const isBrowserMainThread = typeof window !== 'undefined' && typeof document !== 'undefined';
const workerCapable = typeof Worker !== 'undefined';
const workerBridgeAvailable = isBrowserMainThread && workerCapable;
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
  const instance = new CpSatWorker();
  worker = instance;
  workerReadyPromise = new Promise<void>((resolve, reject) => {
    instance.onmessage = (event) => {
      const message = event.data as WorkerResponse;
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
    instance.onerror = (event) => {
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

const locateCpSatAsset = (path: string) => {
  if (path.endsWith('.wasm')) {
    return cpSatWasmUrl;
  }
  return path;
};

const modulePromise = createCpSatModule({
  locateFile: (path: string) => locateCpSatAsset(path),
});

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

const protoTypesPromise = (async () => {
  const { cp_model, sat_parameters } = await schemaPromise;
  const protobuf = await loadProtobuf();
  const root = new protobuf.Root();
  protobuf.parse(sat_parameters, root);
  protobuf.parse(cp_model, root);
  return {
    CpModelProto: root.lookupType('operations_research.sat.CpModelProto'),
    CpSolverResponse: root.lookupType('operations_research.sat.CpSolverResponse'),
    SatParameters: root.lookupType('operations_research.sat.SatParameters'),
  };
})();

class CpSatModel implements CpSatModelInstance {
  private bytes: Uint8Array;
  private metadata: Record<string, unknown>;

  constructor(bytes: Uint8Array, metadata: Record<string, unknown> = {}) {
    this.bytes = bytes;
    this.metadata = metadata;
  }

  toBinary(): Uint8Array {
    return this.bytes;
  }
}

async function createModel(protoObject: Record<string, unknown>) {
  const { CpModelProto } = await protoTypesPromise;
  const err = CpModelProto.verify(protoObject);
  if (err) {
    throw new Error(`Invalid CpModelProto: ${err}`);
  }
  const bytes = CpModelProto.encode(protoObject).finish();
  return new CpSatModel(bytes, { object: protoObject });
}

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

function isCpSatModelInstance(
  value: CpSatModelInstance | Uint8Array,
): value is CpSatModelInstance {
  return typeof (value as CpSatModelInstance).toBinary === 'function';
}

function encodeSatParameters(params: Record<string, unknown>, SatParameters: ProtobufType) {
  if (!params || !Object.keys(params).length) {
    return null;
  }
  const err = SatParameters.verify(params);
  if (err) {
    throw new Error(`Invalid SatParameters: ${err}`);
  }
  return SatParameters.encode(params).finish();
}

function decodeSolverResponse(bytes: Uint8Array, CpSolverResponse: ProtobufType) {
  if (!bytes.length) {
    return null;
  }
  const message = CpSolverResponse.decode(bytes);
  return CpSolverResponse.toObject(message, { longs: String });
}

function cloneResponse<T>(value: T): T {
  if (value === null || typeof value !== 'object') {
    return value;
  }
  if (typeof structuredClone === 'function') {
    return structuredClone(value);
  }
  return JSON.parse(JSON.stringify(value)) as T;
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

async function solveViaWorker(
  model: CpSatModelInstance | Uint8Array,
  params: Record<string, unknown> = {},
) {
  const modelBytes = (isCpSatModelInstance(model) ? model.toBinary() : model) as Uint8Array;
  const { CpSolverResponse, SatParameters } = await protoTypesPromise;
  const paramsBytes = encodeSatParameters(params, SatParameters);
  const id = nextWorkerRequestId++;
  activeWorkerSolveId = id;
  try {
    const response = await postWorkerRequest<WorkerSolveResponse>({
      type: 'solve',
      id,
      modelBytes,
      paramsBytes: paramsBytes ?? undefined,
    });
    const bytes = new Uint8Array(response.bytes);
    const decoded = decodeSolverResponse(bytes, CpSolverResponse);
    const responseObject = cloneResponse(decoded);
    return {
      bytes,
      response: responseObject,
    };
  } finally {
    if (activeWorkerSolveId === id) {
      activeWorkerSolveId = null;
    }
  }
}

async function validateViaWorker(model: CpSatModelInstance | Uint8Array) {
  const modelBytes = (isCpSatModelInstance(model) ? model.toBinary() : model) as Uint8Array;
  const id = nextWorkerRequestId++;
  const response = await postWorkerRequest<WorkerValidateResponse>({
    type: 'validate',
    id,
    modelBytes,
  });
  return { ok: response.ok, message: response.message };
}

async function solveDirect(
  model: CpSatModelInstance | Uint8Array,
  params: Record<string, unknown> = {},
) {
  const [{ CpSolverResponse, SatParameters }, Module] = await Promise.all([
    protoTypesPromise,
    modulePromise,
  ]);
  const modelBytes = (isCpSatModelInstance(model) ? model.toBinary() : model) as Uint8Array;
  const paramsBytes = encodeSatParameters(params, SatParameters);

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

  const safeBytes = new Uint8Array(bytes);
  const decoded = decodeSolverResponse(safeBytes, CpSolverResponse);
  const responseObject = cloneResponse(decoded);
  return {
    bytes: safeBytes,
    response: responseObject,
  };
}

async function validateDirect(model: CpSatModelInstance | Uint8Array) {
  const Module = await modulePromise;
  const modelBytes = (isCpSatModelInstance(model) ? model.toBinary() : model) as Uint8Array;
  const lenPtr = Module._malloc(4);
  const modelPtr = copyBytesToHeap(Module, modelBytes);
  let msgPtr = 0;

  try {
    msgPtr = Module.ccall(
      'validate_model',
      'number',
      ['number', 'number', 'number'],
      [modelPtr, modelBytes.length, lenPtr],
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
    // Worker solves previously posted a cancel message that attempted to call interrupt_solve(),
    // but the worker thread is busy while the solver runs. Terminate the worker to guarantee
    // immediate cancellation and spin up a fresh instance on the next solve.
    terminateWorker('Solve canceled by user.');
    activeWorkerSolveId = null;
    return;
  }
  const Module = await modulePromise;
  Module.ccall('interrupt_solve', 'void', [], []);
}

export const CpSat: CpSatApi = {
  CpSatModel,
  createModel,
  solve: (model, params = {}) =>
    (shouldUseWorkerBridge() ? solveViaWorker(model, params) : solveDirect(model, params)),
  validate: (model) => (shouldUseWorkerBridge() ? validateViaWorker(model) : validateDirect(model)),
  getSchemas: () => schemaPromise,
  loadModule: () => modulePromise,
  loadTypes: () => protoTypesPromise,
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
