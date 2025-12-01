import type { CpSatApi } from './cp_sat_api';

declare global {
  interface Window {
    CpSat?: CpSatApi;
  }
}

export {};
