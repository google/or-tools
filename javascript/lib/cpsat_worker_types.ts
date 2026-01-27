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

export type SchemaRequest = {
  type: 'getSchemas';
  id: number;
};

export type CancelSolve = {
  type: 'cancel_solve',
  id: number
}

export type WorkerRequest = SolveRequest | ValidateRequest | SchemaRequest | CancelSolve;

export type WorkerResponse =
  | { type: 'ready' }
  | { type: 'solveResult'; id: number; bytes: Uint8Array }
  | { type: 'validateResult'; id: number; ok: boolean; message: string }
  | { type: 'schemaResult'; id: number; schemas: { cp_model: string; sat_parameters: string } }
  | { type: 'solved_cancelled', id: number }
  | { type: 'error'; id: number; error: string };
