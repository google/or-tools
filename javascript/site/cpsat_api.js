"use strict";
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
    async function solve(model, params = {}) {
        const [{ CpSolverResponse, SatParameters }, Module] = await Promise.all([
            protoTypesPromise,
            modulePromise,
        ]);
        const modelBytes = (isCpSatModelInstance(model) ? model.toBinary() : model);
        let paramsBytes = null;
        if (params && Object.keys(params).length) {
            const err = SatParameters.verify(params);
            if (err) {
                throw new Error(`Invalid SatParameters: ${err}`);
            }
            paramsBytes = SatParameters.encode(params).finish();
        }
        const lenPtr = Module._malloc(4);
        const modelPtr = copyBytesToHeap(Module, modelBytes);
        const paramsPtr = copyBytesToHeap(Module, paramsBytes);
        let responsePtr = 0;
        try {
            responsePtr = await Module.ccall('solve_model', 'number', ['number', 'number', 'number', 'number', 'number'], [
                modelPtr,
                modelBytes.length,
                paramsPtr,
                paramsBytes ? paramsBytes.length : 0,
                lenPtr,
            ]);
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
        let response = null;
        if (responsePtr && len) {
            bytes = Module.HEAPU8.slice(responsePtr, responsePtr + len);
            Module._free_buffer(responsePtr);
            const message = CpSolverResponse.decode(bytes);
            response = CpSolverResponse.toObject(message, { longs: String });
        }
        else if (responsePtr) {
            Module._free_buffer(responsePtr);
        }
        return { bytes, response };
    }
    async function validate(model) {
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
        solve,
        validate,
        getSchemas: () => schemaPromise,
        loadModule: () => modulePromise,
        loadTypes: () => protoTypesPromise,
    };
    global.CpSat = api;
})(typeof globalThis !== 'undefined' ? globalThis : this);
//# sourceMappingURL=cpsat_api.js.map