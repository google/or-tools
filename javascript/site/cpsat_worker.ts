import type { MainModule } from '../../build/bin/cp_sat_api.js';
import createCpSatModule from '../../build/bin/cp_sat_api.js';
import cpSatWasmUrl from '../../build/bin/cp_sat_api.wasm?url';

export type SolveRequest = {
  type: 'solve';
  id: number;
  modelBytes: Uint8Array;
  paramsBytes?: Uint8Array;
};

export type ValidateRequest = {
  type: 'validate';
  id: number;
  modelBytes: Uint8Array;
};

export type WorkerRequest = SolveRequest | ValidateRequest;

export type WorkerResponse =
  | { type: 'ready' }
  | { type: 'solveResult'; id: number; bytes: Uint8Array }
  | { type: 'validateResult'; id: number; ok: boolean; message: string }
  | { type: 'error'; id: number; error: string };

const workerScope = self as DedicatedWorkerGlobalScope;

const locateCpSatAsset = (path: string) => {
  if (path.endsWith('.wasm')) {
    return cpSatWasmUrl;
  }
  return path;
};

const moduleOptions = {
  locateFile: (path: string) => locateCpSatAsset(path),
  print: (...args: unknown[]) => console.log('[cp_sat_api]', ...args),
  printErr: (...args: unknown[]) => console.error('[cp_sat_api]', ...args),
};

let moduleInstance: MainModule | null = null;

const moduleReady = createCpSatModule(moduleOptions)
  .then((module) => {
    moduleInstance = module;
    workerScope.postMessage({ type: 'ready' } satisfies WorkerResponse);
    return module;
  })
  .catch((error) => {
    console.error('[cpsat_worker] cpSatModule init failed:', error);
    workerScope.postMessage({
      type: 'error',
      id: 0,
      error: String(error),
    } satisfies WorkerResponse);
    throw error;
  });

const readUint32LE = (buffer: ArrayBuffer, ptr: number) =>
  new DataView(buffer, ptr, 4).getUint32(0, true);

const copyBytesToHeap = (bytes: Uint8Array | null) => {
  if (!moduleInstance || !bytes?.length) {
    return 0;
  }
  const ptr = moduleInstance._malloc(bytes.length);
  moduleInstance.HEAPU8.set(bytes, ptr);
  return ptr;
};

async function solveModel(modelBytes: Uint8Array, paramsBytes?: Uint8Array) {
  if (!moduleInstance) {
    throw new Error('Module not initialized.');
  }
  const lenPtr = moduleInstance._malloc(4);
  const modelPtr = copyBytesToHeap(modelBytes);
  const paramsPtr = copyBytesToHeap(paramsBytes ?? null);
  let responsePtr = 0;

  try {
    responsePtr = (await moduleInstance.ccall(
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
    if (modelPtr) moduleInstance._free(modelPtr);
    if (paramsPtr) moduleInstance._free(paramsPtr);
  }

  const len = readUint32LE(moduleInstance.HEAPU8.buffer, lenPtr);
  moduleInstance._free(lenPtr);

  if (!responsePtr || len === 0) {
    if (responsePtr) moduleInstance._free_buffer(responsePtr);
    return new Uint8Array();
  }

  const bytes = moduleInstance.HEAPU8.slice(responsePtr, responsePtr + len);
  moduleInstance._free_buffer(responsePtr);
  return bytes;
}

async function validateModel(modelBytes: Uint8Array) {
  if (!moduleInstance) {
    throw new Error('Module not initialized.');
  }
  const lenPtr = moduleInstance._malloc(4);
  const modelPtr = copyBytesToHeap(modelBytes);
  let msgPtr = 0;

  try {
    msgPtr = moduleInstance.ccall(
      'validate_model',
      'number',
      ['number', 'number', 'number'],
      [modelPtr, modelBytes.length, lenPtr],
    ) as number;
  } finally {
    if (modelPtr) moduleInstance._free(modelPtr);
  }

  const len = readUint32LE(moduleInstance.HEAPU8.buffer, lenPtr);
  moduleInstance._free(lenPtr);

  if (!msgPtr || len === 0) {
    if (msgPtr) moduleInstance._free_buffer(msgPtr);
    return { ok: true, message: '' };
  }

  const messageBytes = moduleInstance.HEAPU8.slice(msgPtr, msgPtr + len);
  moduleInstance._free_buffer(msgPtr);
  const message = new TextDecoder().decode(messageBytes);
  return { ok: false, message };
}

workerScope.onmessage = async (event) => {
  const message = event.data as WorkerRequest;
  try {
    await moduleReady;
    if (!moduleInstance) {
      throw new Error('Module not initialized.');
    }

    if (message.type === 'validate') {
      const validation = await validateModel(message.modelBytes);
      workerScope.postMessage({
        type: 'validateResult',
        id: message.id,
        ok: validation.ok,
        message: validation.message,
      } satisfies WorkerResponse);
      return;
    }

    if (message.type === 'solve') {
      const bytes = await solveModel(message.modelBytes, message.paramsBytes);
      const transfers: Transferable[] = bytes.length ? [bytes.buffer] : [];
      workerScope.postMessage(
        {
          type: 'solveResult',
          id: message.id,
          bytes,
        } satisfies WorkerResponse,
        transfers,
      );
      return;
    }
  } catch (error) {
    console.error('[cpsat_worker] request failed', message?.type, error);
    workerScope.postMessage({
      type: 'error',
      id: message?.id ?? 0,
      error: String(error),
    } satisfies WorkerResponse);
  }
};
