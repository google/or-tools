type SchemaPair = {
  cp_model: string;
  sat_parameters: string;
};

type CpSatModelInstance = {
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

interface ProtobufRoot {
  lookupType(name: string): ProtobufType;
}

interface ProtobufNamespace {
  Root: new () => ProtobufRoot;
  parse(protoText: string, root?: ProtobufRoot): { root: ProtobufRoot };
}

interface CpSatModuleInstance {
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
  getCpModelSchema?: () => string;
  getSatParametersSchema?: () => string;
}

type CpSatModuleFactory = () => Promise<CpSatModuleInstance>;

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
  loadModule(): Promise<CpSatModuleInstance>;
  loadTypes(): Promise<{
    CpModelProto: ProtobufType;
    CpSolverResponse: ProtobufType;
    SatParameters: ProtobufType;
  }>;
};

declare const protobuf: ProtobufNamespace | undefined;
declare const cpSatModule: CpSatModuleFactory | undefined;

(function attachCpSat(global: typeof globalThis & { CpSat?: CpSatApi }) {
  if (!protobuf) {
    console.error('protobuf.js is required before loading cpsat_api.js');
    return;
  }
  if (!cpSatModule) {
    console.error('cpSatModule is not available.');
    return;
  }

  const modulePromise = cpSatModule();
  const schemaPromise: Promise<SchemaPair> = modulePromise.then((Module) => {
    const getCpSchema =
      typeof Module.getCpModelSchema === 'function'
        ? Module.getCpModelSchema.bind(Module)
        : () =>
            Module.ccall('get_cp_model_schema', 'string', [], []) as unknown as string;
    const getSatSchema =
      typeof Module.getSatParametersSchema === 'function'
        ? Module.getSatParametersSchema.bind(Module)
        : () =>
            Module.ccall('get_sat_parameters_schema', 'string', [], []) as unknown as string;
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

  function readUint32LE(buffer: ArrayBuffer, ptr: number) {
    return new DataView(buffer, ptr, 4).getUint32(0, true);
  }

  function copyBytesToHeap(Module: CpSatModuleInstance, bytes: Uint8Array | null) {
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

  async function solve(
    model: CpSatModelInstance | Uint8Array,
    params: Record<string, unknown> = {},
  ) {
    const [{ CpSolverResponse, SatParameters }, Module] = await Promise.all([
      protoTypesPromise,
      modulePromise,
    ]);
    const modelBytes = (isCpSatModelInstance(model) ? model.toBinary() : model) as Uint8Array;

    let paramsBytes: Uint8Array | null = null;
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
    let response: unknown = null;
    if (responsePtr && len) {
      bytes = Module.HEAPU8.slice(responsePtr, responsePtr + len);
      Module._free_buffer(responsePtr);
      const message = CpSolverResponse.decode(bytes);
      response = CpSolverResponse.toObject(message, { longs: String });
    } else if (responsePtr) {
      Module._free_buffer(responsePtr);
    }

    return { bytes, response };
  }

  async function validate(model: CpSatModelInstance | Uint8Array) {
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

  const api: CpSatApi = {
    CpSatModel,
    createModel,
    solve,
    validate,
    getSchemas: () => schemaPromise,
    loadModule: () => modulePromise,
    loadTypes: () => protoTypesPromise,
  };

  global.CpSat = api;
})(typeof globalThis !== 'undefined' ? globalThis : (this as never));
