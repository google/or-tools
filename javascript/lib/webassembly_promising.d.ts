declare global {
  interface WebAssemblyConstructor {
    promising?: (...args: unknown[]) => unknown;
  }
}

export {};
