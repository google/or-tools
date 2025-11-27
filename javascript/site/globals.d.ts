import type { CpSatApi } from './cpsat_api';

declare global {
  interface Window {
    CpSat?: CpSatApi;
  }
}

export {};
