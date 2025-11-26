"use strict";
/// <reference path="./cpsat_worker_types.d.ts" />
const isBrowserMainThread = typeof window !== 'undefined' && typeof document !== 'undefined';
const workerCapable = typeof Worker !== 'undefined';
const workerBridgeAvailable = isBrowserMainThread && workerCapable;
const CPSAT_API_SCRIPT_NAME = 'cpsat_api.js';
function resolveCpsatApiScriptUrl() {
    if (typeof document === 'undefined') {
        return null;
    }
    const currentScript = document.currentScript;
    if (currentScript?.src) {
        return currentScript.src;
    }
    const base = document.baseURI || document.location?.href;
    if (!base) {
        return null;
    }
    const scripts = document.getElementsByTagName('script');
    for (let i = scripts.length - 1; i >= 0; --i) {
        const src = scripts[i].src;
        if (!src) {
            continue;
        }
        try {
            const normalized = new URL(src, base);
            if (normalized.pathname.endsWith(`/${CPSAT_API_SCRIPT_NAME}`)) {
                return normalized.href;
            }
        }
        catch {
            /* ignore invalid URLs */
        }
    }
    return null;
}
const workerScriptUrl = (() => {
    const scriptUrl = resolveCpsatApiScriptUrl();
    return scriptUrl ? new URL('cpsat_worker.js', scriptUrl).href : 'cpsat_worker.js';
})();
const worker = workerBridgeAvailable ? new Worker(workerScriptUrl) : null;
const pendingWorkerRequests = new Map();
let nextWorkerRequestId = 1;
let workerReadyResolve = () => { };
let workerReadyReject = () => { };
const workerReadyPromise = worker
    ? new Promise((resolve, reject) => {
        workerReadyResolve = resolve;
        workerReadyReject = reject;
    })
    : Promise.resolve();
if (worker) {
    worker.onmessage = (event) => {
        const message = event.data;
        if (message.type === 'ready') {
            workerReadyResolve();
            return;
        }
        const pending = pendingWorkerRequests.get(message.id);
        if (message.type === 'error') {
            if (pending) {
                pending.reject(new Error(message.error));
                pendingWorkerRequests.delete(message.id);
            }
            else {
                workerReadyReject?.(new Error(message.error));
            }
            return;
        }
        if (!pending) {
            return;
        }
        pendingWorkerRequests.delete(message.id);
        pending.resolve(message);
    };
    worker.onerror = (event) => {
        workerReadyReject?.(event.error ?? new Error(event.message || 'cpsat_worker error'));
    };
}
async function postWorkerRequest(request) {
    if (!worker) {
        throw new Error('Worker bridge is not available.');
    }
    await workerReadyPromise;
    return new Promise((resolve, reject) => {
        pendingWorkerRequests.set(request.id, {
            resolve: (value) => resolve(value),
            reject,
        });
        const transfers = [];
        if ('paramsBytes' in request && request.paramsBytes) {
            transfers.push(request.paramsBytes.buffer);
        }
        worker.postMessage(request, transfers);
    });
}
(function attachCpSat(global) {
    if (!protobuf) {
        console.error('protobuf.js is required before loading cpsat_api.js');
        return;
    }
    if (!cpSatModule) {
        console.error('cpSatModule is not available.');
        return;
    }
    const modulePromise = cpSatModule();
    const schemaPromise = modulePromise.then((Module) => {
        const getCpSchema = typeof Module.getCpModelSchema === 'function'
            ? Module.getCpModelSchema.bind(Module)
            : () => Module.ccall('get_cp_model_schema', 'string', [], []);
        const getSatSchema = typeof Module.getSatParametersSchema === 'function'
            ? Module.getSatParametersSchema.bind(Module)
            : () => Module.ccall('get_sat_parameters_schema', 'string', [], []);
        return {
            cp_model: getCpSchema(),
            sat_parameters: getSatSchema(),
        };
    });
    const protoTypesPromise = (async () => {
        const { cp_model, sat_parameters } = await schemaPromise;
        const root = new protobuf.Root();
        protobuf.parse(sat_parameters, root);
        protobuf.parse(cp_model, root);
        return {
            CpModelProto: root.lookupType('operations_research.sat.CpModelProto'),
            CpSolverResponse: root.lookupType('operations_research.sat.CpSolverResponse'),
            SatParameters: root.lookupType('operations_research.sat.SatParameters'),
        };
    })();
    class CpSatModel {
        constructor(bytes, metadata = {}) {
            this.bytes = bytes;
            this.metadata = metadata;
        }
        toBinary() {
            return this.bytes;
        }
    }
    async function createModel(protoObject) {
        const { CpModelProto } = await protoTypesPromise;
        const err = CpModelProto.verify(protoObject);
        if (err) {
            throw new Error(`Invalid CpModelProto: ${err}`);
        }
        const bytes = CpModelProto.encode(protoObject).finish();
        return new CpSatModel(bytes, { object: protoObject });
    }
    function readUint32LE(buffer, ptr) {
        return new DataView(buffer, ptr, 4).getUint32(0, true);
    }
    function copyBytesToHeap(Module, bytes) {
        if (!bytes || !bytes.length) {
            return 0;
        }
        const ptr = Module._malloc(bytes.length);
        Module.HEAPU8.set(bytes, ptr);
        return ptr;
    }
    function isCpSatModelInstance(value) {
        return typeof value.toBinary === 'function';
    }
    function encodeSatParameters(params, SatParameters) {
        if (!params || !Object.keys(params).length) {
            return null;
        }
        const err = SatParameters.verify(params);
        if (err) {
            throw new Error(`Invalid SatParameters: ${err}`);
        }
        return SatParameters.encode(params).finish();
    }
    function decodeSolverResponse(bytes, CpSolverResponse) {
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
    function setWorkerBridgePreferred(enabled) {
        workerBridgePreferred = Boolean(enabled);
        if (workerBridgePreferred && !worker) {
            console.warn('Worker bridge requested but no worker is initialized in this environment.');
        }
    }
    async function solveViaWorker(model, params = {}) {
        const modelBytes = (isCpSatModelInstance(model) ? model.toBinary() : model);
        const { CpSolverResponse, SatParameters } = await protoTypesPromise;
        const paramsBytes = encodeSatParameters(params, SatParameters);
        const id = nextWorkerRequestId++;
        const response = await postWorkerRequest({
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
    async function validateViaWorker(model) {
        const modelBytes = (isCpSatModelInstance(model) ? model.toBinary() : model);
        const id = nextWorkerRequestId++;
        const response = await postWorkerRequest({
            type: 'validate',
            id,
            modelBytes,
        });
        return { ok: response.ok, message: response.message };
    }
    async function solveDirect(model, params = {}) {
        const [{ CpSolverResponse, SatParameters }, Module] = await Promise.all([
            protoTypesPromise,
            modulePromise,
        ]);
        const modelBytes = (isCpSatModelInstance(model) ? model.toBinary() : model);
        const paramsBytes = encodeSatParameters(params, SatParameters);
        const lenPtr = Module._malloc(4);
        const modelPtr = copyBytesToHeap(Module, modelBytes);
        const paramsPtr = copyBytesToHeap(Module, paramsBytes);
        let responsePtr = 0;
        try {
            responsePtr = (await Module.ccall('solve_model', 'number', ['number', 'number', 'number', 'number', 'number'], [
                modelPtr,
                modelBytes.length,
                paramsPtr,
                paramsBytes ? paramsBytes.length : 0,
                lenPtr,
            ], { async: true }));
        }
        finally {
            if (modelPtr)
                Module._free(modelPtr);
            if (paramsPtr)
                Module._free(paramsPtr);
        }
        const len = readUint32LE(Module.HEAPU8.buffer, lenPtr);
        Module._free(lenPtr);
        let bytes = new Uint8Array();
        if (responsePtr && len) {
            bytes = Module.HEAPU8.slice(responsePtr, responsePtr + len);
            Module._free_buffer(responsePtr);
        }
        else if (responsePtr) {
            Module._free_buffer(responsePtr);
        }
        const decoded = decodeSolverResponse(bytes, CpSolverResponse);
        return {
            bytes,
            response: decoded,
        };
    }
    async function validateDirect(model) {
        const Module = await modulePromise;
        const modelBytes = (isCpSatModelInstance(model) ? model.toBinary() : model);
        const lenPtr = Module._malloc(4);
        const modelPtr = copyBytesToHeap(Module, modelBytes);
        let msgPtr = 0;
        try {
            msgPtr = Module.ccall('validate_model', 'number', ['number', 'number', 'number'], [modelPtr, modelBytes.length, lenPtr]);
        }
        finally {
            if (modelPtr)
                Module._free(modelPtr);
        }
        const len = readUint32LE(Module.HEAPU8.buffer, lenPtr);
        Module._free(lenPtr);
        if (!msgPtr || len === 0) {
            if (msgPtr)
                Module._free_buffer(msgPtr);
            return { ok: true, message: '' };
        }
        const messageBytes = Module.HEAPU8.slice(msgPtr, msgPtr + len);
        Module._free_buffer(msgPtr);
        const message = new TextDecoder().decode(messageBytes);
        return { ok: false, message };
    }
    const api = {
        CpSatModel,
        createModel,
        solve: (model, params = {}) => (shouldUseWorkerBridge() ? solveViaWorker(model, params) : solveDirect(model, params)),
        validate: (model) => (shouldUseWorkerBridge() ? validateViaWorker(model) : validateDirect(model)),
        getSchemas: () => schemaPromise,
        loadModule: () => modulePromise,
        loadTypes: () => protoTypesPromise,
        setWorkerBridgeEnabled: (enabled) => setWorkerBridgePreferred(enabled),
        isWorkerBridgeEnabled: () => shouldUseWorkerBridge(),
    };
    global.CpSat = api;
})(typeof globalThis !== 'undefined' ? globalThis : this);
//# sourceMappingURL=cpsat_api.js.map