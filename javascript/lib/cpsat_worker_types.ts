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
