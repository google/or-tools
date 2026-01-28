import type { MainModule } from '@internal-wasm/cp_sat_runtime.js';
import { loadCpSat } from './cp_sat_module_loader.js';
import workerScriptUrl from './cpsat_worker.js?worker&url';
import type { WorkerRequest, WorkerResponse } from './cpsat_worker_types.js';
import type { SatParameters } from './generated/sat_parameters.js';

type SchemaPair = {
  cp_model: string;
  sat_parameters: string;
};

export type CpSatSolveResult = {
  response: Record<string, unknown> | null;
  bytes: Uint8Array;
};

type SolverParams = Uint8Array | SatParameters | null;

export type CpSatApi = {
  solve(model: Uint8Array, params?: SolverParams): Promise<CpSatSolveResult>;
  solveRaw(model: Uint8Array, params?: Uint8Array | null): Promise<Uint8Array>;
  validate(model: Uint8Array): Promise<{ ok: boolean; message: string }>;
  getSchemas(): Promise<SchemaPair>;
  createModel(model: Record<string, unknown>): Promise<Uint8Array>;
  loadModule(): Promise<MainModule>;
  setWorkerBridgeEnabled(enabled: boolean): void;
  isWorkerBridgeEnabled(): boolean;
  cancelSolve(): Promise<void>;
};

export type CpSatModelInstance = Uint8Array;

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


let modulePromise: Promise<MainModule> | null = null;

function loadModule() {
  if (shouldUseWorkerBridge()) {
    throw new Error("Wasm should not be loaded on main thread when Worker Bridge is enabled");
  }
  if (!modulePromise) {
    modulePromise = loadCpSat();
  }
  return modulePromise;
}


const schemaPromise: Promise<SchemaPair> = (async () => {
  if (shouldUseWorkerBridge()) {
    // 1. Ask the worker for schemas
    const response = await postWorkerRequest<Extract<WorkerResponse, { type: 'schemaResult' }>>({
      type: 'getSchemas',
      id: nextWorkerRequestId++,
    });
    return response.schemas;
  } else {
    // 2. Fallback to local (Direct) loading only if bridge is disabled
    console.log("AAA load a");
    const Module = await loadModule();
    return {
      cp_model: Module.ccall('get_cp_model_schema', 'string', [], []),
      sat_parameters: Module.ccall('get_sat_parameters_schema', 'string', [], []),
    };
  }
})();

type ProtobufModule = typeof import('protobufjs');
type ProtobufRoot = import('protobufjs').Root;
type CpModelType = import('protobufjs').Type;
type CpSolverResponseType = import('protobufjs').Type;

let protobufModulePromise: Promise<ProtobufModule> | null = null;
let protobufRootPromise: Promise<ProtobufRoot> | null = null;
let cpModelTypePromise: Promise<CpModelType> | null = null;
let cpSolverResponseTypePromise: Promise<CpSolverResponseType> | null = null;
let satParametersTypePromise: Promise<import('protobufjs').Type> | null = null;

function createMissingDependencyError(feature: string) {
  return new Error(
    `CpSat.${feature} requires the optional dependency "protobufjs". Install it with \`npm install protobufjs\` and rebuild.`,
  );
}

async function loadProtobufModule(feature: string): Promise<ProtobufModule> {
  if (!protobufModulePromise) {
    protobufModulePromise = import('protobufjs').catch((error) => {
      protobufModulePromise = null;
      throw createMissingDependencyError(feature);
    });
  }
  try {
    return await protobufModulePromise;
  } catch (error) {
    protobufModulePromise = null;
    throw error;
  }
}

async function resolveProtobufRoot(feature: string): Promise<ProtobufRoot> {
  if (!protobufRootPromise) {
    protobufRootPromise = (async () => {
      const schemas = await schemaPromise;
      const protobufModule = await loadProtobufModule(feature);
      const parsed = protobufModule.parse(schemas.cp_model);
      return parsed.root;
    })();
  }
  try {
    return await protobufRootPromise;
  } catch (error) {
    protobufRootPromise = null;
    throw error;
  }
}

async function resolveCpModelType(): Promise<CpModelType> {
  if (!cpModelTypePromise) {
    cpModelTypePromise = (async () => {
      const root = await resolveProtobufRoot('createModel');
      const cpModelType = root.lookupType('operations_research.sat.CpModelProto');
      if (!cpModelType) {
        throw new Error('CpSat.createModel: cp_model schema did not expose operations_research.sat.CpModelProto.');
      }
      return cpModelType;
    })();
  }
  try {
    return await cpModelTypePromise;
  } catch (error) {
    cpModelTypePromise = null;
    throw error;
  }
}

async function resolveCpSolverResponseType(): Promise<CpSolverResponseType> {
  if (!cpSolverResponseTypePromise) {
    cpSolverResponseTypePromise = (async () => {
      const root = await resolveProtobufRoot('solve');
      const solverType = root.lookupType('operations_research.sat.CpSolverResponse');
      if (!solverType) {
        throw new Error('CpSat.solve: cp_model schema did not expose operations_research.sat.CpSolverResponse.');
      }
      return solverType;
    })();
  }
  try {
    return await cpSolverResponseTypePromise;
  } catch (error) {
    cpSolverResponseTypePromise = null;
    throw error;
  }
}

async function resolveSatParametersType(): Promise<import('protobufjs').Type> {
  if (!satParametersTypePromise) {
    satParametersTypePromise = (async () => {
      const schemas = await schemaPromise;
      const protobufModule = await loadProtobufModule('solve');
      const parsed = protobufModule.parse(schemas.sat_parameters);
      const root = parsed.root;
      const paramsType = root.lookupType('operations_research.sat.SatParameters');
      if (!paramsType) {
        throw new Error('CpSat.solve: sat_parameters schema did not expose operations_research.sat.SatParameters.');
      }
      return paramsType;
    })();
  }
  try {
    return await satParametersTypePromise;
  } catch (error) {
    satParametersTypePromise = null;
    throw error;
  }
}

async function encodeSatParameters(params: Record<string, unknown>): Promise<Uint8Array> {
  const paramsType = await resolveSatParametersType();
  const validationError = paramsType.verify(params);
  if (validationError) {
    throw new Error(`CpSat.solve: ${validationError}`);
  }
  const message = paramsType.create(params);
  return paramsType.encode(message).finish();
}

async function resolveParamsBytes(params?: SolverParams): Promise<Uint8Array | null> {
  if (!params) {
    return null;
  }
  if (params instanceof Uint8Array) {
    return params;
  }
  return encodeSatParameters(params);
}

async function decodeSolverResponse(bytes: Uint8Array): Promise<Record<string, unknown>> {
  const solverType = await resolveCpSolverResponseType();
  const decoded = solverType.decode(bytes);
  return solverType.toObject(decoded, {
    enums: String,
    longs: Number,
    defaults: true,
    arrays: true,
    objects: true,
  }) as Record<string, unknown>;
}

async function createModel(model: Record<string, unknown>): Promise<Uint8Array> {
  const type = await resolveCpModelType();
  const validationError = type.verify(model);
  if (validationError) {
    throw new Error(`CpSat.createModel: ${validationError}`);
  }
  const message = type.create(model);
  return type.encode(message).finish();
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


type WorkerSolveResponse = Extract<WorkerResponse, { type: 'solveResult' }>;
type WorkerValidateResponse = Extract<WorkerResponse, { type: 'validateResult' }>;

async function solveRawViaWorker(modelBytes: Uint8Array, paramsBytes: Uint8Array | null = null) {
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
    return bytes;
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

async function solveRawDirect(modelBytes: Uint8Array, paramsBytes: Uint8Array | null = null) {
  console.log("AAA load b")
  const Module = await loadModule();

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

async function solveRaw(modelBytes: Uint8Array, paramsBytes: Uint8Array | null = null) {
  return shouldUseWorkerBridge()
    ? solveRawViaWorker(modelBytes, paramsBytes)
    : solveRawDirect(modelBytes, paramsBytes);
}

async function solve(modelBytes: Uint8Array, params: SolverParams = null): Promise<CpSatSolveResult> {
  const paramsBytes = await resolveParamsBytes(params);
  const bytes = await solveRaw(modelBytes, paramsBytes);
  let response: Record<string, unknown> | null = null;
  if (bytes.length > 0) {
    response = await decodeSolverResponse(bytes);
  }
  return { bytes, response };
}

async function validateDirect(model: Uint8Array) {
  console.log("AAA load c")
  const Module = await loadModule();
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
  if (shouldUseWorkerBridge()) {
    if (worker && activeWorkerSolveId !== null) {
      // await postWorkerRequest<WorkerSolveResponse>({
      //   type: 'cancel_solve',
      // });
      terminateWorker('Solve canceled by user.');
      activeWorkerSolveId = null;
    }
  } else {
    console.log("AAA load d")
    const Module = await loadModule();
    Module.ccall('interrupt_solve', 'void', [], []);
  }
}

export const CpSat: CpSatApi = {
  solve: (model, params = null) => solve(model, params),
  solveRaw: (model, params = null) => solveRaw(model, params),
  validate: (model) => (shouldUseWorkerBridge() ? validateViaWorker(model) : validateDirect(model)),
  getSchemas: () => schemaPromise,
  createModel,
  loadModule,
  cancelSolve,
  setWorkerBridgeEnabled: (enabled: boolean) => setWorkerBridgePreferred(enabled),
  isWorkerBridgeEnabled: () => shouldUseWorkerBridge(),
};

if (isBrowserMainThread) {
  (window as Window & { CpSat?: CpSatApi }).CpSat = CpSat;
}

export type { WorkerRequest, WorkerResponse } from './cpsat_worker_types.js';
export default CpSat;
