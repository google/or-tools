/// <reference lib="webworker" />
/// <reference path="./cpsat_worker_types.d.ts" />

importScripts('cp_sat_api.js');

(() => {
  const workerName = (self as DedicatedWorkerGlobalScope).name ?? '';
  const isPThreadWorker = workerName.startsWith('em-pthread');
  if (isPThreadWorker) {
    console.debug('[cpsat_worker] skipping custom bootstrap inside pthread worker', workerName);
    return;
  }

  type CpSatModuleInstance = {
    ccall: (
      name: string,
      returnType: string,
      argTypes: string[],
      args: Array<number | string>,
      opts?: { async?: boolean },
    ) => number | string | Promise<number>;
    HEAPU8: Uint8Array & { buffer: ArrayBuffer };
    _malloc(size: number): number;
    _free(ptr: number): void;
    _free_buffer(ptr: number): void;
  };
  type CpSatModuleFactory = (moduleArg?: Record<string, unknown>) => Promise<CpSatModuleInstance>;

  console.debug(
    '[cpsat_worker] worker name:',
    (self as DedicatedWorkerGlobalScope).name,
    'crossOriginIsolated:',
    (self as DedicatedWorkerGlobalScope).crossOriginIsolated,
  );

  const cpSatModule =
    (globalThis as typeof globalThis & { cpSatModule?: CpSatModuleFactory }).cpSatModule;

  if (!cpSatModule) {
    console.error('[cpsat_worker] cpSatModule factory missing on worker global scope');
    throw new Error('cpSatModule is not available inside worker.');
  }

  let moduleInstance: CpSatModuleInstance | null = null;

  const moduleOptions = {
    print: (...args: unknown[]) => console.log('[cp_sat_api]', ...args),
    printErr: (...args: unknown[]) => console.error('[cp_sat_api]', ...args),
  };

  const readyPromise = cpSatModule(moduleOptions)
    .then((module) => {
      moduleInstance = module;
      postMessage({ type: 'ready' });
    })
    .catch((err) => {
      console.error('[cpsat_worker] cpSatModule init failed:', err);
      postMessage({ type: 'error', id: 0, error: String(err) });
    });

  function readUint32LE(buffer: ArrayBuffer, ptr: number) {
    return new DataView(buffer, ptr, 4).getUint32(0, true);
  }

  function copyBytesToHeap(bytes: Uint8Array | null) {
    if (!moduleInstance || !bytes || !bytes.length) {
      return 0;
    }
    const ptr = moduleInstance._malloc(bytes.length);
    moduleInstance.HEAPU8.set(bytes, ptr);
    return ptr;
  }

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
      if (modelPtr) {
        moduleInstance._free(modelPtr);
      }
      if (paramsPtr) {
        moduleInstance._free(paramsPtr);
      }
    }

    const len = readUint32LE(moduleInstance.HEAPU8.buffer, lenPtr);
    moduleInstance._free(lenPtr);

    if (!responsePtr || len === 0) {
      if (responsePtr) {
        moduleInstance._free_buffer(responsePtr);
      }
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
      if (modelPtr) {
        moduleInstance._free(modelPtr);
      }
    }

    const len = readUint32LE(moduleInstance.HEAPU8.buffer, lenPtr);
    moduleInstance._free(lenPtr);

    if (!msgPtr || len === 0) {
      if (msgPtr) {
        moduleInstance._free_buffer(msgPtr);
      }
      return { ok: true, message: '' };
    }

    const messageBytes = moduleInstance.HEAPU8.slice(msgPtr, msgPtr + len);
    moduleInstance._free_buffer(msgPtr);
    const message = new TextDecoder().decode(messageBytes);
    return { ok: false, message };
  }

  onmessage = async (event) => {
    const message = event.data as WorkerRequest;
    await readyPromise;
    try {
      if (message.type === 'validate') {
        const validation = await validateModel(message.modelBytes);
        postMessage({
          type: 'validateResult',
          id: message.id,
          ok: validation.ok,
          message: validation.message,
        });
        return;
      }
      if (message.type === 'solve') {
        const bytes = await solveModel(message.modelBytes, message.paramsBytes);
        const transfers: Transferable[] = [];
        if (bytes.length) {
          transfers.push(bytes.buffer);
        }
        postMessage(
          {
            type: 'solveResult',
            id: message.id,
            bytes,
          },
          transfers,
        );
        return;
      }
    } catch (error) {
      console.error('[cpsat_worker] request failed', message.type, error);
      postMessage({
        type: 'error',
        id: message.id,
        error: String(error),
      });
    }
  };
})();
