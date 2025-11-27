import type { MainModule } from '../../build/bin/cp_sat_api.js';
import createCpSatModule from '../../build/bin/cp_sat_api.js';
import cpSatWasmUrl from '../../build/bin/cp_sat_api.wasm?url';
import CpSatWorker from './cpsat_worker?worker';
import type { WorkerRequest, WorkerResponse } from './cpsat_worker';

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
const worker = workerBridgeAvailable ? new CpSatWorker() : null;
const pendingWorkerRequests = new Map<
  number,
  {
    resolve(value: WorkerResponse): void;
    reject(reason: unknown): void;
  }
>();
let nextWorkerRequestId = 1;

const workerReadyPromise = (() => {
  if (!worker) return Promise.resolve();
  return new Promise<void>((resolve, reject) => {
    worker.onmessage = (event) => {
      const message = event.data as WorkerResponse;
      if (message.type === 'ready') {
        resolve();
        return;
      }
      const pending = pendingWorkerRequests.get(message.id);
      if (message.type === 'error') {
        if (pending) {
          pending.reject(new Error(message.error));
          pendingWorkerRequests.delete(message.id);
        } else {
          reject(new Error(message.error));
        }
        return;
      }
      if (pending) {
        pendingWorkerRequests.delete(message.id);
        pending.resolve(message);
      }
    };
    worker.onerror = (event) => {
      reject(event.error ?? new Error(event.message || 'cpsat_worker error'));
    };
  });
})();

async function postWorkerRequest<T extends WorkerResponse>(request: WorkerRequest): Promise<T> {
  if (!worker) throw new Error('Worker bridge is not available.');
  await workerReadyPromise;
  return new Promise<T>((resolve, reject) => {
    pendingWorkerRequests.set(request.id, {
      resolve: (value: WorkerResponse) => resolve(value as T),
      reject,
    });
    const transfers: Transferable[] = [];
    if ('paramsBytes' in request && request.paramsBytes) {
      transfers.push(request.paramsBytes.buffer);
    }
    worker.postMessage(request, transfers);
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
  print: (...args: unknown[]) => console.log('[cp_sat_api]', ...args),
  printErr: (...args: unknown[]) => console.error('[cp_sat_api]', ...args),
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

let workerBridgePreferred = Boolean(worker);

function shouldUseWorkerBridge() {
  return workerBridgePreferred && Boolean(worker);
}

function setWorkerBridgePreferred(enabled: boolean) {
  workerBridgePreferred = Boolean(enabled);
  if (workerBridgePreferred && !worker) {
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
  const response = await postWorkerRequest<WorkerSolveResponse>({
    type: 'solve',
    id,
    modelBytes,
    paramsBytes: paramsBytes ?? undefined,
  });
  const decoded = decodeSolverResponse(response.bytes, CpSolverResponse);
  return {
    bytes: response.bytes,
    response: decoded,
  };
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

  const decoded = decodeSolverResponse(bytes, CpSolverResponse);
  return {
    bytes,
    response: decoded,
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
};

if (isBrowserMainThread) {
  (window as Window & { CpSat?: CpSatApi }).CpSat = CpSat;
}

export type { CpSatApi };
export default CpSat;
